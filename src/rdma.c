#include <arpa/inet.h>
#include <unistd.h>

#include "log.h"
#include "parameters.h"
#include "rdma.h"

struct IBConnection ib_connections[MAXCONN];
struct IBConnection loopbacks[MAXCONN];

int modify_qp_to_rts(struct ibv_qp *qp, struct QPInfo target_qp) {
    int ret = 0;
    /* change QP state to INIT */
    {
        struct ibv_qp_attr qp_attr;
        memset(&qp_attr, 0, sizeof(qp_attr));

        qp_attr.qp_state   = IBV_QPS_INIT;
        qp_attr.pkey_index = 0;
        qp_attr.port_num   = IB_PORT;
        qp_attr.qp_access_flags =
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
            IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_WRITE;

        ret = ibv_modify_qp(qp, &qp_attr,
            IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |
                IBV_QP_ACCESS_FLAGS);
        check(ret == 0, "Failed to modify qp to INIT. Errno: %d", ret);
    }

    /* Change QP state to RTR */
    {
        struct ibv_qp_attr qp_attr;
        memset(&qp_attr, 0, sizeof(qp_attr));

        qp_attr.qp_state              = IBV_QPS_RTR;
        qp_attr.path_mtu              = IB_MTU;
        qp_attr.dest_qp_num           = target_qp.qp_num;
        qp_attr.rq_psn                = 0;
        qp_attr.max_dest_rd_atomic    = 1;
        qp_attr.min_rnr_timer         = 12;
        qp_attr.ah_attr.is_global     = 0;
        qp_attr.ah_attr.dlid          = target_qp.lid;
        qp_attr.ah_attr.sl            = IB_SL;
        qp_attr.ah_attr.src_path_bits = 0;
        qp_attr.ah_attr.port_num      = IB_PORT;

        if (target_qp.gid.global.interface_id) {
            qp_attr.ah_attr.is_global     = 1;
            qp_attr.ah_attr.grh.hop_limit = 1;
            memcpy(&qp_attr.ah_attr.grh.dgid, &target_qp.gid, 16);
            qp_attr.ah_attr.grh.sgid_index =
                0;  // TODO get sgid index from command prompt
        }

        ret = ibv_modify_qp(qp, &qp_attr,
            IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                IBV_QP_MIN_RNR_TIMER);
        check(ret == 0, "Failed to change qp to RTR. Errno: %d", ret);
    }

    /* Change QP state to RTS */
    {
        struct ibv_qp_attr qp_attr;
        memset(&qp_attr, 0, sizeof(qp_attr));

        qp_attr.qp_state      = IBV_QPS_RTS;
        qp_attr.timeout       = 14;
        qp_attr.retry_cnt     = 7;
        qp_attr.rnr_retry     = 7;
        qp_attr.sq_psn        = 0;
        qp_attr.max_rd_atomic = 1;

        ret = ibv_modify_qp(qp, &qp_attr,
            IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
        check(ret == 0, "Failed to modify qp to RTS. Errno: %d", ret);
    }

    return 0;
error:
    return -1;
}

int post_send(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
    uint32_t imm_data, struct ibv_qp *qp, char *buf, bool signal) {
    int ret = 0;
    struct ibv_send_wr *bad_send_wr;
    enum ibv_send_flags flags = 0;
    struct ibv_sge list       = {
        .addr = (uintptr_t)buf, .length = req_size, .lkey = lkey};

    if (signal) {
        flags |= IBV_SEND_SIGNALED;
    }
    if (req_size <= 48) {
        flags |= IBV_SEND_INLINE;
    }
    struct ibv_send_wr send_wr = {.wr_id = wr_id,
        .sg_list                         = &list,
        .num_sge                         = 1,
        .opcode                          = IBV_WR_SEND_WITH_IMM,
        .send_flags                      = flags,
        /* .send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE, */
        .imm_data = htonl(imm_data)};

    ret = ibv_post_send(qp, &send_wr, &bad_send_wr);
    return ret;
}

int post_recv(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
    struct ibv_qp *qp, char *buf) {
    int ret = 0;
    struct ibv_recv_wr *bad_recv_wr;

    struct ibv_sge list = {
        .addr = (uintptr_t)buf, .length = req_size, .lkey = lkey};

    struct ibv_recv_wr recv_wr = {
        .wr_id = wr_id, .sg_list = &list, .num_sge = 1};

    ret = ibv_post_recv(qp, &recv_wr, &bad_recv_wr);
    return ret;
}
int lock_memory(char *addr, size_t size) {
    unsigned long page_offset, page_size;
    page_size   = sysconf(_SC_PAGE_SIZE);
    page_offset = (unsigned long)addr % page_size;

    addr -= page_offset; /* Adjust addr to page boundary */
    size += page_offset; /* Adjust size with page_offset */

    return mlock(addr, size); /* Lock the memory */
}

int unlock_memory(char *addr, size_t size) {
    unsigned long page_offset, page_size;

    page_size   = sysconf(_SC_PAGE_SIZE);
    page_offset = (unsigned long)addr % page_size;

    addr -= page_offset; /* Adjust addr to page boundary */
    size += page_offset; /* Adjust size with page_offset */

    return munlock(addr, size); /* Unlock the memory */
}

int setup_ib(struct IBConnection *ib_con) {
    int ret                      = 0;
    uint32_t qp_idx              = 0;
    int grh_size                 = 40;
    struct ibv_device **dev_list = NULL;
    int i                        = 0;
    int num                      = 0;

    int max              = 7;
    progressbar *prog_ib = progressbar_new("Setup IB", max);

    memset(ib_con, 0, sizeof(struct IBConnection));

    /* get IB device list */
    dev_list = ibv_get_device_list(&num);
    check(dev_list != NULL, "Failed to get ib device list.");

    /* create IB context */
    for (i = 0; i < num; i++) {
        LOG_INFO("Checking RDMA device: %s", ibv_get_device_name(dev_list[i]));
        if (strcmp(ibv_get_device_name(dev_list[i]),
                user_parameter.device_name) == 0) {
            ib_con->ctx = ibv_open_device(dev_list[i]);
            break;
        }
    }
    check(ib_con->ctx != NULL, "Failed to open ib device.");
    LOG_INFO("Opened RDMA device: %s", ibv_get_device_name(dev_list[i]));

    progressbar_inc(prog_ib);

    /* allocate protection domain */
    ib_con->pd = ibv_alloc_pd(ib_con->ctx);
    check(ib_con->pd != NULL, "Failed to allocate protection domain.");
    LOG_INFO("Allocated PD");
    progressbar_inc(prog_ib);

    /* query IB port attribute */
    ret = ibv_query_port(ib_con->ctx, IB_PORT, &ib_con->port_attr);
    check(ret == 0, "Failed to query IB port information.");

    /* query GID attribute */
    ret = ibv_query_gid(ib_con->ctx, IB_PORT, 0, &ib_con->gid);
    check(ret == 0, "Failed to query GID information.");

    /* register mr */
    ib_con->ib_buf_size = (user_parameter.msg_size + grh_size) *
                          MAX(user_parameter.rx_depth, user_parameter.tx_depth);

    ib_con->ib_buf = (char **)calloc(user_parameter.qps_number, sizeof(char *));
    check(ib_con->ib_buf != NULL, "Failed to allocate ib_buf");

    ib_con->mr = (struct ibv_mr **)calloc(
        user_parameter.qps_number, sizeof(struct ibv_mr *));
    check(ib_con->mr != NULL, "Failed to allocate mr");
    ib_con->cq_send = (struct ibv_cq **)calloc(
        user_parameter.qps_number, sizeof(struct ibv_cq *));

    ib_con->cq_recv = (struct ibv_cq **)calloc(
        user_parameter.qps_number, sizeof(struct ibv_cq *));

    ib_con->qp = (struct ibv_qp **)calloc(
        user_parameter.qps_number, sizeof(struct ibv_qp *));

    ib_con->posted_wr =
        (long long *)calloc(user_parameter.qps_number, sizeof(long long));
    ib_con->completed_wr =
        (long long *)calloc(user_parameter.qps_number, sizeof(long long));

    ib_con->recv_posted_wr =
        (long long *)calloc(user_parameter.qps_number, sizeof(long long));
    ib_con->recv_completed_wr =
        (long long *)calloc(user_parameter.qps_number, sizeof(long long));

    for (qp_idx = 0; qp_idx < user_parameter.qps_number; qp_idx++) {
        ib_con->ib_buf[qp_idx] = (char *)memalign(4096, ib_con->ib_buf_size);
        check(ib_con->ib_buf[qp_idx] != NULL, "Failed to allocate ib_buf");
        LOG_INFO("Allocated buffer");
        progressbar_inc(prog_ib);

        ib_con->mr[qp_idx] = ibv_reg_mr(ib_con->pd,
            (void *)ib_con->ib_buf[qp_idx], ib_con->ib_buf_size,
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                IBV_ACCESS_REMOTE_WRITE);
        check(ib_con->mr[qp_idx] != NULL, "Failed to register mr");
        LOG_INFO("Registered MR");
        progressbar_inc(prog_ib);

        /* query IB device attr */
        ret = ibv_query_device(ib_con->ctx, &ib_con->dev_attr);
        check(ret == 0, "Failed to query device");

        /* create cq */
        ib_con->cq_send[qp_idx] =
            ibv_create_cq(ib_con->ctx, ib_con->dev_attr.max_cqe, NULL, NULL, 0);
        check(ib_con->cq_send[qp_idx] != NULL, "Failed to create cq");
        LOG_INFO("Created send CQ");
        progressbar_inc(prog_ib);

        ib_con->cq_recv[qp_idx] =
            ibv_create_cq(ib_con->ctx, ib_con->dev_attr.max_cqe, NULL, NULL, 0);
        check(ib_con->cq_recv[qp_idx] != NULL, "Failed to create cq");
        LOG_INFO("Created receive CQ");
        progressbar_inc(prog_ib);

        /* create qp */
        struct ibv_qp_init_attr qp_init_attr = {
            .send_cq = ib_con->cq_send[qp_idx],
            .recv_cq = ib_con->cq_recv[qp_idx],
            .cap =
                {
                    // The result of query from the driver is not reliable.
                    .max_send_wr     = user_parameter.tx_depth,
                    .max_recv_wr     = user_parameter.rx_depth,
                    .max_send_sge    = 1,
                    .max_recv_sge    = 1,
                    .max_inline_data = 0,
                },
            .qp_type    = IBV_QPT_RC,
            .sq_sig_all = 0,
        };
        ib_con->qp[qp_idx] = ibv_create_qp(ib_con->pd, &qp_init_attr);
        check(ib_con->qp[qp_idx] != NULL, "Failed to create qp[%d]", errno);
        LOG_INFO("Created QP");
        progressbar_inc(prog_ib);
    }
    progressbar_finish(prog_ib);
    ibv_free_device_list(dev_list);
    return 0;

error:
    if (dev_list != NULL) {
        ibv_free_device_list(dev_list);
    }
    return -1;
}

void close_ib_connection(int ib_con_idx) {
    uint32_t qp_idx             = 0;
    struct IBConnection *ib_con = &(ib_connections[ib_con_idx]);
    for (qp_idx = 0; qp_idx < user_parameter.qps_number; qp_idx++) {
        if (ib_con->qp[qp_idx] != NULL) {
            ibv_destroy_qp(ib_con->qp[qp_idx]);
        }

        if (ib_con->cq_send[qp_idx] != NULL) {
            ibv_destroy_cq(ib_con->cq_send[qp_idx]);
        }

        if (ib_con->cq_recv[qp_idx] != NULL) {
            ibv_destroy_cq(ib_con->cq_recv[qp_idx]);
        }
        if (ib_con->mr[qp_idx] != NULL) {
            ibv_dereg_mr(ib_con->mr[qp_idx]);
        }
        if (ib_con->ib_buf[qp_idx] != NULL) {
            free(ib_con->ib_buf[qp_idx]);
        }
    }
    if (ib_con->posted_wr != NULL) {
        free(ib_con->posted_wr);
    }
    if (ib_con->completed_wr != NULL) {
        free(ib_con->completed_wr);
    }
    if (ib_con->recv_posted_wr != NULL) {
        free(ib_con->posted_wr);
    }
    if (ib_con->recv_completed_wr != NULL) {
        free(ib_con->completed_wr);
    }
    if (ib_con->samples_start != NULL) {
        free(ib_con->samples_start);
    }
    if (ib_con->samples_end != NULL) {
        free(ib_con->samples_end);
    }
    if (ib_con->qp != NULL) {
        free(ib_con->qp);
    }
    if (ib_con->cq_send != NULL) {
        free(ib_con->cq_send);
    }
    if (ib_con->cq_recv != NULL) {
        free(ib_con->cq_recv);
    }
    if (ib_con->pd != NULL) {
        ibv_dealloc_pd(ib_con->pd);
    }
    if (ib_con->mr != NULL) {
        free(ib_con->mr);
    }

    if (ib_con->ctx != NULL) {
        ibv_close_device(ib_con->ctx);
    }

    if (ib_con->ib_buf != NULL) {
        free(ib_con->ib_buf);
    }
}
