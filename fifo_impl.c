#include "ipc_common.h"

typedef struct {
    int fd_read;
    int fd_write;
    char *rd_path;
    char *wr_path;
    int is_server;
} fifo_priv_t;

static int fifo_init_server(ipc_handle_t *handle) {
    fifo_priv_t *priv = (fifo_priv_t*)handle->impl;
    
    LOG_INFO("FIFO Server: Initializing...");
    
    unlink(FIFO_PATH_RD);
    unlink(FIFO_PATH_WR);
    
    if (mkfifo(FIFO_PATH_RD, 0666) == -1) {
        perror("mkfifo read pipe");
        return -1;
    }
    if (mkfifo(FIFO_PATH_WR, 0666) == -1) {
        perror("mkfifo write pipe");
        unlink(FIFO_PATH_RD);
        return -1;
    }
    
    LOG_INFO("FIFO Server: Created pipes - %s (read), %s (write)", 
             FIFO_PATH_RD, FIFO_PATH_WR);
    
    priv->fd_read = open(FIFO_PATH_RD, O_RDWR);
    if (priv->fd_read == -1) {
        perror("open read pipe");
        return -1;
    }
    
    priv->fd_write = open(FIFO_PATH_WR, O_RDWR);
    if (priv->fd_write == -1) {
        perror("open write pipe");
        close(priv->fd_read);
        return -1;
    }
    
    priv->rd_path = strdup(FIFO_PATH_RD);
    priv->wr_path = strdup(FIFO_PATH_WR);
    priv->is_server = 1;
    
    LOG_INFO("FIFO Server: Ready, fd_read=%d, fd_write=%d", 
             priv->fd_read, priv->fd_write);
    
    return 0;
}

static int fifo_init_client(ipc_handle_t *handle) {
    fifo_priv_t *priv = (fifo_priv_t*)handle->impl;
    
    LOG_INFO("FIFO Client: Connecting to server...");
    
    priv->fd_write = open(FIFO_PATH_RD, O_RDWR);
    if (priv->fd_write == -1) {
        perror("open write pipe (client)");
        return -1;
    }
    
    priv->fd_read = open(FIFO_PATH_WR, O_RDWR);
    if (priv->fd_read == -1) {
        perror("open read pipe (client)");
        close(priv->fd_write);
        return -1;
    }
    
    priv->rd_path = strdup(FIFO_PATH_WR);
    priv->wr_path = strdup(FIFO_PATH_RD);
    priv->is_server = 0;
    
    LOG_INFO("FIFO Client: Connected, fd_read=%d, fd_write=%d", 
             priv->fd_read, priv->fd_write);
    
    return 0;
}

static int fifo_send(ipc_handle_t *handle, const void *data, size_t len) {
    fifo_priv_t *priv = (fifo_priv_t*)handle->impl;
    ssize_t ret;
    size_t total_sent = 0;
    const char *ptr = (const char*)data;
    
    while (total_sent < len) {
        ret = write(priv->fd_write, ptr + total_sent, len - total_sent);
        if (ret == -1) {
            if (errno == EINTR) continue;
            LOG_ERROR("FIFO send failed: %s", strerror(errno));
            return -1;
        }
        total_sent += ret;
    }
    
    return 0;
}

static int fifo_recv(ipc_handle_t *handle, void *buf, size_t len) {
    fifo_priv_t *priv = (fifo_priv_t*)handle->impl;
    ssize_t ret;
    size_t total_recv = 0;
    char *ptr = (char*)buf;
    
    while (total_recv < len) {
        ret = read(priv->fd_read, ptr + total_recv, len - total_recv);
        if (ret == -1) {
            if (errno == EINTR) continue;
            LOG_ERROR("FIFO recv failed: %s", strerror(errno));
            return -1;
        }
        if (ret == 0) {
            LOG_ERROR("FIFO recv: connection closed");
            return -1;
        }
        total_recv += ret;
    }
    
    return 0;
}

static int fifo_close(ipc_handle_t *handle) {
    fifo_priv_t *priv = (fifo_priv_t*)handle->impl;
    
    if (!priv) return 0;
    
    if (priv->fd_read > 0) {
        close(priv->fd_read);
        priv->fd_read = -1;
    }
    
    if (priv->fd_write > 0) {
        close(priv->fd_write);
        priv->fd_write = -1;
    }
    
    if (priv->is_server) {
        unlink(FIFO_PATH_RD);
        unlink(FIFO_PATH_WR);
        LOG_INFO("FIFO Server: Removed pipe files");
    }
    
    if (priv->rd_path) free(priv->rd_path);
    if (priv->wr_path) free(priv->wr_path);
    free(priv);
    handle->impl = NULL;
    
    return 0;
}

int fifo_init_server(ipc_handle_t *h) { return fifo_init_server(h); }
int fifo_init_client(ipc_handle_t *h) { return fifo_init_client(h); }
int fifo_send(ipc_handle_t *h, const void *d, size_t l) { return fifo_send(h, d, l); }
int fifo_recv(ipc_handle_t *h, void *b, size_t l) { return fifo_recv(h, b, l); }
int fifo_close(ipc_handle_t *h) { return fifo_close(h); }
