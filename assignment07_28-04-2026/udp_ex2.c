#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <port> <remote_ip> <remote_port>\n", argv[0]);
        exit(1);
    }

    int local_port = atoi(argv[1]);
    char *remote_ip = argv[2];
    int remote_port = atoi(argv[3]);

    int sockfd;
    struct sockaddr_in local_addr, remote_addr;
    socklen_t addr_len = sizeof(remote_addr);

    char buffer[BUFFER_SIZE];

    // UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // Bind 
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(local_port);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr));

    // Setup remote
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(remote_port);
    inet_pton(AF_INET, remote_ip, &remote_addr.sin_addr);

    fd_set read_fds;
    int fdmax = sockfd;

    printf("Chat on port %d to %s:%d\n", local_port, remote_ip, remote_port);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(0, &read_fds);      
        FD_SET(sockfd, &read_fds); 

        select(fdmax + 1, &read_fds, NULL, NULL, NULL);

        if (FD_ISSET(0, &read_fds)) {
            fgets(buffer, BUFFER_SIZE, stdin);

            sendto(sockfd, buffer, strlen(buffer), 0,
                   (struct sockaddr*)&remote_addr, addr_len);
        }

        if (FD_ISSET(sockfd, &read_fds)) {
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                             (struct sockaddr*)&remote_addr, &addr_len);

            buffer[n] = '\0';
            printf("Peer: %s", buffer);
        }
    }
    return 0;
}