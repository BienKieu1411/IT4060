#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SERVER_HOST "lebavui.io.vn"
#define SERVER_PORT "21"
#define BUF_SIZE 8192

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static int send_all(int sock, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sock, buf + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int connect_tcp(const char *host, const char *port) {
    struct addrinfo hints, *res, *p;
    int sock = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(host, port, &hints, &res);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;

        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);

    if (sock < 0) {
        fprintf(stderr, "Cannot connect to %s:%s\n", host, port);
        exit(EXIT_FAILURE);
    }

    return sock;
}

static int read_line(int sock, char *buf, size_t size) {
    size_t i = 0;
    while (i + 1 < size) {
        char c;
        ssize_t n = recv(sock, &c, 1, 0);
        if (n <= 0) return -1;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (int)i;
}

static int read_response(int sock, char *last_line, size_t last_size) {
    char line[BUF_SIZE];
    char code_str[4] = {0};
    int code = 0;
    int first = 1;
    int multiline = 0;

    while (1) {
        if (read_line(sock, line, sizeof(line)) < 0) {
            fprintf(stderr, "Failed to read FTP response\n");
            exit(EXIT_FAILURE);
        }

        printf("<-- %s", line);

        strncpy(last_line, line, last_size - 1);
        last_line[last_size - 1] = '\0';

        if (strlen(line) >= 4 &&
            isdigit((unsigned char)line[0]) &&
            isdigit((unsigned char)line[1]) &&
            isdigit((unsigned char)line[2])) {

            if (first) {
                strncpy(code_str, line, 3);
                code_str[3] = '\0';
                code = atoi(code_str);
                multiline = (line[3] == '-');
                first = 0;

                if (!multiline) return code;
            } else {
                if (strncmp(line, code_str, 3) == 0 && line[3] == ' ') {
                    return code;
                }
            }
        }
    }
}

static void send_cmd(int sock, const char *fmt, ...) {
    char cmd[BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);

    char full_cmd[BUF_SIZE];
    snprintf(full_cmd, sizeof(full_cmd), "%s\r\n", cmd);

    if (strncmp(cmd, "PASS ", 5) == 0) {
        printf("--> PASS ******\n");
    } else {
        printf("--> %s\n", cmd);
    }

    if (send_all(sock, full_cmd, strlen(full_cmd)) < 0) {
        die("send");
    }
}

static void open_passive_data_connection(int ctrl_sock, int *data_sock) {
    char resp[BUF_SIZE];
    int h1, h2, h3, h4, p1, p2;

    send_cmd(ctrl_sock, "PASV");

    int code = read_response(ctrl_sock, resp, sizeof(resp));
    if (code != 227) {
        fprintf(stderr, "PASV failed, code = %d\n", code);
        exit(EXIT_FAILURE);
    }

    char *start = strchr(resp, '(');
    if (!start ||
        sscanf(start + 1, "%d,%d,%d,%d,%d,%d",
               &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        fprintf(stderr, "Cannot parse PASV response: %s\n", resp);
        exit(EXIT_FAILURE);
    }

    char host[64];
    char port[16];

    snprintf(host, sizeof(host), "%d.%d.%d.%d", h1, h2, h3, h4);
    snprintf(port, sizeof(port), "%d", p1 * 256 + p2);

    *data_sock = connect_tcp(host, port);
}

static char *read_data_socket(int data_sock, size_t *out_len) {
    size_t cap = BUF_SIZE;
    size_t len = 0;
    char *data = malloc(cap + 1);

    if (!data) die("malloc");

    while (1) {
        if (len + BUF_SIZE + 1 > cap) {
            cap *= 2;
            char *tmp = realloc(data, cap + 1);
            if (!tmp) {
                free(data);
                die("realloc");
            }
            data = tmp;
        }

        ssize_t n = recv(data_sock, data + len, BUF_SIZE, 0);
        if (n < 0) {
            free(data);
            die("recv data");
        }

        if (n == 0) break;
        len += (size_t)n;
    }

    data[len] = '\0';
    *out_len = len;
    return data;
}

static void write_local_file(const char *filename, const char *data, size_t len) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) die("fopen");

    fwrite(data, 1, len, fp);
    fclose(fp);
}

static int ends_with(const char *s, const char *suffix) {
    size_t len_s = strlen(s);
    size_t len_suffix = strlen(suffix);

    if (len_s < len_suffix) return 0;
    return strcmp(s + len_s - len_suffix, suffix) == 0;
}

static void find_question_file(const char *list_data, char *question_name, size_t size) {
    char *copy = strdup(list_data);
    if (!copy) die("strdup");

    char *token = strtok(copy, " \t\r\n");
    while (token) {
        if (strncmp(token, "question_", 9) == 0 && ends_with(token, ".txt")) {
            snprintf(question_name, size, "%s", token);
            free(copy);
            return;
        }

        token = strtok(NULL, " \t\r\n");
    }

    free(copy);

    fprintf(stderr, "Cannot find question_xxxxxx.txt in file list\n");
    exit(EXIT_FAILURE);
}

