#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
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
int client_count = 0;

void broadcast(int sender_sock, char *msg) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock != sender_sock) {
            send(clients[i].sock, msg, strlen(msg), 0);
        }
    }
}

void remove_client(int sock) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock == sock) {

            if (clients[i].logged_in) {
                printf("Client disconnected: %s (%s)\n",
                       clients[i].id, clients[i].name);

                char msg[BUF_SIZE];
                sprintf(msg, "%s has left the chat\n", clients[i].id);
                broadcast(sock, msg);
            } else {
                printf("Client disconnected: (chua login)\n");
            }

            close(sock);
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
}

int main() {
    int listen_sock, maxfd;
    struct sockaddr_in server;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = INADDR_ANY;

    bind(listen_sock, (struct sockaddr*)&server, sizeof(server));
    listen(listen_sock, 5);

    fd_set readfds;

    printf("Chat server running on port 8080...\n");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listen_sock, &readfds);
        maxfd = listen_sock;

        for (int i = 0; i < client_count; i++) {
            FD_SET(clients[i].sock, &readfds);
            if (clients[i].sock > maxfd)
                maxfd = clients[i].sock;
        }

        select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(listen_sock, &readfds)) {
            int new_sock = accept(listen_sock, NULL, NULL);

            clients[client_count].sock = new_sock;
            clients[client_count].logged_in = 0;
            client_count++;

            char *msg = "Enter: client_id: client_name\n";
            send(new_sock, msg, strlen(msg), 0);
        }

        for (int i = 0; i < client_count; i++) {
            int sock = clients[i].sock;

            if (FD_ISSET(sock, &readfds)) {
                char buffer[BUF_SIZE] = {0};
                int ret = recv(sock, buffer, BUF_SIZE, 0);

                if (ret <= 0) {
                    remove_client(sock);
                    i--;
                    continue;
                }

                buffer[strcspn(buffer, "\n")] = 0;

                if (!clients[i].logged_in) {
                    char *colon = strchr(buffer, ':');

                    if (colon) {
                        *colon = 0;
                        strcpy(clients[i].id, buffer);

                        char *name = colon + 1;
                        while (*name == ' ') name++;
                        strcpy(clients[i].name, name);

                        clients[i].logged_in = 1;

                        printf("Client login: %s (%s)\n",
                               clients[i].id, clients[i].name);

                        char *ok = "Login OK\n";
                        send(sock, ok, strlen(ok), 0);

                        char msg[BUF_SIZE];
                        sprintf(msg, "%s has joined the chat\n",
                                clients[i].id);
                        broadcast(sock, msg);

                    } else {
                        char *fail = "Login FAIL\n";
                        send(sock, fail, strlen(fail), 0);
                    }
                } else {
                    char msg[BUF_SIZE];

                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);

                    sprintf(msg, "%04d/%02d/%02d %02d:%02d:%02d %s: %s\n",
                            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                            t->tm_hour, t->tm_min, t->tm_sec,
                            clients[i].id, buffer);

                    printf("%s", msg);
                    broadcast(sock, msg);
                }
            }
        }
    }
}