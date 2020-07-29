#define _GNU_SOURCE
#include "log.h"
#include "parameters.h"
#include "rdma.h"
#include "rperf.h"
#include "tcpsocket.h"

void *client_ackrtt_post(void *arg) {
    int ret                        = 0;
    int ret2                       = 0;
    long thread_id                 = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx                 = ((struct ThreadInfo *)arg)->ib_con_idx;
    struct IBConnection *ib_con    = &(ib_connections[ib_con_idx]);
    struct IBConnection *loopback1 = &(loopbacks[2 * ib_con_idx]);
    uint32_t samples_count         = 0;
    uint64_t gap_cycle             = 0;
    int qp_idx                     = 0;
    int stop                       = 0;
    char *buf_ptr                  = NULL;
    char *buf_ptr_loopback         = NULL;
    int buf_offset                 = 0;
    long long ops_count            = 0;
    double duration                = 0.0;
    double throughput              = 0.0;
    double bandwidth               = 0.0;
    struct timeval start, end;

    /* rdtsc initialization */
    init_rdtsc(1, 0);

    optimize_thread(thread_id);

    srand((unsigned)time(NULL));

    /** we should calculate the number of outgoing packets per second. */
    if (user_parameter.bw_limiter) {
        gap_cycle = calc_gap_cycle(ib_con);
    }

    ib_con->posted_wr[qp_idx]    = 0;
    loopback1->posted_wr[qp_idx] = 0;
    /* pre-post sends */
    buf_ptr          = ib_con->ib_buf[qp_idx];
    buf_ptr_loopback = loopback1->ib_buf[qp_idx];
    ret = post_send(user_parameter.msg_size, ib_con->mr[qp_idx]->lkey, 0,
        MSG_REGULAR, ib_con->qp[qp_idx], buf_ptr, 1);
    check(ret == 0, "thread[%ld]: failed to post send", thread_id);

    buf_offset = (buf_offset + 1) % user_parameter.tx_depth;
    ops_count += 1;

    LOG_FILE("Thread[%ld]: ready to send", thread_id);
    /** generating messages */
    gettimeofday(&start, NULL);
    struct timespec timer, loopback_timer;

    while (!stop) {
        buf_ptr = ib_con->ib_buf[qp_idx];
        while (ib_con->posted_wr[qp_idx] - ib_con->completed_wr[qp_idx] <=
               user_parameter.tx_depth) {
            ops_count += 1;
            buf_offset = (buf_offset + 1) % user_parameter.tx_depth;
            LOG_VERBOSE(
                "Post: Number of posted: [%d], Number of completed: [%d]",
                ib_con->posted_wr[qp_idx], ib_con->completed_wr[qp_idx]);
            if (user_parameter.sampling && is_sample() &&
                ops_count > NUM_WARMING_UP_OPS) {
                LOG_DEBUG(
                    "Loopback Post: Number of posted: [%d], Number of "
                    "completed: [%d]",
                    loopback1->posted_wr[qp_idx],
                    loopback1->completed_wr[qp_idx]);
                get_rdtsc_timespec(&timer);
                ret = post_send(user_parameter.msg_size,
                    ib_con->mr[qp_idx]->lkey, samples_count, samples_count,
                    ib_con->qp[qp_idx], buf_ptr + buf_offset, 1);
                get_rdtsc_timespec(&loopback_timer);
                ret2 = post_send(user_parameter.msg_size,
                    loopback1->mr[qp_idx]->lkey, samples_count, samples_count,
                    loopback1->qp[qp_idx], buf_ptr_loopback + buf_offset, 1);
                ib_con->samples_start[samples_count]    = timer;
                loopback1->samples_start[samples_count] = loopback_timer;
                LOG_DEBUG("Post SEND loopback1 sample[%d]", samples_count);
                /** cycles_t start = RDTSC(); */
                check(ret == 0,
                    "thread[%ld]: failed to post send for sample "
                    "packet. Errno: [%d]",
                    thread_id, ret);
                check(ret2 == 0,
                    "thread[%ld]: failed to post loopback send for sample "
                    "packet. Errno: [%d]",
                    thread_id, ret2);
                /** ib_con->samples_start[samples_count] = start; */
                samples_count += 1;
                loopback1->posted_wr[qp_idx]++;
            } else {
                post_send(user_parameter.msg_size, ib_con->mr[qp_idx]->lkey,
                    MSG_REGULAR, MSG_REGULAR, ib_con->qp[qp_idx], buf_ptr, 1);
            }
            ib_con->posted_wr[qp_idx]++;
        }
        if (ops_count > ib_con->message_per_thread) {
            break;
        }
    }
    ib_con->samples_count += samples_count;
    gettimeofday(&end, NULL);
    /* dump statistics */
    LOG_FILE("Ops Posted:%lld, Ops Completed:%lld, Samples:%d", ops_count,
        ib_con->regular_messages_count, samples_count);
    duration = (double)((end.tv_sec - start.tv_sec) * 1000000 +
                        (end.tv_usec - start.tv_usec));
    LOG_FILE("thread[%ld]: duration = %fs, message size = %dB\n", thread_id,
        duration / 1000000, user_parameter.msg_size);

    throughput = (double)(ops_count) / duration;
    LOG_FILE("thread[%ld]: throughput = %f (Mops/s)\n", thread_id, throughput);

    bandwidth = (double)(ops_count * user_parameter.msg_size * 8) /
                ((duration / 1000000) * (1024 * 1024 * 1024));

    LOG_FILE("thread[%ld]: bandwidth = %f (Gbs/s)\n", thread_id, bandwidth);

    pthread_exit((void *)0);

error:
    pthread_exit((void *)-1);
}

