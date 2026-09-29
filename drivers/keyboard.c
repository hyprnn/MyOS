/*
 * drivers/keyboard.c - очередь клавиш, перевод HID Usage в UEFI-клавиши, накопитель мыши.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

EFI_INPUT_KEY g_kbd_queue[KBD_QUEUE_SIZE];
UINTN g_kbd_q_head = 0;   /* откуда читать */
UINTN g_kbd_q_tail = 0;   /* куда писать */

BOOLEAN g_kbd_caps = FALSE;
UINT8   g_kbd_layout = 0;      /* 0 - английская, 1 - русская (ЙЦУКЕН) */
BOOLEAN g_kbd_num = TRUE;       /* NumLock: цифровой блок - цифры */
BOOLEAN g_kbd_scroll = FALSE;

/* Огоньки клавиатуры в формате USB: бит 0 Num, 1 Caps, 2 Scroll */
UINT8 kbd_led_bits(void)
{
    return (UINT8)((g_kbd_num ? 1u : 0u) | (g_kbd_caps ? 2u : 0u) |
                   (g_kbd_scroll ? 4u : 0u));
}
UINT8   g_kbd_usb_mods = 0;   /* байт модификаторов из
                                         последнего USB-отчёта */
UINT8   g_kbd_ps2_mods = 0;   /* то же, собранное из PS/2
                                         make/break-кодов, в том же
                                         формате битов */
UINT64  g_kbd_keys_total = 0;

/* Автоповтор (typematic): у USB-клавиатуры его нет в "железе",
   его обязан делать сам хост. У PS/2 - есть, там не нужен. */
UINT8  g_kbd_rep_usage = 0;
UINT64 g_kbd_rep_next_tsc = 0;


void kbd_enqueue(UINT16 scan, CHAR16 uc)
{
    UINTN next = (g_kbd_q_tail + 1u) % KBD_QUEUE_SIZE;

    if (next == g_kbd_q_head)
        return;              /* очередь полна - клавишу теряем */

    g_kbd_queue[g_kbd_q_tail].ScanCode = scan;
    g_kbd_queue[g_kbd_q_tail].UnicodeChar = uc;
    g_kbd_q_tail = next;

    g_kbd_keys_total++;
}

BOOLEAN kbd_dequeue(EFI_INPUT_KEY *key)
{
    if (g_kbd_q_head == g_kbd_q_tail)
        return FALSE;

    key->ScanCode = g_kbd_queue[g_kbd_q_head].ScanCode;
    key->UnicodeChar = g_kbd_queue[g_kbd_q_head].UnicodeChar;
    g_kbd_q_head = (g_kbd_q_head + 1u) % KBD_QUEUE_SIZE;

    return TRUE;
}


/*
 * Символы для HID Usage 0x1E..0x38 (цифровой ряд и знаки):
 * [0] - без Shift, [1] - с Shift. 0 = "не символ" (Enter/Esc/
 * Backspace/Tab обрабатываются отдельно).
 */
const char g_kbd_sym[0x39 - 0x1E][2] = {
    { '1', '!' }, { '2', '@' }, { '3', '#' }, { '4', '$' },
    { '5', '%' }, { '6', '^' }, { '7', '&' }, { '8', '*' },
    { '9', '(' }, { '0', ')' },
    {  0,   0  },   /* 0x28 Enter */
    {  0,   0  },   /* 0x29 Esc */
    {  0,   0  },   /* 0x2A Backspace */
    {  0,   0  },   /* 0x2B Tab */
    { ' ', ' ' },   /* 0x2C Space */
    { '-', '_' }, { '=', '+' }, { '[', '{' }, { ']', '}' },
    { '\\', '|' },
    { '#', '~' },   /* 0x32 - "Non-US #" (есть на европейских
                       клавиатурах) */
    { ';', ':' }, { '\'', '"' }, { '`', '~' }, { ',', '<' },
    { '.', '>' }, { '/', '?' }
};


/* Модификаторы в формате HID: бит0 LCtrl, 1 LShift, 2 LAlt,
   3 LGUI, 4 RCtrl, 5 RShift, 6 RAlt, 7 RGUI */
