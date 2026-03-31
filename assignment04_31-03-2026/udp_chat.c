#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s port_s ip_d port_d\n", argv[0]);
        return 1;
    }

    int port_s = atoi(argv[1]);
    char *ip_d = argv[2];
    int port_d = atoi(argv[3]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) return 1;

    unsigned long ul = 1;
    ioctl(sock, FIONBIO, &ul);

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    struct sockaddr_in addr_s = {0};
    addr_s.sin_family = AF_INET;
    addr_s.sin_addr.s_addr = INADDR_ANY;
    addr_s.sin_port = htons(port_s);

    if (bind(sock, (struct sockaddr *)&addr_s, sizeof(addr_s)) < 0) return 1;

    struct sockaddr_in addr_d = {0};
    addr_d.sin_family = AF_INET;
    addr_d.sin_port = htons(port_d);
    inet_pton(AF_INET, ip_d, &addr_d.sin_addr);

    char recv_buf[BUF_SIZE];
    char input_buf[BUF_SIZE];
    int input_len = 0;

    while (1) {
        struct sockaddr_in sender;
        socklen_t sender_len = sizeof(sender);

        int len = recvfrom(sock, recv_buf, BUF_SIZE - 1, 0, (struct sockaddr *)&sender, &sender_len);

        if (len > 0) {
            recv_buf[len] = 0;
            printf("\n[%s:%d]: %s\n\n", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port), recv_buf);
        }

        char c;
        int n = read(STDIN_FILENO, &c, 1);

        if (n > 0) {
            if (c == '\n') {
                input_buf[input_len] = 0;
                sendto(sock, input_buf, strlen(input_buf), 0,
                       (struct sockaddr *)&addr_d, sizeof(addr_d));
                input_len = 0;
            } else {
                if (input_len < BUF_SIZE - 1)
                    input_buf[input_len++] = c;
            }
        }

        usleep(10000);
    }

    close(sock);
    return 0;
}