void *client_ackrtt_poll(void *arg) {
    int i                       = 0;
    int n                       = 0;
    int qp_idx                  = 0;
    int num_wc                  = 1;
    long thread_id              = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx              = ((struct ThreadInfo *)arg)->ib_con_idx;
    struct IBConnection *ib_con = &(ib_connections[ib_con_idx]);
    struct ibv_cq **cq          = ib_con->cq_send;
    struct ibv_wc *wc           = NULL;
    long long ops_count         = 0;
    uint32_t idx                = 0;

    optimize_thread(thread_id);

    wc = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    check(wc, "thread[%ld]: failed to allocate wc", thread_id);
    LOG_INFO("Polling sends.");

    ib_con->completed_wr[qp_idx] = 0;

    struct timespec timer;
    while (ib_con->polling) {
        LOG_VERBOSE("Poll: Number of posted: [%d], Number of completed: [%d]",
            ib_con->posted_wr[qp_idx], ib_con->completed_wr[qp_idx]);
        /* poll cq */
        n = ibv_poll_cq(cq[qp_idx], num_wc, wc);
        if (n < 0) {
            check(0, "thread[%ld]: Failed to poll cq", thread_id);
        } else if (n > 0) {
            idx = wc[i].wr_id;
            if (idx != MSG_REGULAR && idx != MSG_CTL_STOP) {
                get_rdtsc_timespec(&timer);
                ib_con->samples_end[idx] = timer;
                LOG_VERBOSE("Sent sample[%d]", idx);
            }
            if (wc[i].status != IBV_WC_SUCCESS) {
                if (wc[i].opcode == IBV_WC_SEND) {
                    check(0, "thread[%ld]: send failed status: %s", thread_id,
                        ibv_wc_status_str(wc[i].status));
                } else {
                    check(0, "thread[%ld]: recv failed status: %s", thread_id,
                        ibv_wc_status_str(wc[i].status));
                }
            }
            ops_count += 1;
            ib_con->completed_wr[qp_idx]++;
            LOG_VERBOSE("Ops Count:%lld", ops_count);
        }
    }
    if (wc != NULL) {
        free(wc);
    }
    pthread_exit((void *)0);
error:
    if (wc != NULL) {
        free(wc);
    }
    pthread_exit((void *)-1);
}

