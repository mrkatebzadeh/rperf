#define _GNU_SOURCE
#include <getopt.h>
#include <stdlib.h>
#include "log.h"
#include "parameters.h"
#include "rperf.h"

FILE *log_fp = NULL;

struct UserParameter user_parameter;
char **configuration;
int configc;
int main(int argc, char *argv[]) {
    int ret = load_default_parameters();

    parse_opt(argc, argv);

    ret = init_env();
    check(ret == 0, "Failed to init env");

    if (user_parameter.is_server) {
        ret = server();
    } else {
        ret = client();
    }
    check(ret == 0, "Failed to run RPERF.");

error:
    destroy_env();
    return ret;
}

int init_env() {
    trim(user_parameter.server_name);
    trim(user_parameter.socket_port);
    trim(user_parameter.output_filename);
    if (user_parameter.is_server) {
        user_parameter.server_name[0] = '\0';
        log_fp                        = fopen("server.log", "w");
    } else {
        log_fp = fopen("client.log", "w");
    }
    check(log_fp != NULL, "Failed to open log file");

    LOG_FILE("==== IB Echo Server ====");
    print_user_parameter_info();

    return 0;
error:
    return -1;
}

void destroy_env() {
    LOG_FILE("==== Run Finished ====");
    if (log_fp != NULL) {
        fclose(log_fp);
    }
}

void print_user_parameter_info() {
    char buffer[10];
    LOG_FILE("==== Configuraion ====");

    if (user_parameter.is_server) {
        printf("is_server                 = %s\n", "true");
    } else {
        printf("is_server                 = %s\n", "false");
    }

    LOG_FILE("device_name               = %s", user_parameter.device_name);
    LOG_FILE("msg_size                  = %d", user_parameter.msg_size);
    LOG_FILE("num_QPs                   = %d", user_parameter.qps_number);

    qp_type_to_str(user_parameter.qp_type, buffer);
    LOG_FILE("qp_type                   = %s", buffer);
    test_type_to_str(user_parameter.test_type, buffer);
    LOG_FILE("test_type                 = %s", buffer);
    LOG_FILE("rx_depth                  = %d", user_parameter.rx_depth);
    LOG_FILE("tx_depth                  = %d", user_parameter.tx_depth);
    LOG_FILE("iterations                = %d", user_parameter.iterations);
    LOG_FILE("duration                  = %d", user_parameter.duration);
    LOG_FILE("burst_size                = %d", user_parameter.burst_size);
    LOG_FILE("num_client_post_send      = %d",
        user_parameter.num_client_post_send_threads);
    LOG_FILE("num_client_poll_recv      = %d",
        user_parameter.num_client_poll_recv_threads);
    LOG_FILE("num_client_poll_send      = %d",
        user_parameter.num_client_poll_send_threads);
    LOG_FILE("num_server_post_recv      = %d",
        user_parameter.num_server_post_recv_threads);
    LOG_FILE("num_server_poll_recv      = %d",
        user_parameter.num_server_poll_recv_threads);
    LOG_FILE("num_server_poll_send      = %d",
        user_parameter.num_server_poll_send_threads);
    LOG_FILE("bw_limiter                = %d", user_parameter.bw_limiter);
    LOG_FILE("bandwidth                 = %d", user_parameter.rate_limit);
    LOG_FILE("sampling                  = %d", user_parameter.sampling);
    LOG_FILE("sampling_ratio            = %f", user_parameter.sampling_ratio);
    LOG_FILE("socket port               = %s", user_parameter.socket_port);
    LOG_FILE("realtime                  = %d", user_parameter.is_realtime);
    if (user_parameter.is_server == false) {
        LOG_FILE(
            "server_name               = %s", user_parameter.server_name);
    }

    LOG_FILE("---- End of Configuraion ----");
}

