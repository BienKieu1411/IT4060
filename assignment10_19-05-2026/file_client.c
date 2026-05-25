#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080
#define BUF_SIZE 4096

int recv_line(int sock, char *buf, int size) {
    int i = 0;
    char c;

    while (i < size - 1) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return n;

        buf[i++] = c;

        if (i >= 2 && buf[i - 2] == '\r' && buf[i - 1] == '\n')
            break;
    }

    buf[i] = 0;
    return i;
}

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

    char line[BUF_SIZE];

    recv_line(sock, line, sizeof(line));

    if (strncmp(line, "ERROR", 5) == 0) {
        printf("%s", line);
        close(sock);
        return 0;
    }

    int nfiles = 0;
    sscanf(line, "OK %d", &nfiles);

    printf("Available files: %d\n", nfiles);

    while (1) {
        recv_line(sock, line, sizeof(line));

        if (strcmp(line, "\r\n") == 0)
            break;

        printf("- %s", line);
    }

    while (1) {
        char filename[256];

        printf("Enter filename to download: ");
        fgets(filename, sizeof(filename), stdin);
        filename[strcspn(filename, "\n")] = 0;

        char request[300];
        snprintf(request, sizeof(request), "%s\r\n", filename);
        send(sock, request, strlen(request), 0);

        recv_line(sock, line, sizeof(line));

        if (strncmp(line, "ERROR", 5) == 0) {
            printf("%s", line);
            continue;
        }

        long size = 0;
        sscanf(line, "OK %ld", &size);

        char outname[300];
        snprintf(outname, sizeof(outname), "downloaded_%s", filename);

        FILE *fp = fopen(outname, "wb");
        if (!fp) {
            perror("fopen");
            break;
        }

        char buf[BUF_SIZE];
        long received = 0;

        while (received < size) {
            int need = size - received > BUF_SIZE ? BUF_SIZE : size - received;
            int r = recv(sock, buf, need, 0);
            if (r <= 0) break;

            fwrite(buf, 1, r, fp);
            received += r;
        }

        fclose(fp);

        printf("Downloaded to %s, size = %ld bytes\n", outname, received);
        break;
    }

    close(sock);
    return 0;
}