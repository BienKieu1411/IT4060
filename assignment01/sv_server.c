#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

struct Student {
    char id[20];
    char name[50];
    char birth[20];
    float gpa;
};

int main(int argc, char *argv[]) {
    // Kiểm tra tham số đầu vào
    if (argc != 3) {
        printf("Usage: %s <port> <log_file>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    char *log_file = argv[2];

    // Tạo socket TCP
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) return 1;
    // Khai báo địa chỉ server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // Gắn socket với port
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return 1;
    }

    // Chờ client kết nối
    if (listen(sock, 5) < 0) {
        close(sock);
        return 1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Chấp nhận client
    int client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_len);
    if (client_sock < 0) {
        close(sock);
        return 1;
    }

    struct Student sv;
    // Nhận dữ liệu từ client
    recv(client_sock, &sv, sizeof(sv), 0);
    // In ra màn hình
    printf("MSSV: %s \nName: %s \nBirth Date: %s \nGPA: %.2f\n", sv.id, sv.name, sv.birth, sv.gpa);
    // Lấy thời gian hiện tại
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    // Mở file log
    FILE *f = fopen(log_file, "a");
    if (f == NULL) {
        close(client_sock);
        close(sock);
        return 1;
    }

    // Ghi file log
    fprintf(f, "%s %04d-%02d-%02d %02d:%02d:%02d %s %s %s %.2f\n",
        inet_ntoa(client_addr.sin_addr),
        t->tm_year + 1900,
        t->tm_mon + 1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec,
        sv.id,
        sv.name,
        sv.birth,
        sv.gpa
    );

    fclose(f);
    close(client_sock);
    close(sock);

    return 0;
}