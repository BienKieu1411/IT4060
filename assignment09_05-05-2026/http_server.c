#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define NUM_PROCESSES 8
#define PORT 8080
#define BUF_SIZE 4096

int main() {
    int listener;

    struct sockaddr_in addr;

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                   &(int){1}, sizeof(int)) < 0) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 10) < 0) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);

    printf("HTTP Server listening on port %d with %d processes...\n",
           PORT, NUM_PROCESSES);

    for (int i = 0; i < NUM_PROCESSES; i++) {

        pid_t pid = fork();

        if (pid == 0) {

            printf("[PID %d] Worker process started\n", getpid());

            while (1) {

                int client = accept(listener, NULL, NULL);

                if (client < 0) {

                    if (errno == EINTR)
                        continue;

                    perror("accept() failed");
                    continue;
                }

                printf("[PID %d] Client connected (socket=%d)\n",
                       getpid(), client);

                char buf[BUF_SIZE];

                int ret = recv(client, buf, sizeof(buf) - 1, 0);

                if (ret > 0) {

                    buf[ret] = 0;

                    printf("[PID %d] Received HTTP request:\n%s\n",
                           getpid(), buf);

                    char body[1024];

                    sprintf(body,
                            "<html>"
                            "<body>"
                            "<h1>Hello everyone</h1>"
                            "<p>Processed by PID %d</p>"
                            "</body>"
                            "</html>",
                            getpid());

                    char response[2048];

                    sprintf(response,
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: %ld\r\n"
                            "Connection: close\r\n"
                            "\r\n"
                            "%s",
                            strlen(body),
                            body);

                    send(client,
                         response,
                         strlen(response),
                         0);

                    printf("[PID %d] Response sent successfully\n",
                           getpid());
                }

                close(client);

                printf("[PID %d] Client disconnected\n",
                       getpid());
            }

            close(listener);

            exit(0);
        }
    }

    printf("Press ENTER to stop server...\n");

    getchar();

    kill(0, SIGTERM);

    close(listener);

    return 0;
}