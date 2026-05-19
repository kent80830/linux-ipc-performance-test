#include "ipc_common.h"

extern void* fifo_server_init(void);
extern void* fifo_client_init(void);
extern int fifo_send(void *h, const void *data, size_t len);
extern int fifo_recv(void *h, void *buf, size_t len);
extern void fifo_close(void *h);

extern void* msgq_server_init(void);
extern void* msgq_client_init(void);
extern int msgq_send(void *h, long type, const void *data, size_t len);
extern int msgq_recv(void *h, long type, void *buf, size_t len);
extern void msgq_close(void *h, int is_server);

ipc_handle_t* ipc_server_init(int type) {
    ipc_handle_t *h = malloc(sizeof(ipc_handle_t));
    if (!h) return NULL;
    
    h->type = type;
    
    if (type == IPC_TYPE_FIFO) {
        h->handle = fifo_server_init();
    } else {
        h->handle = msgq_server_init();
    }
    
    if (!h->handle) {
        free(h);
        return NULL;
    }
    
    return h;
}

ipc_handle_t* ipc_client_init(int type) {
    ipc_handle_t *h = malloc(sizeof(ipc_handle_t));
    if (!h) return NULL;
    
    h->type = type;
    
    if (type == IPC_TYPE_FIFO) {
        h->handle = fifo_client_init();
    } else {
        h->handle = msgq_client_init();
    }
    
    if (!h->handle) {
        free(h);
        return NULL;
    }
    
    return h;
}

int ipc_send(ipc_handle_t *h, const void *data, size_t len) {
    if (h->type == IPC_TYPE_FIFO) {
        return fifo_send(h->handle, data, len);
    } else {
        return msgq_send(h->handle, 1, data, len);
    }
}

int ipc_recv(ipc_handle_t *h, void *buf, size_t len) {
    if (h->type == IPC_TYPE_FIFO) {
        return fifo_recv(h->handle, buf, len);
    } else {
        return msgq_recv(h->handle, 1, buf, len);
    }
}

void ipc_close(ipc_handle_t *h, int is_server) {
    if (h) {
        if (h->type == IPC_TYPE_FIFO) {
            fifo_close(h->handle);
        } else {
            msgq_close(h->handle, is_server);
        }
        free(h);
    }
}
