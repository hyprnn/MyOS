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
 * С этапа 3 - по прерываниям: байт от клавиатуры = IRQ 1, от мыши
 * (второй порт контроллера) = IRQ 12; линии и их настройку берём
 * из ACPI (kx_irq_to_gsi), вектор - через I/O APIC. Бит 0 порта
 * 0x64 = "в порту 0x60 есть байт", бит 5 = "этот байт от мыши, а не
 * от клавиатуры". Опрос (ps2_poll) остаётся запасным путём.
 *
 * PS/2-мышь: у многих ноутбуков так подключён и тачпад (в режиме
 * совместимости он притворяется обычной мышью). Протокол - пакеты по
 * 3 байта (кнопки, dX, dY), у мыши с колесом - по 4.
 *
 * Коды - "Scan Code Set 1" (контроллер по умолчанию сам
 * переводит в него коды клавиатуры, бит 6 байта конфигурации
 * "translation"; мы проверяем, что он включён). Нажатие = код,
 * отпускание = код | 0x80; стрелки и т.п. идут с префиксом 0xE0.
 */

BOOLEAN g_ps2_present = FALSE;
BOOLEAN g_ps2_aux_present = FALSE;   /* PS/2-мышь / тачпад */
BOOLEAN g_ps2_aux_wheel = FALSE;
BOOLEAN g_ps2_irq = FALSE;           /* работаем по прерываниям */
UINT64  g_ps2_aux_packets = 0;
static UINT8 g_aux_pkt[4];
static UINTN g_aux_idx = 0;
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

static BOOLEAN ps2_cmd(UINT8 c)
{
    if (!ps2_wait_input_empty())
        return FALSE;
    io_out8(0x64, c);
    return TRUE;
}

static BOOLEAN ps2_read_config(UINT8 *cfg)
{
    if (!ps2_cmd(0x20) || !ps2_wait_output_full())
        return FALSE;
    *cfg = io_in8(0x60);
    return TRUE;
}

static void ps2_write_config(UINT8 cfg)
{
    if (ps2_cmd(0x60) && ps2_wait_input_empty())
        io_out8(0x60, cfg);
    g_ps2_config = cfg;
}

/* Ответ второго порта (мыши) - с таймаутом в мс; -1 = не ответила.
   Только до включения прерываний (иначе байт заберёт обработчик). */
static INTN ps2_aux_read(UINTN ms)
{
    UINT64 start = rdtsc();
    UINT64 lim = (g_tsc_hz / 1000u) * (UINT64)ms;

    while (rdtsc() - start < lim) {
        UINT8 st = io_in8(0x64);
        if (st & 0x01u) {
            UINT8 b = io_in8(0x60);
            if (st & 0x20u)
                return b;
            /* байт клавиатуры посреди настройки мыши - отдать ей */
            ps2_handle_byte(b);
        }
        cpu_pause();
    }

    return -1;
}

/* Команда мыши: 0xD4 ("следующий байт - второму порту") + байт;
   ждём подтверждения 0xFA */
static BOOLEAN ps2_aux_send(UINT8 b)
{
    if (!ps2_cmd(0xD4) || !ps2_wait_input_empty())
        return FALSE;

    io_out8(0x60, b);

    return ps2_aux_read(100) == 0xFA;
}

/* Найти и включить PS/2-мышь (тачпад). FALSE - её нет. */
static BOOLEAN ps2_aux_init(void)
{
    /* 0xA8 - включить второй порт; если он есть, бит 5 байта
       конфигурации ("часы второго порта выключены") станет 0 */
    UINT8 cfg;

    if (!ps2_cmd(0xA8) || !ps2_read_config(&cfg) || (cfg & 0x20u))
        return FALSE;

    /* сброс мыши: 0xFA, затем 0xAA (самотест пройден), 0x00 (ID) */
    if (!ps2_aux_send(0xFF))
        return FALSE;

    if (ps2_aux_read(800) != 0xAA)
        return FALSE;

    ps2_aux_read(50);               /* ID = 0 */

    /* "Волшебная" последовательность мыши с колесом (IntelliMouse):
       частота 200, 100, 80 - после неё ID становится 3, и пакеты -
       4 байта (четвёртый - колесо) */
    ps2_aux_send(0xF3); ps2_aux_send(200);
    ps2_aux_send(0xF3); ps2_aux_send(100);
    ps2_aux_send(0xF3); ps2_aux_send(80);

    if (ps2_aux_send(0xF2) && ps2_aux_read(50) == 3)
        g_ps2_aux_wheel = TRUE;

    ps2_aux_send(0xF3); ps2_aux_send(100);  /* 100 пакетов в секунду */

    /* включить поток пакетов */
    if (!ps2_aux_send(0xF4))
        return FALSE;

    g_aux_idx = 0;

    return TRUE;
}

