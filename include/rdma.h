#ifndef IB_H_
#define IB_H_

#include <arpa/inet.h>
#include <byteswap.h>
#include <endian.h>
#include <infiniband/verbs.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h> /* Support all standards    */
#include <unistd.h>
#include "clock.h"
#include "parameters.h"

#define IB_MTU IBV_MTU_4096
#define IB_PORT 1
#define IB_SL 0
#define IB_WR_ID_STOP 0xE000000000000000
#define NUM_WARMING_UP_OPS 500000
#define TOT_NUM_OPS 5000000
#define MSG_REGULAR 0xFFFFFFFF
#define MSG_CTL_STOP 0xFFFFFFFE
#define MAXCONN 20

int unlock_memory(char *addr, size_t size);
int lock_memory(char *addr, size_t size);

struct ThreadInfo {
    long thread_id;
    int ib_con_idx;
} __attribute__((packed));

struct QPInfo {
    int lid;
    int qp_num;
    union ibv_gid gid;
};

struct IBConnection {
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_mr **mr;
    struct ibv_cq **cq_send;
    struct ibv_cq **cq_recv;
    struct ibv_qp **qp;
    struct ibv_port_attr port_attr;
    struct ibv_device_attr dev_attr;
    union ibv_gid gid;
    long long *posted_wr;
    long long *completed_wr;
    long long *recv_posted_wr;
    long long *recv_completed_wr;
    char **ib_buf;
    size_t ib_buf_size;
    long samples_count;
    struct timespec *samples_start;
    struct timespec *samples_end;
    bool polling;
    int cpu_mhz;
    pthread_mutex_t send_post_mutex;
    pthread_mutex_t recv_post_mutex;
    pthread_mutex_t send_completed_mutex;
    pthread_mutex_t recv_completed_mutex;
    long long message_per_thread;
    long long regular_messages_count;
    int number_of_threads;
};

extern struct IBConnection ib_connections[];
extern struct IBConnection loopbacks[];
#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

int setup_ib(struct IBConnection *ib_con);
void close_ib_connection(int ib_con_idx);

int connect_qp_server(int ib_con_idx, int peer_sockfd);
int connect_qp_client(int ib_con_idx);
int get_connection_server();
int get_connection_client();
int modify_qp_to_rts(struct ibv_qp *qp, struct QPInfo target_qp);

int post_send(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
    uint32_t imm_data, struct ibv_qp *qp, char *buf, bool signal);

int post_recv(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
    struct ibv_qp *qp, char *buf);

int lock_memory(char *addr, size_t size);
int unlock_memory(char *addr, size_t size);
#endif /*ib.h*/
