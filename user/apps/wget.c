/*
 * wget - скачать файл по HTTP.
 *
 *   wget http://example.com/            -> файл index.html в текущей папке
 *   wget http://host:8080/file.bin      -> file.bin
 *   wget -O - http://example.com/       -> на экран
 *   wget -O /usb0p1/page.htm http://... -> в этот файл
 *   wget -O null http://...             -> никуда (проверка скорости)
 * В конце печатается CRC-32 скачанного - сверить с оригиналом.
 *
 * Как это работает: DNS (имя -> адрес), соединение TCP с портом 80,
 * запрос "GET /путь HTTP/1.0" + заголовки, ответ: строка статуса
 * ("HTTP/1.1 200 OK"), заголовки, пустая строка, данные. Переадресации
 * (301/302...) - следуем, но только на http://: HTTPS (шифрование TLS)
 * в MyOS пока нет, а многие сайты требуют именно его.
 * Поддерживается "Transfer-Encoding: chunked" (данные кусками).
 */
#include "myos.h"

#define HDR_MAX 8192

static char g_host[128];
static char g_path[512];
static int  g_port;

/* Разобрать URL в g_host / g_port / g_path. 0 - не понял, -1 - https */
static int parse_url(const char *url)
{
    if (strncmp(url, "https://", 8) == 0)
        return -1;

    if (strncmp(url, "http://", 7) == 0)
        url += 7;

    int k = 0;

    while (*url && *url != '/' && *url != ':' && k < 127)
        g_host[k++] = *url++;

    g_host[k] = '\0';
    g_port = 80;

    if (*url == ':') {
        url++;
        g_port = atoi(url);
        while (*url >= '0' && *url <= '9')
            url++;
    }

    if (*url == '\0')
        strcpy(g_path, "/");
    else {
        strncpy(g_path, url, sizeof(g_path) - 1);
        g_path[sizeof(g_path) - 1] = '\0';
    }

    return (g_host[0] != '\0' && g_port > 0 && g_port < 65536) ? 1 : 0;
}

static int starts_ci(const char *s, const char *pfx)
{
    while (*pfx) {
        if (tolower(*s) != tolower(*pfx))
            return 0;
        s++;
        pfx++;
    }
    return 1;
}

/* Значение заголовка name из блока заголовков (NULL - нет) */
static const char *header(const char *hdrs, const char *name, char *out, int cap)
{
    const char *p = hdrs;
    int nl = (int)strlen(name);

    while (*p) {

        if (starts_ci(p, name) && p[nl] == ':') {
            p += nl + 1;
            while (*p == ' ')
                p++;
            int k = 0;
            while (*p && *p != '\r' && *p != '\n' && k + 1 < cap)
                out[k++] = *p++;
            out[k] = '\0';
            return out;
        }

        while (*p && *p != '\n')
            p++;
        if (*p)
            p++;
    }

    return NULL;
}

/* ---- вывод: файл, экран или никуда ---- */
static int g_out = -1;
static int g_screen = 0;
static int g_null = 0;
static unsigned long long g_written = 0;
static unsigned int g_crc = 0xFFFFFFFFu;

/* CRC-32 (как у zip и Ethernet) - чтобы сверить файл с оригиналом */
static void crc_add(const unsigned char *d, long n)
{
    for (long i = 0; i < n; i++) {
        g_crc ^= d[i];
        for (int k = 0; k < 8; k++)
            g_crc = (g_crc >> 1) ^ (0xEDB88320u & (0u - (g_crc & 1u)));
    }
}

static int out_write(const char *d, long n)
{
    if (n <= 0)
        return 0;

    crc_add((const unsigned char *)d, n);

    if (g_null) {
        g_written += (unsigned long long)n;
        return 0;
    }

    long r = write(g_screen ? 1 : g_out, d, (size_t)n);

    if (r != n) {
        printf("\nwget: cannot write: %s\n", r < 0 ? strerror((int)r) : "disk full?");
        return -1;
    }

    g_written += (unsigned long long)n;
    return 0;
}

/* ---- разбор "chunked": размер (hex) \r\n данные \r\n ... 0 \r\n\r\n ---- */
static int g_chunked = 0;
static long g_chunk_left = 0;     /* байт данных осталось в куске */
static int g_chunk_state = 0;     /* 0 - читаем размер, 1 - данные, 2 - \r\n после данных, 3 - конец */
static char g_chunk_line[32];
static int g_chunk_ll = 0;