BOOLEAN kbd_shift_down(void)
{
    UINT8 m = (UINT8)(g_kbd_usb_mods | g_kbd_ps2_mods);

    return (m & 0x22u) != 0;
}


/* Главное: одно нажатие клавиши (HID Usage) -> EFI_INPUT_KEY
   в очередь */
/*
 * Русская раскладка ЙЦУКЕН: по HID Usage клавиши (её "латинскому"
 * знаку) - строчная кириллическая буква (U+0430..) или Ё (U+0451).
 * 0 - у этой клавиши в русской раскладке буквы нет.
 */
static CHAR16 cyr_letter(UINT8 u)
{
    /* буквы a..z (usage 0x04..0x1D) -> кириллица ЙЦУКЕН */
    static const CHAR16 ltr[26] = {
        /* a */ 0x444,/* b */ 0x438,/* c */ 0x441,/* d */ 0x432,/* e */ 0x443,
        /* f */ 0x430,/* g */ 0x43F,/* h */ 0x440,/* i */ 0x448,/* j */ 0x43E,
        /* k */ 0x43B,/* l */ 0x434,/* m */ 0x44C,/* n */ 0x442,/* o */ 0x449,
        /* p */ 0x437,/* q */ 0x439,/* r */ 0x43A,/* s */ 0x44B,/* t */ 0x435,
        /* u */ 0x433,/* v */ 0x43C,/* w */ 0x446,/* x */ 0x447,/* y */ 0x43D,
        /* z */ 0x44F
    };

    if (u >= 0x04 && u <= 0x1D)
        return ltr[u - 0x04];

    switch (u) {
    case 0x33: return 0x436;   /* ; -> ж */
    case 0x34: return 0x44D;   /* ' -> э */
    case 0x2F: return 0x445;   /* [ -> х */
    case 0x30: return 0x44A;   /* ] -> ъ */
    case 0x36: return 0x431;   /* , -> б */
    case 0x37: return 0x44E;   /* . -> ю */
    case 0x35: return 0x451;   /* ` -> ё */
    default:   return 0;
    }
}

