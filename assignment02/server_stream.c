#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUF 1024

int main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr*)&addr, sizeof(addr));
    listen(s, 5);

    int client = accept(s, NULL, NULL);

    char buf[BUF], prev[BUF] = ""; 
    int count = 0;

    while (1) {
        int len = recv(client, buf, BUF - 1, 0);
        if (len <= 0) break;
        buf[len] = 0;

        // Nối xâu mới nhận được với cuối xâu trước đó
        char merged[BUF + 10];
        sprintf(merged, "%s%s", prev, buf);

        int start = strlen(prev);
        char *p = merged;

        // p += 10 để đếm xâu tiêp theo sau xâu đã đếm
        while ((p = strstr(p, "0123456789")) != NULL) {
            count++;
            p += 10;
        }

        printf("Count = %d\n", count);

        // Copy 9 ký tự cuối cùng (vì độ dài của xâu cần đếm là 10)
        int mlen = strlen(merged);
        if (mlen >= 9) strcpy(prev, merged + mlen - 9);
        else strcpy(prev, merged);

        // Xoá ký tự xuống dòng nếu có
        prev[strcspn(prev, "\n")] = '\0';
    }

    close(client);
    close(s);
}