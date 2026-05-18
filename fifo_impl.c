#ifndef IPC_COMMON_H
#define IPC_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#define FIFO_PATH_RD "./fifo_rd"
#define FIFO_PATH_WR "./fifo_wr"
#define MSGQ_KEY 0x1234

typedef enum {
    IPC_TYPE_FIFO = 0,
    IPC_TYPE_MSGQ
} ipc_type_t;

typedef struct perf_data {
    double avg_latency_us;
    double throughput_mbps;
    int total_bytes;
    double total_time_us;
    int success_count;
} perf_data_t;

typedef struct ipc_handle {
    ipc_type_t type;
    void *impl;
    int (*init_server)(struct ipc_handle *handle);
    int (*init_client)(struct ipc_handle *handle);
    int (*send)(struct ipc_handle *handle, const void *data, size_t len);
    int (*recv)(struct ipc_handle *handle, void *buf, size_t len);
    int (*close)(struct ipc_handle *handle);
} ipc_handle_t;

ipc_handle_t* ipc_create(ipc_type_t type);
void ipc_destroy(ipc_handle_t *handle);

#endif
