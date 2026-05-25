#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PORT 8080
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

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast(int sender_sock, const char *msg) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock != sender_sock && clients[i].logged_in) {
            send(clients[i].sock, msg, strlen(msg), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int sock) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock == sock) {
            if (clients[i].logged_in) {
                char msg[BUF_SIZE];
                snprintf(msg, sizeof(msg), "%s has left the chat\n", clients[i].id);
                printf("%s", msg);

                for (int j = 0; j < client_count; j++) {
                    if (clients[j].sock != sock && clients[j].logged_in) {
                        send(clients[j].sock, msg, strlen(msg), 0);
                    }
                }
            }

            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    int client = *(int *)arg;
    free(arg);

    char buffer[BUF_SIZE];

    send(client, "Enter: client_id: client_name\n", 30, 0);

    Client *me = NULL;

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        int ret = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (ret <= 0) break;

        buffer[ret] = 0;
        buffer[strcspn(buffer, "\r\n")] = 0;

        pthread_mutex_lock(&clients_mutex);

        for (int i = 0; i < client_count; i++) {
            if (clients[i].sock == client) {
                me = &clients[i];
                break;
            }
        }

        if (me == NULL) {
            pthread_mutex_unlock(&clients_mutex);
            break;
        }

        if (!me->logged_in) {
            char *colon = strchr(buffer, ':');

            if (!colon) {
                pthread_mutex_unlock(&clients_mutex);
                send(client, "Login FAIL\n", 11, 0);
                continue;
            }

            *colon = 0;

            strncpy(me->id, buffer, sizeof(me->id) - 1);

            char *name = colon + 1;
            while (*name == ' ') name++;

            strncpy(me->name, name, sizeof(me->name) - 1);
            me->logged_in = 1;

            printf("Client login: %s (%s)\n", me->id, me->name);

            pthread_mutex_unlock(&clients_mutex);

            send(client, "Login OK\n", 9, 0);

            char join_msg[BUF_SIZE];
            snprintf(join_msg, sizeof(join_msg), "%s has joined the chat\n", me->id);
            broadcast(client, join_msg);
        } else {
            char sender_id[50];
            strncpy(sender_id, me->id, sizeof(sender_id) - 1);
            sender_id[sizeof(sender_id) - 1] = 0;

            pthread_mutex_unlock(&clients_mutex);

            time_t now = time(NULL);
            struct tm *t = localtime(&now);

            char msg[BUF_SIZE + 128];
            snprintf(msg, sizeof(msg),
                     "%04d/%02d/%02d %02d:%02d:%02d %s: %s\n",
                     t->tm_year + 1900,
                     t->tm_mon + 1,
                     t->tm_mday,
                     t->tm_hour,
                     t->tm_min,
                     t->tm_sec,
                     sender_id,
                     buffer);

            printf("%s", msg);
            broadcast(client, msg);
        }
    }

    remove_client(client);
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

    printf("Multithread chat_server running on port %d...\n", PORT);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;

        pthread_mutex_lock(&clients_mutex);

        if (client_count >= MAX_CLIENTS) {
            pthread_mutex_unlock(&clients_mutex);
            send(client, "Server full\n", 12, 0);
            close(client);
            continue;
        }

        clients[client_count].sock = client;
        clients[client_count].logged_in = 0;
        clients[client_count].id[0] = 0;
        clients[client_count].name[0] = 0;
        client_count++;

        pthread_mutex_unlock(&clients_mutex);

        int *pclient = malloc(sizeof(int));
        *pclient = client;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, pclient);
        pthread_detach(tid);
    }

    close(listener);
    return 0;
}