static int body_feed(const char *d, long n)
{
    if (!g_chunked)
        return out_write(d, n);

    long i = 0;

    while (i < n && g_chunk_state != 3) {

        if (g_chunk_state == 1) {
            long k = n - i < g_chunk_left ? n - i : g_chunk_left;
            if (out_write(d + i, k) < 0)
                return -1;
            i += k;
            g_chunk_left -= k;
            if (g_chunk_left == 0)
                g_chunk_state = 2;
            continue;
        }

        char c = d[i++];

        if (c == '\n') {
            g_chunk_line[g_chunk_ll] = '\0';
            if (g_chunk_state == 2) {
                g_chunk_state = 0;           /* \r\n после куска */
            } else if (g_chunk_ll > 0) {
                long v = 0;
                for (int k = 0; g_chunk_line[k]; k++) {
                    char h = (char)tolower(g_chunk_line[k]);
                    if (h >= '0' && h <= '9') v = v * 16 + (h - '0');
                    else if (h >= 'a' && h <= 'f') v = v * 16 + (h - 'a' + 10);
                    else break;                  /* ";расширения" */
                }
                g_chunk_left = v;
                g_chunk_state = v ? 1 : 3;
            }
            g_chunk_ll = 0;
        } else if (c != '\r' && g_chunk_ll < 31) {
            g_chunk_line[g_chunk_ll++] = c;
        }
    }

    return 0;
}

static const char *base_name(const char *path)
{
    static char name[64];
    const char *p = path, *last = path;

    for (; *p && *p != '?'; p++)
        if (*p == '/')
            last = p + 1;

    int k = 0;
    while (last[k] && last[k] != '?' && k < 63) {
        name[k] = last[k];
        k++;
    }
    name[k] = '\0';

    return name[0] ? name : "index.html";
}

