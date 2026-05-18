#include "ipc_common.h"
#include <sys/msg.h>

typedef struct {
    long mtype;
    char mtext[1024 * 1024];
} message_t;

typedef struct {
    int msgqid;
    pid_t peer_pid;
    int is_server;
} msgq_priv_t;

static volatile int server_running = 1;

static void cleanup_handler(int sig) {
    LOG_INFO("Message Queue Server: Received signal %d, shutting down...", sig);
    server_running = 0;
}

static int msgq_init_server(ipc_handle_t *handle) {
    msgq_priv_t *priv = (msgq_priv_t*)handle->impl;
    key_t key = MSGQ_KEY;
    
    LOG_INFO("Message Queue Server: Initializing...");
    
    priv->msgqid = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
    if (priv->msgqid == -1) {
        if (errno == EEXIST) {
            priv->msgqid = msgget(key, 0666);
            if (priv->msgqid != -1) {
                msgctl(priv->msgqid, IPC_RMID, NULL);
                priv->msgqid = msgget(key, IPC_CREAT | 0666);
            }
        }
        if (priv->msgqid == -1) {
            perror("msgget server");
            return -1;
        }
    }
    
    priv->is_server = 1;
    priv->peer_pid = 0;
    
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);
    signal(SIGQUIT, cleanup_handler);
    
    LOG_INFO("Message Queue Server: Created msgqid=%d", priv->msgqid);
    
    return 0;
}

static int msgq_init_client(ipc_handle_t *handle) {
    msgq_priv_t *priv = (msgq_priv_t*)handle->impl;
    key_t key = MSGQ_KEY;
    
    LOG_INFO("Message Queue Client: Connecting to server...");
    
    priv->msgqid = msgget(key, 0666);
    if (priv->msgqid == -1) {
        perror("msgget client");
        return -1;
    }
    
    priv->is_server = 0;
    priv->peer_pid = getpid();
    
    LOG_INFO("Message Queue Client: Connected msgqid=%d, my_pid=%d", 
             priv->msgqid, priv->peer_pid);
    
    return 0;
}

static int msgq_send(ipc_handle_t *handle, const void *data, size_t len) {
    msgq_priv_t *priv = (msgq_priv_t*)handle->impl;
    message_t msg;
    
    if (len > sizeof(msg.mtext)) {
        LOG_ERROR("msgq_send: data too large (%zu > %zu)", len, sizeof(msg.mtext));
        return -1;
    }
    
    if (priv->is_server) {
        msg.mtype = priv->peer_pid;
    } else {
        msg.mtype = 1;
    }
    
    memcpy(msg.mtext, data, len);
    
    if (msgsnd(priv->msgqid, &msg, len, 0) == -1) {
        LOG_ERROR("msgsnd failed: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

static int msgq_recv(ipc_handle_t *handle, void *buf, size_t len) {
    msgq_priv_t *priv = (msgq_priv_t*)handle->impl;
    message_t msg;
    ssize_t ret;
    long msgtype;
    
    if (len > sizeof(msg.mtext)) {
        LOG_ERROR("msgq_recv: buffer too large (%zu > %zu)", len, sizeof(msg.mtext));
        return -1;
    }
    
    if (priv->is_server) {
        msgtype = 1;
        
        ret = msgrcv(priv->msgqid, &msg, sizeof(msg.mtext), msgtype, 0);
        if (ret == -1) {
            if (errno != EINTR) {
                LOG_ERROR("msgrcv server failed: %s", strerror(errno));
            }
            return -1;
        }
        
        if (ret >= sizeof(pid_t)) {
            pid_t client_pid;
            memcpy(&client_pid, msg.mtext, sizeof(pid_t));
            priv->peer_pid = client_pid;
            printf("Server receives a message from %d£¡\n", client_pid);
            fflush(stdout);
        }
        
        memcpy(buf, msg.mtext, ret);
        return ret;
        
    } else {
        msgtype = priv->peer_pid;
        
        ret = msgrcv(priv->msgqid, &msg, sizeof(msg.mtext), msgtype, 0);
        if (ret == -1) {
            LOG_ERROR("msgrcv client failed: %s", strerror(errno));
            return -1;
        }
        
        if (ret >= sizeof(pid_t)) {
            pid_t server_pid;
            memcpy(&server_pid, msg.mtext, sizeof(pid_t));
            printf("Client receives a message from %d£¡\n", server_pid);
            fflush(stdout);
        }
        
        memcpy(buf, msg.mtext, ret);
        return ret;
    }
}

static int msgq_close(ipc_handle_t *handle) {
    msgq_priv_t *priv = (msgq_priv_t*)handle->impl;
    
    if (!priv) return 0;
    
    if (priv->is_server && !server_running) {
        if (priv->msgqid != -1) {
            msgctl(priv->msgqid, IPC_RMID, NULL);
            LOG_INFO("Message Queue Server: Removed msgqid=%d", priv->msgqid);
        }
    }
    
    free(priv);
    handle->impl = NULL;
    
    return 0;
}

int msgq_init_server(ipc_handle_t *h) { return msgq_init_server(h); }
int msgq_init_client(ipc_handle_t *h) { return msgq_init_client(h); }
int msgq_send(ipc_handle_t *h, const void *d, size_t l) { return msgq_send(h, d, l); }
int msgq_recv(ipc_handle_t *h, void *b, size_t l) { return msgq_recv(h, b, l); }
int msgq_close(ipc_handle_t *h) { return msgq_close(h); }
