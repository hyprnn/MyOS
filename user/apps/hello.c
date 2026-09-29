/*
 * hello - первая программа MyOS в пользовательском режиме (ring 3).
 * Показывает, что она действительно в кольце 3, свой номер процесса,
 * аргументы, что работает malloc (память через sbrk) и часы ядра.
 */
#include "myos.h"

int main(int argc, char **argv)
{
    unsigned short cs;

    /* младшие 2 бита регистра CS - текущее кольцо процессора */
    __asm__ __volatile__("mov %%cs, %0" : "=r"(cs));

    printf("Hello from ring %d! I am program '%s', pid %d.\n", cs & 3, argv[0], getpid());

    if (argc > 1) {
        printf("My arguments:");
        for (int i = 1; i < argc; i++)
            printf(" [%s]", argv[i]);
        printf("\n");
    }

    char *m = malloc(1000);

    if (m) {
        strcpy(m, "malloc works");
        printf("%s: 1000 bytes at %p (my own heap)\n", m, m);
        free(m);
    }

    struct myos_time t;

    if (gettime(&t) == 0)
        printf("The kernel says it is %02u:%02u:%02u, %u ms since boot.\n",
               t.hour, t.minute, t.second, (unsigned)uptime_ms());

    return 0;
}
