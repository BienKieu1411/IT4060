#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080
#define NUM_THREADS 8
#define BUF_SIZE 4096

int listener;

void *worker_thread(void *arg) {
    long tid = (long)arg;

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;

        char buf[BUF_SIZE] = {0};

        int ret = recv(client, buf, sizeof(buf) - 1, 0);

        if (ret > 0) {
            buf[ret] = 0;

            printf("[Thread %ld] HTTP request:\n%s\n", tid, buf);

            char body[1024];

            snprintf(body, sizeof(body),
                     "<html>"
                     "<body>"
                     "<h1>Hello everyone</h1>"
                     "<p>Processed by thread %ld</p>"
                     "</body>"
                     "</html>",
                     tid);

            char response[2048];

            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/html\r\n"
                     "Content-Length: %ld\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "%s",
                     strlen(body),
                     body);

            send(client, response, strlen(response), 0);
        }

        close(client);
    }

    return NULL;
}

int main() {
    listener = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 20);

    printf("Prethreading HTTP server running on port %d with %d threads...\n",
           PORT, NUM_THREADS);

    pthread_t threads[NUM_THREADS];

    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker_thread, (void *)i);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    close(listener);
    return 0;
}