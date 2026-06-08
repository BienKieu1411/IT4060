#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define ROOT_DIR ".."
#define BUFFER_SIZE 8192
#define HTML_BUFFER_SIZE 65536

void send_all(int client, const char *data, int len) {
    int sent = 0;

    while (sent < len) {
        int n = send(client, data + sent, len - sent, 0);
        if (n <= 0) break;
        sent += n;
    }
}

void send_text_response(int client, const char *status, const char *body) {
    char header[1024];

    sprintf(header,
            "HTTP/1.1 %s\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n",
            status, strlen(body));

    send_all(client, header, strlen(header));
    send_all(client, body, strlen(body));
}

const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');

    if (!ext) return "application/octet-stream";

    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    if (strcmp(ext, ".c") == 0) return "text/plain";
    if (strcmp(ext, ".cpp") == 0) return "text/plain";
    if (strcmp(ext, ".h") == 0) return "text/plain";

    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
    if (strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".bmp") == 0) return "image/bmp";

    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".ogg") == 0) return "audio/ogg";

    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcmp(ext, ".webm") == 0) return "video/webm";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";

    if (strcmp(ext, ".pdf") == 0) return "application/pdf";

    return "application/octet-stream";
}

void send_file(int client, const char *path) {
    FILE *fp = fopen(path, "rb");

    if (!fp) {
        printf("File not found: %s\n", path);

        send_text_response(client, "404 Not Found",
                           "<html><body><h1>404 Not Found</h1></body></html>");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    const char *mime = get_mime_type(path);

    printf("Sending file: %s\n", path);
    printf("Content-Type: %s\n", mime);
    printf("Content-Length: %ld bytes\n", file_size);

    char header[1024];

    sprintf(header,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n",
            mime, file_size);

    send_all(client, header, strlen(header));

    char buffer[4096];
    int len;

    while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send_all(client, buffer, len);
    }

    fclose(fp);
}

void list_directory(int client, const char *path, const char *url_path) {
    DIR *dir = opendir(path);

    if (!dir) {
        printf("Cannot open directory: %s\n", path);

        send_text_response(client, "403 Forbidden",
                           "<html><body><h1>403 Forbidden</h1></body></html>");
        return;
    }

    printf("Listing directory: %s\n", path);

    char body[HTML_BUFFER_SIZE];

    strcpy(body, "<html><body>");
    strcat(body, "<h1>Directory Listing</h1>");
    strcat(body, "<ul>");

    if (strcmp(url_path, "/") != 0) {
        strcat(body, "<li><a href=\"../\"><b>../</b></a></li>");
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;

        if (stat(full_path, &st) < 0) continue;

        char link[2048];

        if (strcmp(url_path, "/") == 0) {
            snprintf(link, sizeof(link), "/%s", entry->d_name);
        } else {
            snprintf(link, sizeof(link), "%s/%s", url_path, entry->d_name);
        }

        strcat(body, "<li><a href=\"");
        strcat(body, link);

        if (S_ISDIR(st.st_mode)) {
            strcat(body, "/");
        }

        strcat(body, "\">");

        if (S_ISDIR(st.st_mode)) {
            strcat(body, "<b>");
            strcat(body, entry->d_name);
            strcat(body, "/");
            strcat(body, "</b>");

            printf("Directory: %s\n", entry->d_name);
        } else {
            strcat(body, "<i>");
            strcat(body, entry->d_name);
            strcat(body, "</i>");

            printf("File: %s\n", entry->d_name);
        }

        strcat(body, "</a></li>");
    }

    strcat(body, "</ul>");
    strcat(body, "</body></html>");

    closedir(dir);

    send_text_response(client, "200 OK", body);
}

void handle_file_request(int client, const char *url_path) {
    char path[2048];

    if (strcmp(url_path, "/") == 0) {
        snprintf(path, sizeof(path), "%s", ROOT_DIR);
    } else {
        snprintf(path, sizeof(path), "%s%s", ROOT_DIR, url_path);
    }

    printf("Requested URL path: %s\n", url_path);
    printf("Mapped local path: %s\n", path);

    struct stat st;

    if (stat(path, &st) < 0) {
        printf("Path not found: %s\n", path);

        send_text_response(client, "404 Not Found",
                           "<html><body><h1>404 Not Found</h1></body></html>");
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        list_directory(client, path, url_path);
    } else {
        send_file(client, path);
    }
}

void handle_client(int client) {
    char request[BUFFER_SIZE];

    int len = recv(client, request, sizeof(request) - 1, 0);

    if (len <= 0) {
        close(client);
        return;
    }

    request[len] = '\0';

    printf("\n===== Received HTTP Request =====\n");
    printf("%s\n", request);
    printf("=================================\n");

    char method[16];
    char url[2048];
    char version[32];

    sscanf(request, "%15s %2047s %31s", method, url, version);

    printf("Request line: %s %s %s\n", method, url, version);
    printf("Method: %s\n", method);
    printf("URL: %s\n", url);

    if (strcmp(method, "GET") != 0) {
        send_text_response(client, "405 Method Not Allowed",
                           "<html><body><h1>Only GET is supported</h1></body></html>");
        close(client);
        return;
    }

    handle_file_request(client, url);

    close(client);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);

    if (listener < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listener);
        return 1;
    }

    if (listen(listener, 10) < 0) {
        perror("listen");
        close(listener);
        return 1;
    }

    printf("File HTTP server is running at:\n");
    printf("http://127.0.0.1:%d/\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client = accept(listener, (struct sockaddr *)&client_addr, &client_len);

        if (client < 0) {
            perror("accept");
            continue;
        }

        printf("\nNew client connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        handle_client(client);
    }

    close(listener);
    return 0;
}