int main(int argc, char **argv)
{
    const char *url = NULL, *outname = NULL;
    int quiet = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-O") == 0 && i + 1 < argc)
            outname = argv[++i];
        else if (strcmp(argv[i], "-q") == 0)
            quiet = 1;
        else
            url = argv[i];
    }

    if (url == NULL) {
        printf("usage: wget [-q] [-O file | -O -] http://host[:port]/path\n");
        return 1;
    }

    static char cur_url[640];
    strncpy(cur_url, url, sizeof(cur_url) - 1);

    char *hdr = malloc(HDR_MAX + 1);
    char *buf = malloc(8192);

    if (!hdr || !buf) {
        printf("wget: out of memory\n");
        return 1;
    }

    for (int redirects = 0; redirects <= 5; redirects++) {

        int pu = parse_url(cur_url);

        if (pu < 0) {
            printf("wget: %s - HTTPS needs encryption (TLS), MyOS has no TLS yet.\n"
                   "      Try the http:// address of the site, if it has one.\n", cur_url);
            return 1;
        }
        if (pu == 0) {
            printf("wget: cannot understand the address '%s'\n", cur_url);
            return 1;
        }

        unsigned int ip;
        char ips[16];
        int r = resolve(g_host, &ip);

        if (r < 0) {
            printf("wget: %s: %s\n", g_host, strerror(r));
            return 1;
        }

        if (!quiet)
            printf("Connecting to %s (%s):%d... ", g_host, ip_to_str(ip, ips), g_port);

        int s = socket(MYOS_SOCK_STREAM);

        if (s < 0) {
            printf("wget: no socket: %s\n", strerror(s));
            return 1;
        }

        sock_timeout(s, 20000);
        r = connect(s, ip, g_port);

        if (r < 0) {
            printf("%s\n", strerror(r));
            close(s);
            return 1;
        }

        if (!quiet)
            printf("connected.\n");

        char req[800];
        int rl;

        if (g_port == 80)
            rl = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\n"
                          "User-Agent: MyOS-wget/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n",
                          g_path, g_host);
        else
            rl = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s:%d\r\n"
                          "User-Agent: MyOS-wget/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n",
                          g_path, g_host, g_port);

        if (send_all(s, req, (size_t)rl) != rl) {
            printf("wget: cannot send the request\n");
            close(s);
            return 1;
        }

        /* --- заголовки ответа: до пустой строки --- */
        int hl = 0, body_at = -1;

        while (body_at < 0) {

            long n = recv(s, hdr + hl, (size_t)(HDR_MAX - hl));

            if (n <= 0) {
                printf("wget: %s\n", n < 0 ? strerror((int)n) : "the server closed the connection");
                close(s);
                return 1;
            }

            hl += (int)n;
            hdr[hl] = '\0';

            for (int k = 3; k < hl; k++)
                if (hdr[k - 3] == '\r' && hdr[k - 2] == '\n' && hdr[k - 1] == '\r' && hdr[k] == '\n') {
                    body_at = k + 1;
                    break;
                }

            if (body_at < 0 && hl >= HDR_MAX) {
                printf("wget: the answer headers are too long\n");
                close(s);
                return 1;
            }
        }

        int code = 0;
        char *sp = strchr(hdr, ' ');
        if (sp)
            code = atoi(sp + 1);

        if (!quiet) {
            char *eol = strchr(hdr, '\r');
            if (eol) *eol = '\0';
            printf("HTTP request sent, answer: %s\n", hdr);
            if (eol) *eol = '\r';
        }

        char val[512];

        if (code >= 300 && code < 400 && header(hdr, "Location", val, sizeof(val))) {

            close(s);

            if (val[0] == '/') {
                char tmp[640];
                snprintf(tmp, sizeof(tmp), "http://%s:%d%s", g_host, g_port, val);
                strcpy(cur_url, tmp);
            } else {
                strncpy(cur_url, val, sizeof(cur_url) - 1);
            }

            if (!quiet)
                printf("Redirected to %s\n", cur_url);
            continue;
        }

        long long total = -1;

        if (header(hdr, "Content-Length", val, sizeof(val)))
            total = atol(val);

        g_chunked = header(hdr, "Transfer-Encoding", val, sizeof(val)) &&
                    starts_ci(val, "chunked");

        if (code != 200)
            printf("wget: the server says %d - saving the answer anyway\n", code);

        /* --- куда писать --- */
        const char *fname = outname ? outname : base_name(g_path);

        g_screen = (strcmp(fname, "-") == 0);
        g_null = (strcmp(fname, "null") == 0);

        if (!g_screen && !g_null) {
            g_out = open(fname, O_WRITE | O_CREATE | O_TRUNC);
            if (g_out < 0) {
                printf("wget: cannot create '%s': %s\n", fname, strerror(g_out));
                close(s);
                return 1;
            }
            if (!quiet) {
                if (total >= 0)
                    printf("Length: %lld bytes. Saving to '%s'\n", total, fname);
                else
                    printf("Length: unknown. Saving to '%s'\n", fname);
            }
        }

        unsigned long t0 = uptime_ms();
        unsigned long long got = 0, next_report = 256 * 1024;
        int fail = 0;

        /* данные, пришедшие вместе с заголовками */
        if (hl > body_at) {
            got += (unsigned long long)(hl - body_at);
            if (body_feed(hdr + body_at, hl - body_at) < 0)
                fail = 1;
        }

        while (!fail && (total < 0 || (long long)got < total) && g_chunk_state != 3) {

            long n = recv(s, buf, 8192);

            if (n == 0)
                break;

            if (n < 0) {
                printf("\nwget: %s\n", strerror((int)n));
                fail = 1;
                break;
            }

            got += (unsigned long long)n;

            if (body_feed(buf, n) < 0)
                fail = 1;

            if (!quiet && !g_screen && got >= next_report) {
                printf("  %llu KiB", got / 1024);
                if (total > 0)
                    printf(" of %lld KiB (%llu%%)", total / 1024, got * 100ull / (unsigned long long)total);
                printf("\n");
                next_report += 256 * 1024;
            }
        }

        close(s);

        if (!g_screen && !g_null)
            close(g_out);

        if (fail)
            return 1;

        if (total >= 0 && (long long)got < total) {
            printf("wget: the connection closed early: %llu of %lld bytes\n", got, total);
            return 1;
        }

        unsigned long ms = uptime_ms() - t0;

        if (ms == 0)
            ms = 1;

        if (!quiet) {
            int named = !g_screen && !g_null;
            if (g_screen)
                printf("\n");
            printf("Done: %llu bytes in %lu ms (%llu KiB/s), CRC-32 %08x%s%s%s\n", g_written, ms,
                   g_written * 1000ull / 1024ull / ms, ~g_crc,
                   named ? ", saved to '" : "", named ? fname : "", named ? "'" : "");
        }

        return 0;
    }

    printf("wget: too many redirects\n");
    return 1;
}
