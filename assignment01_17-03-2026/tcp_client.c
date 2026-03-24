#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // Kiểm tra số lượng tham số dòng lệnh
    if (argc != 3) {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    // Tạo socket TCP
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perror("Create socket failed");
        return 1;
    }

    // Khai báo địa chỉ server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;               
    server_addr.sin_port = htons(port);             
    server_addr.sin_addr.s_addr = inet_addr(ip);    

    // Kết nối tới server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to connect");
        close(sock);
        return 1;
    }

    char buffer[1024];

    while (1) {
        // Nhập dữ liệu
        printf("Enter: ");
        fgets(buffer, sizeof(buffer), stdin);

        // Gửi dữ liệu sang server
        send(sock, buffer, strlen(buffer), 0);

        // Nhập Exit để kết thúc chương trình
        if (strncmp(buffer, "Exit", 4) == 0)
            break;
    }

    // Đóng socket 
    close(sock);

    return 0;
}