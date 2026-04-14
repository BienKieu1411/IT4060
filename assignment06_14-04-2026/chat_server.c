#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <time.h>

#define MAX_CLIENTS 100
#define BUF_SIZE 1024

typedef struct {
    int sock;
    char id[50];
    char name[50];
    int logged_in;
} Client;

Client clients[MAX_CLIENTS];
struct pollfd fds[MAX_CLIENTS + 1];
int nfds = 1;
int client_count = 0;

void broadcast(int sender_sock, char *msg) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock != sender_sock) {
            send(clients[i].sock, msg, strlen(msg), 0);
        }
    }
}

void remove_client(int idx) {
    int sock = fds[idx].fd;

    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock == sock) {
            if (clients[i].logged_in) {
                printf("Client disconnected: %s (%s)\n", clients[i].id, clients[i].name);
                char msg[BUF_SIZE];
                sprintf(msg, "%s has left the chat\n", clients[i].id);
                broadcast(sock, msg);
            } else {
                printf("Client disconnected: (chua login)\n");
            }
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }

    close(sock);
    fds[idx] = fds[nfds - 1];
    nfds--;
}

int main() {
    int listen_sock;
    struct sockaddr_in server;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = INADDR_ANY;

    bind(listen_sock, (struct sockaddr*)&server, sizeof(server));
    listen(listen_sock, 5);

    fds[0].fd = listen_sock;
    fds[0].events = POLLIN;

    printf("Chat server running on port 8080...\n");

    while (1) {
        poll(fds, nfds, -1);

        if (fds[0].revents & POLLIN) {
            int new_sock = accept(listen_sock, NULL, NULL);

            fds[nfds].fd = new_sock;
            fds[nfds].events = POLLIN;
            nfds++;

            clients[client_count].sock = new_sock;
            clients[client_count].logged_in = 0;
            client_count++;

            char *msg = "Enter: client_id: client_name\n";
            send(new_sock, msg, strlen(msg), 0);
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                char buffer[BUF_SIZE] = {0};
                int ret = recv(fds[i].fd, buffer, BUF_SIZE, 0);

                if (ret <= 0) {
                    remove_client(i);
                    i--;
                    continue;
                }

                buffer[strcspn(buffer, "\n")] = 0;

                for (int j = 0; j < client_count; j++) {
                    if (clients[j].sock == fds[i].fd) {
                        if (!clients[j].logged_in) {
                            char *colon = strchr(buffer, ':');

                            if (colon) {
                                *colon = 0;
                                strcpy(clients[j].id, buffer);

                                char *name = colon + 1;
                                while (*name == ' ') name++;
                                strcpy(clients[j].name, name);

                                clients[j].logged_in = 1;

                                printf("Client login: %s (%s)\n", clients[j].id, clients[j].name);

                                send(fds[i].fd, "Login OK\n", 9, 0);

                                char msg[BUF_SIZE];
                                sprintf(msg, "%s has joined the chat\n", clients[j].id);
                                broadcast(fds[i].fd, msg);
                            } else {
                                send(fds[i].fd, "Login FAIL\n", 11, 0);
                            }
                        } else {
                            char msg[BUF_SIZE];

                            time_t now = time(NULL);
                            struct tm *t = localtime(&now);

                            sprintf(msg, "%04d/%02d/%02d %02d:%02d:%02d %s: %s\n",
                                    t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                                    t->tm_hour, t->tm_min, t->tm_sec,
                                    clients[j].id, buffer);

                            printf("%s", msg);
                            broadcast(fds[i].fd, msg);
                        }
                        break;
                    }
                }
            }
        }
    }
}