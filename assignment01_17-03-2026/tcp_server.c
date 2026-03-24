#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // Kiểm tra tham số đầu vào
    if (argc != 4) {
        printf("Usage: %s <port> <hello_file> <log_file>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    char *hello_file = argv[2];
    char *save_file = argv[3];

    // Đọc nội dung file chào
    FILE *f_hello = fopen(hello_file, "r");
    if (f_hello == NULL) {
        return 1;
    }

    char hello[1024] = {0};
    fread(hello, 1, sizeof(hello) - 1, f_hello);
    fclose(f_hello);

    // Tạo socket TCP
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return 1;
    }

    // Khai báo địa chỉ Server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    // Gắn socket với port
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return 1;
    }

    // Chờ Client kết nối
    if (listen(sock, 5) < 0) {
        close(sock);
        return 1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Chấp nhận Client
    int client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_len);
    if (client_sock < 0) {
        close(sock);
        return 1;
    }

    // Gửi câu chào cho Client
    send(client_sock, hello, strlen(hello), 0);
    printf("%s\n", hello);

    // Mở file để lưu dữ liệu nhận được
    FILE *f_log = fopen(save_file, "a");
    if (f_log == NULL) {
        close(client_sock);
        close(sock);
        return 1;
    }

    char buffer[1024];
    int n;

    while ((n = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);
        // Lưu vào file
        fputs(buffer, f_log);
        // Nếu nhận Exit thì dừng
        if (strncmp(buffer, "Exit", 4) == 0)
            break;
    }

    // Đóng file và socket
    fclose(f_log);
    close(client_sock);
    close(sock);

    return 0;
}