/*
 * lib/serial.c - отладочный лог в последовательный порт COM1.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * ЗАЧЕМ: до сих пор единственным способом увидеть, что происходит
 * внутри ОС, был экран - а значит, скриншоты и фото. Последовательный
 * порт (UART 16550, порт 0x3F8) - самый старый и самый простой
 * способ ОС "говорить наружу": в QEMU с опцией `-serial stdio`
 * всё, что ОС пишет в COM1, появляется прямо в терминале, откуда
 * его можно скопировать текстом (или разобрать скриптом - так
 * работает tools/autotest.py).
 *
 * После ExitBootServices сюда дублируется весь вывод консоли. В
 * режиме прошивки - не дублируется: OVMF сама выводит свою консоль
 * в COM1, и строки удваивались бы.
 *
 * У ноутбуков COM-порта обычно нет. Это определяется при
 * инициализации (тест "петли", loopback): порт, который не
 * возвращает записанный байт, считается отсутствующим, и все
 * функции ниже тихо ничего не делают.
 */
#include "myos.h"

#define COM1 0x3F8

BOOLEAN g_serial_ok = FALSE;

BOOLEAN serial_init(void)
{
    io_out8(COM1 + 1, 0x00);    /* выключить прерывания UART */
    io_out8(COM1 + 3, 0x80);    /* DLAB=1: дальше - делитель частоты */
    io_out8(COM1 + 0, 0x01);    /* делитель 1 = 115200 бод */
    io_out8(COM1 + 1, 0x00);
    io_out8(COM1 + 3, 0x03);    /* 8 бит, без чётности, 1 стоп-бит */
    io_out8(COM1 + 2, 0xC7);    /* FIFO включить и очистить */

    /* Тест петли: MCR бит 4 замыкает выход UART на вход -
       записанный байт должен тут же прочитаться обратно */
    io_out8(COM1 + 4, 0x1E);
    io_out8(COM1 + 0, 0xAE);

    if (io_in8(COM1 + 0) != 0xAE) {
        g_serial_ok = FALSE;
        return FALSE;
    }

    io_out8(COM1 + 4, 0x0F);    /* обычный режим, DTR/RTS */

    g_serial_ok = TRUE;
    return TRUE;
}

void serial_putc(char c)
{
    if (!g_serial_ok)
        return;

    /* ждать, пока передатчик освободится (LSR бит 5), но не вечно */
    for (UINTN i = 0; i < 100000; i++) {
        if (io_in8(COM1 + 5) & 0x20)
            break;
    }

    io_out8(COM1, (UINT8)c);
}

/* Строка выводится целиком под спин-замком: иначе строки разных
   потоков (шелл, usb) перемешивались бы по буквам */
static KSPINLOCK g_serial_lock = KSPINLOCK_INIT;

void serial_puts(const char *s)
{
    if (!g_serial_ok)
        return;

    UINT64 fl = kspin_lock(&g_serial_lock);

    while (*s) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s);
        s++;
    }

    kspin_unlock(&g_serial_lock, fl);
}

/* CHAR16-строка: всё, что вне ASCII, - как '?' */
void serial_puts16(const CHAR16 *s)
{
    if (s == NULL || !g_serial_ok)
        return;

    UINT64 fl = kspin_lock(&g_serial_lock);

    while (*s) {
        CHAR16 c = *s;

        if (c == L'\n')
            serial_putc('\r');

        serial_putc((c < 128) ? (char)c : '?');
        s++;
    }

    kspin_unlock(&g_serial_lock, fl);
}
