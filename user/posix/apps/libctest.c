/*
 * libctest - проверка полной libc (picolibc) на MyOS (этап 9).
 *
 *   libctest            все проверки; печатает "libctest: N/N OK"
 *   libctest папка      где создавать временные файлы (по умолчанию /ram)
 *
 * Проверяет то, что понадобится браузеру и другим перенесённым
 * программам: printf с дробными числами, strtod/sscanf, qsort, math.h,
 * большой malloc, FILE * (запись, чтение, перемещение), stat, opendir,
 * время и часовой пояс, chdir/getcwd, setjmp, регулярные выражения,
 * UTF-8. Автотест (tools/autotest.py) ищет строку "OK".
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <regex.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

static int g_total, g_failed;

static void check(int ok, const char *what)
{
    g_total++;

    if (!ok) {
        g_failed++;
        printf("  FAIL: %s\n", what);
    }
}

static int near(double a, double b)
{
    return fabs(a - b) < 1e-9 * (fabs(b) > 1.0 ? fabs(b) : 1.0);
}

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;

    return (x > y) - (x < y);
}

static void test_format(void)
{
    char buf[128];

    snprintf(buf, sizeof(buf), "%.3f|%e|%g|%lld|%zu|%#x|%-4s|%5.1f", 3.14159, 12345.678,
             0.0001, -1234567890123LL, (size_t)42, 255, "ab", -2.25);
    check(strcmp(buf, "3.142|1.234568e+04|0.0001|-1234567890123|42|0xff|ab  | -2.2") == 0 ||
          strcmp(buf, "3.142|1.234568e+04|0.0001|-1234567890123|42|0xff|ab  | -2.3") == 0,
          "printf: числа с плавающей точкой и форматы");

    const char *volatile long_str = "abcdefgh";   /* volatile: чтобы gcc не
                                                     предупреждал об обрезке */
    check(snprintf(buf, 5, "%s", long_str) == 8 && strcmp(buf, "abcd") == 0,
          "snprintf: обрезка и длина");

    check(near(strtod("2.5e3", NULL), 2500.0) && near(atof("-0.125"), -0.125),
          "strtod / atof");

    int n = 0;
    char word[16];
    double d = 0;
    check(sscanf("12 abc 3.5", "%d %15s %lf", &n, word, &d) == 3 && n == 12 &&
          strcmp(word, "abc") == 0 && near(d, 3.5), "sscanf");

    errno = 0;
    long v = strtol("7fffffffffffffffff", NULL, 16);
    check(v == LONG_MAX && errno == ERANGE, "strtol: переполнение -> ERANGE");
    check(strtoul("ff", NULL, 16) == 255 && strtol("-077", NULL, 0) == -63, "strtol: основания");
}

static void test_algo(void)
{
    int a[] = { 5, 3, 9, 1, 7, 2, 8 };
    int key = 7;

    qsort(a, 7, sizeof(int), cmp_int);
    check(a[0] == 1 && a[3] == 5 && a[6] == 9, "qsort");

    int *f = bsearch(&key, a, 7, sizeof(int), cmp_int);
    check(f != NULL && *f == 7, "bsearch");

    check(toupper('q') == 'Q' && isdigit('7') && !isalpha('1'), "ctype");

    char s[] = "a,b,,c";
    char *save = NULL;
    int cnt = 0;
    for (char *t = strtok_r(s, ",", &save); t; t = strtok_r(NULL, ",", &save))
        cnt++;
    check(cnt == 3, "strtok_r");
}

static void test_math(void)
{
    check(near(sqrt(2.0), 1.4142135623730951), "sqrt");
    check(near(sin(M_PI / 2), 1.0) && near(cos(0.0), 1.0), "sin / cos");
    check(near(pow(2.0, 10.0), 1024.0) && near(exp(1.0), M_E) && near(log(M_E), 1.0),
          "pow / exp / log");
    check(floor(-1.5) == -2.0 && ceil(1.2) == 2.0 && fabs(-3.0) == 3.0 && round(2.5) == 3.0,
          "floor / ceil / round");
    check(near(atan2(1.0, 1.0), M_PI / 4) && isnan(sqrt(-1.0)) && isinf(1.0 / 0.0),
          "atan2 / nan / inf");
}

static void test_memory(void)
{
    size_t big = 32u * 1024u * 1024u;
    unsigned char *p = malloc(big);

    check(p != NULL, "malloc 32 МБ");
    if (p != NULL) {
        memset(p, 0xA5, big);
        check(p[0] == 0xA5 && p[big - 1] == 0xA5, "32 МБ: запись и чтение");
        free(p);
    }

    char *r = NULL;
    int ok = 1;
    for (int i = 1; i <= 64; i++) {
        char *nr = realloc(r, (size_t)i * 1000u);
        if (nr == NULL) { ok = 0; break; }
        r = nr;
        r[(size_t)i * 1000u - 1] = (char)i;
        if (i > 1 && r[(size_t)(i - 1) * 1000u - 1] != (char)(i - 1))
            ok = 0;
    }
    free(r);
    check(ok, "realloc: данные сохраняются");

    int *z = calloc(1000, sizeof(int));
    int zero = (z != NULL);
    for (int i = 0; z && i < 1000; i++)
        if (z[i] != 0)
            zero = 0;
    free(z);
    check(zero, "calloc: нули");
}