int load_default_parameters() {
    int argc = 0;
    char *buffer;
    size_t len = 0;
    ssize_t read;
    FILE *config_file = fopen("rdmarc", "r");

    if (config_file == NULL) {
        printf("<<<WARNING: There is no configuration file available>>>\n");
        return argc;
    }
    while ((read = getline(&buffer, &len, config_file)) != -1) {
        argc++;
        trim(buffer);
        if (strstr(buffer, "device_name") != NULL) {
            strcpy(
                user_parameter.device_name, (buffer + strlen("device_name=")));
        } else if (strstr(buffer, "msg_size") != NULL) {
            user_parameter.msg_size = atoi(buffer + strlen("msg_size="));
        } else if (strstr(buffer, "rx_depth") != NULL) {
            user_parameter.rx_depth = atoi(buffer + strlen("rx_depth="));
        } else if (strstr(buffer, "tx_depth") != NULL) {
            user_parameter.tx_depth = atoi(buffer + strlen("tx_depth="));
        } else if (strstr(buffer, "qps_number") != NULL) {
            user_parameter.qps_number = atoi(buffer + strlen("qps_number="));
        } else if (strstr(buffer, "qp_type") != NULL) {
            user_parameter.qp_type =
                qp_type_from_str(buffer + strlen("qp_type="));
        } else if (strstr(buffer, "test_type") != NULL) {
            user_parameter.test_type =
                test_type_from_str(buffer + strlen("test_type="));
        } else if (strstr(buffer, "iterations") != NULL) {
            user_parameter.iterations = atoi(buffer + strlen("iterations="));
        } else if (strstr(buffer, "duration") != NULL) {
            user_parameter.duration = atoi(buffer + strlen("duration="));
        } else if (strstr(buffer, "burst_size") != NULL) {
            user_parameter.burst_size = atoi(buffer + strlen("burst_size="));
        } else if (strstr(buffer, "rate_limit") != NULL) {
            user_parameter.rate_limit = atoi(buffer + strlen("rate_limit="));
        } else if (strstr(buffer, "num_client_post_send_threads") != NULL) {
            user_parameter.num_client_post_send_threads =
                atoi(buffer + strlen("num_client_post_send_threads="));
        } else if (strstr(buffer, "num_client_poll_recv_threads") != NULL) {
            user_parameter.num_client_poll_recv_threads =
                atoi(buffer + strlen("num_client_poll_recv_threads="));
        } else if (strstr(buffer, "num_client_poll_send_threads") != NULL) {
            user_parameter.num_client_poll_send_threads =
                atoi(buffer + strlen("num_client_poll_send_threads="));
        } else if (strstr(buffer, "num_server_post_recv_threads") != NULL) {
            user_parameter.num_server_post_recv_threads =
                atoi(buffer + strlen("num_server_post_recv_threads="));
        } else if (strstr(buffer, "num_server_poll_recv_threads") != NULL) {
            user_parameter.num_server_poll_recv_threads =
                atoi(buffer + strlen("num_server_poll_recv_threads="));
        } else if (strstr(buffer, "num_server_poll_send_threads") != NULL) {
            user_parameter.num_server_poll_send_threads =
                atoi(buffer + strlen("num_server_poll_send_threads="));
        } else if (strstr(buffer, "test_type") != NULL) {
            user_parameter.test_type = atoi(buffer + strlen("test_type="));
        } else if (strstr(buffer, "socket_port") != NULL) {
            strcpy(
                user_parameter.socket_port, (buffer + strlen("socket_port=")));
        } else if (strstr(buffer, "server_name") != NULL) {
            strcpy(
                user_parameter.server_name, (buffer + strlen("server_name=")));
        } else if (strstr(buffer, "output_filename") != NULL) {
            strcpy(user_parameter.output_filename,
                buffer + strlen("output_filename="));
        } else if (strstr(buffer, "is_server") != NULL) {
            user_parameter.is_server =
                strcmp((buffer + strlen("is_server=")), "true") == 0 ? true
                                                                     : false;
        } else if (strstr(buffer, "bw_limiter") != NULL) {
            user_parameter.bw_limiter =
                strcmp((buffer + strlen("bw_limiter=")), "true") == 0 ? true
                                                                      : false;
        } else if (strstr(buffer, "show_result") != NULL) {
            user_parameter.show_result =
                strcmp((buffer + strlen("show_result=")), "true") == 0 ? true
                                                                       : false;
        } else if (strstr(buffer, "verbose") != NULL) {
            user_parameter.verbose =
                strcmp((buffer + strlen("verbose=")), "true") == 0 ? true
                                                                   : false;
        } else if (strstr(buffer, "sampling_ratio") != NULL) {
            user_parameter.sampling_ratio =
                atof(buffer + strlen("sampling_ratio="));
        } else if (strstr(buffer, "sampling") != NULL) {
            user_parameter.sampling =
                strcmp((buffer + strlen("sampling=")), "true") == 0 ? true
                                                                    : false;
        } else if (strstr(buffer, "realtime") != NULL) {
            user_parameter.is_realtime =
                strcmp((buffer + strlen("realtime=")), "true") == 0 ? true
                                                                    : false;
        }
    }
    if (buffer) {
        free(buffer);
    }
    fclose(config_file);
    return argc;
}

