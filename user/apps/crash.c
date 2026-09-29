#pragma GCC diagnostic ignored "-Winfinite-recursion"
/*
 * crash - нарочно сломанная программа (проверка защиты, этап 6).
 * На этапе 1 "crash" в ядре останавливал всю машину экраном паники.
 * Здесь ломается ПРОГРАММА - ядро пишет, что случилось, и работает
 * дальше.
 *   crash         - чтение по адресу 0 (NULL)
 *   crash write   - запись в свой же код (он только для чтения)
 *   crash kernel  - чтение памяти ядра (у программы нет доступа)
 *   crash cli     - привилегированная инструкция (запрет прерываний)
 *   crash div     - деление на ноль
 *   crash stack   - бесконечная рекурсия (переполнение стека)
 *   crash loop    - вечный цикл: остановить Ctrl+C
 */
#include "myos.h"

static int deep(int n)
{
    volatile char pad[256];
    pad[0] = (char)n;
    return deep(n + 1) + pad[0];
}

int main(int argc, char **argv)
{
    const char *how = (argc > 1) ? argv[1] : "null";

    printf("crash: about to do '%s' on purpose...\n", how);

    if (strcmp(how, "write") == 0) {
        *(volatile unsigned char *)(void *)main = 0xC3;
    } else if (strcmp(how, "kernel") == 0) {
        volatile unsigned long x = *(volatile unsigned long *)0xFFFFFFFF80000000ull;
        (void)x;
    } else if (strcmp(how, "cli") == 0) {
        __asm__ __volatile__("cli");
    } else if (strcmp(how, "div") == 0) {
        volatile int z = 0;
        volatile int r = 100 / z;
        (void)r;
    } else if (strcmp(how, "stack") == 0) {
        deep(0);
    } else if (strcmp(how, "loop") == 0) {
        printf("spinning forever - press Ctrl+C\n");
        for (;;) { }
    } else {
        volatile int *p = (volatile int *)0;
        printf("%d\n", *p);
    }

    printf("crash: ...nothing happened?!\n");
    return 0;
}