void *client_loopback_poll(void *arg) {
    int i                          = 0;
    int n                          = 0;
    int qp_idx                     = 0;
    int num_wc                     = 1;
    long thread_id                 = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx                 = ((struct ThreadInfo *)arg)->ib_con_idx;
    struct IBConnection *loopback1 = &(loopbacks[2 * ib_con_idx]);
    struct IBConnection *ib_con    = &(ib_connections[ib_con_idx]);
    struct ibv_wc *wc              = NULL;
    uint32_t idx                   = 0;

    optimize_thread(thread_id);

    loopback1->completed_wr[qp_idx] = 0;

    wc = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    check(wc, "thread[%ld]: failed to allocate wc", thread_id);
    LOG_INFO("Polling loopback sends.");

    struct timespec timer;
    while (ib_con->polling) {
        LOG_VERBOSE("Loopback Poll Send: Number of posted: [%d], Number of completed: [%d]",
            loopback1->posted_wr[qp_idx], loopback1->completed_wr[qp_idx]);
        /* poll cq */
        n = ibv_poll_cq(loopback1->cq_send[qp_idx], num_wc, wc);
        get_rdtsc_timespec(&timer);
        if (n < 0) {
            check(0, "thread[%ld]: Failed to poll cq", thread_id);
        } else if (n > 0) {
            idx = wc[i].wr_id;
            LOG_DEBUG("Poll SEND loopback1", idx);
            if (idx != MSG_REGULAR && idx != MSG_CTL_STOP) {
                loopback1->samples_end[idx] = timer;
                LOG_DEBUG("Poll SEND loopback1 sample[%d]", idx);
                loopback1->completed_wr[qp_idx]++;
            }
            if (wc[i].status != IBV_WC_SUCCESS) {
                if (wc[i].opcode == IBV_WC_SEND) {
                    check(0, "thread[%ld]: send failed status: %s", thread_id,
                        ibv_wc_status_str(wc[i].status));
                } else {
                    check(0, "thread[%ld]: recv failed status: %s", thread_id,
                        ibv_wc_status_str(wc[i].status));
                }
            }
        }
    }
    if (wc != NULL) {
        free(wc);
    }
    pthread_exit((void *)0);
error:
    if (wc != NULL) {
        free(wc);
    }
    pthread_exit((void *)-1);
}

void *client_loopback_recv(void *arg) {
    int i                          = 0;
    int n                          = 0;
    int qp_idx                     = 0;
    int ret                        = 0;
    int num_wc                     = 1000;
    long thread_id                 = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx                 = ((struct ThreadInfo *)arg)->ib_con_idx;
    struct IBConnection *ib_con    = &(ib_connections[ib_con_idx]);
    struct IBConnection *loopback2 = &(loopbacks[2 * ib_con_idx + 1]);
    struct ibv_wc *wc              = NULL;
    uint32_t idx                   = 0;
    int msg_size                   = user_parameter.msg_size;
    char *buf_ptr                  = loopback2->ib_buf[qp_idx];

    optimize_thread(thread_id);

    wc = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    check(wc, "thread[%ld]: failed to allocate wc", thread_id);
    LOG_INFO("Polling loopback recieves.");

    for (i = 0; i < user_parameter.rx_depth; i++) {
        ret = post_recv(msg_size, loopback2->mr[qp_idx]->lkey,
            (uint64_t)buf_ptr, loopback2->qp[qp_idx], buf_ptr);
        check(ret == 0, "Failed to post a loopback receive. Errno: [%d]", ret);
        LOG_DEBUG("Post RECV loopback2 sample[%d]", i);
    }
    while (ib_con->polling) {
        /* poll cq */
        n = ibv_poll_cq(loopback2->cq_recv[qp_idx], num_wc, wc);
        if (n < 0) {
            check(0, "thread[%ld]: Failed to poll cq", thread_id);
        }

        for (i = 0; i < n; i++) {
            LOG_DEBUG("Recieves [%d]", idx);
            ret = post_recv(msg_size, loopback2->mr[qp_idx]->lkey,
                (uint64_t)buf_ptr, loopback2->qp[qp_idx], buf_ptr);
            check(ret == 0, "Failed to post a loopback receive. Errno: [%d]",
                ret);
            if ((idx = ntohl(wc[i].imm_data)) != MSG_REGULAR) {
                LOG_DEBUG("Poll RECV loopback2 sample[%d]", idx);
            }

            if (wc[i].status != IBV_WC_SUCCESS) {
                if (wc[i].opcode == IBV_WC_SEND) {
                    check(0, "thread[%ld]: send failed status: %s", thread_id,
                        ibv_wc_status_str(wc[i].status));
                } else {
                    check(0, "thread[%ld]: recv failed status: %s", thread_id,
                        ibv_wc_status_str(wc[i].status));
                }
            }
        }
    }
    if (wc != NULL) {
        free(wc);
    }
    pthread_exit((void *)0);
error:
    if (wc != NULL) {
        free(wc);
    }
    pthread_exit((void *)-1);
}

