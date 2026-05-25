#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080
#define BUF_SIZE 1024

typedef struct {
    int c1;
    int c2;
} Pair;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
int waiting_client = -1;

void *chat_pair(void *arg) {
    Pair *p = (Pair *)arg;
    int c1 = p->c1;
    int c2 = p->c2;
    free(p);

    send(c1, "You are paired. Start chatting.\n", 32, 0);
    send(c2, "You are paired. Start chatting.\n", 32, 0);

    struct pollfd fds[2];
    fds[0].fd = c1;
    fds[0].events = POLLIN;
    fds[1].fd = c2;
    fds[1].events = POLLIN;

    char buf[BUF_SIZE];

    while (1) {
        int ret = poll(fds, 2, -1);
        if (ret <= 0) break;

        if (fds[0].revents & POLLIN) {
            int n = recv(c1, buf, sizeof(buf), 0);
            if (n <= 0) break;
            send(c2, buf, n, 0);
        }

        if (fds[1].revents & POLLIN) {
            int n = recv(c2, buf, sizeof(buf), 0);
            if (n <= 0) break;
            send(c1, buf, n, 0);
        }
    }

    close(c1);
    close(c2);
    printf("Pair disconnected\n");
    return NULL;
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 10);

    printf("Pair chat server running on port %d...\n", PORT);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;

        pthread_mutex_lock(&queue_mutex);

        if (waiting_client == -1) {
            waiting_client = client;
            send(client, "Waiting for another client...\n", 30, 0);
        } else {
            Pair *p = malloc(sizeof(Pair));
            p->c1 = waiting_client;
            p->c2 = client;
            waiting_client = -1;

            pthread_t tid;
            pthread_create(&tid, NULL, chat_pair, p);
            pthread_detach(tid);
        }

        pthread_mutex_unlock(&queue_mutex);
    }

    close(listener);
    return 0;
}