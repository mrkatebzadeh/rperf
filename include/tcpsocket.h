#ifndef SOCK_H_
#define SOCK_H_

#include <inttypes.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include "log.h"
#include "parameters.h"
#include "rdma.h"

#define SOCK_SYNC_MSG     "sync"

ssize_t sock_read (int sock_fd, void *buffer, size_t len);
ssize_t sock_write (int sock_fd, void *buffer, size_t len);

int sock_create_bind (char *port);
int sock_create_connect (char *server_name, char *port);

int sock_set_qp_info(int sock_fd, struct QPInfo *qp_info);
int sock_get_qp_info(int sock_fd, struct QPInfo *qp_info);

int sock_set_parameter_info(int sock_fd, struct  UserParameter *user_parameter);
int sock_get_parameter_info(int sock_fd, struct UserParameter *user_parameter);

#endif /* SOCK_H_ */
