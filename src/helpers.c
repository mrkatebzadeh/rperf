#define _GNU_SOURCE
#include "log.h"
#include "parameters.h"
#include "rdma.h"
#include "rperf.h"
#include "tcpsocket.h"

int cmpdouble(const void *a, const void *b) {
    return (*(double *)a > *(double *)b)
               ? 1
               : (*(double *)a < *(double *)b) ? -1 : 0;
}

int set_priority_max(long thread_id) {
    pthread_t self;
    self = pthread_self();
    struct sched_param params;
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    return pthread_setschedparam(self, SCHED_FIFO, &params);
}

int set_affinity(long thread_id) {
    pthread_t self;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET((int)thread_id, &cpuset);
    self = pthread_self();
    return pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset);
}

void optimize_thread(long thread_id) {
    int ret;
    if (user_parameter.is_realtime) {
        ret = set_priority_max(thread_id);
        check(ret == 0, "thread[%ld]: failed to set thread realtime priority",
            thread_id);
    }
    ret = set_affinity(thread_id);
    check(ret == 0, "thread[%ld]: failed to set thread affinity", thread_id);

    return;
error:
    LOG_ERROR("Failed to optimized thread #%ld", thread_id);
}

uint64_t calc_gap_cycle(struct IBConnection *ib_con) {
    int packet_per_second         = 0;
    int cpu_mhz                   = 0;
    unsigned int number_of_bursts = 0;
    double gap_time               = 0;
    uint64_t gap_cycle            = 0;
    uint64_t computaion_cycles    = 2000;
    /** the unit of rate_limit is mbps */
    packet_per_second = (user_parameter.rate_limit * 1024 * 1024) /
                        (user_parameter.msg_size * 8) /
                        ib_con->number_of_threads;
    ib_con->cpu_mhz = cpu_mhz = get_cpu_mhz(1);
    number_of_bursts          = packet_per_second / user_parameter.burst_size;
    gap_time                  = 1000000 * (1.0 / number_of_bursts);
    gap_cycle                 = gap_time * cpu_mhz - computaion_cycles;
    if (user_parameter.verbose) {
        LOG_INFO("Rate: %d\n", user_parameter.rate_limit);
        LOG_INFO("PPS: %d\n", packet_per_second);
        LOG_INFO("Burst Size: %d\n", user_parameter.burst_size);
        LOG_INFO("Burst Number: %d\n", number_of_bursts);
        LOG_INFO("Gap Time: %f\n", gap_time);
        LOG_INFO("Gap Cycles: %ld\n", gap_cycle);
    }
    return gap_cycle;
}

