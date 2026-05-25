#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define PORT 8080
#define BUF_SIZE 1024

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect");
        return 1;
    }

    struct pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[1].fd = sock;
    fds[1].events = POLLIN;

    char buf[BUF_SIZE];

    while (1) {
        poll(fds, 2, -1);

        if (fds[0].revents & POLLIN) {
            fgets(buf, sizeof(buf), stdin);
            send(sock, buf, strlen(buf), 0);
        }

        if (fds[1].revents & POLLIN) {
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) break;

            buf[n] = 0;
            printf("%s", buf);
            fflush(stdout);
        }
    }

    close(sock);
    return 0;
}