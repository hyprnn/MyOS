/*
 * httpd - маленький веб-сервер: отдаёт файлы из папки по HTTP.
 *
 *   httpd                  порт 80, текущая папка
 *   httpd 8080 /usb0p1     порт 8080, файлы флешки
 *   httpd 80 /bin -n 3     ответить на 3 запроса и выйти
 *
 * Открой в браузере другого компьютера http://<адрес MyOS>/ (адрес -
 * в ifconfig). В QEMU: -netdev user,hostfwd=tcp::8080-:80 и
 * http://localhost:8080/ на хосте. Ctrl+C - остановить.
 */
#include "myos.h"

static const char *g_root = ".";

static void reply(int s, int code, const char *type, const char *body, long len)
{
    char h[256];
    const char *msg = code == 200 ? "OK" : code == 404 ? "Not Found" : "Bad Request";
    int n = snprintf(h, sizeof(h), "HTTP/1.0 %d %s\r\nServer: MyOS-httpd/1.0\r\n"
                     "Content-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n",
                     code, msg, type, len);

    send_all(s, h, (size_t)n);
    if (body && len > 0)
        send_all(s, body, (size_t)len);
}

static const char *mime(const char *p)
{
    const char *dot = NULL;

    for (; *p; p++)
        if (*p == '.')
            dot = p;

    if (dot == NULL) return "application/octet-stream";
    if (strcmp(dot, ".htm") == 0 || strcmp(dot, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(dot, ".txt") == 0 || strcmp(dot, ".md") == 0 || strcmp(dot, ".c") == 0)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
    return "application/octet-stream";
}

/* Ответить на один запрос; вернуть код и сколько байт отдали */
static int serve(int s, long *sent)
{
    char req[1024];
    int rl = 0;

    *sent = 0;
    sock_timeout(s, 5000);

    /* запрос до пустой строки */
    while (rl < (int)sizeof(req) - 1) {
        long n = recv(s, req + rl, sizeof(req) - 1 - (size_t)rl);
        if (n <= 0)
            break;
        rl += (int)n;
        req[rl] = '\0';
        if (strstr(req, "\r\n\r\n") || strstr(req, "\n\n"))
            break;
    }
    req[rl] = '\0';

    if (strncmp(req, "GET ", 4) != 0) {
        reply(s, 400, "text/plain", "only GET\n", 9);
        return 400;
    }

    char path[200];
    int k = 0;
    const char *p = req + 4;

    while (*p && *p != ' ' && *p != '?' && k < 199)
        path[k++] = *p++;
    path[k] = '\0';

    if (path[0] != '/' || strstr(path, "..")) {
        reply(s, 400, "text/plain", "bad path\n", 9);
        return 400;
    }

    char full[300];
    snprintf(full, sizeof(full), "%s%s", g_root, path);

    struct myos_dirent st;

    if (stat(full, &st) < 0) {
        const char *b = "<html><body><h1>404</h1>No such file on this MyOS.</body></html>\n";
        reply(s, 404, "text/html", b, (long)strlen(b));
        return 404;
    }

    if (st.is_dir) {
        /* список файлов папки */
        char *page = malloc(16384);
        int n = snprintf(page, 16384, "<html><head><meta charset=\"utf-8\"><title>MyOS %s</title></head>"
                         "<body><h2>MyOS: %s</h2><ul>\n", path, path);
        struct myos_dirent d;
        const char *slash = path[strlen(path) - 1] == '/' ? "" : "/";

        for (int i = 0; readdir(full, i, &d) == 1 && n < 15000; i++)
            n += snprintf(page + n, (size_t)(16384 - n), "<li><a href=\"%s%s%s%s\">%s%s</a> %llu</li>\n",
                          path, slash, d.name, d.is_dir ? "/" : "", d.name, d.is_dir ? "/" : "",
                          d.size);

        n += snprintf(page + n, (size_t)(16384 - n), "</ul><p>Served by MyOS httpd.</p></body></html>\n");
        reply(s, 200, "text/html; charset=utf-8", page, n);
        *sent = n;
        free(page);
        return 200;
    }

    int f = open(full, O_READ);

    if (f < 0) {
        reply(s, 404, "text/plain", "cannot open\n", 12);
        return 404;
    }

    reply(s, 200, mime(full), NULL, (long)st.size);

    char buf[4096];

    for (;;) {
        long n = read(f, buf, sizeof(buf));
        if (n <= 0)
            break;
        if (send_all(s, buf, (size_t)n) != n)
            break;
        *sent += n;
    }

    close(f);
    return 200;
}

int main(int argc, char **argv)
{
    int port = 80, limit = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            limit = atoi(argv[++i]);
        else if (argv[i][0] >= '0' && argv[i][0] <= '9')
            port = atoi(argv[i]);
        else
            g_root = argv[i];
    }

    int ls = socket(MYOS_SOCK_STREAM);
    int r = (ls < 0) ? ls : bind(ls, 0, port);

    if (r >= 0)
        r = listen(ls, 8);

    if (r < 0) {
        printf("httpd: cannot listen on port %d: %s\n", port, strerror(r));
        return 1;
    }

    struct myos_netif n;
    char a[16];

    printf("httpd: serving '%s' on port %d. Open http://", g_root, port);
    for (int i = 0; netinfo(i, &n) == 1; i++)
        if (n.up && n.name[0] != 'l') {
            printf("%s", ip_to_str(n.ip, a));
            break;
        }
    printf(":%d/ (Ctrl+C - stop)\n", port);

    for (int count = 1; limit == 0 || count <= limit; count++) {

        struct myos_sockaddr from;
        int s = accept(ls, &from);

        if (s < 0) {
            printf("httpd: %s\n", strerror(s));
            break;
        }

        long sent;
        int code = serve(s, &sent);

        printf("httpd: #%d %s -> %d, %ld bytes\n", count, ip_to_str(from.ip, a), code, sent);
        close(s);
    }

    close(ls);
    return 0;
}