void *client_poll_send_func(void *arg) {
    int qps_number              = user_parameter.qps_number;
    int i                       = 0;
    int n                       = 0;
    int qp_idx                  = 0;
    int num_wc                  = 400;
    long thread_id              = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx              = ((struct ThreadInfo *)arg)->ib_con_idx;
    struct IBConnection *ib_con = &(ib_connections[ib_con_idx]);
    struct ibv_cq **cq          = ib_con->cq_send;
    struct ibv_wc **wc          = NULL;
    long long ops_count         = 0;
    uint32_t idx                = 0;

    optimize_thread(thread_id);

    wc = (struct ibv_wc **)calloc(qps_number, sizeof(struct ibv_wc *));
    check(wc != NULL, "thread[%ld]: failed to allocate wc", thread_id);
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        wc[qp_idx] = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
        check(wc[qp_idx], "thread[%ld]: failed to allocate wc# %d", thread_id,
            qp_idx);
    }
    LOG_INFO("Polling sends.");

    struct timespec timer;
    while (ib_con->polling) {
        /* poll cq */
        for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
            n = ibv_poll_cq(cq[qp_idx], num_wc, wc[qp_idx]);
            if (n < 0) {
                check(0, "thread[%ld]: Failed to poll cq", thread_id);
            } else if (n > 0) {
                __sync_add_and_fetch(&(ib_con->completed_wr[qp_idx]), n);
            }

            for (i = 0; i < n; i++) {
                idx = wc[qp_idx][i].wr_id;
                if (idx != MSG_REGULAR && idx != MSG_CTL_STOP) {
                    get_rdtsc_timespec(&timer);
                    ib_con->samples_end[idx] = timer;
                    LOG_VERBOSE("Sent sample[%d]", idx);
                }
                if (wc[qp_idx][i].status != IBV_WC_SUCCESS) {
                    if (wc[qp_idx][i].opcode == IBV_WC_SEND) {
                        check(0, "thread[%ld]: send failed status: %s",
                            thread_id, ibv_wc_status_str(wc[qp_idx][i].status));
                    } else {
                        check(0, "thread[%ld]: recv failed status: %s",
                            thread_id, ibv_wc_status_str(wc[qp_idx][i].status));
                    }
                }
                ops_count += 1;
            }
        }
        LOG_VERBOSE("Ops Count:%lld", ops_count);
    }
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        if (wc[qp_idx] != NULL) {
            free(wc[qp_idx]);
        }
    }
    free(wc);
    pthread_exit((void *)0);
error:
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        if (wc[qp_idx] != NULL) {
            free(wc[qp_idx]);
        }
    }
    if (wc != NULL) {
        free(wc);
    }
    pthread_exit((void *)-1);
}

