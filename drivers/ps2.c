/*
 * drivers/ps2.c - PS/2-клавиатура (i8042).
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"


/* ================================================================
 * 5b. PS/2-клавиатура (контроллер i8042, порты 0x60/0x64)
 * ================================================================
 *
 * Встроенная клавиатура ноутбука почти всегда подключена не по
 * USB, а через встроенный контроллер (EC), который изображает
 * старый добрый i8042 - поэтому без этого драйвера на реальном
 * ноутбуке после ExitBootServices не работала бы как раз родная
 * клавиатура. QEMU (машина по умолчанию) тоже эмулирует i8042.
 *
 * Работаем опросом (прерывания IRQ1 не нужны): бит 0 порта
 * 0x64 = "в порту 0x60 есть байт", бит 5 = "этот байт от мыши
 * (второй порт), а не от клавиатуры".
 *
 * Коды - "Scan Code Set 1" (контроллер по умолчанию сам
 * переводит в него коды клавиатуры, бит 6 байта конфигурации
 * "translation"; мы проверяем, что он включён). Нажатие = код,
 * отпускание = код | 0x80; стрелки и т.п. идут с префиксом 0xE0.
 */

BOOLEAN g_ps2_present = FALSE;
BOOLEAN g_ps2_e0 = FALSE;
UINTN   g_ps2_skip = 0;       /* сколько байт Pause/Break
                                         ещё пропустить */
UINT64  g_ps2_bytes = 0;
UINT8   g_ps2_config = 0;

/* Set 1 (без префикса) -> HID Usage. 0 = нет/не нужна */
const UINT8 g_ps2_to_hid[0x59] = {
    /* 0x00 */ 0x00, 0x29, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
    /* 0x08 */ 0x24, 0x25, 0x26, 0x27, 0x2D, 0x2E, 0x2A, 0x2B,
    /* 0x10 */ 0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C,
    /* 0x18 */ 0x12, 0x13, 0x2F, 0x30, 0x28, 0xE0, 0x04, 0x16,
    /* 0x20 */ 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x33,
    /* 0x28 */ 0x34, 0x35, 0xE1, 0x31, 0x1D, 0x1B, 0x06, 0x19,
    /* 0x30 */ 0x05, 0x11, 0x10, 0x36, 0x37, 0x38, 0xE5, 0x55,
    /* 0x38 */ 0xE2, 0x2C, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
    /* 0x40 */ 0x3F, 0x40, 0x41, 0x42, 0x43, 0x53, 0x47, 0x5F,
    /* 0x48 */ 0x60, 0x61, 0x56, 0x5C, 0x5D, 0x5E, 0x57, 0x59,
    /* 0x50 */ 0x5A, 0x5B, 0x62, 0x63, 0x00, 0x00, 0x00, 0x44,
    /* 0x58 */ 0x45
};

/* Set 1 с префиксом 0xE0 -> HID Usage */
UINT8 ps2_e0_to_hid(UINT8 code)
{
    switch (code) {
    case 0x1C: return 0x58;   /* KP Enter */
    case 0x1D: return 0xE4;   /* RCtrl */
    case 0x35: return 0x54;   /* KP / */
    case 0x38: return 0xE6;   /* RAlt */
    case 0x47: return 0x4A;   /* Home */
    case 0x48: return 0x52;   /* Up */
    case 0x49: return 0x4B;   /* PgUp */
    case 0x4B: return 0x50;   /* Left */
    case 0x4D: return 0x4F;   /* Right */
    case 0x4F: return 0x4D;   /* End */
    case 0x50: return 0x51;   /* Down */
    case 0x51: return 0x4E;   /* PgDn */
    case 0x52: return 0x49;   /* Insert */
    case 0x53: return 0x4C;   /* Delete */
    default:   return 0x00;   /* в т.ч. "фальшивые" E0 2A/E0 AA */
    }
}


