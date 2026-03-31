#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>

#define MAX_CLIENTS 64
#define BUF_SIZE 256

typedef struct {
    int fd;
    int state;
    char name[100];
    char mssv[50];
} Client;

void make_email(char *name, char *mssv, char *email) {
    char words[10][50];
    int n = 0;

    char temp[100];
    strcpy(temp, name);

    char *token = strtok(temp, " ");
    while (token != NULL) {
        strcpy(words[n++], token);
        token = strtok(NULL, " ");
    }

    char ten[50];
    strcpy(ten, words[n - 1]);

    for (int i = 0; ten[i]; i++)
        ten[i] = tolower(ten[i]);

    char initials[20] = "";

    for (int i = 0; i < n - 1; i++) {
        char c[2];
        c[0] = tolower(words[i][0]);
        c[1] = '\0';
        strcat(initials, c);
    }

    char last6[7];
    int len = strlen(mssv);

    if (len <= 6)
        strcpy(last6, mssv);
    else
        strcpy(last6, mssv + len - 6);

    sprintf(email, "%s.%s%s@sis.hust.edu.vn", ten, initials, last6);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listener, (struct sockaddr*)&addr, sizeof(addr));
    listen(listener, 5);

    printf("Server listening at 8080...\n");

    Client clients[MAX_CLIENTS];
    int nclients = 0;

    char buf[BUF_SIZE];

    while (1) {
        int client = accept(listener, NULL, NULL);

        if (client != -1) {
            printf("New client connected: %d\n", client);

            ioctl(client, FIONBIO, &ul);

            clients[nclients].fd = client;
            clients[nclients].state = 0;

            send(client, "Full name: ", 11, 0);

            nclients++;
        }

        for (int i = 0; i < nclients; i++) {
            int len = recv(clients[i].fd, buf, BUF_SIZE - 1, 0);

            if (len <= 0) {
                if (errno == EWOULDBLOCK)
                    continue;
                else
                    continue;
            }

            buf[len] = 0;
            buf[strcspn(buf, "\r\n")] = 0;

            if (clients[i].state == 0) {
                strcpy(clients[i].name, buf);

                send(clients[i].fd, "MSSV: ", 6, 0);

                clients[i].state = 1;
            }
            else if (clients[i].state == 1) {
                strcpy(clients[i].mssv, buf);

                char email[200];
                make_email(clients[i].name, clients[i].mssv, email);

                char out[256];
                sprintf(out, "Email: %s\n", email);

                send(clients[i].fd, out, strlen(out), 0);
                printf("Sent email to client %d: %s\n", clients[i].fd, email);
            }
        }

        usleep(1000);
    }

    close(listener);
    return 0;
}