int connect_qp_server(int ib_con_idx, int peer_sockfd) {
    int ret = 0, n = 0;
    uint32_t qp_idx   = 0;
    char sock_buf[64] = {'\0'};
    struct QPInfo *local_qp_info;
    struct QPInfo *remote_qp_info;
    struct IBConnection *ib_con = &(ib_connections[ib_con_idx]);

    local_qp_info = (struct QPInfo *)calloc(
        user_parameter.qps_number, sizeof(struct QPInfo));
    remote_qp_info = (struct QPInfo *)calloc(
        user_parameter.qps_number, sizeof(struct QPInfo));

    for (qp_idx = 0; qp_idx < user_parameter.qps_number; qp_idx++) {
        /* init local qp_info */
        local_qp_info[qp_idx].lid    = ib_con->port_attr.lid;
        local_qp_info[qp_idx].qp_num = ib_con->qp[qp_idx]->qp_num;
        local_qp_info[qp_idx].gid    = ib_con->gid;
        /* get qp_info from client */
        ret = sock_get_qp_info(peer_sockfd, &(remote_qp_info[qp_idx]));
        check(ret == 0, "Failed to get qp_info from client");

        /* send qp_info to client */
        ret = sock_set_qp_info(peer_sockfd, &(local_qp_info[qp_idx]));
        check(ret == 0, "Failed to send qp_info to client");

        /* change send QP state to RTS */
        ret = modify_qp_to_rts(ib_con->qp[qp_idx], remote_qp_info[qp_idx]);
        check(ret == 0, "Failed to modify qp to rts");

        LOG_INFO("==== Start of IB Config ====");
        LOG_INFO("\tqp[%" PRIu32 "] <-> qp[%" PRIu32 "]",
            ib_con->qp[qp_idx]->qp_num, remote_qp_info[qp_idx].qp_num);
        LOG_INFO("==== End of IB Config ====");

        /* sync with clients */
        n = sock_read(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
        check(n == sizeof(SOCK_SYNC_MSG), "Failed to receive sync from client");

        n = sock_write(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
        check(n == sizeof(SOCK_SYNC_MSG), "Failed to write sync to client");
    }
    close(peer_sockfd);

    return 0;

error:
    if (peer_sockfd > 0) {
        close(peer_sockfd);
    }

    return -1;
}

int connect_qp_client(int ib_con_idx) {
    int ret = 0, n = 0;
    int peer_sockfd   = 0;
    char sock_buf[64] = {'\0'};
    uint32_t qp_idx   = 0;
    struct QPInfo *local_qp_info;
    struct QPInfo *remote_qp_info;
    struct QPInfo *loopback1_qp_info;
    struct QPInfo *loopback2_qp_info;
    struct IBConnection *ib_con    = &(ib_connections[ib_con_idx]);
    struct IBConnection *loopback1 = &(loopbacks[2 * ib_con_idx]);
    struct IBConnection *loopback2 = &(loopbacks[2 * ib_con_idx + 1]);

    peer_sockfd = sock_create_connect(
        user_parameter.server_name, user_parameter.socket_port);
    local_qp_info = (struct QPInfo *)calloc(
        user_parameter.qps_number, sizeof(struct QPInfo));
    remote_qp_info = (struct QPInfo *)calloc(
        user_parameter.qps_number, sizeof(struct QPInfo));
    loopback1_qp_info = (struct QPInfo *)calloc(
        user_parameter.qps_number, sizeof(struct QPInfo));
    loopback2_qp_info = (struct QPInfo *)calloc(
        user_parameter.qps_number, sizeof(struct QPInfo));
    check(peer_sockfd > 0, "Failed to create peer_sockfd");

    for (qp_idx = 0; qp_idx < user_parameter.qps_number; qp_idx++) {
        local_qp_info[qp_idx].lid    = ib_con->port_attr.lid;
        local_qp_info[qp_idx].qp_num = ib_con->qp[qp_idx]->qp_num;
        local_qp_info[qp_idx].gid    = ib_con->gid;

        loopback1_qp_info[qp_idx].lid    = loopback1->port_attr.lid;
        loopback1_qp_info[qp_idx].qp_num = loopback1->qp[qp_idx]->qp_num;
        loopback1_qp_info[qp_idx].gid    = loopback1->gid;
        loopback2_qp_info[qp_idx].lid    = loopback2->port_attr.lid;
        loopback2_qp_info[qp_idx].qp_num = loopback2->qp[qp_idx]->qp_num;
        loopback2_qp_info[qp_idx].gid    = loopback2->gid;

        /* send qp_info to server */
        ret = sock_set_qp_info(peer_sockfd, &(local_qp_info[qp_idx]));
        check(ret == 0, "Failed to send qp_info to server");

        /* get qp_info from server */
        ret = sock_get_qp_info(peer_sockfd, &(remote_qp_info[qp_idx]));
        check(ret == 0, "Failed to get qp_info from server");

        /* change QP state to RTS */
        ret = modify_qp_to_rts(ib_con->qp[qp_idx], remote_qp_info[qp_idx]);
        check(ret == 0, "Failed to modify qp to rts");

        /* change loopback1 QP state to RTS */
        ret =
            modify_qp_to_rts(loopback1->qp[qp_idx], loopback2_qp_info[qp_idx]);
        check(ret == 0, "Failed to modify loopback1 qp to rts");

        /* change loopback2 QP state to RTS */
        ret =
            modify_qp_to_rts(loopback2->qp[qp_idx], loopback1_qp_info[qp_idx]);
        check(ret == 0, "Failed to modify loopback2 qp to rts");

        LOG_INFO("==== IB Config ====");
        LOG_INFO("\tqp[%" PRIu32 "] <-> qp[%" PRIu32 "]",
            ib_con->qp[qp_idx]->qp_num, remote_qp_info[qp_idx].qp_num);
        LOG_INFO("\tlqp[%" PRIu32 "] <-> lqp[%" PRIu32 "]",
            loopback1->qp[qp_idx]->qp_num, loopback2->qp[qp_idx]->qp_num);
        LOG_INFO("\tlqp lid[%" PRIu32 "] <-> lqp lid[%" PRIu32 "]",
            loopback1->port_attr.lid, loopback2->port_attr.lid);
        LOG_INFO("---- End of IB Config ----");

        /* sync with server */
        n = sock_write(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
        check(n == sizeof(SOCK_SYNC_MSG), "Failed to write sync to client");

        n = sock_read(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
        check(n == sizeof(SOCK_SYNC_MSG), "Failed to receive sync from client");
    }
    close(peer_sockfd);
    return 0;

error:
    if (peer_sockfd > 0) {
        close(peer_sockfd);
    }

    return -1;
}

bool is_sample() {
    return rand() < user_parameter.sampling_ratio * ((double)RAND_MAX + 1.0);
}

QPType qp_type_from_str(char *str) {
    if (strcmp(str, "UD") == 0)
        return UD;
    else if (strcmp(str, "UC") == 0)
        return UC;
    return RC;
}

void qp_type_to_str(QPType qpt, char *buf) {
    switch (qpt) {
        case UD:
            strcpy(buf, "UD");
            break;
        case UC:
            strcpy(buf, "UC");
            break;
        case RC:
            strcpy(buf, "RC");
            break;
    }
}

TestType test_type_from_str(char *str) {
    if (strcmp(str, "AccRtt") == 0)
        return AckRtt;
    else if (strcmp(str, "PPRtt") == 0)
        return PPRtt;
    else if (strcmp(str, "WrRtt") == 0)
        return WrRtt;
    else if (strcmp(str, "RdRtt") == 0)
        return RdRtt;
    else if (strcmp(str, "Coflow") == 0)
        return CoFlow;
    return AckRtt;
}

void test_type_to_str(TestType tt, char *buf) {
    switch (tt) {
        case AckRtt:
            strcpy(buf, "AckRtt");
            break;
        case PPRtt:
            strcpy(buf, "PPRtt");
            break;
        case WrRtt:
            strcpy(buf, "WrRtt");
            break;
        case RdRtt:
            strcpy(buf, "RdRtt");
            break;
        case CoFlow:
            strcpy(buf, "CoFlow");
            break;
    }
}