void ps2_handle_byte(UINT8 b)
{
    g_ps2_bytes++;

    if (g_ps2_skip > 0) {
        g_ps2_skip--;
        return;
    }

    if (b == 0xE0) {
        g_ps2_e0 = TRUE;
        return;
    }

    if (b == 0xE1) {
        /* Pause/Break: E1 1D 45 E1 9D C5 - ещё 5 байт, не
           клавиша для нас */
        g_ps2_skip = 5;
        return;
    }

    BOOLEAN released = (b & 0x80u) != 0;
    UINT8 code = (UINT8)(b & 0x7Fu);
    UINT8 u;

    if (g_ps2_e0) {
        u = ps2_e0_to_hid(code);
        g_ps2_e0 = FALSE;
    } else {
        u = (code < sizeof(g_ps2_to_hid)) ? g_ps2_to_hid[code] : 0;
    }

    if (u == 0)
        return;

    if (u >= 0xE0 && u <= 0xE7) {

        UINT8 bit = (UINT8)(1u << (u - 0xE0));

        if (released)
            g_kbd_ps2_mods = (UINT8)(g_kbd_ps2_mods & ~bit);
        else
            g_kbd_ps2_mods = (UINT8)(g_kbd_ps2_mods | bit);

        return;
    }

    if (!released)
        kbd_press_usage(u);
}


BOOLEAN ps2_wait_input_empty(void)
{
    for (UINTN i = 0; i < 100000; i++) {
        if (!(io_in8(0x64) & 0x02u))
            return TRUE;
        cpu_pause();
    }
    return FALSE;
}

BOOLEAN ps2_wait_output_full(void)
{
    for (UINTN i = 0; i < 100000; i++) {
        if (io_in8(0x64) & 0x01u)
            return TRUE;
        cpu_pause();
    }
    return FALSE;
}


void ps2_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT8 st = io_in8(0x64);

    if (st == 0xFF) {
        /* на "безлегасийных" машинах порт просто не отвечает */
        g_ps2_present = FALSE;
        print(out, "  PS/2 (i8042): not present\n");
        return;
    }

    /* выкинуть то, что осталось в буфере от прошивки */
    for (UINTN i = 0; i < 32 && (io_in8(0x64) & 0x01u); i++)
        (void)io_in8(0x60);

    /* прочитать байт конфигурации (команда 0x20) */
    BOOLEAN have_cfg = FALSE;

    if (ps2_wait_input_empty()) {

        io_out8(0x64, 0x20);

        if (ps2_wait_output_full()) {
            g_ps2_config = io_in8(0x60);
            have_cfg = TRUE;
        }
    }

    if (have_cfg) {

        /* бит 6 - перевод в Set 1 (нужен нам), бит 4 - 1 =
           клавиатурный порт ВЫКЛЮЧЕН (нужен 0) */
        UINT8 want = (UINT8)((g_ps2_config | 0x40u) & ~0x10u);

        if (want != g_ps2_config && ps2_wait_input_empty()) {

            io_out8(0x64, 0x60);

            if (ps2_wait_input_empty())
                io_out8(0x60, want);

            g_ps2_config = want;
        }
    }

    /* 0xAE - включить клавиатурный порт (на случай, если
       прошивка его выключила) */
    if (ps2_wait_input_empty())
        io_out8(0x64, 0xAE);

    g_ps2_present = TRUE;

    print(out, "  PS/2 (i8042): present, config=0x");
    print_hex(out, g_ps2_config, 2);
    print(out, have_cfg ? "" : " (could not read)");
    print(out, " - keyboard polled on ports 0x60/0x64\n");
}


void ps2_poll(void)
{
    if (!g_ps2_present)
        return;

    for (UINTN i = 0; i < 32; i++) {

        UINT8 st = io_in8(0x64);

        if (!(st & 0x01u))
            break;

        UINT8 b = io_in8(0x60);

        if (st & 0x20u)
            continue;        /* байт от PS/2-мыши - не наш */

        ps2_handle_byte(b);
    }
}
