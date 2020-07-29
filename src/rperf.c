#define _GNU_SOURCE
#include "rperf.h"
#include "log.h"
#include "parameters.h"
#include "rdma.h"
#include "tcpsocket.h"

int server() {
    int ret = 0, i = 0;
    int sockfd               = 0;
    int peer_sockfd          = 0;
    int ib_con_idx           = 0;
    bool stop                = false;
    bool thread_ret_normally = true;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_in);

    pthread_t threads[MAXCONN];
    pthread_attr_t attr;
    void *status;
    struct ThreadInfo arguments[MAXCONN];

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    sockfd = sock_create_bind(user_parameter.socket_port);

    check(sockfd > 0, "Failed to create server socket.");

    listen(sockfd, 10);
    while (!stop) {
        LOG_INFO("Waiting for client #%d...", ib_con_idx);

        peer_sockfd =
            accept(sockfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
        check(peer_sockfd > 0, "Failed to create peer_sockfd");
        arguments[ib_con_idx].ib_con_idx = ib_con_idx;
        arguments[ib_con_idx].thread_id  = peer_sockfd;
        ret = pthread_create(&threads[ib_con_idx], &attr, server_func,
            (void *)&arguments[ib_con_idx]);
        check(ret == 0, "Failed to create server_runner[%d]", ib_con_idx);
        ib_con_idx += 1;
    }

    for (i = 0; i < ib_con_idx; i++) {
        ret = pthread_join(threads[i], &status);
        check(ret == 0, "Failed to join thread[%d].", i);
        if ((long)status != 0) {
            thread_ret_normally = false;
            LOG_ERROR("server_thread[%d]: failed to execute", i);
        }
        close_ib_connection(ib_con_idx);
    }
    if (!thread_ret_normally) {
        goto error;
    }
    close(sockfd);
    return 0;

error:
    if (sockfd > 0) {
        close(sockfd);
    }
    return -1;
}

void *server_func(void *arg) {
    int ret          = 0;
    long peer_sockfd = ((struct ThreadInfo *)arg)->thread_id;
    int ib_con_idx   = ((struct ThreadInfo *)arg)->ib_con_idx;
    LOG_INFO("Preparing IB device...");

    ret = setup_ib(&ib_connections[ib_con_idx]);
    check(ret == 0, "Failed to setup IB device.");

    LOG_INFO("Exchanging QP info...");
    ret = connect_qp_server(ib_con_idx, peer_sockfd);
    check(ret == 0, "Failed to connect server's qp.");

    LOG_INFO("Serving...");
    run_test_server(ib_con_idx);
    LOG_INFO("Cleaning up...");
    pthread_exit((void *)0);
error:
    LOG_INFO("Cleaning up...");
    pthread_exit((void *)-1);
}

int client() {
    int ret        = 0;
    int ib_con_idx = 0;

    ret = setup_ib(&ib_connections[ib_con_idx]);
    check(ret == 0, "Failed to setup IB device.");

    ret = setup_ib(&loopbacks[2 * ib_con_idx]);
    check(ret == 0, "Failed to setup IB device for loopback1.");

    ret = setup_ib(&loopbacks[2 * ib_con_idx + 1]);
    check(ret == 0, "Failed to setup IB device for loopback2.");

    ret = connect_qp_client(ib_con_idx);
    check(ret == 0, "Failed to connect client's qp.");

    run_test_client(ib_con_idx);
    close_ib_connection(ib_con_idx);
    return 0;

error:
    return -1;
}

