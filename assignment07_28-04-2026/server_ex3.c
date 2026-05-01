#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 9000
#define MAX_CLIENTS 100
#define MAX_TOPICS 50
#define BUFFER_SIZE 1024

typedef struct {
    char name[50];
    int clients[MAX_CLIENTS];
    int count;
} Topic;

Topic topics[MAX_TOPICS];
int topic_count = 0;

// Hàm tìm topic
int find_topic(char *name) {
    for (int i = 0; i < topic_count; i++)
        if (strcmp(topics[i].name, name) == 0)
            return i;
    return -1;
}

// Hàm tạo topic
int create_topic(char *name) {
    strcpy(topics[topic_count].name, name);
    topics[topic_count].count = 0;
    return topic_count++;
}

// Hàm đăng ký
void subscribe(int fd, char *name) {
    int idx = find_topic(name);
    if (idx == -1) idx = create_topic(name);

    for (int i = 0; i < topics[idx].count; i++)
        if (topics[idx].clients[i] == fd) return;

    topics[idx].clients[topics[idx].count++] = fd;
}

// Hàm publish
void publish(char *name, char *msg) {
    int idx = find_topic(name);
    if (idx == -1) return;

    char out[BUFFER_SIZE];
    snprintf(out, sizeof(out), "[%s] %s\n", name, msg);

    for (int i = 0; i < topics[idx].count; i++)
        send(topics[idx].clients[i], out, strlen(out), 0);
}

int main() {
    int server_fd, newfd, fdmax;
    struct sockaddr_in addr;
    fd_set master, read_fds;
    char buffer[BUFFER_SIZE];

    // Tạo socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    FD_ZERO(&master);
    FD_SET(server_fd, &master);
    fdmax = server_fd;

    printf("Server on port %d...\n", PORT);

    while (1) {
        read_fds = master;
        select(fdmax + 1, &read_fds, NULL, NULL, NULL);

        for (int i = 0; i <= fdmax; i++) {
            if (!FD_ISSET(i, &read_fds)) continue;

            // Client mới
            if (i == server_fd) {
                newfd = accept(server_fd, NULL, NULL);
                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;
            }
            else {
                int n = recv(i, buffer, BUFFER_SIZE - 1, 0);

                if (n <= 0) {
                    close(i);
                    FD_CLR(i, &master);
                }
                else {
                    buffer[n] = '\0';
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    if (strncmp(buffer, "SUB", 3) == 0) {
                        char topic[50];
                        sscanf(buffer, "SUB %s", topic);
                        subscribe(i, topic);
                    }
                    else if (strncmp(buffer, "PUB", 3) == 0) {
                        char topic[50], msg[512];
                        sscanf(buffer, "PUB %s %[^\n]", topic, msg);
                        publish(topic, msg);
                    }
                }
            }
        }
    }
    return 0;
}