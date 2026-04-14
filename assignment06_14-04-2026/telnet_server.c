#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#define MAX_CLIENTS 100
#define BUF_SIZE 1024

typedef struct {
    int sock;
    int logged_in;
    char user[50];
} Client;

Client clients[MAX_CLIENTS];
struct pollfd fds[MAX_CLIENTS + 1];
int nfds = 1;
int client_count = 0;

int check_login(char *user, char *pass) {
    FILE *f = fopen("user.txt", "r");
    if (!f) return 0;

    char u[50], p[50];
    while (fscanf(f, "%s %s", u, p) != EOF) {
        if (strcmp(u, user) == 0 && strcmp(p, pass) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

void remove_client(int idx) {
    int sock = fds[idx].fd;

    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock == sock) {
            if (clients[i].logged_in)
                printf("[Client %d] %s disconnected\n", sock, clients[i].user);
            else
                printf("[Client %d] disconnected\n", sock);

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

    printf("Telnet server running on port 8080...\n");

    while (1) {
        poll(fds, nfds, -1);

        if (fds[0].revents & POLLIN) {
            int new_sock = accept(listen_sock, NULL, NULL);

            fds[nfds].fd = new_sock;
            fds[nfds].events = POLLIN;
            nfds++;

            clients[client_count].sock = new_sock;
            clients[client_count].logged_in = 0;
            memset(clients[client_count].user, 0, 50);
            client_count++;

            printf("[Client %d] connected\n", new_sock);

            send(new_sock, "Enter username password:\n", 27, 0);
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

                buffer[strcspn(buffer, "\r\n")] = 0;

                for (int j = 0; j < client_count; j++) {
                    if (clients[j].sock == fds[i].fd) {
                        if (!clients[j].logged_in) {
                            char user[50] = {0}, pass[50] = {0};
                            sscanf(buffer, "%s %s", user, pass);

                            if (check_login(user, pass)) {
                                clients[j].logged_in = 1;
                                strcpy(clients[j].user, user);

                                printf("[Client %d] Login OK: %s\n", fds[i].fd, user);
                                send(fds[i].fd, "Login OK\n", 9, 0);

                                char prompt[100];
                                sprintf(prompt, "%s@server> ", user);
                                send(fds[i].fd, prompt, strlen(prompt), 0);
                            } else {
                                printf("[Client %d] Login FAIL\n", fds[i].fd);
                                send(fds[i].fd, "Login FAIL\nEnter username password:\n", 42, 0);
                            }
                        } else {
                            printf("[Client %d - %s] cmd: %s\n", fds[i].fd, clients[j].user, buffer);

                            char cmd[256];
                            snprintf(cmd, sizeof(cmd), "%s > out.txt 2>&1", buffer);
                            system(cmd);

                            FILE *f = fopen("out.txt", "r");
                            if (f) {
                                char line[BUF_SIZE];
                                while (fgets(line, sizeof(line), f)) {
                                    send(fds[i].fd, line, strlen(line), 0);
                                }
                                fclose(f);
                            } else {
                                send(fds[i].fd, "Cannot execute command\n", 23, 0);
                            }

                            char prompt[100];
                            sprintf(prompt, "%s@server> ", clients[j].user);
                            send(fds[i].fd, prompt, strlen(prompt), 0);
                        }
                        break;
                    }
                }
            }
        }
    }
}