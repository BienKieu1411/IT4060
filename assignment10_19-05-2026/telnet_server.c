#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080
#define BUF_SIZE 1024

int check_login(char *user, char *pass) {
    FILE *f = fopen("user.txt", "r");
    if (!f) return 0;

    char u[50], p[50];

    while (fscanf(f, "%49s %49s", u, p) != EOF) {
        if (strcmp(u, user) == 0 && strcmp(p, pass) == 0) {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

void *handle_client(void *arg) {
    int client = *(int *)arg;
    free(arg);

    int logged_in = 0;
    char username[50] = {0};

    send(client, "Enter username password:\n", 25, 0);

    while (1) {
        char buffer[BUF_SIZE] = {0};

        int ret = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (ret <= 0) break;

        buffer[ret] = 0;
        buffer[strcspn(buffer, "\r\n")] = 0;

        if (strlen(buffer) == 0) continue;

        if (!logged_in) {
            char user[50] = {0};
            char pass[50] = {0};

            if (sscanf(buffer, "%49s %49s", user, pass) != 2) {
                send(client,
                     "Invalid format\nEnter username password:\n",
                     40,
                     0);
                continue;
            }

            if (check_login(user, pass)) {
                logged_in = 1;
                strcpy(username, user);

                send(client, "Login OK\n", 9, 0);

                char prompt[100];
                snprintf(prompt, sizeof(prompt), "%s@server> ", username);
                send(client, prompt, strlen(prompt), 0);
            } else {
                send(client,
                     "Login FAIL\nEnter username password:\n",
                     36,
                     0);
            }
        } else {
            char filename[128];
            snprintf(filename, sizeof(filename), "out_%lu.txt",
                     (void *)pthread_self());

            char cmd[512];
            snprintf(cmd, sizeof(cmd), "%s > %s 2>&1", buffer, filename);

            system(cmd);

            FILE *fp = fopen(filename, "r");

            if (fp) {
                char out_buf[BUF_SIZE];

                while (fgets(out_buf, sizeof(out_buf), fp)) {
                    send(client, out_buf, strlen(out_buf), 0);
                }

                fclose(fp);
                remove(filename);
            } else {
                send(client, "Command error\n", 14, 0);
            }

            char prompt[100];
            snprintf(prompt, sizeof(prompt), "%s@server> ", username);
            send(client, prompt, strlen(prompt), 0);
        }
    }

    close(client);
    return NULL;
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    bind(listener, (struct sockaddr *)&server, sizeof(server));
    listen(listener, 10);

    printf("Multithread telnet_server running on port %d...\n", PORT);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;

        int *pclient = malloc(sizeof(int));
        *pclient = client;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, pclient);
        pthread_detach(tid);
    }

    close(listener);
    return 0;
}