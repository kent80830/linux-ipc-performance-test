#include "ipc_common.h"
#include <sys/time.h>
#include <sys/wait.h>
#include <math.h>

static long long get_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void demonstrate_communication(ipc_type_t type) {
    ipc_handle_t *server = ipc_create(type);
    ipc_handle_t *client = ipc_create(type);
    
    if (!server || !client) {
        printf("Failed to create IPC handles for demonstration\n");
        if (server) ipc_destroy(server);
        if (client) ipc_destroy(client);
        return;
    }
    
    pid_t pid = fork();
    
    if (pid == 0) {
        sleep(1);
        
        if (client->init_client(client) == 0) {
            pid_t my_pid = getpid();
            char buffer[256];
            
            printf("\n=== Client Demo ===\n");
            printf("Client PID: %d\n", my_pid);
            
            client->send(client, &my_pid, sizeof(my_pid));
            client->recv(client, buffer, sizeof(buffer));
            
            printf("Client received: %s\n", buffer);
            client->close(client);
        }
        
        ipc_destroy(client);
        exit(0);
        
    } else if (pid > 0) {
        if (server->init_server(server) == 0) {
            pid_t client_pid;
            char response[256];
            
            printf("\n=== Server Demo ===\n");
            printf("Server PID: %d\n", getpid());
            
            server->recv(server, &client_pid, sizeof(client_pid));
            printf("Server received client PID: %d\n", client_pid);
            
            snprintf(response, sizeof(response), 
                     "Hello from server (PID=%d)!", getpid());
            server->send(server, response, strlen(response) + 1);
            
            server->close(server);
        }
        
        wait(NULL);
        ipc_destroy(server);
    } else {
        perror("fork");
        ipc_destroy(server);
        ipc_destroy(client);
    }
}

static perf_data_t run_performance_test(ipc_type_t type, int data_size, int iterations) {
    ipc_handle_t *server = ipc_create(type);
    ipc_handle_t *client = ipc_create(type);
    perf_data_t result = {0};
    int pipefd[2];
    
    if (!server || !client) {
        LOG_ERROR("Failed to create IPC handles");
        if (server) ipc_destroy(server);
        if (client) ipc_destroy(client);
        return result;
    }
    
    char *send_buf = (char*)malloc(data_size);
    char *recv_buf = (char*)malloc(data_size);
    
    if (!send_buf || !recv_buf) {
        LOG_ERROR("Failed to allocate test buffers");
        free(send_buf);
        free(recv_buf);
        ipc_destroy(server);
        ipc_destroy(client);
        return result;
    }
    
    srand(time(NULL) ^ (getpid() << 16));
    for (int i = 0; i < data_size; i++) {
        send_buf[i] = rand() % 256;
    }
    
    if (pipe(pipefd) == -1) {
        perror("pipe");
        free(send_buf);
        free(recv_buf);
        ipc_destroy(server);
        ipc_destroy(client);
        return result;
    }
    
    pid_t pid = fork();
    
    if (pid == 0) {
        close(pipefd[0]);
        
        if (client->init_client(client) == -1) {
            LOG_ERROR("Client init failed");
            exit(1);
        }
        
        char sync_char = 'R';
        write(pipefd[1], &sync_char, 1);
        
        long long start_time = get_usec();
        int success = 0;
        
        for (int i = 0; i < iterations; i++) {
            if (client->send(client, send_buf, data_size) == -1) break;
            if (client->recv(client, recv_buf, data_size) == -1) break;
            if (memcmp(send_buf, recv_buf, data_size) != 0) break;
            success++;
        }
        
        long long end_time = get_usec();
        
        write(pipefd[1], &success, sizeof(int));
        write(pipefd[1], &start_time, sizeof(long long));
        write(pipefd[1], &end_time, sizeof(long long));
        
        close(pipefd[1]);
        client->close(client);
        ipc_destroy(client);
        exit(0);
        
    } else if (pid > 0) {
        close(pipefd[1]);
        
        if (server->init_server(server) == -1) {
            LOG_ERROR("Server init failed");
            kill(pid, SIGTERM);
            wait(NULL);
            free(send_buf);
            free(recv_buf);
            ipc_destroy(server);
            ipc_destroy(client);
            return result;
        }
        
        char sync_char;
        read(pipefd[0], &sync_char, 1);
        
        for (int i = 0; i < iterations; i++) {
            if (server->recv(server, recv_buf, data_size) == -1) break;
            if (server->send(server, recv_buf, data_size) == -1) break;
        }
        
        int success;
        long long start_time, end_time;
        read(pipefd[0], &success, sizeof(int));
        read(pipefd[0], &start_time, sizeof(long long));
        read(pipefd[0], &end_time, sizeof(long long));
        
        int status;
        wait(&status);
        
        close(pipefd[0]);
        
        if (success > 0) {
            result.total_time_us = end_time - start_time;
            result.total_bytes = data_size * success * 2;
            result.avg_latency_us = result.total_time_us / success;
            result.throughput_mbps = (result.total_bytes * 8.0) / result.total_time_us;
            result.success_count = success;
        }
        
        server->close(server);
        ipc_destroy(server);
        
    } else {
        LOG_ERROR("Fork failed");
    }
    
    free(send_buf);
    free(recv_buf);
    ipc_destroy(client);
    
    return result;
}

