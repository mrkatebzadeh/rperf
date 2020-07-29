#ifndef CLIENT_H_
#define CLIENT_H_

#include <sched.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/time.h>
#include "rdma.h"

int run_client(int ib_con_idx);
int run_server(int ib_con_idx);
int server();
int client();
bool is_sample();
int cmpdouble(const void *a, const void *b);
int set_priority_max(long thread_id);
int set_affinity(long thread_id);
void optimize_thread(long thread_id);
uint64_t calc_gap_cycle(struct IBConnection *ib_con);
int connect_qp_server(int ib_con_idx, int peer_sockfd);
int connect_qp_client(int ib_con_idx);
bool is_sample();
QPType qp_type_from_str(char *str);
void qp_type_to_str(QPType qpt, char *buf);
TestType test_type_from_str(char *str);
void test_type_to_str(TestType tt, char *buf);
int get_connection_server();
int get_connection_client();
void *client_ackrtt_post(void *arg);
void *client_ackrtt_poll(void *arg);
void *client_loopback_poll(void *arg);
void *client_loopback_recv(void *arg);
void *server_func(void *arg);
void *client_poll_send_func(void *arg);
void *client_poll_recv_func(void *arg);
void *client_thread_func(void *arg);
void *server_post_recv_func(void *arg);
void *server_poll_send_func(void *arg);
void *server_thread(void *arg);
int run_test_server(int ib_con_idx);
int run_test_client(int ib_con_idx);

#endif /* client.h */
