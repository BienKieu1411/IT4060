#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_LINE 2048

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static ssize_t send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return (ssize_t)sent;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <server_ip> <port> <nickname>\n", prog);
    fprintf(stderr, "Nickname chi gom chu thuong va chu so, vi du: user1\n");
}

static int connect_to_server(const char *host, const char *port) {
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    int sock = -1;
    for (p = res; p; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) break;
        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);
    if (sock < 0) die("connect");
    return sock;
}

static void print_help(void) {
    printf("\nCommands:\n");
    printf("  <text>                 gui tin nhan phong: MSG <text>\n");
    printf("  /msg <text>            gui tin nhan phong\n");
    printf("  /pmsg <nick> <text>    gui tin nhan rieng\n");
    printf("  /op <nick>             chuyen quyen chu phong\n");
    printf("  /kick <nick>           kick nguoi dung\n");
    printf("  /topic <topic>         dat chu de phong\n");
    printf("  /quit                  thoat\n");
    printf("  /help                  hien thi tro giup\n\n");
}

static void build_line(char *out, size_t outsz, const char *prefix, const char *text) {
    out[0] = '\0';
    strncat(out, prefix, outsz - 1);
    strncat(out, text, outsz - strlen(out) - 1);

    size_t n = strlen(out);
    if (n == 0 || out[n - 1] != '\n') {
        if (n + 1 < outsz) {
            out[n] = '\n';
            out[n + 1] = '\0';
        }
    }
}

static void make_command(char *out, size_t outsz, const char *input) {
    while (*input == ' ' || *input == '\t') input++;

    if (strncmp(input, "/msg ", 5) == 0) {
        build_line(out, outsz, "MSG ", input + 5);
    } else if (strncmp(input, "/pmsg ", 6) == 0) {
        build_line(out, outsz, "PMSG ", input + 6);
    } else if (strncmp(input, "/op ", 4) == 0) {
        build_line(out, outsz, "OP ", input + 4);
    } else if (strncmp(input, "/kick ", 6) == 0) {
        build_line(out, outsz, "KICK ", input + 6);
    } else if (strncmp(input, "/topic ", 7) == 0) {
        build_line(out, outsz, "TOPIC ", input + 7);
    } else if (strncmp(input, "/quit", 5) == 0) {
        build_line(out, outsz, "QUIT", "");
    } else {
        build_line(out, outsz, "MSG ", input);
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    int sock = connect_to_server(argv[1], argv[2]);
    printf("Connected to %s:%s\n", argv[1], argv[2]);

    char join_cmd[MAX_LINE];
    snprintf(join_cmd, sizeof(join_cmd), "JOIN %s\n", argv[3]);
    if (send_all(sock, join_cmd, strlen(join_cmd)) < 0) die("send JOIN");

    print_help();

    struct pollfd fds[2];
    char buf[MAX_LINE];

    while (1) {
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = sock;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            die("poll");
        }

        if (fds[1].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) {
            ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                printf("\nServer closed connection.\n");
                break;
            }
            buf[n] = '\0';
            printf("%s", buf);
            fflush(stdout);
        }

        if (fds[0].revents & POLLIN) {
            if (!fgets(buf, sizeof(buf), stdin)) break;
            if (strcmp(buf, "/help\n") == 0 || strcmp(buf, "/help\r\n") == 0) {
                print_help();
                continue;
            }

            char cmd[MAX_LINE];
            make_command(cmd, sizeof(cmd), buf);
            if (send_all(sock, cmd, strlen(cmd)) < 0) {
                printf("Send failed.\n");
                break;
            }
            if (strcmp(cmd, "QUIT\n") == 0) {
                /* Van tiep tuc vong lap de doc 100 OK tu server. */
            }
        }
    }

    close(sock);
    return EXIT_SUCCESS;
}
