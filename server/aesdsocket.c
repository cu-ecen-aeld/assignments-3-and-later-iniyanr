#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/queue.h>
#include <time.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define RECV_BUF_SIZE 1024


struct thread_info_s{
    pthread_t thread_id;
    int client_fd;
    struct sockaddr_in client_addr;
    int is_complete;
    SLIST_ENTRY(thread_info_s) entries;
};    

int server_fd = -1;
int keep_running = 1;
pthread_mutex_t file_mutex;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        keep_running = 0;
        if (server_fd != -1) {
            shutdown(server_fd, SHUT_RDWR);
        }
    }
}

void* timer_thread(void* arg) {
    (void)arg;
    while (keep_running) {
        for (int i = 0; i < 10 && keep_running; i++) {
            sleep(1);
        }
        if (!keep_running) break;

        time_t rawtime;
        struct tm *info;
        char buffer[100];

        time(&rawtime);
        info = localtime(&rawtime);

        strftime(buffer, sizeof(buffer), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", info);

        pthread_mutex_lock(&file_mutex);
        int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) {
            write(fd, buffer, strlen(buffer));
            close(fd);
        }
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
}


void* client_thread(void* thread_param){
    struct thread_info_s *t_info = (struct thread_info_s *)thread_param;
    char *packet_buf = malloc(RECV_BUF_SIZE);
    size_t buf_capacity = RECV_BUF_SIZE;
    size_t total_bytes = 0;
    ssize_t bytes_read;
    char recv_byte;

    if (!packet_buf) goto cleanup;

    while ((bytes_read = recv(t_info->client_fd, &recv_byte, 1, 0)) > 0) {
        if (total_bytes + 1 >= buf_capacity) {
            buf_capacity += RECV_BUF_SIZE;
            packet_buf = realloc(packet_buf, buf_capacity);
        }
        packet_buf[total_bytes++] = recv_byte;

        if (recv_byte == '\n') {
            pthread_mutex_lock(&file_mutex);
            
            int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if(fd >= 0){
                write(fd, packet_buf, total_bytes);
                close(fd);
            }

            fd = open(DATA_FILE, O_RDONLY);
            if(fd >= 0) {
                char read_buf[RECV_BUF_SIZE];
                ssize_t nr;
                while ((nr = read(fd, read_buf, sizeof(read_buf))) > 0) {
                    send(t_info->client_fd, read_buf, nr, 0);
                }
                close(fd);
            }
            pthread_mutex_unlock(&file_mutex);
            
            total_bytes = 0; 
        }
    }

cleanup:
    free(packet_buf);
    close(t_info->client_fd);
    syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(t_info->client_addr.sin_addr));
    t_info->is_complete = 1;
    return thread_param;
}


int main(int argc, char *argv[]) {
    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        if (daemon(0, 0) == -1) {
            perror("Daemon failed");
        }
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        return -1;
    }
    pthread_mutex_init(&file_mutex, NULL);
    SLIST_HEAD(thread_list_s, thread_info_s) head;
    SLIST_INIT(&head);

    pthread_t time_tid;
    pthread_create(&time_tid, NULL, timer_thread, NULL);

    while(keep_running){
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            if (errno == EINTR || !keep_running) {
                break; 
            }
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }
        syslog(LOG_INFO,"Accepted connection from : %s", inet_ntoa(client_addr.sin_addr));
        struct thread_info_s *t_info  = malloc(sizeof(struct thread_info_s));
        t_info -> client_fd = client_fd;
        t_info -> client_addr = client_addr;
        t_info -> is_complete = 0;
        if(pthread_create(&t_info->thread_id, NULL, client_thread, t_info) != 0){
            syslog(LOG_ERR, "thread creation failed");
            free(t_info);
            close(client_fd);
        }else{
            SLIST_INSERT_HEAD(&head, t_info, entries);
        }
        struct thread_info_s *it, *tmp;
        it = SLIST_FIRST(&head);
        while (it != NULL) {
            tmp = SLIST_NEXT(it, entries);
            if (it->is_complete) {
                pthread_join(it->thread_id, NULL);
                SLIST_REMOVE(&head, it, thread_info_s, entries);
                free(it);
            }
            it = tmp;
        }
    }


    pthread_join(time_tid, NULL);

    while(!SLIST_EMPTY(&head)){
            struct thread_info_s * it = SLIST_FIRST(&head);
            pthread_join(it->thread_id, NULL);
            SLIST_REMOVE_HEAD(&head, entries);
            free(it);
        }

    pthread_mutex_destroy(&file_mutex);
    close(server_fd);
    remove(DATA_FILE);
    syslog(LOG_INFO, "Successfully cleaned up. Exiting.");
    closelog();

    return 0;
}