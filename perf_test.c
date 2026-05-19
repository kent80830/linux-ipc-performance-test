#include "ipc_common.h"

long long get_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

void generic_demo(int ipc_type) {
    pid_t pid = fork();
    
    if (pid == 0) {
        sleep(1);
        ipc_handle_t *client = ipc_client_init(ipc_type);
        
        if (client) {
            pid_t my_pid = getpid();
            char buf[256];
            
            printf("\n=== Client Demo ===\n");
            printf("Client PID: %d\n", my_pid);
            
            ipc_send(client, &my_pid, sizeof(my_pid));
            ipc_recv(client, buf, sizeof(buf));
            printf("Client received: %s\n", buf);
            
            ipc_close(client, 0);
        }
        exit(0);
        
    } else {
        ipc_handle_t *server = ipc_server_init(ipc_type);
        
        if (server) {
            pid_t client_pid;
            char response[256];
            
            printf("\n=== Server Demo ===\n");
            printf("Server PID: %d\n", getpid());
            
            ipc_recv(server, &client_pid, sizeof(client_pid));
            printf("Server received client PID: %d\n", client_pid);
            
            snprintf(response, sizeof(response), 
                     "Hello from server (PID=%d)!", getpid());
            ipc_send(server, response, strlen(response) + 1);
            
            ipc_close(server, 1);
        }
        
        wait(NULL);
    }
}

void test_fifo_perf(int data_size, const char *size_name, int iterations) {
    int pipefd[2];
    pipe(pipefd);
    
    char *send_buf = malloc(data_size);
    char *recv_buf = malloc(data_size);
    for (int i = 0; i < data_size; i++) send_buf[i] = rand() % 256;
    
    pid_t pid = fork();
    
    if (pid == 0) {
        close(pipefd[0]);
        ipc_handle_t *client = ipc_client_init(IPC_TYPE_FIFO);
        
        char sync = 'R';
        write(pipefd[1], &sync, 1);
        
        for (int i = 0; i < iterations; i++) {
            ipc_send(client, send_buf, data_size);
            ipc_recv(client, recv_buf, data_size);
        }
        
        ipc_close(client, 0);
        exit(0);
        
    } else {
        close(pipefd[1]);
        ipc_handle_t *server = ipc_server_init(IPC_TYPE_FIFO);
        
        char sync;
        read(pipefd[0], &sync, 1);
        
        long long start = get_usec();
        
        for (int i = 0; i < iterations; i++) {
            ipc_recv(server, recv_buf, data_size);
            ipc_send(server, recv_buf, data_size);
        }
        
        long long end = get_usec();
        
        wait(NULL);
        close(pipefd[0]);
        ipc_close(server, 1);
        
        double avg_latency = (end - start) / (double)iterations;
        double throughput = (data_size * iterations * 2 * 8.0) / (end - start);
        
        printf("| %-20s | %-9s | %-16.2f | %-17.2f |\n", 
               "FIFO (Named Pipe)", size_name, avg_latency, throughput);
    }
    
    free(send_buf);
    free(recv_buf);
}

void test_msgq_perf(int data_size, const char *size_name, int iterations) {
    int pipefd[2];
    pipe(pipefd);
    
    char *send_buf = malloc(data_size);
    char *recv_buf = malloc(data_size);
    for (int i = 0; i < data_size; i++) send_buf[i] = rand() % 256;
    
    pid_t pid = fork();
    
    if (pid == 0) {
        close(pipefd[0]);
        ipc_handle_t *client = ipc_client_init(IPC_TYPE_MSGQ);
        pid_t my_pid = getpid();
        
        char sync = 'R';
        write(pipefd[1], &sync, 1);
        
        long long start = get_usec();
        
        for (int i = 0; i < iterations; i++) {
            ipc_send(client, send_buf, data_size);
            ipc_recv(client, recv_buf, data_size);
        }
        
        long long end = get_usec();
        
        write(pipefd[1], &start, sizeof(long long));
        write(pipefd[1], &end, sizeof(long long));
        
        ipc_close(client, 0);
        exit(0);
        
    } else {
        close(pipefd[1]);
        ipc_handle_t *server = ipc_server_init(IPC_TYPE_MSGQ);
        
        char sync;
        read(pipefd[0], &sync, 1);
        
        long long start, end;
        
        for (int i = 0; i < iterations; i++) {
            ipc_recv(server, recv_buf, data_size);
            ipc_send(server, recv_buf, data_size);
        }
        
        read(pipefd[0], &start, sizeof(long long));
        read(pipefd[0], &end, sizeof(long long));
        
        wait(NULL);
        close(pipefd[0]);
        ipc_close(server, 1);
        
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
    printf("                    Current IPC Type: %s\n",
           CURRENT_IPC_TYPE == IPC_TYPE_FIFO ? "FIFO" : "Message Queue");
    printf("================================================================================\n");
    
    printf("\n========== Part 1: Communication Demonstration ==========\n");
    printf("\n--- Using Generic IPC Interface ---\n");
    generic_demo(CURRENT_IPC_TYPE);
    
    printf("\n\n========== Part 2: Performance Benchmark ==========\n");
    printf("\n");
    printf("================================================================================\n");
    printf("| IPC Type             | Data Size | Avg Latency (us) | Throughput (MB/s) |\n");
    printf("================================================================================\n");
    
    int sizes[] = {1, 64, 1024, 65536};
    const char *size_names[] = {"1B", "64B", "1KB", "64KB"};
    int iterations = 10;
    
    if (CURRENT_IPC_TYPE == IPC_TYPE_FIFO) {
        printf("\n--- FIFO Performance Test ---\n");
        for (int i = 0; i < 4; i++) {
            test_fifo_perf(sizes[i], size_names[i], iterations);
            usleep(500000);
        }
    } else {
        printf("\n--- Message Queue Performance Test ---\n");
        for (int i = 0; i < 4; i++) {
            test_msgq_perf(sizes[i], size_names[i], iterations);
            usleep(500000);
        }
    }
    
    printf("================================================================================\n");
    
    printf("\n\n========== Part 3: Ethical Considerations ==========\n");
    printf("\n1. DATA PRIVACY & SECURITY\n");
    printf("2. RESOURCE FAIRNESS\n");
    printf("3. ENERGY EFFICIENCY\n");
    printf("4. AUDITABILITY & ACCOUNTABILITY\n");
    printf("5. CODE ETHICS\n");
    printf("\n================================================================================\n");
    
    return 0;
}