static void test_files(const char *dir)
{
    char path[256], path2[256], line[64];

    snprintf(path, sizeof(path), "%s/LCTEST.TXT", dir);
    snprintf(path2, sizeof(path2), "%s/LCTEST2.TXT", dir);
    remove(path);
    remove(path2);

    FILE *f = fopen(path, "w");
    check(f != NULL, "fopen на запись");
    if (f == NULL)
        return;
    for (int i = 0; i < 100; i++)
        fprintf(f, "line %03d\n", i);
    check(fclose(f) == 0, "fclose");

    struct stat st;
    check(stat(path, &st) == 0 && st.st_size == 900 && S_ISREG(st.st_mode), "stat: размер файла");

    f = fopen(path, "r");
    check(f != NULL, "fopen на чтение");
    if (f == NULL)
        return;
    check(fgets(line, sizeof(line), f) && strcmp(line, "line 000\n") == 0, "fgets");
    check(fseek(f, 9 * 50, SEEK_SET) == 0 && fgets(line, sizeof(line), f) &&
          strcmp(line, "line 050\n") == 0, "fseek + fgets");
    check(ftell(f) == 9 * 51, "ftell");
    check(fseek(f, -9, SEEK_END) == 0 && fgets(line, sizeof(line), f) &&
          strcmp(line, "line 099\n") == 0, "fseek от конца");
    check(fgets(line, sizeof(line), f) == NULL && feof(f), "конец файла");
    struct stat fst;
    check(fstat(fileno(f), &fst) == 0 && fst.st_size == 900, "fstat");
    fclose(f);

    f = fopen(path, "a");
    if (f) {
        fputs("tail\n", f);
        fclose(f);
    }
    check(stat(path, &st) == 0 && st.st_size == 905, "fopen \"a\": дописать");

    check(rename(path, path2) == 0 && access(path, F_OK) != 0 && access(path2, F_OK) == 0,
          "rename");
    check(remove(path2) == 0 && access(path2, F_OK) != 0, "remove");

    errno = 0;
    check(fopen("/nonexistent/x", "r") == NULL && errno == ENOENT, "fopen: нет файла -> ENOENT");
}

static void test_dirs(void)
{
    DIR *d = opendir("/bin");
    int n = 0, found = 0;

    check(d != NULL, "opendir /bin");
    if (d != NULL) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            n++;
            if (strcmp(e->d_name, "libctest") == 0)
                found = 1;
        }
        closedir(d);
    }
    check(n > 3 && found, "readdir: в /bin есть libctest");

    char cwd[256];
    check(chdir("/bin") == 0 && getcwd(cwd, sizeof(cwd)) && strcmp(cwd, "/bin") == 0,
          "chdir / getcwd");
    check(access("libctest", F_OK) == 0, "путь от текущей папки");
    check(chdir("/nonexistent") != 0, "chdir в несуществующую папку");
}

static void test_time(void)
{
    time_t t = time(NULL);

    check(t > 1700000000, "time(): время похоже на настоящее");

    struct tm tm;
    char buf[64];
    time_t fixed = 1000000000;               /* 2001-09-09 01:46:40 UTC */
    gmtime_r(&fixed, &tm);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    check(strcmp(buf, "2001-09-09 01:46:40") == 0, "gmtime + strftime");
    check(mktime(&tm) != (time_t)-1, "mktime");

    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    struct timespec req = { 0, 30 * 1000000 };
    nanosleep(&req, NULL);
    clock_gettime(CLOCK_MONOTONIC, &b);
    long ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000;
    check(ms >= 29 && ms < 1000, "nanosleep 30 мс");

    time_t now = time(NULL);
    struct tm lt;
    check(localtime_r(&now, &lt) != NULL && lt.tm_year >= 123, "localtime");
}

static jmp_buf g_jb;

static void jump_back(int v)
{
    longjmp(g_jb, v);
}

static void test_misc(void)
{
    volatile int stage = 0;

    if (setjmp(g_jb) == 0) {
        stage = 1;
        jump_back(5);
    } else {
        stage = 2;
    }
    check(stage == 2, "setjmp / longjmp");

    regex_t re;
    regmatch_t m[2];
    check(regcomp(&re, "([0-9]+)px", REG_EXTENDED) == 0 &&
          regexec(&re, "width: 120px;", 2, m, 0) == 0 && m[1].rm_so == 7 && m[1].rm_eo == 10,
          "регулярные выражения");
    regfree(&re);

    setlocale(LC_ALL, "C.UTF-8");
    wchar_t w[16];
    size_t n = mbstowcs(w, "привет", 16);
    check(n == 6 && w[0] == 0x43F, "UTF-8: mbstowcs");

    check(setenv("MYVAR", "42", 1) == 0 && strcmp(getenv("MYVAR"), "42") == 0, "setenv / getenv");

    uint32_t r1 = arc4random(), r2 = arc4random();
    check(r1 != r2, "arc4random");

    check(getpid() > 0, "getpid");
}

int main(int argc, char **argv)
{
    const char *dir = (argc > 1) ? argv[1] : "/ram";

    printf("libctest: picolibc on MyOS, temp files in %s\n", dir);

    test_format();
    test_algo();
    test_math();
    test_memory();
    test_files(dir);
    test_dirs();
    test_time();
    test_misc();

    if (g_failed == 0)
        printf("libctest: %d/%d OK\n", g_total, g_total);
    else
        printf("libctest: %d of %d FAILED\n", g_failed, g_total);

    return g_failed ? 1 : 0;
}