void kbd_press_usage(UINT8 u)
{
    BOOLEAN shift = kbd_shift_down();

    /* Ctrl+Space - переключить раскладку EN <-> RU */
    {
        UINT8 mods = (UINT8)(g_kbd_usb_mods | g_kbd_ps2_mods);
        if ((mods & 0x11u) && u == 0x2C) {
            g_kbd_layout ^= 1u;
            return;
        }
    }

    /* русская раскладка: буква/знак -> кириллица (кроме Ctrl-сочетаний) */
    {
        UINT8 mods = (UINT8)(g_kbd_usb_mods | g_kbd_ps2_mods);
        if (g_kbd_layout == 1u && !(mods & 0x11u)) {
            CHAR16 c = cyr_letter(u);
            if (c != 0) {
                BOOLEAN up = shift ? !g_kbd_caps : g_kbd_caps;
                /* заглавные: ё(0x451)->Ё(0x401); а(0x430..)->А(0x410..) */
                if (up)
                    c = (c == 0x451) ? 0x401 : (CHAR16)(c - 0x20);
                kbd_enqueue(0, c);
                return;
            }
        }
    }

    if (u >= 0x04 && u <= 0x1D) {

        /* Ctrl+C - остановить программу, которая сейчас работает
           (этап 6); если программы нет - обычный символ 3 */
        UINT8 mods = (UINT8)(g_kbd_usb_mods | g_kbd_ps2_mods);

        if ((mods & 0x11u) && u == 0x06) {
            if (g_fg_proc != NULL) {
                proc_ctrl_c();
                return;
            }
            kbd_enqueue(0, 3);
            return;
        }

        /* буквы a..z */
        BOOLEAN upper = shift ? !g_kbd_caps : g_kbd_caps;
        CHAR16 c = (CHAR16)((upper ? 'A' : 'a') + (u - 0x04));

        kbd_enqueue(0, c);
        return;
    }

    if (u == 0x28 || u == 0x58) {           /* Enter, KP Enter */
        kbd_enqueue(0, CHAR_CARRIAGE_RETURN);
        return;
    }

    if (u == 0x29) {                        /* Esc */
        kbd_enqueue(0x17, 0);
        return;
    }

    if (u == 0x2A) {                        /* Backspace */
        kbd_enqueue(0, CHAR_BACKSPACE);
        return;
    }

    if (u == 0x2B) {                        /* Tab */
        kbd_enqueue(0, 0x0009);
        return;
    }

    if (u >= 0x1E && u <= 0x38) {

        char c = g_kbd_sym[u - 0x1E][shift ? 1 : 0];

        if (c != 0)
            kbd_enqueue(0, (CHAR16)(unsigned char)c);

        return;
    }

    if (u == 0x39) {                        /* Caps Lock */
        g_kbd_caps = !g_kbd_caps;
        g_kbd_leds_dirty = TRUE;             /* зажечь огонёк (usbhid.c) */
        return;
    }

    if (u == 0x53) {                        /* Num Lock */
        g_kbd_num = !g_kbd_num;
        g_kbd_leds_dirty = TRUE;
        return;
    }

    if (u == 0x47) {                        /* Scroll Lock */
        g_kbd_scroll = !g_kbd_scroll;
        g_kbd_leds_dirty = TRUE;
        return;
    }

    if (u >= 0x3A && u <= 0x43) {           /* F1..F10 */
        kbd_enqueue((UINT16)(0x0B + (u - 0x3A)), 0);
        return;
    }

    if (u == 0x44) { kbd_enqueue(0x15, 0); return; }   /* F11 */
    if (u == 0x45) { kbd_enqueue(0x16, 0); return; }   /* F12 */
    if (u == 0x49) { kbd_enqueue(0x07, 0); return; }   /* Insert */
    if (u == 0x4A) { kbd_enqueue(0x05, 0); return; }   /* Home */
    if (u == 0x4B) { kbd_enqueue(0x09, 0); return; }   /* PgUp */
    if (u == 0x4C) { kbd_enqueue(0x08, 0); return; }   /* Delete */
    if (u == 0x4D) { kbd_enqueue(0x06, 0); return; }   /* End */
    if (u == 0x4E) { kbd_enqueue(0x0A, 0); return; }   /* PgDn */
    if (u == 0x4F) { kbd_enqueue(0x03, 0); return; }   /* Right */
    if (u == 0x50) { kbd_enqueue(0x04, 0); return; }   /* Left */
    if (u == 0x51) { kbd_enqueue(0x02, 0); return; }   /* Down */
    if (u == 0x52) { kbd_enqueue(0x01, 0); return; }   /* Up */

    /* цифровой блок при выключенном NumLock - стрелки и т.п.
       (как на любой PC-клавиатуре) */
    if (!g_kbd_num && u >= 0x59 && u <= 0x63) {
        static const UINT16 nav[11] = {
            0x06, 0x02, 0x0A, 0x04, 0x00, 0x03, 0x05, 0x01, 0x09, 0x07, 0x08
        };  /* 1 End, 2 Down, 3 PgDn, 4 Left, 5 -, 6 Right, 7 Home,
               8 Up, 9 PgUp, 0 Ins, . Del */
        if (nav[u - 0x59] != 0)
            kbd_enqueue(nav[u - 0x59], 0);
        return;
    }

    /* цифровой блок (NumLock включён) */
    if (u == 0x54) { kbd_enqueue(0, '/'); return; }
    if (u == 0x55) { kbd_enqueue(0, '*'); return; }
    if (u == 0x56) { kbd_enqueue(0, '-'); return; }
    if (u == 0x57) { kbd_enqueue(0, '+'); return; }

    if (u >= 0x59 && u <= 0x61) {
        kbd_enqueue(0, (CHAR16)('1' + (u - 0x59)));
        return;
    }

    if (u == 0x62) { kbd_enqueue(0, '0'); return; }
    if (u == 0x63) { kbd_enqueue(0, '.'); return; }

    /* всё остальное (мультимедиа, модификаторы и т.п.) -
       не символы, игнорируем */
}


