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

int find_topic(char *name) {
    for (int i = 0; i < topic_count; i++)
        if (strcmp(topics[i].name, name) == 0)
            return i;
    return -1;
}

int create_topic(char *name) {
    if (topic_count >= MAX_TOPICS) return -1;
    strcpy(topics[topic_count].name, name);
    topics[topic_count].count = 0;
    printf("Create topic: %s\n", name);
    return topic_count++;
}

void remove_client(int fd);

void subscribe(int fd, char *name) {
    int idx = find_topic(name);
    if (idx == -1) idx = create_topic(name);
    if (idx == -1) return;

    for (int i = 0; i < topics[idx].count; i++)
        if (topics[idx].clients[i] == fd) return;

    if (topics[idx].count < MAX_CLIENTS) {
        topics[idx].clients[topics[idx].count++] = fd;
        printf("Client %d SUB %s (total: %d)\n", fd, name, topics[idx].count);
    }
}

void unsubscribe(int fd, char *name) {
    int idx = find_topic(name);
    if (idx == -1) return;

    for (int i = 0; i < topics[idx].count; i++) {
        if (topics[idx].clients[i] == fd) {
            topics[idx].clients[i] = topics[idx].clients[topics[idx].count - 1];
            topics[idx].count--;
            printf("Client %d UNSUB %s (total: %d)\n", fd, name, topics[idx].count);
            return;
        }
    }
}

void publish(char *name, char *msg, fd_set *master) {
    int idx = find_topic(name);
    if (idx == -1) {
        printf("Publish failed, topic not found: %s\n", name);
        return;
    }

    char out[BUFFER_SIZE];
    snprintf(out, sizeof(out), "[%s] %s\n", name, msg);

    printf("Publish [%s] to %d clients: %s\n", name, topics[idx].count, msg);

    for (int i = 0; i < topics[idx].count; i++) {
        int fd = topics[idx].clients[i];
        if (send(fd, out, strlen(out), 0) <= 0) {
            printf("Client %d send failed, removing\n", fd);
            close(fd);
            FD_CLR(fd, master);
            remove_client(fd);
            i--;
        }
    }
}

void remove_client(int fd) {
    for (int i = 0; i < topic_count; i++) {
        for (int j = 0; j < topics[i].count; j++) {
            if (topics[i].clients[j] == fd) {
                topics[i].clients[j] = topics[i].clients[topics[i].count - 1];
                topics[i].count--;
                j--;
            }
        }
    }
    printf("Client %d removed from all topics\n", fd);
}

int main() {
    int server_fd, newfd, fdmax;
    struct sockaddr_in addr;
    fd_set master, read_fds;
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    printf("Server started on port %d\n", PORT);

    FD_ZERO(&master);
    FD_SET(server_fd, &master);
    fdmax = server_fd;

    while (1) {
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0) break;

        for (int i = 0; i <= fdmax; i++) {
            if (!FD_ISSET(i, &read_fds)) continue;

            if (i == server_fd) {
                newfd = accept(server_fd, NULL, NULL);
                if (newfd < 0) continue;
                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;
                printf("New client connected: %d\n", newfd);
            } else {
                int n = recv(i, buffer, BUFFER_SIZE - 1, 0);

                if (n <= 0) {
                    printf("Client %d disconnected\n", i);
                    close(i);
                    FD_CLR(i, &master);
                    remove_client(i);
                } else {
                    buffer[n] = '\0';
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    printf("Client %d: %s\n", i, buffer);

                    if (strncmp(buffer, "SUB", 3) == 0) {
                        char topic[50];
                        if (sscanf(buffer, "SUB %49s", topic) == 1)
                            subscribe(i, topic);
                    } else if (strncmp(buffer, "UNSUB", 5) == 0) {
                        char topic[50];
                        if (sscanf(buffer, "UNSUB %49s", topic) == 1)
                            unsubscribe(i, topic);
                    } else if (strncmp(buffer, "PUB", 3) == 0) {
                        char topic[50], msg[BUFFER_SIZE];

                        char *p = strchr(buffer, ' ');
                        if (!p) continue;
                        p++;

                        char *p2 = strchr(p, ' ');
                        if (!p2) continue;

                        int len = p2 - p;
                        if (len >= (int)sizeof(topic)) len = sizeof(topic) - 1;
                        strncpy(topic, p, len);
                        topic[len] = '\0';

                        strncpy(msg, p2 + 1, sizeof(msg) - 1);
                        msg[sizeof(msg) - 1] = '\0';

                        publish(topic, msg, &master);
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}