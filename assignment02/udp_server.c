#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr, client;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    char buf[1024];
    socklen_t len = sizeof(client);

    while (1) {
        int n = recvfrom(sock, buf, sizeof(buf)-1, 0,
                         (struct sockaddr*)&client, &len);

        buf[n] = 0;
        printf("Received: %s", buf);

        sendto(sock, buf, n, 0,
               (struct sockaddr*)&client, len);
    }
}