void parse_opt(int argc, char **argv) {
    int c;
    while (1) {
        static struct option long_options[] = {

            {"verbose", no_argument, 0, 'v'}, {"server", no_argument, 0, 's'},
            {"sample", no_argument, 0, 'S'}, {"realtime", no_argument, 0, 'e'},
            {"bw_limiter", no_argument, 0, 'l'},
            {"device_name", required_argument, 0, 'd'},
            {"msg_size", required_argument, 0, 'm'},
            {"rx_depth", required_argument, 0, 'r'},
            {"tx_depth", required_argument, 0, 't'},
            {"qps", required_argument, 0, 'q'},
            {"qp_type", required_argument, 0, 'Q'},
            {"test_type", required_argument, 0, 'T'},
            {"concurrent_msg", required_argument, 0, 'n'},
            {"duration", required_argument, 0, 'D'},
            {"iterations", required_argument, 0, 'I'},
            {"burst_size", required_argument, 0, 'b'},
            {"bandwidth", required_argument, 0, 'w'},
            {"client_post_threads", required_argument, 0, 'z'},
            {"client_poll_recv_threads", required_argument, 0, 'x'},
            {"client_poll_send_threads", required_argument, 0, 'c'},
            {"server_post_threads", required_argument, 0, 'Z'},
            {"server_poll_recv_threads", required_argument, 0, 'X'},
            {"server_poll_send_threads", required_argument, 0, 'C'},
            {"port", required_argument, 0, 'p'},
            {"destination", required_argument, 0, 'i'},
            {"output", required_argument, 0, 'o'},
            {"sample_ratio", required_argument, 0, 'k'}, {0, 0, 0, 0}};

        int option_index = 0;

        c = getopt_long(argc, argv,
            "vseSd:lm:r:t:q:n:D:I:b:w:z:x:c:Z:X:C:T:Q:p:i:o:k:t", long_options,
            &option_index);

        if (c == -1) break;

        switch (c) {
            case 0:
                if (long_options[option_index].flag != 0) break;
                printf("option %s", long_options[option_index].name);
                if (optarg) printf(" with arg %s", optarg);
                printf("\n");
                break;

            case 's':
                user_parameter.is_server = true;
                break;

            case 'S':
                user_parameter.sampling = true;
                break;

            case 'd':
	        strcpy(user_parameter.device_name, optarg);
                break;

            case 'l':
                user_parameter.bw_limiter = true;
                break;

            case 'e':
                user_parameter.is_realtime = true;
                break;

            case 'm':
                user_parameter.msg_size = atoi(optarg);
                break;

            case 'r':
                user_parameter.rx_depth = atoi(optarg);
                break;

            case 't':
                user_parameter.tx_depth = atoi(optarg);
                break;

            case 'q':
                user_parameter.qps_number = atoi(optarg);
                break;
            case 'Q':
                user_parameter.qp_type = qp_type_from_str(optarg);
                break;
            case 'T':
                user_parameter.test_type = test_type_from_str(optarg);
                break;
            case 'D':
                user_parameter.duration = atoi(optarg);
                break;

            case 'I':
                user_parameter.iterations = atoi(optarg);
                break;

            case 'b':
                user_parameter.burst_size = atoi(optarg);
                break;

            case 'w':
                user_parameter.rate_limit = atoi(optarg);
                break;

            case 'z':
                user_parameter.num_client_post_send_threads = atoi(optarg);
                break;

            case 'x':
                user_parameter.num_client_poll_recv_threads = atoi(optarg);
                break;

            case 'c':
                user_parameter.num_client_poll_send_threads = atoi(optarg);
                break;

            case 'Z':
                user_parameter.num_server_post_recv_threads = atoi(optarg);
                break;

            case 'X':
                user_parameter.num_server_poll_recv_threads = atoi(optarg);
                break;

            case 'C':
                user_parameter.num_server_poll_send_threads = atoi(optarg);
                break;

            case 'p':
                strcpy(user_parameter.socket_port, optarg);
                break;

            case 'i':
                strcpy(user_parameter.server_name, optarg);
                break;

            case 'o':
                strcpy(user_parameter.output_filename, optarg);
                break;

            case 'k':
                user_parameter.sampling_ratio = atof(optarg);
                break;

            case 'h':
                printf("RDMALatency\n");
                break;

            default:
                printf("%c\n", c);
                // abort ();
        }
    }
}
void trim(char *s) {
    char *p = s;
    int l   = strlen(p);

    while (isspace(p[l - 1])) p[--l] = 0;
    while (*p && isspace(*p)) ++p, --l;

    memmove(s, p, l + 1);
}
