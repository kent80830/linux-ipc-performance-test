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

long long get_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

void fifo_demo(void) {
    pid_t pid = fork();
    
    if (pid == 0) {
        sleep(1);
        void *client = fifo_client_init();
        
        pid_t my_pid = getpid();
        char buf[256];
        
        printf("\n=== FIFO Client Demo ===\n");
        printf("Client PID: %d\n", my_pid);
        
        fifo_send(client, &my_pid, sizeof(my_pid));
        fifo_recv(client, buf, sizeof(buf));
        printf("Client received: %s\n", buf);
        
        fifo_close(client);
        exit(0);
        
    } else {
        void *server = fifo_server_init();
        pid_t client_pid;
        char response[256];
        
        printf("\n=== FIFO Server Demo ===\n");
        printf("Server PID: %d\n", getpid());
        
        fifo_recv(server, &client_pid, sizeof(client_pid));
        printf("Server received client PID: %d\n", client_pid);
        
        snprintf(response, sizeof(response), "Hello from FIFO server (PID=%d)!", getpid());
        fifo_send(server, response, strlen(response) + 1);
        
        fifo_close(server);
        wait(NULL);
    }
}

void msgq_demo(void) {
    pid_t pid = fork();
    
    if (pid == 0) {
        sleep(1);
        void *client = msgq_client_init();
        
        pid_t my_pid = getpid();
        char buf[256];
        
        printf("\n=== Message Queue Client Demo ===\n");
        printf("Client PID: %d\n", my_pid);
        
        msgq_send(client, 1, &my_pid, sizeof(my_pid));
        msgq_recv(client, my_pid, buf, sizeof(buf));
        printf("Client received: %s\n", buf);
        
        msgq_close(client, 0);
        exit(0);
        
    } else {
        void *server = msgq_server_init();
        pid_t client_pid;
        char response[256];
        
        printf("\n=== Message Queue Server Demo ===\n");
        printf("Server PID: %d\n", getpid());
        
        msgq_recv(server, 1, &client_pid, sizeof(client_pid));
        printf("Server received client PID: %d\n", client_pid);
        
        snprintf(response, sizeof(response), "Hello from MsgQ server (PID=%d)!", getpid());
        msgq_send(server, client_pid, response, strlen(response) + 1);
        
        msgq_close(server, 1);
        wait(NULL);
    }
}

void test_fifo(int data_size, const char *size_name, int iterations) {
    int pipefd[2];
    pipe(pipefd);
    
    char *send_buf = malloc(data_size);
    char *recv_buf = malloc(data_size);
    for (int i = 0; i < data_size; i++) send_buf[i] = rand() % 256;
    
    pid_t pid = fork();
    
    if (pid == 0) {
        close(pipefd[0]);
        void *client = fifo_client_init();
        
        char sync = 'R';
        write(pipefd[1], &sync, 1);
        
        for (int i = 0; i < iterations; i++) {
            fifo_send(client, send_buf, data_size);
            fifo_recv(client, recv_buf, data_size);
        }
        
        fifo_close(client);
        exit(0);
        
    } else {
        close(pipefd[1]);
        void *server = fifo_server_init();
        
        char sync;
        read(pipefd[0], &sync, 1);
        
        long long start = get_usec();
        
        for (int i = 0; i < iterations; i++) {
            fifo_recv(server, recv_buf, data_size);
            fifo_send(server, recv_buf, data_size);
        }
        
        long long end = get_usec();
        
        wait(NULL);
        close(pipefd[0]);
        fifo_close(server);
        
        double avg_latency = (end - start) / (double)iterations;
        double throughput = (data_size * iterations * 2 * 8.0) / (end - start);
        
        printf("| %-20s | %-9s | %-16.2f | %-17.2f |\n", 
               "FIFO (Named Pipe)", size_name, avg_latency, throughput);
    }
    
    free(send_buf);
    free(recv_buf);
}

void test_msgq(int data_size, const char *size_name, int iterations) {
    int pipefd[2];
    pipe(pipefd);
    
    char *send_buf = malloc(data_size);
    char *recv_buf = malloc(data_size);
    for (int i = 0; i < data_size; i++) send_buf[i] = rand() % 256;
    
    pid_t pid = fork();
    
    if (pid == 0) {
        close(pipefd[0]);
        void *client = msgq_client_init();
        pid_t my_pid = getpid();
        
        char sync = 'R';
        write(pipefd[1], &sync, 1);
        
        long long start = get_usec();
        
        for (int i = 0; i < iterations; i++) {
            msgq_send(client, 1, send_buf, data_size);
            msgq_recv(client, my_pid, recv_buf, data_size);
        }
        
        long long end = get_usec();
        
        write(pipefd[1], &start, sizeof(long long));
        write(pipefd[1], &end, sizeof(long long));
        
        msgq_close(client, 0);
        exit(0);
        
    } else {
        close(pipefd[1]);
        void *server = msgq_server_init();
        
        char sync;
        read(pipefd[0], &sync, 1);
        
        long long start, end;
        
        for (int i = 0; i < iterations; i++) {
            msgq_recv(server, 1, recv_buf, data_size);
            msgq_send(server, getpid(), recv_buf, data_size);
        }
        
        read(pipefd[0], &start, sizeof(long long));
        read(pipefd[0], &end, sizeof(long long));
        
        wait(NULL);
        close(pipefd[0]);
        msgq_close(server, 1);
        
        double avg_latency = (end - start) / (double)iterations;
        double throughput = (data_size * iterations * 2 * 8.0) / (end - start);
        
        printf("| %-20s | %-9s | %-16.2f | %-17.2f |\n", 
               "Message Queue", size_name, avg_latency, throughput);
    }
    
    free(send_buf);
    free(recv_buf);
}

int main(void) {
    printf("\n");
    printf("================================================================================\n");
    printf("                    Linux IPC Mechanisms Performance Test\n");
    printf("                         FIFO & Message Queue\n");
    printf("================================================================================\n");
    
    printf("\n========== Part 1: Communication Demonstration ==========\n");
    
    printf("\n--- FIFO Communication Demo ---\n");
    fifo_demo();
    
    printf("\n--- Message Queue Communication Demo ---\n");
    msgq_demo();
    
    printf("\n\n========== Part 2: Performance Benchmark ==========\n");
    printf("\n");
    printf("================================================================================\n");
    printf("| IPC Type             | Data Size | Avg Latency (us) | Throughput (MB/s) |\n");
    printf("================================================================================\n");
    
    int sizes[] = {1, 64, 1024, 65536};
    const char *size_names[] = {"1B", "64B", "1KB", "64KB"};
    int iterations = 10;
    
    printf("\n--- FIFO Performance Test ---\n");
    for (int i = 0; i < 4; i++) {
        test_fifo(sizes[i], size_names[i], iterations);
        usleep(500000);
    }
    
    printf("--------------------------------------------------------------------------------\n");
    
    printf("\n--- Message Queue Performance Test ---\n");
    for (int i = 0; i < 4; i++) {
        test_msgq(sizes[i], size_names[i], iterations);
        usleep(500000);
    }
    
    
    printf("================================================================================\n");
    
    return 0;
}