void *client_poll_recv_func(void *arg) {
    int qps_number              = user_parameter.qps_number;
    int i                       = 0;
    int n                       = 0;
    int qp_idx                  = 0;
    int num_wc                  = 1000;
    long thread_id              = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx              = ((struct ThreadInfo *)arg)->ib_con_idx;
    struct IBConnection *ib_con = &(ib_connections[ib_con_idx]);
    struct ibv_cq **cq          = ib_con->cq_recv;
    struct ibv_wc **wc          = NULL;
    uint32_t idx                = 0;

    optimize_thread(thread_id);

    wc = (struct ibv_wc **)calloc(qps_number, sizeof(struct ibv_wc *));
    check(wc != NULL, "thread[%ld]: failed to allocate wc", thread_id);
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        wc[qp_idx] = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
        check(wc[qp_idx], "thread[%ld]: failed to allocate wc# %d", thread_id,
            qp_idx);
    }
    LOG_FILE("Polling recievs");
    while (ib_con->polling) {
        /* poll cq */
        for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
            n = ibv_poll_cq(cq[qp_idx], num_wc, wc[qp_idx]);
            if (n < 0) {
                check(0, "thread[%ld]: Failed to poll cq", thread_id);
            }

            for (i = 0; i < n; i++) {
                LOG_VERBOSE("Recieves [%d]", idx);
                if ((idx = ntohl(wc[qp_idx][i].imm_data)) != MSG_REGULAR) {
                    LOG_VERBOSE("Recieves sample[%d]", idx);
                }

                if (wc[qp_idx][i].status != IBV_WC_SUCCESS) {
                    if (wc[qp_idx][i].opcode == IBV_WC_SEND) {
                        check(0, "thread[%ld]: send failed status: %s",
                            thread_id, ibv_wc_status_str(wc[qp_idx][i].status));
                    } else {
                        check(0, "thread[%ld]: recv failed status: %s",
                            thread_id, ibv_wc_status_str(wc[qp_idx][i].status));
                    }
                }
            }
        }
    }
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        if (wc[qp_idx] != NULL) {
            free(wc[qp_idx]);
        }
    }
    free(wc);
    pthread_exit((void *)0);
error:
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        if (wc[qp_idx] != NULL) {
            free(wc[qp_idx]);
        }
    }
    if (wc != NULL) {
        free(wc);
    }
    pthread_exit((void *)-1);
}

void *server_post_recv_func(void *arg) {
    long thread_id              = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx              = ((struct ThreadInfo *)arg)->ib_con_idx;
    struct IBConnection *ib_con = &(ib_connections[ib_con_idx]);
    char *buf_ptr               = NULL;
    int qp_idx                  = 0;
    int msg_size                = user_parameter.msg_size;
    struct ibv_qp **qp          = ib_con->qp;

    optimize_thread(thread_id);

    while (ib_con->polling) {
        buf_ptr = ib_con->ib_buf[qp_idx];
        while (
            ib_con->recv_posted_wr[qp_idx] - ib_con->recv_completed_wr[qp_idx] <
            user_parameter.rx_depth) {
            LOG_VERBOSE(
                "Post: Number of posted: [%d], Number of completed: [%d]",
                ib_con->recv_posted_wr[qp_idx],
                ib_con->recv_completed_wr[qp_idx]);
            post_recv(msg_size, ib_con->mr[qp_idx]->lkey, (uint64_t)buf_ptr,
                qp[qp_idx], buf_ptr);
            __sync_add_and_fetch(&(ib_con->recv_posted_wr[qp_idx]), 1);
        }
    }
    pthread_exit((void *)0);
}

void *server_poll_send_func(void *arg) {
    int i                       = 0;
    int n                       = 0;
    long thread_id              = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx              = ((struct ThreadInfo *)arg)->ib_con_idx;
    struct IBConnection *ib_con = &(ib_connections[ib_con_idx]);
    int qps_number              = user_parameter.qps_number;
    int num_wc                  = 200;
    int qp_idx                  = 0;

    struct ibv_cq **cq = ib_con->cq_send;
    struct ibv_wc **wc = NULL;

    optimize_thread(thread_id);

    wc = (struct ibv_wc **)calloc(qps_number, sizeof(struct ibv_wc *));
    check(wc != NULL, "thread[%ld]: failed to allocate wc", thread_id);

    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        wc[qp_idx] = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    }

    while (ib_con->polling) {
        /* poll cq */
        for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
            n = ibv_poll_cq(cq[qp_idx], num_wc, wc[qp_idx]);
            if (n < 0) {
                check(0, "thread[%ld]: Failed to poll cq", thread_id);
            } else if (n > 0) {
                __sync_add_and_fetch(&(ib_con->completed_wr[qp_idx]), n);
            }

            for (i = 0; i < n; i++) {
                if (wc[qp_idx][i].status != IBV_WC_SUCCESS) {
                    if (wc[qp_idx][i].opcode == IBV_WC_SEND) {
                        check(0, "thread[%ld]: send failed status: %s",
                            thread_id, ibv_wc_status_str(wc[qp_idx][i].status));
                    } else {
                        check(0, "thread[%ld]: recv failed status: %s",
                            thread_id, ibv_wc_status_str(wc[qp_idx][i].status));
                    }
                }
            }
        }
    }

    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        if (wc[qp_idx] != NULL) {
            free(wc[qp_idx]);
        }
    }
    free(wc);
    pthread_exit((void *)0);
