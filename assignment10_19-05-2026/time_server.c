#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PORT 8080
#define BUF_SIZE 256

void *handle_client(void *arg) {
    int client = *(int *)arg;
    free(arg);

    char *welcome =
        "Welcome to Time Server\n"
        "Command format: GET_TIME [format]\n"
        "Supported formats:\n"
        "- dd/mm/yyyy\n"
        "- dd/mm/yy\n"
        "- mm/dd/yyyy\n"
        "- mm/dd/yy\n";

    send(client, welcome, strlen(welcome), 0);

    while (1) {
        char buf[BUF_SIZE] = {0};

        int len = recv(client, buf, sizeof(buf) - 1, 0);
        if (len <= 0) break;

        buf[len] = 0;
        buf[strcspn(buf, "\r\n")] = 0;

        char cmd[32] = {0};
        char format[32] = {0};
        char extra[32] = {0};

        int n = sscanf(buf, "%31s %31s %31s", cmd, format, extra);

        if (n != 2 || strcmp(cmd, "GET_TIME") != 0) {
            send(client,
                 "Error: Invalid syntax\nUsage: GET_TIME [format]\n",
                 47,
                 0);
            continue;
        }

        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        char result[64];
        int valid = 1;

        if (strcmp(format, "dd/mm/yyyy") == 0) {
            strftime(result, sizeof(result), "%d/%m/%Y\n", t);
        } else if (strcmp(format, "dd/mm/yy") == 0) {
            strftime(result, sizeof(result), "%d/%m/%y\n", t);
        } else if (strcmp(format, "mm/dd/yyyy") == 0) {
            strftime(result, sizeof(result), "%m/%d/%Y\n", t);
        } else if (strcmp(format, "mm/dd/yy") == 0) {
            strftime(result, sizeof(result), "%m/%d/%y\n", t);
        } else {
            valid = 0;
        }

        if (valid) {
            send(client, result, strlen(result), 0);
        } else {
            send(client, "Error: Unsupported time format\n", 31, 0);
        }
    }

    close(client);
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

    printf("Multithread time_server running on port %d...\n", PORT);

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