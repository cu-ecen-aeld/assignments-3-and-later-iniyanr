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
#include <fcntl.h>
#include <errno.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define RECV_BUF_SIZE 1024

int server_fd = -1;
int keep_running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        keep_running = 0;
        if (server_fd != -1) {
            shutdown(server_fd, SHUT_RDWR);
        }
    }
}

int main(int argc, char *argv[]) {
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Set up signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Create Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        return -1;
    }

    // CRITICAL: Reuse address to prevent "Address already in use" errors
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

    // Daemon mode check
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

    while (keep_running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

        if (client_fd == -1) {
            if (errno == EINTR) break; 
            perror("Accept failed");
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        // Use a dynamic buffer to store received bytes until a newline is found
        char *packet_buf = malloc(RECV_BUF_SIZE);
        size_t buf_capacity = RECV_BUF_SIZE;
        size_t total_bytes = 0;
        ssize_t bytes_read;
        char recv_byte;

        // Receive byte by byte to simplify newline detection
        while ((bytes_read = recv(client_fd, &recv_byte, 1, 0)) > 0) {
            if (total_bytes + 1 >= buf_capacity) {
                buf_capacity += RECV_BUF_SIZE;
                packet_buf = realloc(packet_buf, buf_capacity);
            }
            packet_buf[total_bytes++] = recv_byte;

            if (recv_byte == '\n') {
                // Write to file
                int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
                write(fd, packet_buf, total_bytes);
                close(fd);

                // Reset for next potential packet in same connection
                total_bytes = 0;

                // Send the full file content back
                fd = open(DATA_FILE, O_RDONLY);
                char read_buf[RECV_BUF_SIZE];
                ssize_t nr;
                while ((nr = read(fd, read_buf, sizeof(read_buf))) > 0) {
                    send(client_fd, read_buf, nr, 0);
                }
                close(fd);
            }
        }

        free(packet_buf);
        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));
    }

    // Cleanup
    close(server_fd);
    remove(DATA_FILE);
    syslog(LOG_INFO, "Successfully cleaned up. Exiting.");
    closelog();

    return 0;
}