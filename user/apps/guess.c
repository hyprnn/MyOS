/*
 * guess - игра "угадай число": программа загадывает число от 1 до
 * 100, ты угадываешь, она подсказывает "больше" или "меньше".
 */
#include "myos.h"

int main(void)
{
    char line[64];

    srand((unsigned)uptime_ms() * 2654435761u + (unsigned)getpid());

    int secret = (int)(rand() % 100u) + 1;
    int tries = 0;

    printf("I thought of a number from 1 to 100. Guess it! (empty line - give up)\n");

    for (;;) {
        printf("Your guess: ");
        if (!getline_in(line, sizeof(line)) || line[0] == '\0') {
            printf("It was %d. Bye!\n", secret);
            return 0;
        }

        int g = atoi(line);
        tries++;

        if (g < secret)
            printf("%d is too small.\n", g);
        else if (g > secret)
            printf("%d is too big.\n", g);
        else {
            printf("Yes! %d it is - you needed %d tries.\n", secret, tries);
            return 0;
        }
    }
}
