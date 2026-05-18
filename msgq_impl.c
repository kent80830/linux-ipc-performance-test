#include "ipc_common.h"

typedef struct {
    int msgqid;
} msgq_handle_t;

typedef struct {
    long mtype;
    char mtext[65536];
} msgbuf_t;

msgq_handle_t* msgq_server_init(void) {
    msgq_handle_t *h = malloc(sizeof(msgq_handle_t));
    if (!h) {
        LOG_ERROR("malloc failed");
        return NULL;
    }
    
    int old = msgget(MSGQ_KEY, 0666);
    if (old != -1) {
        msgctl(old, IPC_RMID, NULL);
    }
    
    int msgqid = msgget(MSGQ_KEY, IPC_CREAT | 0666);
    if (msgqid == -1) {
        perror("msgget server");
        free(h);
        return NULL;
    }
    
    h->msgqid = msgqid;
    LOG_INFO("Message Queue Server: created msgqid=%d", h->msgqid);
    
    return h;
}

msgq_handle_t* msgq_client_init(void) {
    msgq_handle_t *h = malloc(sizeof(msgq_handle_t));
    if (!h) {
        LOG_ERROR("malloc failed");
        return NULL;
    }
    
    int msgqid = msgget(MSGQ_KEY, 0666);
    if (msgqid == -1) {
        perror("msgget client");
        free(h);
        return NULL;
    }
    
    h->msgqid = msgqid;
    LOG_INFO("Message Queue Client: connected msgqid=%d", h->msgqid);
    
    return h;
}

int msgq_send(msgq_handle_t *h, long type, const void *data, size_t len) {
    msgbuf_t *msg = malloc(sizeof(long) + len);
    if (!msg) return -1;
    
    msg->mtype = type;
    memcpy(msg->mtext, data, len);
    
    int ret = msgsnd(h->msgqid, msg, len, 0);
    free(msg);
    
    return ret == 0 ? 0 : -1;
}

int msgq_recv(msgq_handle_t *h, long type, void *buf, size_t len) {
    msgbuf_t *msg = malloc(sizeof(long) + len);
    if (!msg) return -1;
    
    ssize_t ret = msgrcv(h->msgqid, msg, len, type, 0);
    if (ret != -1) {
        memcpy(buf, msg->mtext, ret);
    }
    free(msg);
    
    return ret != -1 ? 0 : -1;
}

void msgq_close(msgq_handle_t *h, int is_server) {
    if (h) {
        if (is_server) {
            msgctl(h->msgqid, IPC_RMID, NULL);
            LOG_INFO("Message Queue Server: removed");
        }
        free(h);
    }
}
