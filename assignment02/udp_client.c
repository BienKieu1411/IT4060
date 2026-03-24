#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    char buf[1024];

    while (1) {
        fgets(buf, sizeof(buf), stdin);

        sendto(sock, buf, strlen(buf), 0,
               (struct sockaddr*)&server, sizeof(server));

        int n = recvfrom(sock, buf, sizeof(buf)-1, 0, NULL, NULL);
        buf[n] = 0;

        printf("Server: %s", buf);
    }
}