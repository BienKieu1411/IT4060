#include <arpa/inet.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080
#define BUF_SIZE 4096
#define DIR_PATH "server_files"

int is_regular_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int send_all(int sock, const void *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(sock, (char *)buf + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return sent;
}

void handle_client(int client) {
    DIR *dir = opendir(DIR_PATH);
    if (!dir) {
        send_all(client, "ERROR No files to download\r\n", 28);
        close(client);
        return;
    }

    char list[BUF_SIZE * 4] = "";
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", DIR_PATH, entry->d_name);

        if (is_regular_file(path)) {
            strcat(list, entry->d_name);
            strcat(list, "\r\n");
            count++;
        }
    }
    closedir(dir);

    if (count == 0) {
        send_all(client, "ERROR No files to download\r\n", 28);
        close(client);
        return;
    }

    char header[128];
    snprintf(header, sizeof(header), "OK %d\r\n", count);
    send_all(client, header, strlen(header));
    send_all(client, list, strlen(list));
    send_all(client, "\r\n", 2);

    while (1) {
        char filename[256] = "";
        int n = recv(client, filename, sizeof(filename) - 1, 0);
        if (n <= 0) break;

        filename[n] = 0;
        filename[strcspn(filename, "\r\n")] = 0;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", DIR_PATH, filename);

        if (!is_regular_file(path)) {
            send_all(client, "ERROR File not found\r\n", 22);
            continue;
        }

        FILE *fp = fopen(path, "rb");
        if (!fp) {
            send_all(client, "ERROR Cannot open file\r\n", 24);
            continue;
        }

        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        snprintf(header, sizeof(header), "OK %ld\r\n", size);
        send_all(client, header, strlen(header));

        char buf[BUF_SIZE];
        long remaining = size;

        while (remaining > 0) {
            int r = fread(buf, 1, sizeof(buf), fp);
            if (r <= 0) break;
            send_all(client, buf, r);
            remaining -= r;
        }

        fclose(fp);
        break;
    }

    close(client);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 10);

    signal(SIGCHLD, SIG_IGN);

    printf("File server running on port %d...\n", PORT);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;

        pid_t pid = fork();

        if (pid == 0) {
            close(listener);
            handle_client(client);
            exit(0);
        }

        close(client);
    }

    close(listener);
    return 0;
}