error:
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        if (wc[qp_idx] != NULL) {
            free(wc[qp_idx]);
        }
    }
    if (wc != NULL) {
        free(wc);
    }
    pthread_exit((void *)-1);
}

void *server_thread(void *arg) {
    int ret = 0, i = 0, n = 0;
    long thread_id              = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx              = ((struct ThreadInfo *)arg)->ib_con_idx;
    struct IBConnection *ib_con = &(ib_connections[ib_con_idx]);
    int msg_size                = user_parameter.msg_size;
    int qps_number              = user_parameter.qps_number;
    int num_wc                  = 8000;
    int qp_idx                  = 0;

    struct ibv_qp **qp = ib_con->qp;
    struct ibv_cq **cq = ib_con->cq_recv;
    struct ibv_wc **wc = NULL;
    char *buf_ptr      = NULL;
    int buf_offset     = 0;
    size_t buf_size    = ib_con->ib_buf_size;
    uint32_t idx       = 0;

    struct timeval start, end;
    long long ops_count = 0;
    double duration     = 0.0;
    double throughput   = 0.0;
    double bandwidth    = 0.0;

    optimize_thread(thread_id);

    wc = (struct ibv_wc **)calloc(qps_number, sizeof(struct ibv_wc *));
    check(wc != NULL, "thread[%ld]: failed to allocate wc", thread_id);

    /* pre-post recvs */
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        wc[qp_idx] = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
        buf_ptr    = ib_con->ib_buf[qp_idx];
        ret = post_recv(msg_size, ib_con->mr[qp_idx]->lkey, (uint64_t)buf_ptr,
            qp[qp_idx], buf_ptr);
        check(ret == 0, "thread[%ld]: failed to post recv", thread_id);
        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr += buf_offset;
        __sync_add_and_fetch(&(ib_con->recv_posted_wr[qp_idx]), 1);
    }
    while (ib_con->polling) {
        /* poll cq */
        for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
            buf_ptr = ib_con->ib_buf[qp_idx];
            n       = ibv_poll_cq(cq[qp_idx], num_wc, wc[qp_idx]);
            if (n < 0) {
                check(0, "thread[%ld]: Failed to poll cq", thread_id);
            } else if (n > 0) {
                __sync_add_and_fetch(&(ib_con->posted_wr[qp_idx]), n);
            }

            for (i = 0; i < n; i++) {
                idx = ntohl(wc[qp_idx][i].imm_data);
                if (idx != MSG_REGULAR && idx != MSG_CTL_STOP) {
                    LOG_VERBOSE("Sample[%d]", idx);
                    /** debug ("sample[%d] = %lld",idx, ops_count); */
                    /* ret = post_send (msg_size, ib_con->mr[qp_idx]->lkey, 0,
                     * idx, */
                    /*                  qp[qp_idx], buf_ptr, true); */
                    /* check (ret == 0, */
                    /*         "thread[%ld]: failed to post send[%d]",
                     * thread_id, idx); */
                } else if (wc[qp_idx][i].status != IBV_WC_SUCCESS) {
                    if (wc[qp_idx][i].opcode == IBV_WC_SEND) {
                        check(0, "thread[%ld]: send failed status: %s",
                            thread_id, ibv_wc_status_str(wc[qp_idx][i].status));
                    } else {
                        check(0, "thread[%ld]: recv failed status: %s",
                            thread_id, ibv_wc_status_str(wc[qp_idx][i].status));
                    }
                } else if (idx == MSG_CTL_STOP) {
                    ib_con->polling = false;
                    break;
                }
                ops_count += 1;
                LOG_VERBOSE("OPS: %lld", ops_count);
                __sync_add_and_fetch(&(ib_con->recv_completed_wr[qp_idx]), 1);
                if (ops_count == NUM_WARMING_UP_OPS) {
                    gettimeofday(&start, NULL);
                }
            }
        }
    }

    gettimeofday(&end, NULL);
    /* dump statistics */
    duration = (double)((end.tv_sec - start.tv_sec) * 1000000 +
                        (end.tv_usec - start.tv_usec));

    LOG_INFO("thread[%ld]: duration = %fs, message size = %dB\n", thread_id,
        duration / 1000000, msg_size);

    throughput = (double)(ops_count) / duration;
    LOG_INFO("thread[%ld]: throughput = %f (Mops/s)\n", thread_id, throughput);

    bandwidth = (double)(ops_count * msg_size * 8) /
                ((duration / 1000000) * (1024 * 1024 * 1024));
    LOG_INFO("thread[%ld]: bandwidth = %f (Gbs/s)\n", thread_id, bandwidth);
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        if (wc[qp_idx] != NULL) {
            free(wc[qp_idx]);
        }
    }
    free(wc);
    pthread_exit((void *)0);
