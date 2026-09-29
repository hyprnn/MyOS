/*
 * nc (netcat) - "голое" TCP-соединение: что набрано - уходит, что
 * пришло - печатается.
 *
 *   nc 10.0.2.2 7777             соединиться
 *   nc -l 7777                   ждать, пока соединятся с нами
 *   nc example.com 80 "GET / HTTP/1.0"
 *                                отправить строку (+ пустую строку),
 *                                напечатать ответ, выйти
 *
 * Набор - в текстовом шелле (в терминале рабочего стола программа
 * только печатает пришедшее). Esc - выход.
 */
#include "myos.h"

static void pump(int s, const char *once)
{
    char buf[1024];
    char line[256];
    int ll = 0;

    sock_timeout(s, 50);

    if (once) {
        char req[600];
        int n = snprintf(req, sizeof(req), "%s\r\n\r\n", once);
        send_all(s, req, (size_t)n);
        sock_timeout(s, 10000);
    }

    for (;;) {

        long n = recv(s, buf, sizeof(buf));

        if (n > 0) {
            write(1, buf, (size_t)n);
            continue;
        }

        if (n == 0) {
            printf("\n[the other side closed the connection]\n");
            return;
        }

        if (n != MYOS_EAGAIN) {
            printf("\n[%s]\n", strerror((int)n));
            return;
        }

        if (once) {
            printf("\n[no more data]\n");
            return;
        }

        /* клавиатура: копим строку, Enter - отправить */
        for (;;) {
            int k = getkey();

            if (k == 0)
                break;

            if (k == 0x100 + KEY_ESC || k == 27) {
                printf("\n[bye]\n");
                return;
            }

            if (k == '\r' || k == '\n') {
                line[ll++] = '\n';
                putchar('\n');
                send_all(s, line, (size_t)ll);
                ll = 0;
            } else if (k == 8) {
                if (ll > 0) {
                    ll--;
                    printf("\b \b");
                }
            } else if (k >= 32 && k < 127 && ll < 250) {
                line[ll++] = (char)k;
                putchar(k);
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "-l") == 0) {

        int port = atoi(argv[2]);
        int ls = socket(MYOS_SOCK_STREAM);

        if (ls < 0 || bind(ls, 0, port) < 0 || listen(ls, 1) < 0) {
            printf("nc: cannot listen on port %d\n", port);
            return 1;
        }

        printf("Waiting for a connection on port %d (Ctrl+C - stop)...\n", port);

        struct myos_sockaddr from;
        int s = accept(ls, &from);

        close(ls);

        if (s < 0) {
            printf("nc: %s\n", strerror(s));
            return 1;
        }

        char a[16];
        printf("Connected: %s:%u. Type lines, Esc - quit.\n", ip_to_str(from.ip, a), from.port);
        pump(s, NULL);
        close(s);
        return 0;
    }

    if (argc < 3) {
        printf("usage: nc <host> <port> [\"line to send\"]   |   nc -l <port>\n");
        return 1;
    }

    unsigned int ip;
    int r = resolve(argv[1], &ip);

    if (r < 0) {
        printf("nc: %s: %s\n", argv[1], strerror(r));
        return 1;
    }

    int s = socket(MYOS_SOCK_STREAM);

    sock_timeout(s, 15000);
    r = connect(s, ip, atoi(argv[2]));

    if (r < 0) {
        printf("nc: %s\n", strerror(r));
        return 1;
    }

    if (argc < 4)
        printf("Connected. Type lines, Esc - quit.\n");

    pump(s, argc >= 4 ? argv[3] : NULL);
    close(s);

    return 0;
}