/* Какие клавиши НЕ повторяются при удержании */
BOOLEAN kbd_usage_repeats(UINT8 u)
{
    if (u == 0x39 || u == 0x53 || u == 0x47)   /* Caps/Num/Scroll Lock */
        return FALSE;

    if (u >= 0xE0)                 /* модификаторы */
        return FALSE;

    return TRUE;
}


/*
 * Разбор boot-отчёта USB-клавиатуры (8 байт):
 *   байт 0 - модификаторы (биты как в kbd_shift_down)
 *   байт 1 - зарезервирован
 *   байты 2..7 - коды (HID Usage) до шести ОДНОВРЕМЕННО
 *                зажатых клавиш, 0 = пусто
 * Это СОСТОЯНИЕ, а не событие "нажата": клавиша держится -
 * она есть в каждом отчёте. Поэтому нажатие = код есть в новом
 * отчёте, но не было в предыдущем.
 */
void kbd_usb_boot_report(
    volatile UINT8 *rep,
    UINTN len,
    UINT8 prev[6]
)
{
    if (len < 3)
        return;

    UINT8 cur[6];

    for (UINTN i = 0; i < 6; i++)
        cur[i] = (2u + i < len) ? rep[2 + i] : 0;

    /* "Phantom state" (ErrorRollOver): зажато слишком много
       клавиш сразу, все слоты = 0x01 - отчёт не информативен */
    if (cur[0] == 0x01)
        return;

    g_kbd_usb_mods = rep[0];

    for (UINTN i = 0; i < 6; i++) {

        UINT8 u = cur[i];

        if (u < 0x04)
            continue;

        BOOLEAN was_down = FALSE;

        for (UINTN j = 0; j < 6; j++) {
            if (prev[j] == u)
                was_down = TRUE;
        }

        if (!was_down) {

            kbd_press_usage(u);

            if (kbd_usage_repeats(u)) {

                g_kbd_rep_usage = u;
                g_kbd_rep_next_tsc =
                    rdtsc() +
                    (g_tsc_hz / 1000u) * KBD_REPEAT_DELAY_MS;
            }
        }
    }

    /* клавиша автоповтора отпущена? */
    if (g_kbd_rep_usage != 0) {

        BOOLEAN still = FALSE;

        for (UINTN i = 0; i < 6; i++) {
            if (cur[i] == g_kbd_rep_usage)
                still = TRUE;
        }

        if (!still)
            g_kbd_rep_usage = 0;
    }

    for (UINTN i = 0; i < 6; i++)
        prev[i] = cur[i];
}


void kbd_repeat_tick(void)
{
    if (g_kbd_rep_usage == 0 || g_tsc_hz == 0)
        return;

    UINT64 now = rdtsc();

    if (now < g_kbd_rep_next_tsc)
        return;

    kbd_press_usage(g_kbd_rep_usage);

    UINT64 step = (g_tsc_hz / 1000u) * KBD_REPEAT_RATE_MS;

    g_kbd_rep_next_tsc += step;

    /* если долго не опрашивали - не выдаём пачку повторов
       разом, а начинаем отсчёт заново от "сейчас" */
    if (g_kbd_rep_next_tsc < now)
        g_kbd_rep_next_tsc = now + step;
}


/* ================================================================
 * 5c. Мышь: накопитель движения между опросами
 * ================================================================
 *
 * Драйвер складывает сюда dX/dY/колесо из каждого отчёта, а
 * наш EFI_SIMPLE_POINTER_PROTOCOL->GetState (которым пользуется
 * GUI) отдаёт накопленное с прошлого вызова и обнуляет - ровно
 * так же, как это делал драйвер мыши прошивки.
 */
INT64  g_kmouse_dx = 0;
INT64  g_kmouse_dy = 0;
INT64  g_kmouse_dz = 0;
UINT32 g_kmouse_buttons = 0;
BOOLEAN g_kmouse_present = FALSE;
UINT64 g_kmouse_reports = 0;
