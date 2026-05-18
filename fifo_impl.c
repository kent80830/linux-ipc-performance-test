#include "ipc_common.h"

typedef struct {
    int fd_read;
    int fd_write;
} fifo_handle_t;

fifo_handle_t* fifo_server_init(void) {
    fifo_handle_t *h = malloc(sizeof(fifo_handle_t));
    if (!h) {
        LOG_ERROR("malloc failed");
        return NULL;
    }
    
    unlink(FIFO_PATH_RD);
    unlink(FIFO_PATH_WR);
    
    if (mkfifo(FIFO_PATH_RD, 0666) == -1) {
        perror("mkfifo read pipe");
        free(h);
        return NULL;
    }
    if (mkfifo(FIFO_PATH_WR, 0666) == -1) {
        perror("mkfifo write pipe");
        unlink(FIFO_PATH_RD);
        free(h);
        return NULL;
    }
    
    LOG_INFO("FIFO Server: pipes created");
    
    h->fd_read = open(FIFO_PATH_RD, O_RDWR);
    h->fd_write = open(FIFO_PATH_WR, O_RDWR);
    
    if (h->fd_read == -1 || h->fd_write == -1) {
        perror("open pipe failed");
        free(h);
        return NULL;
    }
    
    return h;
}

fifo_handle_t* fifo_client_init(void) {
    fifo_handle_t *h = malloc(sizeof(fifo_handle_t));
    if (!h) {
        LOG_ERROR("malloc failed");
        return NULL;
    }
    
    h->fd_write = open(FIFO_PATH_RD, O_RDWR);
    h->fd_read = open(FIFO_PATH_WR, O_RDWR);
    
    if (h->fd_write == -1 || h->fd_read == -1) {
        perror("open pipe failed");
        free(h);
        return NULL;
    }
    
    LOG_INFO("FIFO Client: connected");
    return h;
}

int fifo_send(fifo_handle_t *h, const void *data, size_t len) {
    ssize_t ret;
    size_t total_sent = 0;
    const char *ptr = (const char*)data;
    
    while (total_sent < len) {
        ret = write(h->fd_write, ptr + total_sent, len - total_sent);
        if (ret == -1) {
            if (errno == EINTR) continue;
            LOG_ERROR("FIFO send failed: %s", strerror(errno));
            return -1;
        }
        total_sent += ret;
    }
    
    return 0;
}

int fifo_recv(fifo_handle_t *h, void *buf, size_t len) {
    ssize_t ret;
    size_t total_recv = 0;
    char *ptr = (char*)buf;
    
    while (total_recv < len) {
        ret = read(h->fd_read, ptr + total_recv, len - total_recv);
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

void fifo_close(fifo_handle_t *h) {
    if (h) {
        if (h->fd_read > 0) close(h->fd_read);
        if (h->fd_write > 0) close(h->fd_write);
        unlink(FIFO_PATH_RD);
        unlink(FIFO_PATH_WR);
        free(h);
        LOG_INFO("FIFO: closed and cleaned up");
    }
}
