#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 8192

void send_all(int client, const char *data, int len) {
    int sent = 0;

    while (sent < len) {
        int n = send(client, data + sent, len - sent, 0);
        if (n <= 0) break;
        sent += n;
    }
}

void send_html(int client, const char *status, const char *body) {
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

void send_home_page(int client) {
    const char *body =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>HTTP Calculator</title>"
        "</head>"
        "<body>"
        "<h1>HTTP Calculator</h1>"

        "<h2>GET Method</h2>"
        "<form method='GET' action='/calc'>"
        "First number: <input type='number' step='any' name='a'><br><br>"
        "Second number: <input type='number' step='any' name='b'><br><br>"
        "Operator: "
        "<select name='op'>"
        "<option value='add'>Addition</option>"
        "<option value='sub'>Subtraction</option>"
        "<option value='mul'>Multiplication</option>"
        "<option value='div'>Division</option>"
        "</select><br><br>"
        "<button type='submit'>Calculate by GET</button>"
        "</form>"

        "<hr>"

        "<h2>POST Method</h2>"
        "<form method='POST' action='/calc'>"
        "First number: <input type='number' step='any' name='a'><br><br>"
        "Second number: <input type='number' step='any' name='b'><br><br>"
        "Operator: "
        "<select name='op'>"
        "<option value='add'>Addition</option>"
        "<option value='sub'>Subtraction</option>"
        "<option value='mul'>Multiplication</option>"
        "<option value='div'>Division</option>"
        "</select><br><br>"
        "<button type='submit'>Calculate by POST</button>"
        "</form>"

        "</body>"
        "</html>";

    send_html(client, "200 OK", body);
}

void url_decode(char *s) {
    char *src = s;
    char *dst = s;

    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
}

int get_param(const char *query, const char *key, char *out, int out_size) {
    char temp[2048];

    strncpy(temp, query, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, "&");

    while (token) {
        char *eq = strchr(token, '=');

        if (eq) {
            *eq = '\0';

            char *k = token;
            char *v = eq + 1;

            if (strcmp(k, key) == 0) {
                strncpy(out, v, out_size - 1);
                out[out_size - 1] = '\0';
                url_decode(out);
                return 1;
            }
        }

        token = strtok(NULL, "&");
    }

    return 0;
}

void handle_calculation(int client, const char *query) {
    char a_str[64];
    char b_str[64];
    char op[64];

    printf("Parsed parameters: %s\n", query);

    if (!get_param(query, "a", a_str, sizeof(a_str)) ||
        !get_param(query, "b", b_str, sizeof(b_str)) ||
        !get_param(query, "op", op, sizeof(op))) {
        send_html(client, "400 Bad Request",
                  "<html><body>"
                  "<h1>400 Bad Request</h1>"
                  "<p>Missing parameters: a, b, op</p>"
                  "<a href='/'>Back</a>"
                  "</body></html>");
        return;
    }

    double a = atof(a_str);
    double b = atof(b_str);
    double result = 0;
    char symbol = '?';

    if (strcmp(op, "add") == 0) {
        result = a + b;
        symbol = '+';
    } else if (strcmp(op, "sub") == 0) {
        result = a - b;
        symbol = '-';
    } else if (strcmp(op, "mul") == 0) {
        result = a * b;
        symbol = '*';
    } else if (strcmp(op, "div") == 0) {
        if (b == 0) {
            send_html(client, "400 Bad Request",
                      "<html><body>"
                      "<h1>Error</h1>"
                      "<p>Division by zero is not allowed.</p>"
                      "<a href='/'>Back</a>"
                      "</body></html>");
            return;
        }

        result = a / b;
        symbol = '/';
    } else {
        send_html(client, "400 Bad Request",
                  "<html><body>"
                  "<h1>Unknown operator</h1>"
                  "<a href='/'>Back</a>"
                  "</body></html>");
        return;
    }

    printf("Calculation: %.2f %c %.2f = %.2f\n", a, symbol, b, result);

    char body[2048];

    sprintf(body,
            "<!DOCTYPE html>"
            "<html>"
            "<head><title>Result</title></head>"
            "<body>"
            "<h1>Calculation Result</h1>"
            "<p>%.2f %c %.2f = <b>%.2f</b></p>"
            "<a href='/'>Back to calculator</a>"
            "</body>"
            "</html>",
            a, symbol, b, result);

    send_html(client, "200 OK", body);
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

    if (strcmp(method, "GET") == 0) {
        if (strcmp(url, "/") == 0) {
            send_home_page(client);
        } else if (strncmp(url, "/calc?", 6) == 0) {
            char *query = strchr(url, '?');
            query++;
            handle_calculation(client, query);
        } else {
            send_html(client, "404 Not Found",
                      "<html><body><h1>404 Not Found</h1></body></html>");
        }
    } else if (strcmp(method, "POST") == 0) {
        if (strcmp(url, "/calc") == 0) {
            char *body = strstr(request, "\r\n\r\n");

            if (body) {
                body += 4;
                printf("POST body: %s\n", body);
                handle_calculation(client, body);
            } else {
                send_html(client, "400 Bad Request",
                          "<html><body><h1>Missing POST body</h1></body></html>");
            }
        } else {
            send_html(client, "404 Not Found",
                      "<html><body><h1>404 Not Found</h1></body></html>");
        }
    } else {
        send_html(client, "405 Method Not Allowed",
                  "<html><body><h1>405 Method Not Allowed</h1></body></html>");
    }

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

    printf("Calculator HTTP server is running at:\n");
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