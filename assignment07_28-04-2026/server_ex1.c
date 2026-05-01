#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8080
#define BUFFER_SIZE 1024

char encode_char(char c) {
    if (isalpha(c)) {
        if (c == 'z') return 'a';
        if (c == 'Z') return 'A';
        return c + 1;
    } else if (isdigit(c)) {
       return ('9' - c) + '0';
    }
    return c;
}

void encode_string(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = encode_char(str[i]);
    }
}

int main() {
    int server_fd, newfd, fdmax;
    int count_clients = 0;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen;

    fd_set master, read_fds;

    char buffer[BUFFER_SIZE];

    // Tạo socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    // Bind
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind Error!");
        exit(1);
    }

    // Listen
    if (listen(server_fd, 10) < 0) {
        perror("Listen Error!");
        exit(1);
    }

    // Init fd_set
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    FD_SET(server_fd, &master);
    fdmax = server_fd;

    printf("Server running on port %d...\n", PORT);

    while (1) {
        read_fds = master;

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("Select Error!");
            exit(1);
        }

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_fd) {
                    addrlen = sizeof(client_addr);
                    newfd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
                    if (newfd == -1) {
                        perror("Accept Error!");
                        continue;
                    }
                    FD_SET(newfd, &master);
                    if (newfd > fdmax) fdmax = newfd;
                    count_clients++;
                    char msg[BUFFER_SIZE];
                    sprintf(msg, "Hello! There are %d clients connected.\n", count_clients);
                    send(newfd, msg, strlen(msg), 0);

                    printf("New connection (fd=%d)\n", newfd);
                }
                else {
                    int nbytes = recv(i, buffer, BUFFER_SIZE - 1, 0);

                    if (nbytes <= 0) {
                        // client disconnect
                        if (nbytes == 0)
                            printf("Client %d disconnected\n", i);
                        else
                            perror("recv");

                        close(i);
                        FD_CLR(i, &master);
                        count_clients--;
                    }
                    else {
                        buffer[nbytes] = '\0';
                        buffer[strcspn(buffer, "\r\n")] = 0;
                        // Exit
                        if (strcmp(buffer, "exit") == 0) {
                            char *bye = "Goodbye\n";
                            send(i, bye, strlen(bye), 0);

                            close(i);
                            FD_CLR(i, &master);
                            printf("Client %d exited\n", i);
                        }
                        else {
                            // Encode
                            encode_string(buffer);
                            strcat(buffer, "\n");
                            // Gửi lại
                            send(i, buffer, strlen(buffer), 0);
                        }
                    }
                }
            }
        }
    }

    return 0;
}