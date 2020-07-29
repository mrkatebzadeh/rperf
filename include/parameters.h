#ifndef PARAMETERS_H_
#define PARAMETERS_H_

#include <ctype.h>
#include <infiniband/verbs.h>
#include <stdbool.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define MAXSTR 2048

typedef enum { DURATION = 0, ITERATION, INFINITE } StopType;
typedef enum { AckRtt = 0, PPRtt, WrRtt, RdRtt, CoFlow } TestType;
typedef enum { RC = IBV_QPT_RC, UD = IBV_QPT_UD, UC = IBV_QPT_UC } QPType;

struct UserParameter {
    uint32_t msg_size;
    uint32_t rx_depth;
    uint32_t tx_depth;
    uint32_t qps_number;
    uint32_t iterations;
    uint32_t duration;
    uint32_t burst_size;
    uint32_t rate_limit;

    uint32_t num_client_post_send_threads;
    uint32_t num_client_poll_recv_threads;
    uint32_t num_client_poll_send_threads;

    uint32_t num_server_post_recv_threads;
    uint32_t num_server_poll_recv_threads;
    uint32_t num_server_poll_send_threads;
    StopType stop_type;
    TestType test_type;
    QPType qp_type;
    char socket_port[MAXSTR];
    char server_name[MAXSTR];
    char device_name[MAXSTR];
    char output_filename[MAXSTR];
    bool is_server;
    bool bw_limiter;
    bool sampling;
    bool show_result;
    bool verbose;
    bool is_realtime;
    float sampling_ratio;

} __attribute__((aligned(64)));

extern struct UserParameter user_parameter;

void print_user_parameter_info();
int load_default_parameters();
void parse_opt(int argc, char **argv);
void trim(char *s);
int init_env();
void destroy_env();
#endif
