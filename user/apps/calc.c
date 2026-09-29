/*
 * calc - калькулятор (переехал из ядра в программу на этапе 6).
 *   calc 6 * (3 + 4)      - посчитать и выйти
 *   calc                  - спрашивать выражения, пустая строка - выход
 * Целые числа, + - * / %, скобки, унарный минус.
 */
#include "myos.h"

static const char *p;
static int err;

static void skip(void) { while (isspace(*p)) p++; }

static long expr(void);

static long factor(void)
{
    skip();

    if (*p == '(') {
        p++;
        long v = expr();
        skip();
        if (*p == ')') p++; else err = 1;
        return v;
    }

    if (*p == '-') { p++; return -factor(); }
    if (*p == '+') { p++; return factor(); }

    if (!isdigit(*p)) { err = 1; return 0; }

    long v = 0;
    while (isdigit(*p)) v = v * 10 + (*p++ - '0');
    return v;
}

static long term(void)
{
    long v = factor();

    for (;;) {
        skip();
        char op = *p;
        if (op != '*' && op != '/' && op != '%' && op != 'x' && op != 'X')
            return v;
        p++;
        long r = factor();
        if (op == '*' || op == 'x' || op == 'X') v *= r;
        else if (r == 0) { err = 2; return 0; }
        else if (op == '/') v /= r;
        else v %= r;
    }
}

static long expr(void)
{
    long v = term();

    for (;;) {
        skip();
        if (*p == '+') { p++; v += term(); }
        else if (*p == '-') { p++; v -= term(); }
        else return v;
    }
}

static void calc(const char *s)
{
    p = s;
    err = 0;

    long v = expr();

    skip();

    if (*p != '\0' && !err)
        err = 1;

    if (err == 2)
        printf("division by zero\n");
    else if (err)
        printf("cannot understand: %s\n", s);
    else
        printf("%ld\n", v);
}

int main(int argc, char **argv)
{
    char line[200];

    if (argc > 1) {
        line[0] = '\0';
        for (int i = 1; i < argc; i++) {
            if (strlen(line) + strlen(argv[i]) + 2 >= sizeof(line))
                break;
            strcat(line, argv[i]);
            strcat(line, " ");
        }
        calc(line);
        return 0;
    }

    printf("Calculator. Type an expression like 2*(3+4); an empty line exits.\n");

    for (;;) {
        printf("calc> ");
        if (!getline_in(line, sizeof(line)) || line[0] == '\0' || line[0] == 'q')
            break;
        calc(line);
    }

    return 0;
}