static char *reverse_content(const char *data, size_t len, size_t *out_len) {
    while (len > 0 && (data[len - 1] == '\n' || data[len - 1] == '\r')) {
        len--;
    }

    char *rev = malloc(len + 1);
    if (!rev) die("malloc reverse");

    for (size_t i = 0; i < len; i++) {
        rev[i] = data[len - 1 - i];
    }

    rev[len] = '\0';
    *out_len = len;
    return rev;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage:\n");
        printf("  %s <mssv> <birth_day> [password_override]\n", argv[0]);
        printf("\nExample:\n");
        printf("  %s 20235272 14\n", argv[0]);
        printf("  %s 20235272 14 2023527214\n", argv[0]);
        return 1;
    }

    const char *mssv = argv[1];
    int birth_day = atoi(argv[2]);

    if (strlen(mssv) < 4) {
        fprintf(stderr, "Invalid MSSV\n");
        return 1;
    }

    char username[128];
    char password[128];

    snprintf(username, sizeof(username), "user_%s", mssv);

    if (argc >= 4) {
        snprintf(password, sizeof(password), "%s", argv[3]);
    } else {
        const char *last4 = mssv + strlen(mssv) - 4;
        snprintf(password, sizeof(password), "%s%02d", last4, birth_day);
    }

    printf("Connecting to FTP server %s...\n", SERVER_HOST);

    int ctrl_sock = connect_tcp(SERVER_HOST, SERVER_PORT);
    char resp[BUF_SIZE];

    read_response(ctrl_sock, resp, sizeof(resp));

    send_cmd(ctrl_sock, "USER %s", username);
    read_response(ctrl_sock, resp, sizeof(resp));

    send_cmd(ctrl_sock, "PASS %s", password);
    int login_code = read_response(ctrl_sock, resp, sizeof(resp));

    if (login_code != 230) {
        fprintf(stderr, "Login failed. Check username/password.\n");
        close(ctrl_sock);
        return 1;
    }

    send_cmd(ctrl_sock, "TYPE I");
    read_response(ctrl_sock, resp, sizeof(resp));

    printf("\n========== LIST FILES ==========\n");

    int data_sock;
    open_passive_data_connection(ctrl_sock, &data_sock);

    send_cmd(ctrl_sock, "LIST");
    read_response(ctrl_sock, resp, sizeof(resp));

    size_t list_len;
    char *list_data = read_data_socket(data_sock, &list_len);
    close(data_sock);

    read_response(ctrl_sock, resp, sizeof(resp));

    printf("\nFile list:\n%s\n", list_data);

    char question_name[256];
    find_question_file(list_data, question_name, sizeof(question_name));
    free(list_data);

    printf("Question file found: %s\n", question_name);

    printf("\n========== DOWNLOAD QUESTION ==========\n");

    open_passive_data_connection(ctrl_sock, &data_sock);

    send_cmd(ctrl_sock, "RETR %s", question_name);
    read_response(ctrl_sock, resp, sizeof(resp));

    size_t question_len;
    char *question_content = read_data_socket(data_sock, &question_len);
    close(data_sock);

    read_response(ctrl_sock, resp, sizeof(resp));

    write_local_file(question_name, question_content, question_len);

    printf("Downloaded %s\n", question_name);
    printf("Question content: %s\n", question_content);

    printf("\n========== CREATE ANSWER ==========\n");

    size_t answer_len;
    char *answer_content = reverse_content(question_content, question_len, &answer_len);

    char answer_name[256];
    if (strncmp(question_name, "question_", 9) == 0) {
        snprintf(answer_name, sizeof(answer_name), "answer_%s", question_name + 9);
    } else {
        snprintf(answer_name, sizeof(answer_name), "answer.txt");
    }

    write_local_file(answer_name, answer_content, answer_len);

    printf("Answer file created: %s\n", answer_name);
    printf("Answer content: %s\n", answer_content);

    printf("\n========== UPLOAD ANSWER ==========\n");

    open_passive_data_connection(ctrl_sock, &data_sock);

    send_cmd(ctrl_sock, "STOR %s", answer_name);
    read_response(ctrl_sock, resp, sizeof(resp));

    if (send_all(data_sock, answer_content, answer_len) < 0) {
        die("send answer data");
    }

    close(data_sock);

    read_response(ctrl_sock, resp, sizeof(resp));

    printf("Uploaded %s successfully.\n", answer_name);

    send_cmd(ctrl_sock, "QUIT");
    read_response(ctrl_sock, resp, sizeof(resp));

    close(ctrl_sock);

    free(question_content);
    free(answer_content);

    printf("\nDone.\n");

    return 0;
}