#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/select.h>

#define BUF_SIZE 1024

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sock, (struct sockaddr*)&server, sizeof(server));

    fd_set readfds;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(0, &readfds);

        select(sock + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(0, &readfds)) {
            char msg[BUF_SIZE];
            fgets(msg, BUF_SIZE, stdin);
            msg[strcspn(msg, "\r\n")] = 0;  
            send(sock, msg, strlen(msg), 0);
        }

        if (FD_ISSET(sock, &readfds)) {
            char buffer[BUF_SIZE] = {0};
            int ret = recv(sock, buffer, BUF_SIZE, 0);
            if (ret <= 0) break;

            printf("%s", buffer);
            fflush(stdout);        
        }
    }

    close(sock);
}