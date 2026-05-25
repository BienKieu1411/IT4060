#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>

#define BUF_SIZE 1024

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sock, (struct sockaddr*)&server, sizeof(server));

    struct pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[1].fd = sock;
    fds[1].events = POLLIN;

    while (1) {
        poll(fds, 2, -1);

        if (fds[0].revents & POLLIN) {
            char msg[BUF_SIZE];
            fgets(msg, BUF_SIZE, stdin);
            send(sock, msg, strlen(msg), 0);
        }

        if (fds[1].revents & POLLIN) {
            char buffer[BUF_SIZE] = {0};
            int ret = recv(sock, buffer, BUF_SIZE, 0);
            if (ret <= 0) break;
            printf("%s", buffer);
        }
    }

    close(sock);
}