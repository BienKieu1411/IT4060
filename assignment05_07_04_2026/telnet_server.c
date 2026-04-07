#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 100
#define BUF_SIZE 1024

typedef struct {
    int sock;
    int logged_in;
    char user[50];
} Client;

Client clients[MAX_CLIENTS];
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

void remove_client(int sock) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock == sock) {
            if (clients[i].logged_in)
                printf("[Client %d] %s disconnected\n", sock, clients[i].user);
            else
                printf("[Client %d] disconnected\n", sock);

            close(sock);
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
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

    fd_set readfds;

    printf("Telnet server running on port 8080...\n");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listen_sock, &readfds);

        int maxfd = listen_sock;

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
            memset(clients[client_count].user, 0, sizeof(clients[client_count].user));
            client_count++;

            printf("[Client %d] connected\n", new_sock);

            char *msg = "Enter username password:\n";
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
                buffer[strcspn(buffer, "\r\n")] = 0;  
                if (!clients[i].logged_in) {
                    char user[50] = {0}, pass[50] = {0};
                    sscanf(buffer, "%s %s", user, pass);

                    if (check_login(user, pass)) {
                        clients[i].logged_in = 1;
                        strcpy(clients[i].user, user);

                        printf("[Client %d] Login OK: %s\n", sock, user);
                        send(sock, "Login OK\n", strlen("Login OK\n"), 0);

                        char prompt[100];
                        sprintf(prompt, "%s@server> ", clients[i].user);  
                        send(sock, prompt, strlen(prompt), 0);
                    } else {
                        printf("[Client %d] Login FAIL\n", sock);
                        send(sock, "Login FAIL\nEnter username password:\n",
                             strlen("Login FAIL\nEnter username password:\n"), 0);
                    }
                } else {
                    printf("[Client %d - %s] cmd: %s\n", sock, clients[i].user, buffer);

                    char cmd[256];
                    snprintf(cmd, sizeof(cmd), "%s > out.txt 2>&1", buffer); 
                    system(cmd);

                    FILE *f = fopen("out.txt", "r");
                    if (f) {
                        char line[BUF_SIZE];
                        while (fgets(line, sizeof(line), f)) {
                            send(sock, line, strlen(line), 0);
                        }
                        fclose(f);
                    } else {
                        send(sock, "Cannot execute command\n", strlen("Cannot execute command\n"), 0);
                    }

                    char prompt[100];
                    sprintf(prompt, "%s@server> ", clients[i].user);
                    send(sock, prompt, strlen(prompt), 0);
                }
            }
        }
    }
}