/* Байт от PS/2-мыши: собрать пакет */
static void ps2_aux_byte(UINT8 b)
{
    /* у первого байта пакета бит 3 всегда 1 - если нет, мы
       рассинхронизировались: ждём настоящее начало */
    if (g_aux_idx == 0 && !(b & 0x08u))
        return;

    g_aux_pkt[g_aux_idx++] = b;

    if (g_aux_idx < (g_ps2_aux_wheel ? 4u : 3u))
        return;

    g_aux_idx = 0;

    UINT8 b0 = g_aux_pkt[0];

    if (b0 & 0xC0u)                 /* переполнение - пакет мусорный */
        return;

    INT32 dx = (INT32)g_aux_pkt[1] - ((b0 & 0x10u) ? 256 : 0);
    INT32 dy = (INT32)g_aux_pkt[2] - ((b0 & 0x20u) ? 256 : 0);

    g_kmouse_dx += dx;
    g_kmouse_dy -= dy;              /* у PS/2 "вверх" - плюс */

    if (g_ps2_aux_wheel)
        g_kmouse_dz -= (INT8)g_aux_pkt[3];

    g_kmouse_buttons = b0 & 0x07u;  /* левая, правая, средняя */
    g_kmouse_reports++;
    g_ps2_aux_packets++;
}

/* Забрать всё, что есть в контроллере, и разнести по адресатам.
   Только с запрещёнными прерываниями (в обработчике или под
   замком). */
void ps2_service(void)
{
    if (!g_ps2_present)
        return;

    for (UINTN i = 0; i < 32; i++) {

        UINT8 st = io_in8(0x64);

        if (!(st & 0x01u))
            break;

        UINT8 b = io_in8(0x60);

        if (st & 0x20u) {
            if (g_ps2_aux_present)
                ps2_aux_byte(b);
        } else {
            ps2_handle_byte(b);
        }
    }
}

static void ps2_irq(void)
{
    ps2_service();
}

void ps2_poll(void)
{
    ps2_service();
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

    UINT8 cfg = 0;
    BOOLEAN have_cfg = ps2_read_config(&cfg);

    g_ps2_config = cfg;

    if (have_cfg) {
        /* пока настраиваем - без прерываний от контроллера (биты 0,
           1); бит 6 - перевод в Set 1 (нужен), бит 4 = 0 - клавиатура
           включена */
        ps2_write_config((UINT8)((cfg | 0x40u) & ~0x13u));
    }

    /* 0xAE - включить клавиатурный порт */
    ps2_cmd(0xAE);

    g_ps2_present = TRUE;

    /* мышь / тачпад на втором порту */
    g_ps2_aux_present = have_cfg && ps2_aux_init();

    /* --- прерывания: IRQ 1 (клавиатура) и IRQ 12 (мышь) --- */
    BOOLEAN lvl, low;
    UINT32 gsi1 = kx_irq_to_gsi(1, &lvl, &low);
    BOOLEAN ok1 = kx_ioapic_route(gsi1, KX_VEC_PS2_KBD, lvl, low);
    BOOLEAN ok12 = FALSE;
    UINT32 gsi12 = 0;

    if (g_ps2_aux_present) {
        gsi12 = kx_irq_to_gsi(12, &lvl, &low);
        ok12 = kx_ioapic_route(gsi12, KX_VEC_PS2_AUX, lvl, low);
    }

    kx_irq_register(KX_VEC_PS2_KBD, ps2_irq);
    kx_irq_register(KX_VEC_PS2_AUX, ps2_irq);

    if (have_cfg && ps2_read_config(&cfg)) {
        if (ok1)
            cfg |= 0x01u;           /* прерывание от клавиатуры */
        if (ok12)
            cfg |= 0x02u;           /* прерывание от мыши */
        ps2_write_config(cfg);
    }

    g_ps2_irq = ok1;

    kprintf(out, "  PS/2 (i8042): present, config=0x%02x - keyboard on %s",
            g_ps2_config, ok1 ? "IRQ 1" : "polling");
    if (ok1)
        kprintf(out, " (GSI %u, vector 0x%02x)", gsi1, KX_VEC_PS2_KBD);
    print(out, "\n");

    if (g_ps2_aux_present)
        kprintf(out, "  PS/2 mouse/touchpad: found%s, IRQ 12 -> GSI %u%s\n",
                g_ps2_aux_wheel ? " (with wheel)" : "", gsi12,
                ok12 ? "" : " (NOT routed, polling)");
    else
        print(out, "  PS/2 mouse/touchpad: none (touchpads on new laptops are usually I2C)\n");

    /* забрать то, что успело прийти за настройку */
    ps2_service();
}

/* Огоньки PS/2-клавиатуры: команда 0xED + байт (бит 0 Scroll, 1 Num,
   2 Caps). Подтверждения 0xFA заберёт обработчик прерывания -
   ps2_handle_byte их игнорирует. */
void ps2_set_leds(UINT8 usb_bits)
{
    if (!g_ps2_present)
        return;

    UINT8 v = (UINT8)(((usb_bits & 4u) ? 1u : 0u) |     /* Scroll */
                      ((usb_bits & 1u) ? 2u : 0u) |     /* Num */
                      ((usb_bits & 2u) ? 4u : 0u));     /* Caps */

    if (!ps2_wait_input_empty())
        return;
    io_out8(0x60, 0xED);

    /* клавиатура должна ответить 0xFA, прежде чем принять байт;
       немного подождать (ответ может забрать и обработчик) */
    for (UINTN i = 0; i < 2000; i++)
        cpu_pause();

    if (!ps2_wait_input_empty())
        return;
    io_out8(0x60, v);
}
