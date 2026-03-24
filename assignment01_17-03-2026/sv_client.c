#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

struct Student {
    char id[20];
    char name[50];
    char birth[20];
    float gpa;
};

int main(int argc, char *argv[]) {
    // Kiểm tra tham số dòng lệnh
    if (argc != 3) {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    // Tạo socket TCP
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) return 1;

    // Khai báo địa chỉ server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    // Kết nối server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return 1;
    }

    struct Student sv;

    // Nhập thông tin sinh viên
    printf("MSSV: ");
    fgets(sv.id, sizeof(sv.id), stdin);
    sv.id[strcspn(sv.id, "\n")] = 0;

    printf("Name: ");
    fgets(sv.name, sizeof(sv.name), stdin);
    sv.name[strcspn(sv.name, "\n")] = 0;

    printf("Birth Date: ");
    fgets(sv.birth, sizeof(sv.birth), stdin);
    sv.birth[strcspn(sv.birth, "\n")] = 0;

    printf("GPA: ");
    scanf("%f", &sv.gpa);

    // Gửi sang server
    send(sock, &sv, sizeof(sv), 0);

    close(sock);
    return 0;
}