int run_test_server(int ib_con_idx) {
    int ret                  = 0;
    long num_threads         = user_parameter.num_server_poll_recv_threads;
    long num_polling_threads = user_parameter.num_server_poll_send_threads;
    long num_posting_threads = user_parameter.num_server_post_recv_threads;
    long i                   = 0;
    long thread_num          = 0;
    long offset              = 3;

    struct ThreadInfo *arguments = (struct ThreadInfo *)calloc(
        num_polling_threads + num_posting_threads + num_threads,
        sizeof(struct ThreadInfo));

    struct IBConnection *ib_con = &(ib_connections[ib_con_idx]);
    pthread_t *threads          = NULL;
    pthread_t *polling_threads  = NULL;
    pthread_t *posting_threads  = NULL;
    pthread_attr_t attr;
    void *status;

    pthread_mutex_init(&(ib_con->send_post_mutex), NULL);
    pthread_mutex_init(&(ib_con->recv_post_mutex), NULL);
    pthread_mutex_init(&(ib_con->send_completed_mutex), NULL);
    pthread_mutex_init(&(ib_con->recv_completed_mutex), NULL);

    // ib_con->message_per_thread = TOT_NUM_OPS / num_threads;

    ib_con->polling = true;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    threads = (pthread_t *)calloc(num_threads, sizeof(pthread_t));
    check(threads != NULL, "Failed to allocate threads.");

    polling_threads =
        (pthread_t *)calloc(num_polling_threads, sizeof(pthread_t));
    check(threads != NULL, "Failed to allocate polling_threads.");

    posting_threads =
        (pthread_t *)calloc(num_posting_threads, sizeof(pthread_t));
    check(threads != NULL, "Failed to allocate posting_threads.");

    for (i = 0; i < num_threads; i++) {
        arguments[thread_num].thread_id  = thread_num * 2 + offset;
        arguments[thread_num].ib_con_idx = ib_con_idx;
        ret                              = pthread_create(
            &threads[i], &attr, server_thread, (void *)&arguments[thread_num]);
        check(ret == 0, "Failed to create server_thread[%ld]", i);
        thread_num += 1;
    }

    for (i = 0; i < num_polling_threads; i++) {
        arguments[thread_num].thread_id  = thread_num * 2 + offset;
        arguments[thread_num].ib_con_idx = ib_con_idx;
        ret = pthread_create(&polling_threads[i], &attr, server_poll_send_func,
            (void *)&arguments[thread_num]);
        check(ret == 0, "Failed to create server_thread[%ld]", i);
        thread_num += 1;
    }

    for (i = 0; i < num_posting_threads; i++) {
        arguments[thread_num].thread_id  = thread_num * 2 + offset;
        arguments[thread_num].ib_con_idx = ib_con_idx;
        ret = pthread_create(&posting_threads[i], &attr, server_post_recv_func,
            (void *)&arguments[thread_num]);
        check(ret == 0, "Failed to create server_thread[%ld]", i);
        thread_num += 1;
    }
    bool thread_ret_normally = true;
    for (i = 0; i < num_threads; i++) {
        ret = pthread_join(threads[i], &status);
        check(ret == 0, "Failed to join thread[%ld].", i);
        if ((long)status != 0) {
            thread_ret_normally = false;
            LOG_ERROR("server_thread[%ld]: failed to execute", i);
        }
    }

    ib_con->polling = false;
    for (i = 0; i < num_polling_threads; i++) {
        ret = pthread_join(polling_threads[i], &status);
        check(ret == 0, "Failed to join thread[%ld].", i);
        if ((long)status != 0) {
            thread_ret_normally = false;
            LOG_ERROR("server_thread[%ld]: failed to execute", i);
        }
    }
    if (thread_ret_normally == false) {
        goto error;
    }

    pthread_attr_destroy(&attr);
    free(threads);
    free(posting_threads);
    free(polling_threads);

    free(arguments);
    return 0;

error:
    if (threads != NULL) {
        free(threads);
    }
    if (polling_threads != NULL) {
        free(polling_threads);
    }
    if (posting_threads != NULL) {
        free(posting_threads);
    }
    pthread_attr_destroy(&attr);
    free(arguments);

    return -1;
}

