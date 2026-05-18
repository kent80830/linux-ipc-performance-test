#include "ipc_common.h"

extern int fifo_init_server(ipc_handle_t*);
extern int fifo_init_client(ipc_handle_t*);
extern int fifo_send(ipc_handle_t*, const void*, size_t);
extern int fifo_recv(ipc_handle_t*, void*, size_t);
extern int fifo_close(ipc_handle_t*);

extern int msgq_init_server(ipc_handle_t*);
extern int msgq_init_client(ipc_handle_t*);
extern int msgq_send(ipc_handle_t*, const void*, size_t);
extern int msgq_recv(ipc_handle_t*, void*, size_t);
extern int msgq_close(ipc_handle_t*);

typedef struct {
    int fd_read;
    int fd_write;
    char *rd_path;
    char *wr_path;
    int is_server;
} fifo_priv_t;

typedef struct {
    int msgqid;
    pid_t peer_pid;
    int is_server;
} msgq_priv_t;

ipc_handle_t* ipc_create(ipc_type_t type) {
    ipc_handle_t *handle = (ipc_handle_t*)calloc(1, sizeof(ipc_handle_t));
    if (!handle) {
        LOG_ERROR("ipc_create: malloc handle failed");
        return NULL;
    }
    
    handle->type = type;
    
    switch (type) {
    case IPC_TYPE_FIFO:
        handle->impl = calloc(1, sizeof(fifo_priv_t));
        if (!handle->impl) {
            free(handle);
            LOG_ERROR("ipc_create: malloc fifo_priv failed");
            return NULL;
        }
        handle->init_server = fifo_init_server;
        handle->init_client = fifo_init_client;
        handle->send = fifo_send;
        handle->recv = fifo_recv;
        handle->close = fifo_close;
        break;
        
    case IPC_TYPE_MSGQ:
        handle->impl = calloc(1, sizeof(msgq_priv_t));
        if (!handle->impl) {
            free(handle);
            LOG_ERROR("ipc_create: malloc msgq_priv failed");
            return NULL;
        }
        handle->init_server = msgq_init_server;
        handle->init_client = msgq_init_client;
        handle->send = msgq_send;
        handle->recv = msgq_recv;
        handle->close = msgq_close;
        break;
        
    default:
        LOG_ERROR("Unknown IPC type: %d", type);
        free(handle);
        return NULL;
    }
    
    return handle;
}

void ipc_destroy(ipc_handle_t *handle) {
    if (handle) {
        if (handle->close) {
            handle->close(handle);
        }
        free(handle);
    }
}
