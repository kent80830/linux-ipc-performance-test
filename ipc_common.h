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

#define IPC_TYPE_FIFO 0
#define IPC_TYPE_MSGQ 1

#define CURRENT_IPC_TYPE IPC_TYPE_MSGQ   

typedef struct {
    double avg_latency_us;
    double throughput_mbps;
    int total_bytes;
    double total_time_us;
    int success_count;
} perf_data_t;

typedef struct {
    int type;
    void *handle;
} ipc_handle_t;

ipc_handle_t* ipc_server_init(int type);
ipc_handle_t* ipc_client_init(int type);
int ipc_send(ipc_handle_t *h, const void *data, size_t len);
int ipc_recv(ipc_handle_t *h, void *buf, size_t len);
void ipc_close(ipc_handle_t *h, int is_server);

#endif