static void print_performance_results(void) {
    int test_sizes[] = {1, 64, 1024, 65536, 1048576};
    const char *size_names[] = {"1B", "64B", "1KB", "64KB", "1MB"};
    
    ipc_type_t types[] = {IPC_TYPE_FIFO, IPC_TYPE_MSGQ};
    const char *type_names[] = {"FIFO (Named Pipe)", "Message Queue"};
    
    const int iterations = 1000;
    
    printf("\n");
    printf("================================================================================\n");
    printf("                    IPC Performance Test Results\n");
    printf("                         Test iterations: 1000\n");
    printf("================================================================================\n");
    printf("| IPC Type             | Data Size | Avg Latency (us) | Throughput (MB/s) |\n");
    printf("================================================================================\n");
    
    for (int t = 0; t < 2; t++) {
        for (int s = 0; s < 5; s++) {
            printf("| %-20s | %-9s | ", type_names[t], size_names[s]);
            fflush(stdout);
            
            perf_data_t result = run_performance_test(types[t], test_sizes[s], iterations);
            
            if (result.success_count >= iterations / 2) {
                printf("%-16.2f | %-17.2f |\n", 
                       result.avg_latency_us, result.throughput_mbps);
            } else {
                printf("%-16s | %-17s |\n", "FAILED", "FAILED");
            }
            
            sleep(1);
        }
        if (t < 1) {
            printf("--------------------------------------------------------------------------------\n");
        }
    }
    
    printf("================================================================================\n");
    printf("\n");
}

int main(int argc, char *argv[]) {
    printf("\n");
    printf("================================================================================\n");
    printf("                    Linux IPC Mechanisms Performance Test\n");
    printf("                         FIFO & Message Queue\n");
    printf("================================================================================\n");
    
    printf("\n\n========== Part 1: Communication Demonstration ==========\n");
    printf("\n--- FIFO Communication Demo ---");
    demonstrate_communication(IPC_TYPE_FIFO);
    
    printf("\n--- Message Queue Communication Demo ---");
    demonstrate_communication(IPC_TYPE_MSGQ);
    
    printf("\n\n========== Part 2: Performance Benchmark ==========\n");
    print_performance_results();
    
    printf("\n\n========== Part 3: Ethical Considerations ==========\n");
    printf("\n");
    printf("Based on the performance test results above, the following ethical issues\n");
    printf("should be considered in IPC system design:\n\n");
    
    printf("1. DATA PRIVACY & SECURITY\n");
    printf("   - FIFO pipes use file system permissions (0666), which may allow\n");
    printf("     unauthorized access if not properly configured.\n");
    printf("   - Message queues have no built-in encryption.\n\n");
    
    printf("2. RESOURCE FAIRNESS\n");
    printf("   - Large data packets show significantly higher latency.\n");
    printf("   - Real-time systems require predictable latency bounds.\n\n");
    
    printf("3. ENERGY EFFICIENCY\n");
    printf("   - Message queues involve kernel copying, increasing CPU usage.\n");
    printf("   - At scale, inefficient IPC can increase carbon footprint.\n\n");
    
    printf("4. AUDITABILITY & ACCOUNTABILITY\n");
    printf("   - Anonymous pipes and message queues lack identity tracking.\n\n");
    
    printf("5. CODE ETHICS\n");
    printf("   - Orphaned processes may cause resource leaks.\n");
    printf("   - Use proper signal handling for resource cleanup.\n\n");
    
    printf("================================================================================\n");
    
    return 0;
}