error:
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        if (wc[qp_idx] != NULL) {
            free(wc[qp_idx]);
        }
    }
    if (wc != NULL) {
        free(wc);
    }
    pthread_exit((void *)-1);
}

void *client_thread_func(void *arg) {
    int ret                     = 0;
    unsigned int k              = 0;
    long thread_id              = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx              = ((struct ThreadInfo *)arg)->ib_con_idx;
    struct IBConnection *ib_con = &(ib_connections[ib_con_idx]);
    int msg_size                = user_parameter.msg_size;
    int qps_number              = user_parameter.qps_number;
    int burst_size              = user_parameter.burst_size;
    uint32_t samples_count      = 0;
    int qp_idx                  = 0;
    bool stop                   = false;

    time_t t;
    struct ibv_qp **qp    = ib_con->qp;
    char *buf_ptr         = NULL;
    int buf_offset        = 0;
    size_t buf_size       = ib_con->ib_buf_size;
    bool sw_limiter       = user_parameter.bw_limiter;
    uint64_t gap_cycle    = 0;
    uint64_t gap_deadline = 0;
    long long burst_iter  = 0;
    bool sending_burst    = true;
    struct timeval start, end;
    long long ops_count = 0;
    double duration     = 0.0;
    double throughput   = 0.0;
    double bandwidth    = 0.0;

    /* rdtsc initialization */
    init_rdtsc(1, 0);

    optimize_thread(thread_id);

    bool sampling = user_parameter.sampling;

    srand((unsigned)time(&t));

    /** we should calculate the number of outgoing packets per second. */
    if (sw_limiter) {
        gap_cycle = calc_gap_cycle(ib_con);
    }

    /* pre-post sends */
    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        buf_ptr = ib_con->ib_buf[qp_idx];
        ret     = post_send(msg_size, ib_con->mr[qp_idx]->lkey, 0, MSG_REGULAR,
            qp[qp_idx], buf_ptr, true);
        check(ret == 0, "thread[%ld]: failed to post send", thread_id);

        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr += buf_offset;
        __sync_add_and_fetch(&(ib_con->posted_wr[qp_idx]), 1);
        ops_count += 1;
    }

    for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
        buf_ptr = ib_con->ib_buf[qp_idx];
        /** get samples' echo */
        /* ret = post_recv (msg_size, ib_con->mr[qp_idx]->lkey, */
        /*         (uint64_t)buf_ptr, qp[qp_idx], buf_ptr); */
        /* check (ret == 0, "thread[%ld]: failed to post recv", thread_id);
         */
        buf_offset = (buf_offset + msg_size) % buf_size;
        buf_ptr += buf_offset;
        __sync_add_and_fetch(&(ib_con->recv_posted_wr[qp_idx]), 1);
    }

    LOG_FILE("Thread[%ld]: ready to send", thread_id);
    /** generating messages */
    gettimeofday(&start, NULL);
    struct timespec timer;
    while (stop != true) {
        if (!sending_burst) {
            if (RDTSC() >= gap_deadline) {
                sending_burst = true;
                burst_iter    = 0;
            }
        } else {
            for (qp_idx = 0; qp_idx < qps_number; qp_idx++) {
                if (!sw_limiter || sending_burst) {
                    buf_ptr = ib_con->ib_buf[qp_idx];
                    for (k = ib_con->posted_wr[qp_idx] -
                             ib_con->completed_wr[qp_idx];
                         k < user_parameter.tx_depth && burst_iter < burst_size;
                         k++) {
                        ops_count += 1;
                        buf_offset = (buf_offset + msg_size) % buf_size;
                        if (sampling && is_sample() &&
                            ops_count > NUM_WARMING_UP_OPS) {
                            /* ret = post_recv (msg_size, */
                            /*         ib_con->mr[qp_idx]->lkey, */
                            /*         (uint64_t)buf_ptr, */
                            /*         qp[qp_idx], */
                            /*         buf_ptr); */
                            /* check (ret == 0, */
                            /*         "thread[%ld]: failed to post recv",
                             * thread_id); */
                            LOG_VERBOSE("Posting a sample: #%d", samples_count);
                            ret = post_send(msg_size, ib_con->mr[qp_idx]->lkey,
                                samples_count, samples_count, qp[qp_idx],
                                buf_ptr, true);
                            get_rdtsc_timespec(&timer);
                            ib_con->samples_start[samples_count] = timer;
                            /** cycles_t start = RDTSC(); */
                            check(ret == 0,
                                "thread[%ld]: failed to post send for sample "
                                "packet",
                                thread_id);
                            /** ib_con->samples_start[samples_count] = start; */
                            samples_count += 1;
                            __sync_add_and_fetch(
                                &(ib_con->posted_wr[qp_idx]), 1);
                            /* __sync_add_and_fetch(
                             * &(ib_con->recv_posted_wr[qp_idx]), 1); */
                            burst_iter += sw_limiter;
                            continue;
                        }
                        post_send(msg_size, ib_con->mr[qp_idx]->lkey,
                            MSG_REGULAR, MSG_REGULAR, qp[qp_idx], buf_ptr,
                            true);

                        __sync_add_and_fetch(&(ib_con->posted_wr[qp_idx]), 1);
                        burst_iter += sw_limiter;
                    }
                    if (sw_limiter && burst_iter >= burst_size) {
                        burst_iter    = 0;
                        sending_burst = false;
                        gap_deadline  = RDTSC() + gap_cycle;
                    }
                }
                /* loop through all wc */
            }
            if (ops_count > ib_con->message_per_thread) {
                break;
            }
        }
    }
    ib_con->samples_count += samples_count;
    gettimeofday(&end, NULL);
    /* dump statistics */
    LOG_FILE("Ops Posted:%lld, Ops Completed:%lld, Samples:%d", ops_count,
        ib_con->regular_messages_count, samples_count);
    duration = (double)((end.tv_sec - start.tv_sec) * 1000000 +
                        (end.tv_usec - start.tv_usec));
    LOG_FILE("thread[%ld]: duration = %fs, message size = %dB\n", thread_id,
        duration / 1000000, msg_size);

    throughput = (double)(ops_count) / duration;
    LOG_FILE("thread[%ld]: throughput = %f (Mops/s)\n", thread_id, throughput);

    bandwidth = (double)(ops_count * msg_size * 8) /
                ((duration / 1000000) * (1024 * 1024 * 1024));

    LOG_FILE("thread[%ld]: bandwidth = %f (Gbs/s)\n", thread_id, bandwidth);

    pthread_exit((void *)0);

error:
    pthread_exit((void *)-1);
}