int run_test_client(int ib_con_idx) {
    int ret                    = 0;
    long num_client_threads    = user_parameter.num_client_post_send_threads;
    long num_poll_recv_threads = user_parameter.num_client_poll_recv_threads;
    long num_poll_send_threads = user_parameter.num_client_poll_send_threads;
    long i                     = 0;
    int thread_num             = 0;
    long offset                = 3;
    size_t size                = 0;
    void *status;
    struct ThreadInfo *arguments = (struct ThreadInfo *)calloc(
        num_poll_send_threads + num_poll_recv_threads + num_client_threads,
        sizeof(struct ThreadInfo));
    struct IBConnection *ib_con    = &(ib_connections[ib_con_idx]);
    struct IBConnection *loopback1 = &(loopbacks[2 * ib_con_idx]);

    pthread_attr_t attr;
    pthread_t *client_threads   = NULL;
    pthread_t *poll_send_thread = NULL;
    pthread_t *poll_recv_thread = NULL;

    ib_con->number_of_threads = num_client_threads;
    LOG_INFO("===== Run Client ====");

    size = user_parameter.iterations * (user_parameter.sampling_ratio + 0.05) *
           sizeof(struct timespec);

    ib_con->samples_start = (struct timespec *)calloc(1, size);
    ret                   = lock_memory((char *)(ib_con->samples_start), size);
    check(ret != -1, "Failed to lock samples_start");

    ib_con->samples_end = (struct timespec *)calloc(1, size);
    ret                 = lock_memory((char *)(ib_con->samples_end), size);
    check(ret != -1, "Failed to lock samples_end");

    loopback1->samples_start = (struct timespec *)calloc(1, size);
    ret = lock_memory((char *)(loopback1->samples_start), size);
    check(ret != -1, "Failed to lock loopback1 samples_start");

    loopback1->samples_end = (struct timespec *)calloc(1, size);
    ret = lock_memory((char *)(loopback1->samples_end), size);
    check(ret != -1, "Failed to lock loopback1 samples_end");

    ib_con->polling            = true;
    ib_con->message_per_thread = user_parameter.iterations / num_client_threads;
    /* initialize threads */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    poll_send_thread =
        (pthread_t *)calloc(num_poll_send_threads, sizeof(pthread_t));
    check(poll_send_thread != NULL, "Failed to allocate poll_send_thread.");

    for (i = 0; i < num_poll_send_threads; i++) {
        arguments[thread_num].thread_id  = thread_num * 2 + offset;
        arguments[thread_num].ib_con_idx = ib_con_idx;
        ret = pthread_create(&poll_send_thread[i], &attr, client_ackrtt_poll,
            (void *)&arguments[thread_num]);
        check(ret == 0, "Failed to create client_ackrtt_poll[%ld]",
            arguments[thread_num].thread_id);
        thread_num += 1;
    }

    for (i = 0; i < num_poll_send_threads; i++) {
        arguments[thread_num].thread_id  = thread_num * 2 + offset;
        arguments[thread_num].ib_con_idx = ib_con_idx;
        ret = pthread_create(&poll_send_thread[i], &attr, client_loopback_poll,
            (void *)&arguments[thread_num]);
        check(ret == 0, "Failed to create client_loopback_poll[%ld]",
            arguments[thread_num].thread_id);
        thread_num += 1;
    }

    poll_recv_thread =
        (pthread_t *)calloc(num_poll_recv_threads, sizeof(pthread_t));
    check(poll_send_thread != NULL, "Failed to allocate poll_send_thread.");

    for (i = 0; i < num_poll_recv_threads; i++) {
        arguments[thread_num].thread_id  = thread_num * 2 + offset;
        arguments[thread_num].ib_con_idx = ib_con_idx;
        ret = pthread_create(&poll_recv_thread[i], &attr, client_loopback_recv,
            (void *)&arguments[thread_num]);
        check(ret == 0, "Failed to create client_loopback_recv[%ld]",
            arguments[thread_num].thread_id);
        thread_num += 1;
    }

    client_threads = (pthread_t *)calloc(num_client_threads, sizeof(pthread_t));
    check(client_threads != NULL, "Failed to allocate client_threads.");

    for (i = 0; i < num_client_threads; i++) {
        arguments[thread_num].thread_id  = thread_num * 2 + offset;
        arguments[thread_num].ib_con_idx = ib_con_idx;
        ret = pthread_create(&client_threads[i], &attr, client_ackrtt_post,
            (void *)&arguments[thread_num]);
        check(ret == 0, "Failed to create client_thread[%ld]", i);
        thread_num += 1;
    }

    bool thread_ret_normally = true;
    for (i = 0; i < num_client_threads; i++) {
        ret = pthread_join(client_threads[i], &status);
        check(ret == 0, "Failed to join client_thread[%ld].", i);
        if ((long)status != 0) {
            thread_ret_normally = false;
            LOG_ERROR("Thread[%ld]: failed to execute", i);
        }
    }

    ib_con->polling     = false;
    thread_ret_normally = true;
    for (i = 0; i < num_poll_send_threads; i++) {
        ret = pthread_join(poll_send_thread[i], &status);
        check(ret == 0, "Failed to join poll_send_thread[%ld].", i);
        if ((long)status != 0) {
            thread_ret_normally = false;
            LOG_ERROR("Thread[%ld]: failed to execute", i);
        }
    }

    thread_ret_normally = true;
    for (i = 0; i < num_poll_recv_threads; i++) {
        ret = pthread_join(poll_recv_thread[i], &status);
        check(ret == 0, "Failed to join poll_recv_thread[%ld].", i);
        if ((long)status != 0) {
            thread_ret_normally = false;
            LOG_ERROR("Thread[%ld]: failed to execute", i);
        }
    }

    if (thread_ret_normally == false) {
        goto error;
    }
    /** ------------------------------------------------------------------------- */
    uint32_t j, counter = 0;
    double sum        = 0;
    FILE *histogram   = fopen(user_parameter.output_filename, "w");
    FILE *histogram_raw   = fopen(strcat(user_parameter.output_filename, "_raw"), "w");
    double *latencies = (double *)calloc(ib_con->samples_count, sizeof(double));
    double *latencies_on_wire =
        (double *)calloc(ib_con->samples_count, sizeof(double));
    double *latencies_loopback =
        (double *)calloc(ib_con->samples_count, sizeof(double));
    for (j = 0; j < ib_con->samples_count; j++) {
        double lat1 =
            time_diff_in_ns(ib_con->samples_end[j], ib_con->samples_start[j]);
        double lat2 = time_diff_in_ns(
            loopback1->samples_end[j], loopback1->samples_start[j]);
        latencies_on_wire[j]  = lat1 / 1000;
        latencies_loopback[j] = lat2 / 1000;
        latencies[j]          = lat1 - lat2;
    }
    for (j = 0; j < ib_con->samples_count; j++) {
        double lat = latencies[j];
	fprintf(histogram_raw, "%d,%0.1f,%0.1f,%f\n", j, latencies_on_wire[j],
                latencies_loopback[j], latencies[j]);
        if (lat > 0.0) {
            counter += 1;
            sum += lat;
            fprintf(histogram, "%d,%0.1f,%0.1f,%f\n", j, latencies_on_wire[j],
                latencies_loopback[j], latencies[j]);
        } else {
            LOG_INFO("Latency equal or less than zero %d: %lf", j, lat);
        }
    }
    fclose(histogram);
    fclose(histogram_raw);
    qsort(latencies, ib_con->samples_count, sizeof(double), cmpdouble);
    LOG_FILE("Average Latency: %fns", sum / counter);
    LOG_FILE(
        "50th    Latency: %fns", latencies[(int)(0.5 * ib_con->samples_count)]);
    LOG_FILE("99th    Latency: %fns",
        latencies[(int)(0.99 * ib_con->samples_count)]);
    LOG_FILE("99.9th  Latency: %fns",
        latencies[(int)(0.999 * ib_con->samples_count)]);
    LOG_FILE("99.99th Latency: %fns",
        latencies[(int)(0.9999 * ib_con->samples_count)]);
    /** ------------------------------------------------------------------------- */

    ret = post_send(user_parameter.msg_size, ib_con->mr[0]->lkey, 0,
        MSG_CTL_STOP, ib_con->qp[0], ib_con->ib_buf[0], true);
    check(ret == 0, "thread[%d]: failed to post send", thread_num);
    pthread_attr_destroy(&attr);
    free(client_threads);
    free(arguments);
    ret = unlock_memory((char *)(ib_con->samples_start), size);
    check(ret != -1, "Failed to lock samples_start");

    ret = unlock_memory((char *)(ib_con->samples_end), size);
    check(ret != -1, "Failed to lock samples_end");

    ret = unlock_memory((char *)(loopback1->samples_start), size);
    check(ret != -1, "Failed to lock samples_start");

    ret = unlock_memory((char *)(loopback1->samples_end), size);
    check(ret != -1, "Failed to lock samples_end");

    return 0;

error:
    ret = post_send(user_parameter.msg_size, ib_con->mr[0]->lkey, 0,
        MSG_CTL_STOP, ib_con->qp[0], ib_con->ib_buf[0], true);
    if (client_threads != NULL) {
        free(client_threads);
    }
    pthread_attr_destroy(&attr);
    ret = unlock_memory((char *)(ib_con->samples_start), size);
    check(ret != -1, "Failed to lock samples_start");

    ret = unlock_memory((char *)(ib_con->samples_end), size);
    check(ret != -1, "Failed to lock samples_end");

    ret = unlock_memory((char *)(loopback1->samples_start), size);
    check(ret != -1, "Failed to lock samples_start");

    ret = unlock_memory((char *)(loopback1->samples_end), size);
    check(ret != -1, "Failed to lock samples_end");

    free(arguments);
    return -1;
}
