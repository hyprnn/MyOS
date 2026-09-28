#include "efi.h"

/*
 * memcpy/memmove/memset/memcmp. Сами мы их нигде не зовём, но с
 * оптимизацией (-O2 в build.sh) компилятор вправе САМ вставить их
 * вызовы - например, для копирования большой структуры или вместо
 * цикла обнуления массива. libc у нас нет, поэтому без этих
 * определений линковка бы не прошла. hidden - чтобы вызовы шли
 * напрямую, без таблиц динамической линковки, которых в UEFI-
 * образе нет.
 */
typedef __SIZE_TYPE__ myos_size_t;

__attribute__((used, visibility("hidden")))
void *memcpy(void *dst, const void *src, myos_size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    for (myos_size_t i = 0; i < n; i++)
        d[i] = s[i];

    return dst;
}

__attribute__((used, visibility("hidden")))
void *memmove(void *dst, const void *src, myos_size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s) {
        for (myos_size_t i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        for (myos_size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }

    return dst;
}

__attribute__((used, visibility("hidden")))
void *memset(void *dst, int value, myos_size_t n)
{
    unsigned char *d = (unsigned char *)dst;

    for (myos_size_t i = 0; i < n; i++)
        d[i] = (unsigned char)value;

    return dst;
}

__attribute__((used, visibility("hidden")))
int memcmp(const void *a, const void *b, myos_size_t n)
{
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;

    for (myos_size_t i = 0; i < n; i++) {
        if (x[i] != y[i])
            return (int)x[i] - (int)y[i];
    }

    return 0;
}

#define LINE_MAX 128

/* Текущий цвет текста */
static UINTN g_color = 0x0F;

/*
 * Атрибут (цвет), который реально действует
 * на экране ПРЯМО СЕЙЧАС — обновляется в
 * set_color() при каждом вызове SetAttribute.
 *
 * В отличие от g_color (который меняется только
 * командой "color"), это отражает и временные
 * перекраски (лого, заголовки fetch, ошибки и т.д.),
 * поэтому именно g_current_attr пишется в scrollback
 * вместе с каждым символом.
 */
static UINTN g_current_attr = 0x0F;

/* Время загрузки */
static EFI_TIME g_boot_time;
static BOOLEAN  g_have_boot_time = FALSE;

/*
 * Хэндл образа, полученный efi_main(). Нужен позже для
 * ExitBootServices(ImageHandle, MapKey) - без него прошивка
 * не сможет проверить, что выход из Boot Services запрашивает
 * именно наше приложение, поэтому храним его глобально, а не
 * просто отбрасываем, как раньше.
 */
static EFI_HANDLE g_image_handle = NULL;

/*
 * TRUE после команды "ebs": прошивки больше нет, ОС работает на
 * собственных драйверах (см. большой блок KERNEL MODE ближе к
 * концу файла). Объявлено здесь, в начале, потому что на него
 * смотрят и более ранние команды (например, fetch).
 */
static BOOLEAN g_kernel_mode = FALSE;


/* ============================================================
 * RAM filesystem
 * ============================================================ */

#define FS_MAX_FILES   32
#define FS_NAME_MAX    32
#define FS_DATA_MAX    2048

typedef struct {
    BOOLEAN used;
    CHAR16  name[FS_NAME_MAX];
    CHAR16  data[FS_DATA_MAX];
    UINTN   size;
} FS_FILE;

static FS_FILE g_fs[FS_MAX_FILES];


/* ============================================================
 * Command history
 * ============================================================ */

#define HIST_MAX 16

static CHAR16 g_history[HIST_MAX][LINE_MAX];
static UINTN  g_history_count = 0;


/*
 * Обёртка над SetAttribute(), которая ещё и
 * запоминает текущий цвет в g_current_attr,
 * чтобы scrollback знал, каким цветом был
 * напечатан каждый символ.
 */
static void set_color(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINTN attr
)
{
    g_current_attr = attr;
    out->SetAttribute(out, attr);
}


/* ============================================================
 * Terminal scrollback
 *
 * TERMINAL_ROWS = физическая высота обычного UEFI
 * text mode. Обычно OVMF использует 80x25.
 *
 * Последняя строка оставляется под prompt.
 * ============================================================ */

#define SCROLLBACK_MAX_LINES     256
#define SCROLLBACK_LINE_MAX      128
#define TERMINAL_ROWS            25

/*
 * Сколько строк истории помещается на экран при прокрутке
 * (PageUp/PageDown). В режиме прошивки консоль 80x25 -> 24 строки
 * (последняя под prompt). После "ebs" консоль своя, и строк в ней
 * больше (зависит от разрешения) - kernel mode выставляет это
 * значение заново. Поэтому переменная, а не константа.
 */
static UINTN g_scrollback_visible_rows = 24;

#define SCROLLBACK_VISIBLE_ROWS  ((int)g_scrollback_visible_rows)

static CHAR16 g_scrollback[
    SCROLLBACK_MAX_LINES
][SCROLLBACK_LINE_MAX];

/*
 * Цвет (атрибут) каждого символа g_scrollback,
 * индексы синхронизированы 1-в-1.
 */
static UINT8 g_scrollback_attr[
    SCROLLBACK_MAX_LINES
][SCROLLBACK_LINE_MAX];

static UINTN g_scrollback_count = 0;
static UINTN g_scrollback_line_len = 0;

/*
 * 0 = самый низ
 * 1 = на один экран вверх
 * 2 = ещё выше
 */
static int g_scrollback_view = 0;

/*
 * TRUE, когда мы просто перерисовываем
 * уже существующий scrollback.
 *
 * В этот момент новые символы в историю
 * записывать нельзя.
 */
static BOOLEAN g_scrollback_replaying = FALSE;


/* ============================================================
 * Scrollback internals
 * ============================================================ */

static void scrollback_newline(void)
{
    UINTN index =
        g_scrollback_count % SCROLLBACK_MAX_LINES;

    g_scrollback[index][g_scrollback_line_len] = 0;

    g_scrollback_count++;
    g_scrollback_line_len = 0;
}


static void scrollback_char(CHAR16 c)
{
    if (g_scrollback_replaying)
        return;

    if (c == L'\r')
        return;

    if (c == L'\n') {
        scrollback_newline();
        return;
    }

    /*
     * Управляющие символы не записываем.
     */
    if (c < 32)
        return;

    if (g_scrollback_line_len <
        SCROLLBACK_LINE_MAX - 1) {

        UINTN index =
            g_scrollback_count %
            SCROLLBACK_MAX_LINES;

        g_scrollback[index][g_scrollback_line_len] = c;

        g_scrollback_attr[index][g_scrollback_line_len] =
            (UINT8)g_current_attr;

        g_scrollback_line_len++;
    }
}


/*
 * Перерисовать prompt + текущую строку ввода.
 *
 * ВАЖНО:
 * здесь НЕ используется print(), иначе prompt
 * снова попадёт в scrollback.
 */
static void redraw_input(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const CHAR16 *line
)
{
    CHAR16 prompt[] = L"> ";

    out->OutputString(out, prompt);

    if (line)
        out->OutputString(out, (CHAR16 *)line);
}


/* ============================================================
 * Scrollback renderer
 *
 * view = 0:
 *     последние строки
 *
 * view > 0:
 *     страницы выше
 * ============================================================ */

static void scrollback_render(
    EFI_SYSTEM_TABLE *st,
    int view
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (g_scrollback_count == 0)
        return;

    /*
     * Сколько строк реально хранится.
     */
    UINTN total =
        (g_scrollback_count < SCROLLBACK_MAX_LINES)
        ? g_scrollback_count
        : SCROLLBACK_MAX_LINES;

    /*
     * Сколько строк истории помещается,
     * потому что последнюю строку оставляем
     * под prompt.
     */
    UINTN visible =
        SCROLLBACK_VISIBLE_ROWS;

    int max_view;

    if (total > visible)
        max_view =
            (int)total - (int)visible;
    else
        max_view = 0;

    if (view < 0)
        view = 0;

    if (view > max_view)
        view = max_view;

    g_scrollback_view = view;


    /*
     * Самая старая строка ring-buffer.
     */
    UINTN oldest;

    if (g_scrollback_count <=
        SCROLLBACK_MAX_LINES) {

        oldest = 0;

    } else {

        oldest =
            g_scrollback_count %
            SCROLLBACK_MAX_LINES;
    }


    /*
     * Вычисляем начало страницы ОТ КОНЦА.
     *
     * view = 0:
     *
     *     [........][последние 24 строки]
     *
     * view = 1:
     *
     *     [.......][24 строки перед последними]
     */
    int start_offset =
        (int)total -
        (int)visible -
        view;

    if (start_offset < 0)
        start_offset = 0;


    UINTN start =
        (oldest + (UINTN)start_offset)
        % SCROLLBACK_MAX_LINES;


    /*
     * Теперь выводим историю.
     */
    g_scrollback_replaying = TRUE;

    out->ClearScreen(out);

    /*
     * ClearScreen() не гарантирует сохранение
     * текущего атрибута текста на всех
     * реализациях UEFI, поэтому явно
     * восстанавливаем цвет перед отрисовкой
     * истории — иначе при прокрутке вверх
     * текст мог перекраситься в цвет по
     * умолчанию.
     */
    set_color(
        out,
        g_color
    );


    for (UINTN row = 0;
         row < visible;
         row++) {

        UINTN logical =
            (UINTN)start_offset + row;

        if (logical >= total)
            break;


        UINTN index =
            (start + row)
            % SCROLLBACK_MAX_LINES;


        /*
         * Выводим строку посимвольно, меняя
         * атрибут при каждой смене цвета —
         * так сохраняется исходная раскраска
         * (лого, fetch, ошибки и т.д.) даже
         * при прокрутке вверх.
         */
        UINTN line_len = 0;

        while (line_len < SCROLLBACK_LINE_MAX &&
               g_scrollback[index][line_len] != 0)
            line_len++;

        /* заведомо невозможное значение атрибута,
         * чтобы первый символ строки гарантированно
         * выставил цвет */
        UINTN cur_attr = 0xFFFF;

        for (UINTN col = 0;
             col < line_len;
             col++) {

            UINTN attr =
                g_scrollback_attr[index][col];

            if (attr != cur_attr) {
                out->SetAttribute(
                    out,
                    attr
                );
                cur_attr = attr;
            }

            CHAR16 ch[2];

            ch[0] = g_scrollback[index][col];
            ch[1] = 0;

            out->OutputString(
                out,
                ch
            );
        }


        /*
         * UEFI лучше явно получать CRLF.
         */
        CHAR16 nl[2];

        nl[0] = L'\r';
        nl[1] = 0;

        out->OutputString(out, nl);

        nl[0] = L'\n';

        out->OutputString(out, nl);
    }


    /*
     * Возвращаем "живой" цвет — тот, которым
     * реально печатает терминал сейчас, — чтобы
     * промпт после выхода из scrollback рисовался
     * правильно, а не последним цветом истории.
     */
    out->SetAttribute(
        out,
        g_current_attr
    );

    g_scrollback_replaying = FALSE;
}


/* ============================================================
 * Output helpers
 * ============================================================ */

static void print(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const char *s
)
{
    CHAR16 buf[2];

    buf[1] = 0;

    while (*s) {

        if (*s == '\n') {

            scrollback_char(L'\n');

            buf[0] = L'\r';
            out->OutputString(out, buf);

            buf[0] = L'\n';
            out->OutputString(out, buf);

        } else {

            CHAR16 c =
                (CHAR16)(unsigned char)*s;

            scrollback_char(c);

            buf[0] = c;

            out->OutputString(
                out,
                buf
            );
        }

        s++;
    }

    /*
     * Если мы не перерисовываем scrollback,
     * новый вывод возвращает нас вниз.
     */
    if (!g_scrollback_replaying)
        g_scrollback_view = 0;
}


/*
 * Вывод CHAR16 строки.
 *
 * Нужен для:
 *   - UEFI строк
 *   - файлов
 *   - пользовательского ввода
 *   - FirmwareVendor
 */
static void print16(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const CHAR16 *s
)
{
    if (!s) {
        print(out, "?");
        return;
    }

    while (*s) {

        scrollback_char(*s);

        CHAR16 buf[2];

        buf[0] = *s;
        buf[1] = 0;

        out->OutputString(
            out,
            buf
        );

        s++;
    }

    if (!g_scrollback_replaying)
        g_scrollback_view = 0;
}


/*
 * Вывод беззнакового числа в десятичном виде.
 *
 * ВАЖНО: раньше эта функция вызывалась по всему файлу
 * (cmd_ls, cmd_touch, cmd_fetch, calc, ...), но нигде
 * не была определена - это ошибка компиляции
 * (implicit declaration of function). Добавлена здесь,
 * как можно раньше, чтобы быть видимой для всех
 * последующих вызовов в файле.
 */
static void print_uint(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 value
)
{
    CHAR16 digits[21];
    CHAR16 buf[21];
    UINTN  n = 0;

    if (value == 0) {
        print(out, "0");
        return;
    }

    while (value > 0 && n < 20) {
        digits[n++] = (CHAR16)(L'0' + (value % 10));
        value /= 10;
    }

    for (UINTN i = 0; i < n; i++)
        buf[i] = digits[n - 1 - i];

    buf[n] = 0;

    print16(out, buf);
}


/*
 * Печать знакового числа (для дельт мыши dX/dY из HID-отчётов -
 * они приходят как знаковый байт, движение "влево"/"вверх"
 * должно печататься со знаком минус, а не как огромное
 * беззнаковое число). Просто выводит '-' при отрицательном
 * значении и дальше переиспользует print_uint на модуле числа.
 */
static void print_int(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    INT32 value
)
{
    if (value < 0) {
        print(out, "-");
        print_uint(out, (UINT64)(UINT32)(-value));
    } else {
        print_uint(out, (UINT64)(UINT32)value);
    }
}


/*
 * То же самое, но всегда минимум 2 цифры
 * (с ведущим нулём) - используется для часов/минут/секунд.
 * Тоже отсутствовала в исходнике - вторая недостающая
 * функция, вызывавшая ошибку компиляции.
 */
static void print_uint2(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINTN value
)
{
    if (value < 10)
        print(out, "0");

    print_uint(out, (UINT64)value);
}


/*
 * Шестнадцатеричная печать с ведущими нулями до нужной
 * ширины (digits символов) - для адресов MMIO, ID устройств
 * и т.п., где удобнее фиксированная ширина, чем print_uint.
 */
static void print_hex(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 value,
    UINTN digits
)
{
    static const char hex_chars[] = "0123456789ABCDEF";

    CHAR16 buf[17];

    if (digits > 16)
        digits = 16;

    for (UINTN i = 0; i < digits; i++) {

        UINTN shift = (digits - 1 - i) * 4;

        buf[i] = (CHAR16)hex_chars[(value >> shift) & 0xF];
    }

    buf[digits] = 0;

    print16(out, buf);
}


/* ============================================================
 * PCI Configuration Space - "Legacy Mechanism #1", порты
 * 0xCF8 (CONFIG_ADDRESS) / 0xCFC (CONFIG_DATA).
 *
 * Это обычные x86 I/O-порты чипсета (южного моста/PCH),
 * а не сервис прошивки - в отличие от EFI_PCI_IO_PROTOCOL,
 * который является Boot Services-протоколом и перестаёт
 * существовать вместе с остальными после ExitBootServices.
 * Именно поэтому шаг 2 ("найти xHCI-контроллер") сделан
 * через порты напрямую: этот код одинаково работает и до,
 * и после выхода из Boot Services - ОС в этом месте уже
 * не спрашивает разрешения у прошивки.
 * ============================================================ */

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static inline void io_out32(UINT16 port, UINT32 value)
{
    __asm__ __volatile__(
        "outl %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline UINT32 io_in32(UINT16 port)
{
    UINT32 value;

    __asm__ __volatile__(
        "inl %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

/*
 * Читает 32-битное слово конфигурационного пространства PCI
 * по (bus, dev, func, offset). offset должен быть выровнен
 * по 4 байтам - младшие 2 бита адреса игнорируются самим
 * протоколом Mechanism #1, поэтому маскируем их явно.
 */
static UINT32 pci_config_read32(
    UINT8 bus, UINT8 dev, UINT8 func, UINT8 offset
)
{
    UINT32 address =
        (1u << 31) |
        ((UINT32)bus  << 16) |
        ((UINT32)dev  << 11) |
        ((UINT32)func << 8)  |
        ((UINT32)offset & 0xFCu);

    io_out32(PCI_CONFIG_ADDRESS, address);

    return io_in32(PCI_CONFIG_DATA);
}

static void pci_config_write32(
    UINT8 bus, UINT8 dev, UINT8 func, UINT8 offset, UINT32 value
)
{
    UINT32 address =
        (1u << 31) |
        ((UINT32)bus  << 16) |
        ((UINT32)dev  << 11) |
        ((UINT32)func << 8)  |
        ((UINT32)offset & 0xFCu);

    io_out32(PCI_CONFIG_ADDRESS, address);
    io_out32(PCI_CONFIG_DATA, value);
}

/*
 * Command-регистр (offset 0x04, нижние 16 бит) по умолчанию
 * может не иметь включённых Memory Space (бит 1) и Bus Master
 * (бит 2) - тогда MMIO-регистры устройства не гарантированно
 * отвечают, а DMA (понадобится позже, для настоящих передач
 * по USB) не будет работать вообще. Прошивка обычно включает
 * это сама для устройств, которые использует, но мы xHCI
 * нашли сами, в обход прошивки, - поэтому не полагаемся на
 * неё и включаем явно.
 */
static void pci_enable_device(UINT8 bus, UINT8 dev, UINT8 func)
{
    UINT32 cmd_dword =
        pci_config_read32(bus, dev, func, 0x04);

    cmd_dword |= 0x0006u; /* бит1: Memory Space, бит2: Bus Master */

    /*
     * бит10: Interrupt Disable. Наш драйвер полностью
     * опросный (polling) - мы никогда не настраиваем
     * IDT/обработчик прерываний. Если это не запретить,
     * контроллер может в какой-то момент дёрнуть Legacy
     * PCI IRQ (например, из-за уже подключённого
     * устройства при старте RS=1) - и это прерывание
     * улетит в никуда (нет нашего обработчика), что на
     * практике выглядит как необъяснимое зависание системы
     * в случайной, но привязанной по времени, а не по коду,
     * точке. Ставим бит явно, чтобы контроллер физически не
     * мог поднять эту линию.
     */
    cmd_dword |= 0x0400u;

    pci_config_write32(bus, dev, func, 0x04, cmd_dword);
}

/*
 * BAR0 контроллера может быть 64-битным (тип в битах[2:1]
 * самого BAR0 равен 0b10) - тогда верхняя половина адреса
 * лежит в следующем регистре, BAR1. Собираем оба случая
 * в один 64-битный адрес; для 32-битного BAR верхние 32 бита
 * просто нулевые.
 */
static UINT64 pci_read_bar_address(
    UINT8 bus, UINT8 dev, UINT8 func, UINT8 bar_offset
)
{
    UINT32 bar0 = pci_config_read32(bus, dev, func, bar_offset);

    /* бит0 = 0 -> memory-mapped BAR (не I/O-порты) */
    if ((bar0 & 0x1) != 0)
        return 0;

    UINT64 addr = (UINT64)(bar0 & 0xFFFFFFF0u);

    UINT32 bar_type = (bar0 >> 1) & 0x3;

    if (bar_type == 0x2) {

        UINT32 bar1 =
            pci_config_read32(bus, dev, func, bar_offset + 4);

        addr |= ((UINT64)bar1) << 32;
    }

    return addr;
}

/*
 * Класс/подкласс/prog-if лежат в одном dword по офсету 0x08
 * заголовка PCI-устройства: байт 0x08=RevisionID, 0x09=ProgIF,
 * 0x0A=SubClass, 0x0B=BaseClass (little-endian, поэтому именно
 * такой порядок при извлечении битовыми сдвигами).
 */
#define PCI_CLASS_DWORD_BASE_CLASS(d)  (UINT8)(((d) >> 24) & 0xFF)
#define PCI_CLASS_DWORD_SUB_CLASS(d)   (UINT8)(((d) >> 16) & 0xFF)
#define PCI_CLASS_DWORD_PROG_IF(d)     (UINT8)(((d) >> 8)  & 0xFF)

#define PCI_CLASS_SERIAL_BUS  0x0C
#define PCI_SUBCLASS_USB      0x03
#define PCI_PROGIF_UHCI       0x00
#define PCI_PROGIF_OHCI       0x10
#define PCI_PROGIF_EHCI       0x20
#define PCI_PROGIF_XHCI       0x30

/*
 * Ищет первый попавшийся xHCI-контроллер (класс 0x0C,
 * подкласс 0x03, prog-if 0x30) полным перебором bus/dev/func -
 * без рекурсивного обхода мостов, зато просто и надёжно:
 * непопулярные комбинации быстро отсеиваются по VendorID
 * == 0xFFFF. Возвращает TRUE и заполняет out-параметры, если
 * контроллер найден.
 */
static BOOLEAN pci_find_xhci(
    UINT8 *out_bus,
    UINT8 *out_dev,
    UINT8 *out_func,
    UINT64 *out_mmio
)
{
    for (UINTN bus = 0; bus < 256; bus++) {

        for (UINTN dev = 0; dev < 32; dev++) {

            UINT32 id0 =
                pci_config_read32(
                    (UINT8)bus, (UINT8)dev, 0, 0x00
                );

            if ((id0 & 0xFFFF) == 0xFFFF)
                continue; /* устройства на function 0 нет вообще */

            UINT32 hdr_dword =
                pci_config_read32(
                    (UINT8)bus, (UINT8)dev, 0, 0x0C
                );

            BOOLEAN multi_func =
                (((hdr_dword >> 16) & 0x80) != 0);

            UINTN max_func = multi_func ? 8 : 1;

            for (UINTN func = 0; func < max_func; func++) {

                UINT32 id =
                    (func == 0)
                        ? id0
                        : pci_config_read32(
                              (UINT8)bus, (UINT8)dev,
                              (UINT8)func, 0x00
                          );

                if ((id & 0xFFFF) == 0xFFFF)
                    continue;

                UINT32 class_dword =
                    pci_config_read32(
                        (UINT8)bus, (UINT8)dev,
                        (UINT8)func, 0x08
                    );

                UINT8 base_class =
                    PCI_CLASS_DWORD_BASE_CLASS(class_dword);
                UINT8 sub_class =
                    PCI_CLASS_DWORD_SUB_CLASS(class_dword);
                UINT8 prog_if =
                    PCI_CLASS_DWORD_PROG_IF(class_dword);

                if (
                    base_class == PCI_CLASS_SERIAL_BUS &&
                    sub_class  == PCI_SUBCLASS_USB &&
                    prog_if    == PCI_PROGIF_XHCI
                ) {

                    *out_bus  = (UINT8)bus;
                    *out_dev  = (UINT8)dev;
                    *out_func = (UINT8)func;

                    *out_mmio =
                        pci_read_bar_address(
                            (UINT8)bus, (UINT8)dev,
                            (UINT8)func, 0x10
                        );

                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}


/* ============================================================
 * xHCI - Capability Registers (только чтение, шаг 3).
 *
 * MMIO - это просто обычная память по физическому адресу
 * из BAR0 (см. pci_read_bar_address выше), а не порты и не
 * протокол - значит, ни один UEFI-вызов тут не нужен вообще,
 * читаем/пишем напрямую через volatile-указатель.
 *
 * ВАЖНО (ограничение этого шага): это работает только пока
 * физический адрес == виртуальному, то есть страничная
 * трансляция остаётся той identity-mapping, которую заранее
 * настроила прошивка UEFI (типично для x86_64 UEFI - она сама
 * держит 1:1 отображение первых десятков/сотен ГБ физической
 * памяти). Мы этим просто пользуемся, но НЕ настраиваем
 * страницы сами. Если понадобится работать с MMIO выше того,
 * что замаппила прошивка, - это будет уже отдельный шаг
 * (собственные таблицы страниц).
 * ============================================================ */

static inline UINT32 mmio_read32(UINT64 addr)
{
    return *(volatile UINT32 *)(UINTN)addr;
}

static inline void mmio_write32(UINT64 addr, UINT32 value)
{
    *(volatile UINT32 *)(UINTN)addr = value;
}

/*
 * Свежая память от AllocatePages не гарантированно нулевая -
 * а контроллер xHCI обязан видеть нули в неинициализированных
 * полях DCBAA/колец (Cycle bit и т.п. иначе воспримутся как
 * мусор). Своего memset в freestanding-окружении нет, пишем
 * руками.
 */
static void raw_zero_mem(volatile UINT8 *p, UINTN n)
{
    for (UINTN i = 0; i < n; i++)
        p[i] = 0;
}

/*
 * Структура Capability Registers у xHCI (спека xHCI, раздел
 * "Host Controller Capability Registers") - только поля,
 * нужные на этом шаге. Не читаем как C-структуру напрямую
 * (риск выравнивания/паддинга), а достаём каждое поле через
 * mmio_read32 по фиксированному офсету - надёжнее.
 */
typedef struct {
    UINT8  CapLength;   /* длина этого блока регистров, байт */
    UINT16 HciVersion;  /* версия xHCI, BCD, напр. 0x0100 = 1.0 */
    UINT8  MaxSlots;    /* макс. число Device Slot'ов */
    UINT16 MaxIntrs;    /* макс. число линий прерываний */
    UINT8  MaxPorts;    /* число портов корневого хаба -
                            то, куда физически втыкается мышь */
    UINT32 DbOff;       /* офсет Doorbell-регистров от MMIO-базы */
    UINT32 RtsOff;      /* офсет Runtime-регистров от MMIO-базы */
    UINT32 ExtCapOff;   /* офсет (в байтах от MMIO-базы) первого
                            элемента списка Extended Capabilities,
                            0 если списка нет - нужен для
                            BIOS-to-OS Handoff */
} XHCI_CAP_INFO;

/*
 * PORTSC - один из самых "минных" регистров xHCI: часть битов
 * обычные RW (Port Reset, Port Power, ...), часть - RW1CS
 * (запись 1 туда СБРАСЫВАЕТ соответствующий бит status-change,
 * запись 0 не трогает его вообще), а PED (Port Enabled) отдельно
 * опасен - запись 1 в него ВЫКЛЮЧАЕТ порт. Поэтому нельзя просто
 * прочитать слово, поменять один бит и записать назад целиком -
 * так можно случайно сбросить событие или выключить порт.
 * portsc_base_for_write() обнуляет все RW1CS-биты и PED перед
 * тем, как вызывающий код добавит свой собственный бит поверх.
 */
#define PORTSC_BIT_CCS  (1u << 0)
#define PORTSC_BIT_PED  (1u << 1)
#define PORTSC_BIT_PR   (1u << 4)
#define PORTSC_BIT_CSC  (1u << 17)
#define PORTSC_BIT_PEC  (1u << 18)
#define PORTSC_BIT_WRC  (1u << 19)
#define PORTSC_BIT_OCC  (1u << 20)
#define PORTSC_BIT_PRC  (1u << 21)
#define PORTSC_BIT_PLC  (1u << 22)
#define PORTSC_BIT_CEC  (1u << 23)

#define PORTSC_RW1CS_MASK \
    (PORTSC_BIT_CSC | PORTSC_BIT_PEC | PORTSC_BIT_WRC | \
     PORTSC_BIT_OCC | PORTSC_BIT_PRC | PORTSC_BIT_PLC | \
     PORTSC_BIT_CEC)

static UINT32 portsc_base_for_write(UINT32 cur)
{
    return cur & ~(PORTSC_RW1CS_MASK | PORTSC_BIT_PED);
}

/*
 * Firmware-независимая задержка в миллисекундах.
 *
 * ЗАЧЕМ ЭТО ОТДЕЛЬНО ОТ Stall(): весь код ниже (сброс
 * контроллера, ожидание событий, опрос отчётов мыши) теперь
 * выполняется ЦЕЛИКОМ ПОСЛЕ ExitBootServices (см. команду
 * "ebs") - к этому моменту BootServices->Stall вызывать уже
 * нельзя (Boot Services не гарантированно существуют вообще).
 * Обычный volatile busy-wait цикл, тот же приём, что уже
 * используется в самом демо ExitBootServices для анимации
 * (EBS_SPIN_PER_STEP) - грубо, без калибровки под частоту
 * конкретного CPU, но не зависит вообще ни от чьего кода,
 * кроме нашего собственного.
 */
#define XHCI_SPIN_PER_MS 300000UL

/*
 * Раньше опрос отчётов мыши (см. xhci_address_device_and_
 * get_descriptor ниже) останавливался по нажатию клавиши -
 * ConIn после ExitBootServices больше не существует, поэтому
 * опрос теперь ограничен числом отчётов и числом "пустых"
 * попыток подряд (мышь может вообще не шевелиться какое-то
 * время - это не ошибка, просто ждём дальше, но не вечно).
 */
#define XHCI_POLL_MAX_REPORTS       200
#define XHCI_POLL_MAX_IDLE_ATTEMPTS 20

/*
 * TSC (Time Stamp Counter) - счётчик тактов процессора, читается
 * одной инструкцией rdtsc. Сам по себе он не знает, сколько
 * тактов в секунде, - эту частоту нужно один раз измерить
 * (откалибровать) по эталонным часам. Это делает переход в kernel
 * mode (команда "ebs", см. kx_pit_measure_tsc_hz). Пока
 * g_tsc_hz == 0, busy_wait_ms ниже работает по-старому, грубым
 * счётчиком - так, как и было до этого шага.
 */
static UINT64 g_tsc_hz = 0;

static inline UINT64 rdtsc(void)
{
    UINT32 lo, hi;

    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));

    return ((UINT64)hi << 32) | lo;
}

static inline void cpu_pause(void)
{
    __asm__ __volatile__("pause" ::: "memory");
}

static void tsc_delay_us(UINT64 us)
{
    UINT64 start = rdtsc();
    UINT64 cycles = (us * g_tsc_hz) / 1000000ull;

    while ((rdtsc() - start) < cycles)
        cpu_pause();
}

static void busy_wait_ms(UINTN ms)
{
    if (g_tsc_hz != 0) {

        /* таймер откалиброван - точная задержка по TSC */
        tsc_delay_us((UINT64)ms * 1000ull);
        return;
    }

    for (UINTN ms_i = 0; ms_i < ms; ms_i++) {

        for (
            volatile UINT64 spin = 0;
            spin < XHCI_SPIN_PER_MS;
            spin++
        ) { }
    }
}

/*
 * Сброс контроллера по спеке xHCI: если он уже был запущен -
 * сначала остановить (RS=0) и дождаться HCHalted=1, потом
 * поставить HCRST=1 и ждать, пока и сам HCRST не сбросится,
 * и CNR (Controller Not Ready) в USBSTS не станет 0 - только
 * после этого с регистрами вообще можно работать дальше.
 * Опрос через busy_wait_ms(1) за итерацию, лимит - секунда,
 * как верхняя граница по спеке на HCRST. Раньше здесь стоял
 * BootServices->Stall(1000) - заменено на firmware-независимую
 * задержку, потому что эта функция теперь вызывается уже после
 * ExitBootServices (см. команду "ebs").
 */
static BOOLEAN xhci_reset_controller(UINT64 op_base)
{
    UINT32 cmd = mmio_read32(op_base + 0x00);

    if (cmd & 0x1u) {

        cmd &= ~0x1u;
        mmio_write32(op_base + 0x00, cmd);

        for (UINTN i = 0; i < 1000; i++) {

            busy_wait_ms(1);

            if (mmio_read32(op_base + 0x04) & 0x1u)
                break;
        }
    }

    cmd = mmio_read32(op_base + 0x00);
    cmd |= 0x2u; /* HCRST */
    mmio_write32(op_base + 0x00, cmd);

    for (UINTN i = 0; i < 1000; i++) {

        busy_wait_ms(1);

        UINT32 c = mmio_read32(op_base + 0x00);
        UINT32 s = mmio_read32(op_base + 0x04);

        if (!(c & 0x2u) && !(s & (1u << 11)))
            return TRUE;
    }

    return FALSE;
}

/*
 * ВАЖНО - вероятная настоящая причина случайных зависаний,
 * не связанная с порядком регистров: пока мы НЕ вызвали
 * ExitBootServices (а команда "xhci" тестируется именно
 * до этого), прошивка (OVMF) сама всё ещё жива и может
 * (если у неё есть встроенный UsbMouseDxe - см. коммент
 * про EFI_SIMPLE_POINTER_PROTOCOL в gui_start) в фоне,
 * по таймеру/прерыванию, периодически опрашивать этот же
 * самый xHCI-контроллер (читать те же PORTSC/Event Ring),
 * чтобы поставлять данные в EFI_SIMPLE_POINTER_PROTOCOL.
 * Это происходит АСИНХРОННО и может сработать в любой
 * момент нашего кода (даже посреди печати строки на
 * экран - отсюда и зависание ровно на разных байтах
 * "Port N" в разных запусках). Если мы при этом делаем
 * Host Controller Reset и переписываем DCBAA/Command
 * Ring/Event Ring "из-под" прошивки, у нас с ней начинается
 * настоящая гонка за одно и то же железо.
 *
 * Лечится через Boot Services DisconnectController():
 * находим EFI_HANDLE, на котором прошивка опубликовала
 * EFI_PCI_IO_PROTOCOL именно для этого PCI-устройства
 * (сверяем bus/dev/func через GetLocation - других способов
 * связать голый bus/dev/func с EFI_HANDLE у нас нет, мы же
 * нашли контроллер в обход прошивки, через порты 0xCF8/0xCFC),
 * и отключаем от него все привязанные драйверы. После этого
 * прошивка больше не трогает регистры этого контроллера, и
 * дальше мы полностью одни с железом - ровно то же самое
 * состояние гонки, из-за которого в итоге всё равно придётся
 * когда-нибудь звать ExitBootServices, только сделанное чуть
 * раньше и выборочно (для одного устройства, шелл при этом
 * продолжает нормально работать).
 *
 * Локальные typedef'ы для приведения голых VOID* из
 * EFI_BOOT_SERVICES (см. пояснение про это в efi.h) -
 * объявлены прямо тут, а не переиспользуют одноимённые
 * GUI_*, потому что те определены значительно ниже по
 * файлу (после этой функции), а typedef должен быть виден
 * до использования.
 */
typedef EFI_STATUS (EFIAPI *XHCI_LOCATE_HANDLE_BUFFER)(
    UINT32 SearchType,
    EFI_GUID *Protocol,
    VOID *SearchKey,
    UINTN *NoHandles,
    EFI_HANDLE **Buffer
);

typedef EFI_STATUS (EFIAPI *XHCI_HANDLE_PROTOCOL)(
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID **Interface
);

typedef EFI_STATUS (EFIAPI *XHCI_DISCONNECT_CONTROLLER)(
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE DriverImageHandle,
    EFI_HANDLE ChildHandle
);

typedef EFI_STATUS (EFIAPI *XHCI_FREE_POOL)(VOID *Buffer);

static void xhci_disconnect_firmware_driver(
    EFI_SYSTEM_TABLE *st,
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT8 target_bus,
    UINT8 target_dev,
    UINT8 target_func
)
{
    XHCI_LOCATE_HANDLE_BUFFER LocateHandleBuffer =
        (XHCI_LOCATE_HANDLE_BUFFER)
            st->BootServices->LocateHandleBuffer;
    XHCI_HANDLE_PROTOCOL HandleProtocol =
        (XHCI_HANDLE_PROTOCOL)
            st->BootServices->HandleProtocol;
    XHCI_DISCONNECT_CONTROLLER DisconnectController =
        (XHCI_DISCONNECT_CONTROLLER)
            st->BootServices->DisconnectController;
    XHCI_FREE_POOL FreePool =
        (XHCI_FREE_POOL)st->BootServices->FreePool;

    if (
        !LocateHandleBuffer || !HandleProtocol ||
        !DisconnectController
    ) {

        print(
            out,
            "(BootServices does not expose "
            "LocateHandleBuffer/HandleProtocol/"
            "DisconnectController - skipping firmware "
            "driver disconnect, race with firmware is "
            "still possible.)\n"
        );

        return;
    }

    EFI_GUID pciio_guid = EFI_PCI_IO_PROTOCOL_GUID;
    UINTN handle_count = 0;
    EFI_HANDLE *handles = NULL;

    if (
        LocateHandleBuffer(
            2, /* ByProtocol */
            &pciio_guid,
            NULL,
            &handle_count,
            &handles
        ) != EFI_SUCCESS ||
        handles == NULL
    ) {

        print(
            out,
            "(no EFI_PCI_IO_PROTOCOL handles found at all - "
            "nothing to disconnect.)\n"
        );

        return;
    }

    BOOLEAN disconnected_any = FALSE;

    for (UINTN i = 0; i < handle_count; i++) {

        EFI_PCI_IO_PROTOCOL *pci_io = NULL;

        if (
            HandleProtocol(
                handles[i], &pciio_guid, (VOID **)&pci_io
            ) != EFI_SUCCESS ||
            pci_io == NULL ||
            pci_io->GetLocation == NULL
        ) {
            continue;
        }

        UINTN seg = 0, bus = 0, dev = 0, func = 0;

        if (
            pci_io->GetLocation(
                pci_io, &seg, &bus, &dev, &func
            ) != EFI_SUCCESS
        ) {
            continue;
        }

        if (
            bus == (UINTN)target_bus &&
            dev == (UINTN)target_dev &&
            func == (UINTN)target_func
        ) {

            print(
                out,
                "Firmware PciIo handle for this xHCI found - "
                "disconnecting any driver bound to it "
                "(DisconnectController)...\n"
            );

            EFI_STATUS dc_status =
                DisconnectController(handles[i], NULL, NULL);

            if (dc_status == EFI_SUCCESS) {

                print(
                    out,
                    "DisconnectController: OK - firmware "
                    "should no longer touch this controller "
                    "in the background.\n"
                );

            } else {

                print(
                    out,
                    "DisconnectController: nothing was "
                    "bound (or it refused) - continuing "
                    "anyway, this is not necessarily fatal.\n"
                );
            }

            disconnected_any = TRUE;
        }
    }

    if (!disconnected_any) {

        print(
            out,
            "(none of the PciIo handles matched this "
            "bus/dev/func - could not confirm the firmware "
            "driver was disconnected; a race with firmware "
            "is still possible.)\n"
        );
    }

    if (FreePool)
        FreePool(handles);
}

static void xhci_read_cap_regs(UINT64 mmio_base, XHCI_CAP_INFO *info)
{
    UINT32 dword0 = mmio_read32(mmio_base + 0x00);

    info->CapLength  = (UINT8)(dword0 & 0xFF);
    info->HciVersion = (UINT16)((dword0 >> 16) & 0xFFFF);

    UINT32 hcsparams1 = mmio_read32(mmio_base + 0x04);

    info->MaxSlots = (UINT8)(hcsparams1 & 0xFF);
    info->MaxIntrs = (UINT16)((hcsparams1 >> 8) & 0x7FF);
    info->MaxPorts = (UINT8)((hcsparams1 >> 24) & 0xFF);

    info->DbOff  = mmio_read32(mmio_base + 0x14) & 0xFFFFFFFCu;
    info->RtsOff = mmio_read32(mmio_base + 0x18) & 0xFFFFFFE0u;

    /*
     * HCCPARAMS1, офсет 0x10. Биты [31:16] - xECP: офсет (в
     * ДВОЙНЫХ СЛОВАХ, не в байтах!) от MMIO-базы до первого
     * элемента списка Extended Capabilities. 0 - списка нет.
     */
    UINT32 hccparams1 = mmio_read32(mmio_base + 0x10);
    UINT32 xecp_dwords = (hccparams1 >> 16) & 0xFFFFu;

    info->ExtCapOff =
        (xecp_dwords == 0) ? 0 : (xecp_dwords * 4);
}

/*
 * BIOS-to-OS Handoff (USB Legacy Support Capability, xHCI spec
 * раздел 7.2). До этого шага мы просто лезли в регистры
 * контроллера напрямую, ни слова не сказав прошивке - а на
 * многих платформах (в т.ч. в QEMU/OVMF, как выяснилось на
 * практике) прошивка по умолчанию считает, что ВЛАДЕЕТ
 * контроллером сама (для эмуляции USB-клавиатуры/мыши на этапе
 * загрузки, обычно через SMI). Если не забрать явно владение
 * перед тем как сбрасывать контроллер и переписывать его
 * DCBAA/Command Ring, прошивка может продолжать параллельно
 * трогать тот же контроллер своим кодом - и тогда наши изменения
 * ломают её ожидания. На практике это выглядело как "случайное"
 * зависание где-то в самой прошивке (не в нашем коде) через
 * произвольное время после старта контроллера - см. историю
 * отладки этого шага.
 *
 * Список Extended Capabilities - однонаправленный связный
 * список: у каждого элемента dword[0] содержит Capability ID
 * (биты[7:0]) и Next Capability Pointer (биты[15:8], смещение
 * до следующего элемента В ДВОЙНЫХ СЛОВАХ ОТ ТЕКУЩЕГО ЭЛЕМЕНТА,
 * 0 = конец списка). Capability ID = 1 - это как раз USB Legacy
 * Support Capability (структура USBLEGSUP).
 *
 * Внутри USBLEGSUP (offset 0x00 relative к найденному элементу):
 *   бит 16 = HC BIOS Owned Semaphore (1, пока владеет прошивка)
 *   бит 24 = HC OS Owned Semaphore   (мы ставим 1, чтобы попросить)
 * USBLEGCTLSTS (offset 0x04 от того же элемента) - управление
 * SMI: обнуляем биты разрешения SMI (SMI Enable), чтобы прошивка
 * больше не получала прерываний по событиям этого контроллера.
 *
 * Возвращает TRUE, если либо handoff прошёл успешно, либо
 * capability вообще не найдена (значит и отбирать не у кого -
 * это нормальный случай, не ошибка).
 */
static BOOLEAN xhci_bios_handoff(
    EFI_SYSTEM_TABLE *st,
    UINT64 mmio_base,
    UINT32 ext_cap_off,
    SIMPLE_TEXT_OUTPUT_INTERFACE *out
)
{
    if (ext_cap_off == 0) {

        print(
            out,
            "No Extended Capabilities list - nothing "
            "to hand off, continuing.\n"
        );

        return TRUE;
    }

    UINT32 cur_off = ext_cap_off;

    /* защита от кольца/повреждённого списка */
    for (UINTN guard = 0; guard < 64; guard++) {

        UINT32 dword0 =
            mmio_read32(mmio_base + cur_off);

        UINT8 cap_id = (UINT8)(dword0 & 0xFFu);
        UINT8 next_dwords = (UINT8)((dword0 >> 8) & 0xFFu);

        if (cap_id == 1) {

            /* Нашли USB Legacy Support Capability */

            print(
                out,
                "USB Legacy Support Capability found "
                "at offset 0x"
            );
            print_hex(out, cur_off, 4);
            print(out, ", USBLEGSUP=0x");
            print_hex(out, dword0, 8);
            print(out, "\n");

            if ((dword0 & (1u << 16)) == 0) {

                print(
                    out,
                    "BIOS Owned Semaphore already 0 - "
                    "firmware does not currently claim "
                    "this controller, nothing to do.\n"
                );

                return TRUE;
            }

            /* Просим владение: HC OS Owned Semaphore = 1 */
            mmio_write32(
                mmio_base + cur_off,
                dword0 | (1u << 24)
            );

            print(
                out,
                "Requested ownership (OS Owned Semaphore "
                "= 1), waiting for firmware to release "
                "BIOS Owned Semaphore...\n"
            );

            BOOLEAN handed_off = FALSE;

            /* по спеке BIOS обязан ответить быстро, но
               даём с запасом - до ~2 секунд */
            for (UINTN i = 0; i < 2000; i++) {

                if (st->BootServices->Stall)
                    st->BootServices->Stall(1000);

                UINT32 cur =
                    mmio_read32(mmio_base + cur_off);

                if ((cur & (1u << 16)) == 0) {
                    handed_off = TRUE;
                    break;
                }
            }

            if (!handed_off) {

                print(
                    out,
                    "Firmware did not release the BIOS "
                    "Owned Semaphore within the timeout - "
                    "taking the controller over anyway "
                    "(some firmware never clears this bit "
                    "even though it stops interfering).\n"
                );

            } else {

                print(
                    out,
                    "Handoff complete - BIOS Owned "
                    "Semaphore is now 0.\n"
                );
            }

            /*
             * USBLEGCTLSTS сразу после USBLEGSUP (offset
             * +0x04). Обнуляем все биты разрешения SMI
             * (обычно верхняя половина dword'а), чтобы
             * прошивка больше не просыпалась по SMI на
             * события этого контроллера, пока им управляем
             * мы. RW1C-биты статуса (нижняя половина) не
             * трогаем записью 1 намеренно - незачем лишний
             * раз что-то там сбрасывать, нам это не мешает.
             */
            UINT32 ctlsts =
                mmio_read32(mmio_base + cur_off + 0x04);

            mmio_write32(
                mmio_base + cur_off + 0x04,
                ctlsts & 0x0000FFFFu
            );

            print(
                out,
                "SMI generation for this controller "
                "disabled (USBLEGCTLSTS SMI-enable bits "
                "cleared).\n"
            );

            return TRUE;
        }

        if (next_dwords == 0)
            break;

        cur_off += (UINT32)next_dwords * 4;
    }

    print(
        out,
        "No USB Legacy Support Capability in the "
        "Extended Capabilities list - nothing to hand "
        "off, continuing.\n"
    );

    return TRUE;
}


/* ============================================================
 * Basic string helpers
 * ============================================================ */

static UINTN char16_len(const CHAR16 *s)
{
    UINTN n = 0;

    while (s[n])
        n++;

    return n;
}


static void char16_copy(
    CHAR16 *dst,
    const CHAR16 *src,
    UINTN max
)
{
    UINTN i = 0;

    if (max == 0)
        return;

    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}


static int char16_eq(
    const CHAR16 *a,
    const CHAR16 *b
)
{
    while (*a && *b) {

        if (*a != *b)
            return 0;

        a++;
        b++;
    }

    return *a == 0 && *b == 0;
}


/* ============================================================
 * Parsing helpers
 * ============================================================ */

static int streq(
    const CHAR16 *a,
    const char *b
)
{
    while (*a && *b) {

        if (*a !=
            (CHAR16)(unsigned char)*b)
            return 0;

        a++;
        b++;
    }

    return *a == 0 && *b == 0;
}


static int starts_with(
    const CHAR16 *a,
    const char *prefix
)
{
    while (*prefix) {

        if (*a !=
            (CHAR16)(unsigned char)*prefix)
            return 0;

        a++;
        prefix++;
    }

    return 1;
}


static UINTN parse_uint(
    const CHAR16 *s
)
{
    while (*s == L' ')
        s++;

    UINTN v = 0;

    while (*s >= L'0' &&
           *s <= L'9') {

        v =
            v * 10 +
            (UINTN)(*s - L'0');

        s++;
    }

    return v;
}


static CHAR16 *skip_ws16(
    CHAR16 *s
)
{
    while (*s == L' ')
        s++;

    return s;
}


static CHAR16 *take_word(
    CHAR16 *s,
    CHAR16 *out,
    UINTN max
)
{
    UINTN i = 0;

    if (max == 0)
        return s;

    while (*s &&
           *s != L' ' &&
           i < max - 1) {

        out[i++] = *s++;
    }

    out[i] = 0;

    return s;
}


/* ============================================================
 * Input
 * ============================================================ */

static void erase_input_line(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINTN len
)
{
    for (UINTN i = 0; i < len; i++)
        print(out, "\b \b");
}


/*
 * Чтение строки.
 *
 * PageUp    = scrollback вверх
 * PageDown  = scrollback вниз
 * Home      = самый верх истории
 * End       = самый низ
 *
 * Up/Down   = command history
 */
static void read_line(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *line,
    UINTN max
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    SIMPLE_INPUT_INTERFACE *in =
        st->ConIn;

    UINTN len = 0;

    int history_pos = -1;

    line[0] = 0;


    for (;;) {

        EFI_INPUT_KEY key;

        EFI_STATUS status =
            in->ReadKeyStroke(
                in,
                &key
            );


        if (status != EFI_SUCCESS) {

            st->BootServices->Stall(
                10000
            );

            continue;
        }


        /* ====================================================
         * ENTER
         * ==================================================== */

        if (key.UnicodeChar ==
            CHAR_CARRIAGE_RETURN) {

            line[len] = 0;

            print(out, "\n");

            return;
        }


        /* ====================================================
         * BACKSPACE
         * ==================================================== */

        if (key.UnicodeChar ==
            CHAR_BACKSPACE) {

            if (len > 0) {

                len--;

                line[len] = 0;

                print(out, "\b \b");
            }

            continue;
        }


        /* ====================================================
         * SPECIAL KEYS
         * ==================================================== */

        if (key.UnicodeChar == 0) {


            /* =================================================
             * PAGE UP
             * ScanCode 0x09
             * ================================================= */

            if (key.ScanCode == 0x09) {

                int step =
                    SCROLLBACK_VISIBLE_ROWS - 1;

                if (step < 1)
                    step = 1;

                int new_view =
                    g_scrollback_view + step;

                scrollback_render(
                    st,
                    new_view
                );

                redraw_input(
                    out,
                    line
                );

                continue;
            }


            /* =================================================
             * PAGE DOWN
             * ScanCode 0x0A
             * ================================================= */

            if (key.ScanCode == 0x0A) {

                int step =
                    SCROLLBACK_VISIBLE_ROWS - 1;

                if (step < 1)
                    step = 1;

                int new_view =
                    g_scrollback_view - step;

                if (new_view < 0)
                    new_view = 0;

                scrollback_render(
                    st,
                    new_view
                );

                redraw_input(
                    out,
                    line
                );

                continue;
            }


            /* =================================================
             * HOME
             * ScanCode 0x05
             * ================================================= */

            if (key.ScanCode == 0x05) {

                scrollback_render(
                    st,
                    999999
                );

                redraw_input(
                    out,
                    line
                );

                continue;
            }


            /* =================================================
             * END
             * ScanCode 0x06
             * ================================================= */

            if (key.ScanCode == 0x06) {

                scrollback_render(
                    st,
                    0
                );

                redraw_input(
                    out,
                    line
                );

                continue;
            }


            /* =================================================
             * UP — command history
             * ScanCode 1
             * ================================================= */

            if (key.ScanCode == 1) {

                if (g_history_count > 0) {

                    if (history_pos == -1) {

                        history_pos =
                            (int)g_history_count - 1;

                    } else if (history_pos > 0) {

                        history_pos--;
                    }


                    /*
                     * Стираем текущий ввод.
                     */
                    for (UINTN i = 0;
                         i < len;
                         i++) {

                        print(
                            out,
                            "\b \b"
                        );
                    }


                    UINTN index;


                    if (g_history_count <=
                        HIST_MAX) {

                        index =
                            (UINTN)history_pos;

                    } else {

                        UINTN start =
                            g_history_count %
                            HIST_MAX;

                        index =
                            (start +
                             (UINTN)history_pos)
                            % HIST_MAX;
                    }


                    char16_copy(
                        line,
                        g_history[index],
                        max
                    );

                    len =
                        char16_len(line);


                    print16(
                        out,
                        line
                    );
                }

                continue;
            }


            /* =================================================
             * DOWN — command history
             * ScanCode 2
             * ================================================= */

            if (key.ScanCode == 2) {

                if (history_pos != -1) {

                    for (UINTN i = 0;
                         i < len;
                         i++) {

                        print(
                            out,
                            "\b \b"
                        );
                    }


                    history_pos++;


                    if (history_pos >=
                            (int)g_history_count ||
                        history_pos >=
                            (int)HIST_MAX) {

                        history_pos = -1;

                        len = 0;

                        line[0] = 0;

                    } else {

                        UINTN index;


                        if (g_history_count <=
                            HIST_MAX) {

                            index =
                                (UINTN)history_pos;

                        } else {

                            UINTN start =
                                g_history_count %
                                HIST_MAX;

                            index =
                                (start +
                                 (UINTN)history_pos)
                                % HIST_MAX;
                        }


                        char16_copy(
                            line,
                            g_history[index],
                            max
                        );

                        len =
                            char16_len(line);


                        print16(
                            out,
                            line
                        );
                    }
                }

                continue;
            }


            continue;
        }


        /* ====================================================
         * NORMAL CHARACTER
         * ==================================================== */

        if (len < max - 1) {

            line[len++] =
                key.UnicodeChar;

            line[len] = 0;


            CHAR16 echo[2] = {
                key.UnicodeChar,
                0
            };


            out->OutputString(
                out,
                echo
            );


            history_pos = -1;
        }
    }
}


/* ============================================================
 * RAM filesystem
 * ============================================================ */

static int fs_find(
    const CHAR16 *name
)
{
    for (int i = 0;
         i < FS_MAX_FILES;
         i++) {

        if (g_fs[i].used &&
            char16_eq(
                g_fs[i].name,
                name
            ))
            return i;
    }

    return -1;
}


static int fs_find_free(void)
{
    for (int i = 0;
         i < FS_MAX_FILES;
         i++) {

        if (!g_fs[i].used)
            return i;
    }

    return -1;
}


static void cmd_ls(
    EFI_SYSTEM_TABLE *st
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    int any = 0;

    for (int i = 0;
         i < FS_MAX_FILES;
         i++) {

        if (!g_fs[i].used)
            continue;

        any = 1;

        print16(
            out,
            g_fs[i].name
        );

        print(out, "  (");

        print_uint(
            out,
            g_fs[i].size
        );

        print(out, " bytes)\n");
    }

    if (!any) {

        print(
            out,
            "(empty - no files. use 'touch <name>' or 'write <name> <text>')\n"
        );
    }
}


static void cmd_touch(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: touch <name>\n"
        );

        return;
    }


    if (fs_find(name) >= 0) {

        print(
            out,
            "File already exists.\n"
        );

        return;
    }


    int idx =
        fs_find_free();


    if (idx < 0) {

        print(
            out,
            "Filesystem full (max "
        );

        print_uint(
            out,
            FS_MAX_FILES
        );

        print(out, " files).\n");

        return;
    }


    g_fs[idx].used = TRUE;

    char16_copy(
        g_fs[idx].name,
        name,
        FS_NAME_MAX
    );

    g_fs[idx].data[0] = 0;

    g_fs[idx].size = 0;

    print(out, "Created.\n");
}


static void cmd_cat(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: cat <name>\n"
        );

        return;
    }


    int idx =
        fs_find(name);


    if (idx < 0) {

        print(
            out,
            "No such file.\n"
        );

        return;
    }


    if (g_fs[idx].size == 0) {

        print(
            out,
            "(empty file)\n"
        );

        return;
    }


    /*
     * Важно: print16(), а не прямой OutputString(),
     * чтобы cat попадал в scrollback.
     */
    print16(
        out,
        g_fs[idx].data
    );

    print(out, "\n");
}


static void cmd_write(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name,
    CHAR16 *text
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: write <name> <text>\n"
        );

        return;
    }


    int idx =
        fs_find(name);


    if (idx < 0)
        idx = fs_find_free();


    if (idx < 0) {

        print(
            out,
            "Filesystem full.\n"
        );

        return;
    }


    g_fs[idx].used = TRUE;


    char16_copy(
        g_fs[idx].name,
        name,
        FS_NAME_MAX
    );


    char16_copy(
        g_fs[idx].data,
        text,
        FS_DATA_MAX
    );


    g_fs[idx].size =
        char16_len(
            g_fs[idx].data
        );


    print(out, "Written (");

    print_uint(
        out,
        g_fs[idx].size
    );

    print(out, " bytes).\n");
}


static void cmd_append(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name,
    CHAR16 *text
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: append <name> <text>\n"
        );

        return;
    }


    int idx =
        fs_find(name);


    if (idx < 0)
        idx = fs_find_free();


    if (idx < 0) {

        print(
            out,
            "Filesystem full.\n"
        );

        return;
    }


    if (!g_fs[idx].used) {

        g_fs[idx].used = TRUE;

        char16_copy(
            g_fs[idx].name,
            name,
            FS_NAME_MAX
        );

        g_fs[idx].data[0] = 0;

        g_fs[idx].size = 0;
    }


    UINTN cur =
        g_fs[idx].size;

    UINTN need =
        char16_len(text);

    UINTN sep =
        (cur > 0) ? 1 : 0;


    if (cur + sep + need >=
        FS_DATA_MAX) {

        print(
            out,
            "File too large, cannot append that much.\n"
        );

        return;
    }


    if (sep)
        g_fs[idx].data[cur++] =
            L'\n';


    for (UINTN i = 0;
         i < need;
         i++) {

        g_fs[idx].data[cur++] =
            text[i];
    }


    g_fs[idx].data[cur] = 0;

    g_fs[idx].size = cur;


    print(
        out,
        "Appended ("
    );

    print_uint(
        out,
        g_fs[idx].size
    );

    print(
        out,
        " bytes total).\n"
    );
}


static void cmd_rm(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: rm <name>\n"
        );

        return;
    }


    int idx =
        fs_find(name);


    if (idx < 0) {

        print(
            out,
            "No such file.\n"
        );

        return;
    }


    g_fs[idx].used = FALSE;

    g_fs[idx].size = 0;


    print(
        out,
        "Deleted.\n"
    );
}


static void cmd_mv(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *oldname,
    CHAR16 *newname
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(oldname) == 0 ||
        char16_len(newname) == 0) {

        print(
            out,
            "Usage: mv <old> <new>\n"
        );

        return;
    }


    int idx =
        fs_find(oldname);


    if (idx < 0) {

        print(
            out,
            "No such file.\n"
        );

        return;
    }


    if (fs_find(newname) >= 0) {

        print(
            out,
            "Target name already exists.\n"
        );

        return;
    }


    char16_copy(
        g_fs[idx].name,
        newname,
        FS_NAME_MAX
    );


    print(
        out,
        "Renamed.\n"
    );
}


static void cmd_cp(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *src,
    CHAR16 *dst
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(src) == 0 ||
        char16_len(dst) == 0) {

        print(
            out,
            "Usage: cp <src> <dst>\n"
        );

        return;
    }


    int si =
        fs_find(src);


    if (si < 0) {

        print(
            out,
            "No such file.\n"
        );

        return;
    }


    if (fs_find(dst) >= 0) {

        print(
            out,
            "Target name already exists.\n"
        );

        return;
    }


    int di =
        fs_find_free();


    if (di < 0) {

        print(
            out,
            "Filesystem full.\n"
        );

        return;
    }


    g_fs[di].used = TRUE;


    char16_copy(
        g_fs[di].name,
        dst,
        FS_NAME_MAX
    );


    char16_copy(
        g_fs[di].data,
        g_fs[si].data,
        FS_DATA_MAX
    );


    g_fs[di].size =
        g_fs[si].size;


    print(
        out,
        "Copied.\n"
    );
}


/* ============================================================
 * Text editor
 * ============================================================ */

static void cmd_edit(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *name
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    if (char16_len(name) == 0) {

        print(
            out,
            "Usage: edit <name>\n"
        );

        return;
    }


    int idx =
        fs_find(name);


    if (idx < 0)
        idx = fs_find_free();


    if (idx < 0) {

        print(
            out,
            "Filesystem full.\n"
        );

        return;
    }


    if (!g_fs[idx].used) {

        g_fs[idx].used = TRUE;

        char16_copy(
            g_fs[idx].name,
            name,
            FS_NAME_MAX
        );
    }


    print(
        out,
        "Editing '"
    );

    print16(
        out,
        name
    );

    print(
        out,
        "'. Type lines, finish with a line containing just '.'\n"
    );


    CHAR16 buf[FS_DATA_MAX];

    UINTN pos = 0;

    buf[0] = 0;


    CHAR16 ln[LINE_MAX];


    for (;;) {

        print(
            out,
            ": "
        );


        read_line(
            st,
            ln,
            LINE_MAX
        );


        if (ln[0] == L'.' &&
            ln[1] == 0)
            break;


        UINTN llen =
            char16_len(ln);


        if (pos > 0 &&
            pos + 1 < FS_DATA_MAX) {

            buf[pos++] =
                L'\n';
        }


        for (UINTN i = 0;
             i < llen &&
             pos < FS_DATA_MAX - 1;
             i++) {

            buf[pos++] =
                ln[i];
        }


        buf[pos] = 0;


        if (pos >= FS_DATA_MAX - 1) {

            print(
                out,
                "(file full, stopping edit)\n"
            );

            break;
        }
    }


    char16_copy(
        g_fs[idx].data,
        buf,
        FS_DATA_MAX
    );


    g_fs[idx].size =
        char16_len(
            g_fs[idx].data
        );


    print(
        out,
        "Saved ("
    );

    print_uint(
        out,
        g_fs[idx].size
    );

    print(
        out,
        " bytes).\n"
    );
}


/* ============================================================
 * Calculator
 * ============================================================ */

static void cmd_calc(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *rest
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    rest =
        skip_ws16(rest);


    CHAR16 a[32];
    CHAR16 op[8];
    CHAR16 b[32];


    rest =
        take_word(
            rest,
            a,
            32
        );


    rest =
        skip_ws16(rest);


    rest =
        take_word(
            rest,
            op,
            8
        );


    rest =
        skip_ws16(rest);


    take_word(
        rest,
        b,
        32
    );


    if (char16_len(a) == 0 ||
        char16_len(op) == 0 ||
        char16_len(b) == 0) {

        print(
            out,
            "Usage: calc <a> <+|-|*|/> <b>\n"
        );

        return;
    }


    INTN x =
        (INTN)parse_uint(a);

    INTN y =
        (INTN)parse_uint(b);

    INTN r;


    if (char16_eq(op, L"+")) {

        r = x + y;

    } else if (char16_eq(op, L"-")) {

        r = x - y;

    } else if (char16_eq(op, L"*")) {

        r = x * y;

    } else if (char16_eq(op, L"/")) {

        if (y == 0) {

            print(
                out,
                "Error: division by zero.\n"
            );

            return;
        }

        r = x / y;

    } else {

        print(
            out,
            "Unknown operator. Use + - * /\n"
        );

        return;
    }


    if (r < 0) {

        print(out, "-");

        r = -r;
    }


    print_uint(
        out,
        (UINT64)r
    );

    print(
        out,
        "\n"
    );
}


/* ============================================================
 * Fetch helpers
 * ============================================================ */

static void print_label(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const char *label
)
{
    set_color(
        out,
        0x0B
    );

    print(
        out,
        label
    );

    set_color(
        out,
        0x0F
    );
}


static void print_separator(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out
)
{
    set_color(
        out,
        0x08
    );

    print(
        out,
        "----------------------------------------\n"
    );

    set_color(
        out,
        0x0F
    );
}


static void print_bool(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    BOOLEAN value
)
{
    set_color(
        out,
        value ? 0x0A : 0x0C
    );

    print(
        out,
        value ? "Yes" : "No"
    );

    set_color(
        out,
        0x0F
    );
}


/* ============================================================
 * Fetch / neofetch
 * ============================================================ */

static void cmd_fetch(
    EFI_SYSTEM_TABLE *st
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;

    UINTN saved =
        g_color;


    /* --------------------------------------------------------
     * ASCII logo
     * -------------------------------------------------------- */

    const char *logo[] = {

        " /$$      /$$            /$$$$$$   /$$$$$$ ",
        "| $$$    /$$$           /$$__  $$ /$$__  $$",
        "| $$$$  /$$$$ /$$   /$$| $$  \\ $$| $$  \\__/",
        "| $$ $$/$$ $$| $$  | $$| $$  | $$|  $$$$$$ ",
        "| $$  $$$| $$| $$  | $$| $$  | $$ \\____  $$",
        "| $$\\  $ | $$| $$  | $$| $$  | $$ /$$  \\ $$",
        "| $$ \\/  | $$|  $$$$$$$|  $$$$$$/|  $$$$$$/",
        "|__/     |__/ \\____  $$ \\______/  \\______/ ",
        "              /$$  | $$                    ",
        "             |  $$$$$$/                    ",
        "              \\______/                     "
    };


    UINTN logo_colors[] = {

        0x0B,
        0x0B,
        0x0B,

        0x03,
        0x03,
        0x03,

        0x01,
        0x01,
        0x01,

        0x09,
        0x09
    };


    for (UINTN i = 0;
         i < 11;
         i++) {

        set_color(
            out,
            logo_colors[i]
        );

        print(
            out,
            logo[i]
        );

        print(
            out,
            "\n"
        );
    }


    set_color(
        out,
        0x0F
    );

    print(
        out,
        "\n"
    );


    /* --------------------------------------------------------
     * Header
     * -------------------------------------------------------- */

    set_color(
        out,
        0x0B
    );

    print(
        out,
        "MyOS"
    );

    set_color(
        out,
        0x08
    );

    print(
        out,
        " @ "
    );

    set_color(
        out,
        0x0F
    );

    print(
        out,
        "UEFI bare-metal environment\n"
    );


    print_separator(out);


    /* --------------------------------------------------------
     * Operating system
     * -------------------------------------------------------- */

    print_label(
        out,
        "OS"
    );

    print(
        out,
        "        : MyOS 0.1"
    );

    print(
        out,
        " (x86_64, bare-metal UEFI)\n"
    );


    print_label(
        out,
        "Kernel"
    );

    print(
        out,
        "    : efi_main"
    );

    print(
        out,
        " (no Linux/Windows underneath)\n"
    );


    print_label(
        out,
        "Shell"
    );

    print(
        out,
        "     : myos-shell"
    );

    print(
        out,
        " (built-in command loop)\n"
    );


    print_label(
        out,
        "Architecture"
    );

    print(
        out,
        " : x86_64\n"
    );


    print_label(
        out,
        "Boot Mode"
    );

    print(
        out,
        "    : UEFI\n"
    );


    /* --------------------------------------------------------
     * Firmware
     * -------------------------------------------------------- */

    print_separator(out);


    print_label(
        out,
        "Firmware"
    );

    print(
        out,
        "  : "
    );


    if (st->FirmwareVendor)
        print16(
            out,
            st->FirmwareVendor
        );
    else
        print(
            out,
            "Unknown"
        );


    print(
        out,
        " rev "
    );


    print_uint(
        out,
        st->FirmwareRevision
    );


    print(
        out,
        "\n"
    );


    print_label(
        out,
        "UEFI"
    );

    print(
        out,
        "       : "
    );


    print_uint(
        out,
        (st->Hdr.Revision >> 16) &
        0xFFFF
    );


    print(
        out,
        "."
    );


    print_uint(
        out,
        st->Hdr.Revision &
        0xFFFF
    );


    print(
        out,
        "\n"
    );


    /* --------------------------------------------------------
     * Secure Boot
     * -------------------------------------------------------- */

    UINT8 secure_boot = 0;

    UINTN secure_boot_size =
        sizeof(secure_boot);


    EFI_GUID global_variable =
    {
        0x8BE4DF61,
        0x93CA,
        0x11D2,
        {
            0xAA,
            0x0D,
            0x00,
            0xE0,
            0x98,
            0x03,
            0x2B,
            0x8C
        }
    };


    EFI_STATUS sb_status =
        st->RuntimeServices->GetVariable(
            L"SecureBoot",
            &global_variable,
            NULL,
            &secure_boot_size,
            &secure_boot
        );


    print_label(
        out,
        "Secure Boot"
    );

    print(
        out,
        " : "
    );


    if (sb_status == EFI_SUCCESS) {

        print_bool(
            out,
            secure_boot != 0
        );

    } else {

        set_color(
            out,
            0x08
        );

        print(
            out,
            "Unknown"
        );

        set_color(
            out,
            0x0F
        );
    }


    print(
        out,
        "\n"
    );


    /* --------------------------------------------------------
     * Boot time / uptime
     * -------------------------------------------------------- */

    if (g_have_boot_time) {

        EFI_TIME now;


        if (st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now,
                NULL
            ) == EFI_SUCCESS) {


            INT64 now_secs =
                (INT64)now.Hour * 3600 +
                (INT64)now.Minute * 60 +
                now.Second;


            INT64 boot_secs =
                (INT64)g_boot_time.Hour * 3600 +
                (INT64)g_boot_time.Minute * 60 +
                g_boot_time.Second;


            INT64 secs =
                now_secs - boot_secs;


            if (secs < 0)
                secs += 86400;


            print_label(
                out,
                "Uptime"
            );

            print(
                out,
                "      : "
            );


            print_uint(
                out,
                (UINT64)secs / 3600
            );

            print(
                out,
                "h "
            );


            print_uint(
                out,
                ((UINT64)secs / 60) % 60
            );

            print(
                out,
                "m "
            );


            print_uint(
                out,
                (UINT64)secs % 60
            );

            print(
                out,
                "s\n"
            );
        }
    }


    /* --------------------------------------------------------
     * Current date/time
     * -------------------------------------------------------- */

    {
        EFI_TIME now;


        if (st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now,
                NULL
            ) == EFI_SUCCESS) {


            print_label(
                out,
                "Date"
            );

            print(
                out,
                "        : "
            );


            print_uint(
                out,
                now.Day
            );

            print(
                out,
                "."
            );


            print_uint(
                out,
                now.Month
            );

            print(
                out,
                "."
            );


            print_uint(
                out,
                now.Year
            );


            print(
                out,
                " "
            );


            if (now.Hour < 10)
                print(
                    out,
                    "0"
                );


            print_uint(
                out,
                now.Hour
            );


            print(
                out,
                ":"
            );


            if (now.Minute < 10)
                print(
                    out,
                    "0"
                );


            print_uint(
                out,
                now.Minute
            );


            print(
                out,
                ":"
            );


            if (now.Second < 10)
                print(
                    out,
                    "0"
                );


            print_uint(
                out,
                now.Second
            );


            print(
                out,
                "\n"
            );
        }
    }


    /* --------------------------------------------------------
     * GOP
     * -------------------------------------------------------- */

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop =
        NULL;


    EFI_GUID gop_guid =
        EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;


    EFI_STATUS gop_status =
        st->BootServices->LocateProtocol(
            &gop_guid,
            NULL,
            (void **)&gop
        );


    if (gop_status == EFI_SUCCESS &&
        gop != NULL) {


        print_separator(out);


        print_label(
            out,
            "GPU"
        );

        print(
            out,
            "        : EFI Graphics Output Protocol\n"
        );


        print_label(
            out,
            "Resolution"
        );

        print(
            out,
            " : "
        );


        if (gop->Mode &&
            gop->Mode->Info) {

            print_uint(
                out,
                gop->Mode->Info->
                    HorizontalResolution
            );


            print(
                out,
                "x"
            );


            print_uint(
                out,
                gop->Mode->Info->
                    VerticalResolution
            );


            print(
                out,
                "\n"
            );
        }


        print_label(
            out,
            "Pixel Format"
        );

        print(
            out,
            " : "
        );


        if (gop->Mode &&
            gop->Mode->Info) {


            switch (
                gop->Mode->Info->PixelFormat
            ) {

                case PixelRedGreenBlueReserved8BitPerColor:

                    print(
                        out,
                        "RGB"
                    );

                    break;


                case PixelBlueGreenRedReserved8BitPerColor:

                    print(
                        out,
                        "BGR"
                    );

                    break;


                case PixelBitMask:

                    print(
                        out,
                        "BitMask"
                    );

                    break;


                case PixelBltOnly:

                    print(
                        out,
                        "BltOnly"
                    );

                    break;


                default:

                    print(
                        out,
                        "Unknown"
                    );

                    break;
            }


            print(
                out,
                "\n"
            );
        }
    }


    /* --------------------------------------------------------
     * Console
     * -------------------------------------------------------- */

    print_separator(out);


    print_label(
        out,
        "Terminal"
    );

    print(
        out,
        g_kernel_mode ? "    : MyOS framebuffer console (8x16 font)\n"
                      : "    : EFI_SIMPLE_TEXT_OUTPUT\n"
    );


    print_label(
        out,
        "Input"
    );

    print(
        out,
        g_kernel_mode ? "       : MyOS USB HID + PS/2 drivers\n"
                      : "       : EFI_SIMPLE_TEXT_INPUT\n"
    );


    print_label(
        out,
        "Text Mode"
    );

    print(
        out,
        "   : "
    );


    if (out->Mode) {

        print_uint(
            out,
            out->Mode->Mode
        );


        print(
            out,
            " / "
        );


        if (out->Mode->MaxMode > 0)
            print_uint(
                out,
                out->Mode->MaxMode - 1
            );
        else
            print_uint(
                out,
                0
            );
    }


    print(
        out,
        "\n"
    );


    /* --------------------------------------------------------
     * UEFI services
     * -------------------------------------------------------- */

    print_label(
        out,
        "Boot Services"
    );

    print(
        out,
        " : "
    );


    /* после "ebs" st->BootServices указывает на НАШУ таблицу-
       прокладку, а не на прошивку - честно говорим "нет" */
    print_bool(
        out,
        st->BootServices != NULL && !g_kernel_mode
    );


    print(
        out,
        "\n"
    );


    print_label(
        out,
        "Runtime Services"
    );

    print(
        out,
        " : "
    );


    print_bool(
        out,
        st->RuntimeServices != NULL
    );


    print(
        out,
        "\n"
    );


    /* --------------------------------------------------------
     * System table
     * -------------------------------------------------------- */

    print_label(
        out,
        "System Table"
    );

    print(
        out,
        "  : "
    );


    print_uint(
        out,
        (UINT64)(UINTN)st
    );


    print(
        out,
        "\n"
    );


    /* --------------------------------------------------------
     * Memory
     * -------------------------------------------------------- */

    print_separator(out);


    print_label(
        out,
        "Memory"
    );

    print(
        out,
        g_kernel_mode ? "      : final UEFI memory map (see 'mem')\n"
                      : "      : UEFI memory map available\n"
    );


    print_label(
        out,
        "Allocator"
    );

    print(
        out,
        g_kernel_mode ? "   : MyOS bitmap page allocator\n"
                      : "   : EFI Boot Services\n"
    );


    /* --------------------------------------------------------
     * Palette
     *
     * Используем ### вместо "███".
     *
     * Причина:
     * print() работает с char* и не является UTF-8
     * декодером. Символ █ в UTF-8 занимает несколько
     * байтов и раньше мог превращаться в мусор.
     * -------------------------------------------------------- */

    print(
        out,
        "\n"
    );


    print_label(
        out,
        "Colors"
    );

    print(
        out,
        "      : "
    );


    for (UINTN i = 0;
         i < 8;
         i++) {

        set_color(
            out,
            i
        );

        print(
            out,
            "###"
        );
    }


    set_color(
        out,
        0x0F
    );


    print(
        out,
        "\n"
    );


    print_label(
        out,
        "Bright"
    );

    print(
        out,
        "      : "
    );


    for (UINTN i = 8;
         i < 16;
         i++) {

        set_color(
            out,
            i
        );

        print(
            out,
            "###"
        );
    }


    set_color(
        out,
        saved
    );


    print(
        out,
        "\n"
    );


    print_separator(out);


    /* --------------------------------------------------------
     * Footer
     * -------------------------------------------------------- */

    set_color(
        out,
        0x08
    );


    print(
        out,
        "MyOS 0.1 | x86_64 | UEFI | bare-metal"
    );


    set_color(
        out,
        0x0F
    );


    print(
        out,
        "\n\n"
    );
}


/* ============================================================
 * Command history
 * ============================================================ */

static void push_history(
    CHAR16 *line
)
{
    if (char16_len(line) == 0)
        return;


    char16_copy(
        g_history[
            g_history_count % HIST_MAX
        ],
        line,
        LINE_MAX
    );


    g_history_count++;
}


static void cmd_history(
    EFI_SYSTEM_TABLE *st
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;


    if (g_history_count == 0) {

        print(
            out,
            "(no history yet)\n"
        );

        return;
    }


    UINTN shown =
        (g_history_count < HIST_MAX)
        ? g_history_count
        : HIST_MAX;


    UINTN start =
        (g_history_count < HIST_MAX)
        ? 0
        : (g_history_count % HIST_MAX);


    for (UINTN i = 0;
         i < shown;
         i++) {


        UINTN idx =
            (start + i) % HIST_MAX;


        print_uint(
            out,
            g_history_count -
            shown +
            i +
            1
        );


        print(
            out,
            "  "
        );


        print16(
            out,
            g_history[idx]
        );


        print(
            out,
            "\n"
        );
    }
}


/* ============================================================
 * Графическая оболочка ("start")
 *
 *   - рабочий стол с иконками (About / Fetch / Help / Exit),
 *   - панель задач внизу экрана с живыми часами,
 *   - клик по иконке открывает своё окно с текстом,
 *   - у окна есть кнопка закрытия "X",
 *   - курсор двигается стрелками (без мыши),
 *   - Enter работает как клик левой кнопкой мыши,
 *   - Esc закрывает открытое окно, а если окон нет —
 *     выходит из GUI обратно в консоль.
 *
 * Текст рисуется собственным простым битмап-шрифтом
 * 5x7 (см. таблицу gui_font ниже) — никаких внешних
 * зависимостей, просто прямая запись пикселей в
 * framebuffer GOP. Никакого оконного менеджера, никаких
 * слоёв, полный перерисов каждого кадра.
 * ============================================================ */

#define GUI_CURSOR_SIZE   12
#define GUI_CURSOR_STEP   12

/* Чувствительность мыши: условных "пикселей на мм" движения.
   Подбиралось на глаз - если курсор летает слишком быстро/
   медленно на конкретном устройстве, поправить это число. */
#define GUI_MOUSE_PIXELS_PER_MM 8

/*
 * Параметры демо-анимации команды "ebs" (см. run_command):
 * без BootServices->Stall у нас нет откалиброванного таймера,
 * поэтому скорость анимации регулируется только числом итераций
 * пустого busy-wait цикла - грубо, зависит от частоты CPU.
 * Если на конкретной машине анимация слишком быстрая/медленная,
 * поправить EBS_SPIN_PER_STEP.
 */
#define EBS_BOX_SIZE      24
#define EBS_STEPS         160
#define EBS_SPIN_PER_STEP 6000000UL
#define GUI_TASKBAR_H     22
#define GUI_MENU_H        14

/* Пункты меню "Start" теперь рисуются не как значки
   на столе, а как строки выпадающего списка. */
#define GUI_ICON_W        110  /* ширина пункта меню */
#define GUI_ICON_H         18  /* высота пункта меню */
#define GUI_ICON_GAP        0
#define GUI_ICON_COUNT      8

#define GUI_ACT_ABOUT        0
#define GUI_ACT_FETCH        1
#define GUI_ACT_HELP         2
#define GUI_ACT_NOTEPAD      3
#define GUI_ACT_TERMINAL     4
#define GUI_ACT_EXPLORER     5
#define GUI_ACT_MINESWEEPER  6
#define GUI_ACT_EXIT         7

/* Не пункт меню Start, а отдельное "окно" контента:
   открывается кликом по файлу в Проводнике. Держим его
   после всех пунктов меню, чтобы не путать с иконками. */
#define GUI_ACT_FILEVIEW  8

/* ============================================================
 * Сапёр (Minesweeper)
 *
 * Поле фиксированного размера 9x9 с 10 минами (уровень
 * "новичок" классического сапёра) - этого достаточно на
 * любом разумном разрешении экрана, а размер клетки под
 * конкретное окно подбирается динамически в
 * gui_ms_compute_layout(), см. ниже.
 *
 * Управление - в стиле всего остального интерфейса (без
 * мыши): стрелки двигают общий курсор, Enter открывает
 * клетку под курсором (или жмёт улыбающийся смайлик/клетку
 * меню), а клавиша F ставит/снимает флажок на клетке под
 * курсором.
 * ============================================================ */
#define GUI_MS_COLS    9
#define GUI_MS_ROWS    9
#define GUI_MS_MINES   10

/* "Проводник": сколько строк файлов максимум показываем
   в окне (плюс заголовок и, если файлов больше, строка
   "... и ещё N"). */
#define GUI_EXPLORER_MAX_ROWS   10
#define GUI_EXPLORER_LINE_LEN   40

/* Окно "просмотра файла", открываемое кликом по строке
   в Проводнике: показывает содержимое файла построчно. */
#define GUI_FILEVIEW_MAX_ROWS   14
#define GUI_FILEVIEW_LINE_LEN   40

/* Терминал внутри GUI: своё окно с чёрным viewport'ом
   и живым вводом, а не выход из GUI в текстовый режим -
   как отдельное приложение (наподобие kitty), а не
   отдельный режим ОС. */
#define GUI_TERM_MAX_LINES   11
#define GUI_TERM_LINE_LEN    46

/*
 * В efi.h AllocatePool/FreePool объявлены как
 * VOID* (см. заглушки в EFI_BOOT_SERVICES), поэтому
 * приводим их к нормальным сигнатурам сами —
 * это нужно для двойной буферизации кадра (фикс
 * мерцания при перерисовке).
 */
typedef EFI_STATUS (EFIAPI *GUI_ALLOCATE_POOL)(
    UINTN PoolType, UINTN Size, VOID **Buffer
);

typedef EFI_STATUS (EFIAPI *GUI_FREE_POOL)(
    VOID *Buffer
);

#define GUI_EFI_BOOT_SERVICES_DATA 4

/*
 * Аналогично приводим GetMemoryMap/ExitBootServices - в efi.h
 * это тоже голые VOID* (см. заглушки в EFI_BOOT_SERVICES).
 * MemoryMap оставляем как VOID*: для самого ExitBootServices
 * важен только корректный MapKey, разбирать записи карты
 * памяти по дескрипторам этому шагу пока не нужно.
 */
typedef EFI_STATUS (EFIAPI *GUI_GET_MEMORY_MAP)(
    UINTN *MemoryMapSize,
    VOID *MemoryMap,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
);

typedef EFI_STATUS (EFIAPI *GUI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE ImageHandle,
    UINTN MapKey
);

/*
 * AllocatePages - тоже голый VOID* в efi.h. Нужна отдельно от
 * AllocatePool: буферы xHCI (Device Context Base Address Array,
 * Command Ring) обязаны лежать на 64-байтной границе (спека
 * xHCI), а AllocatePool такого не гарантирует. AllocatePages
 * всегда отдаёт память, выровненную по границе страницы (4 КиБ) -
 * с большим запасом достаточно.
 */
typedef EFI_STATUS (EFIAPI *GUI_ALLOCATE_PAGES)(
    UINTN Type,
    UINTN MemoryType,
    UINTN Pages,
    UINT64 *Memory
);

#define GUI_ALLOCATE_ANY_PAGES 0

/*
 * Упаковать R,G,B в 32-битный пиксель под
 * тот PixelFormat, который реально отдаёт GOP.
 * Неизвестные/BLT-only форматы просто трактуем
 * как BGR — это самый частый случай на практике
 * (QEMU/OVMF, VirtualBox).
 */
static UINT32 gui_pack(
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    UINT8 r, UINT8 g, UINT8 b
)
{
    if (fmt == PixelRedGreenBlueReserved8BitPerColor) {

        return (UINT32)r |
               ((UINT32)g << 8) |
               ((UINT32)b << 16);
    }

    return (UINT32)b |
           ((UINT32)g << 8) |
           ((UINT32)r << 16);
}


static void gui_fill_rect(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN w, UINTN h,
    UINT32 color
)
{
    if (x < 0) {

        if ((UINTN)(-x) >= w)
            return;

        w -= (UINTN)(-x);
        x = 0;
    }

    if (y < 0) {

        if ((UINTN)(-y) >= h)
            return;

        h -= (UINTN)(-y);
        y = 0;
    }

    if (x >= (INTN)fb_w || y >= (INTN)fb_h)
        return;

    if ((UINTN)x + w > fb_w)
        w = fb_w - (UINTN)x;

    if ((UINTN)y + h > fb_h)
        h = fb_h - (UINTN)y;

    for (UINTN row = 0; row < h; row++) {

        volatile UINT32 *dst =
            fb +
            (UINTN)(y + (INTN)row) * stride +
            (UINTN)x;

        for (UINTN col = 0; col < w; col++)
            dst[col] = color;
    }
}


/*
 * Нарисовать рамку прямоугольника (только контур,
 * толщиной 1 логический пиксель * scale).
 */
static void gui_draw_border(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN w, UINTN h,
    UINT32 color
)
{
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y, w, 1, color);
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y + (INTN)h - 1, w, 1, color);
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y, 1, h, color);
    gui_fill_rect(fb, stride, fb_w, fb_h, x + (INTN)w - 1, y, 1, h, color);
}


/*
 * Рамка в стиле "объёмных" 3D-кнопок/панелей ранних
 * графических интерфейсов (Win9x и подобных): двойная
 * обводка светлым/тёмным, создающая иллюзию выпуклости
 * (raised == TRUE) или вдавленности (raised == FALSE).
 * Это НЕ копия чужих ассетов, просто тот же общий приём
 * рисования "фаски" сплошными прямоугольниками.
 */
static void gui_draw_bevel(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN w, UINTN h,
    UINT32 c_hi,     /* внешняя светлая грань   */
    UINT32 c_light,  /* внутренняя светлая грань */
    UINT32 c_shadow, /* внутренняя тёмная грань  */
    UINT32 c_dark,   /* внешняя тёмная грань     */
    BOOLEAN raised
)
{
    UINT32 out_tl = raised ? c_hi     : c_dark;
    UINT32 out_br = raised ? c_dark   : c_hi;
    UINT32 in_tl  = raised ? c_light  : c_shadow;
    UINT32 in_br  = raised ? c_shadow : c_light;

    /* внешняя грань */
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y, w, 1, out_tl);
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y, 1, h, out_tl);
    gui_fill_rect(fb, stride, fb_w, fb_h, x, y + (INTN)h - 1, w, 1, out_br);
    gui_fill_rect(fb, stride, fb_w, fb_h, x + (INTN)w - 1, y, 1, h, out_br);

    /* внутренняя грань (только если есть место) */
    if (w > 2 && h > 2) {

        gui_fill_rect(fb, stride, fb_w, fb_h, x + 1, y + 1, w - 2, 1, in_tl);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 1, y + 1, 1, h - 2, in_tl);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 1, y + (INTN)h - 2, w - 2, 1, in_br);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + (INTN)w - 2, y + 1, 1, h - 2, in_br);
    }
}


/* Автоматически сгенерированный компактный битмап-шрифт 5x7. */
typedef struct { char ch; UINT8 rows[7]; } GUI_GLYPH;

static const GUI_GLYPH gui_font[] = {
    { ' ', { 0, 0, 0, 0, 0, 0, 0 } },
    { '!', { 4, 4, 4, 4, 0, 4, 0 } },
    { '\'', { 4, 4, 0, 0, 0, 0, 0 } },
    { '-', { 0, 0, 0, 14, 0, 0, 0 } },
    { '.', { 0, 0, 0, 0, 0, 4, 0 } },
    { '0', { 14, 17, 19, 21, 25, 17, 14 } },
    { '1', { 4, 12, 4, 4, 4, 4, 14 } },
    { '2', { 14, 17, 1, 2, 4, 8, 31 } },
    { '3', { 14, 17, 1, 6, 1, 17, 14 } },
    { '4', { 2, 6, 10, 18, 31, 2, 2 } },
    { '5', { 31, 16, 30, 1, 1, 17, 14 } },
    { '6', { 14, 16, 30, 17, 17, 17, 14 } },
    { '7', { 31, 1, 2, 4, 8, 8, 8 } },
    { '8', { 14, 17, 17, 14, 17, 17, 14 } },
    { '9', { 14, 17, 17, 15, 1, 17, 14 } },
    { ':', { 0, 4, 0, 0, 4, 0, 0 } },
    { ',', { 0, 0, 0, 0, 0, 4, 8 } },
    { '(', { 2, 4, 8, 8, 8, 4, 2 } },
    { ')', { 8, 4, 2, 2, 2, 4, 8 } },
    { '=', { 0, 0, 31, 0, 31, 0, 0 } },
    { '/', { 1, 1, 2, 4, 8, 16, 16 } },
    { '_', { 0, 0, 0, 0, 0, 0, 31 } },
    { 'A', { 14, 17, 17, 31, 17, 17, 17 } },
    { 'B', { 30, 17, 17, 30, 17, 17, 30 } },
    { 'C', { 15, 16, 16, 16, 16, 16, 15 } },
    { 'D', { 30, 17, 17, 17, 17, 17, 30 } },
    { 'E', { 31, 16, 16, 30, 16, 16, 31 } },
    { 'F', { 31, 16, 16, 30, 16, 16, 16 } },
    { 'G', { 15, 16, 16, 19, 17, 17, 15 } },
    { 'H', { 17, 17, 17, 31, 17, 17, 17 } },
    { 'I', { 14, 4, 4, 4, 4, 4, 14 } },
    { 'J', { 1, 1, 1, 1, 17, 17, 14 } },
    { 'K', { 17, 18, 20, 24, 20, 18, 17 } },
    { 'L', { 16, 16, 16, 16, 16, 16, 31 } },
    { 'M', { 17, 27, 21, 17, 17, 17, 17 } },
    { 'N', { 17, 25, 21, 19, 17, 17, 17 } },
    { 'O', { 14, 17, 17, 17, 17, 17, 14 } },
    { 'P', { 30, 17, 17, 30, 16, 16, 16 } },
    { 'Q', { 14, 17, 17, 17, 21, 18, 13 } },
    { 'R', { 30, 17, 17, 30, 20, 18, 17 } },
    { 'S', { 15, 16, 16, 14, 1, 1, 30 } },
    { 'T', { 31, 4, 4, 4, 4, 4, 4 } },
    { 'U', { 17, 17, 17, 17, 17, 17, 14 } },
    { 'V', { 17, 17, 17, 17, 17, 10, 4 } },
    { 'W', { 17, 17, 17, 17, 21, 27, 17 } },
    { 'X', { 17, 17, 10, 4, 10, 17, 17 } },
    { 'Y', { 17, 17, 10, 4, 4, 4, 4 } },
    { 'Z', { 31, 1, 2, 4, 8, 16, 31 } },
};

#define GUI_FONT_COUNT (sizeof(gui_font) / sizeof(gui_font[0]))


static const GUI_GLYPH *gui_find_glyph(char c)
{
    for (UINTN i = 0; i < GUI_FONT_COUNT; i++) {

        if (gui_font[i].ch == c)
            return &gui_font[i];
    }

    return NULL;
}


/*
 * Нарисовать один символ шрифтом 5x7, увеличенным
 * в scale раз. Возвращает ширину символа в пикселях
 * (вместе с межсимвольным интервалом), чтобы вызывающий
 * код мог сдвинуть x для следующего символа.
 */
static UINTN gui_draw_char(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN scale,
    UINT32 color,
    char c
)
{
    const GUI_GLYPH *glyph = gui_find_glyph(c);

    if (glyph != NULL) {

        for (UINTN row = 0; row < 7; row++) {

            UINT8 bits = glyph->rows[row];

            for (UINTN col = 0; col < 5; col++) {

                if (bits & (1 << (4 - col))) {

                    gui_fill_rect(
                        fb, stride, fb_w, fb_h,
                        x + (INTN)(col * scale),
                        y + (INTN)(row * scale),
                        scale, scale,
                        color
                    );
                }
            }
        }
    }

    return (5 * scale) + scale;
}


static UINTN gui_draw_text(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN scale,
    UINT32 color,
    const char *s
)
{
    INTN start_x = x;

    while (*s != '\0') {

        x += (INTN)gui_draw_char(
            fb, stride, fb_w, fb_h,
            x, y, scale, color, *s
        );

        s++;
    }

    return (UINTN)(x - start_x);
}


static UINTN gui_text_width(const char *s, UINTN scale)
{
    UINTN w = 0;

    while (*s != '\0') {
        w += (5 * scale) + scale;
        s++;
    }

    return w;
}


/*
 * Перевести число в десятичную строку (без libc).
 * Возвращает длину строки, buf должен вмещать
 * минимум 21 байт (64-битное число + '\0').
 */
static UINTN gui_uint_to_str(UINT64 v, char *buf)
{
    char tmp[21];
    UINTN n = 0;

    if (v == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    while (v > 0) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }

    for (UINTN i = 0; i < n; i++)
        buf[i] = tmp[n - 1 - i];

    buf[n] = '\0';

    return n;
}


/*
 * Шестнадцатеричная печать с ведущими нулями до нужной
 * ширины - аналог print_hex(), но пишет в char*-буфер для
 * gui_draw_text() (пиксельный шрифт), а не в ConOut. Нужна
 * именно эта отдельная версия, потому что после
 * ExitBootServices печатать через ConOut уже нельзя, а
 * показать MMIO-адрес найденного xHCI как-то надо.
 */
static UINTN gui_hex_to_str(UINT64 v, UINTN digits, char *buf)
{
    static const char hex_chars[] = "0123456789ABCDEF";

    if (digits > 16)
        digits = 16;

    for (UINTN i = 0; i < digits; i++) {

        UINTN shift = (digits - 1 - i) * 4;

        buf[i] = hex_chars[(v >> shift) & 0xF];
    }

    buf[digits] = '\0';

    return digits;
}


/*
 * Дополнить число слева нулём до 2 цифр
 * (для часов/минут/секунд в панели задач).
 */
static UINTN gui_uint2_to_str(UINT32 v, char *buf)
{
    if (v > 99)
        v = 99;

    buf[0] = (char)('0' + (v / 10));
    buf[1] = (char)('0' + (v % 10));
    buf[2] = '\0';

    return 2;
}


typedef struct {
    INTN x, y;
    UINTN w, h;
    const char *label;
    UINT8 action;
} GUI_ICON;


typedef struct {
    const char *title;
    const char **lines;
    UINTN line_count;
} GUI_WINDOW_CONTENT;


/*
 * Состояние поля Сапёра. Живёт локально в gui_start()
 * (как и term_lines/explorer_buf), сюда только объявление
 * типа - конкретный экземпляр создаётся в gui_start().
 */
typedef struct {
    UINT8 mine[GUI_MS_ROWS][GUI_MS_COLS];
    UINT8 adj[GUI_MS_ROWS][GUI_MS_COLS];
    UINT8 revealed[GUI_MS_ROWS][GUI_MS_COLS];
    UINT8 flagged[GUI_MS_ROWS][GUI_MS_COLS];

    BOOLEAN generated;  /* мины расставлены? (лениво, после 1-го клика) */
    BOOLEAN over;       /* игра закончена (выигрыш или проигрыш)       */
    BOOLEAN won;

    UINTN flags_used;
    UINTN revealed_count;
    UINTN timer;

    INTN boom_r, boom_c; /* какая мина взорвалась (для красной клетки) */
} GUI_MS_STATE;


/*
 * Геометрия окна Сапёра, пересчитывается заново на каждый
 * кадр из общего win_x/win_y/win_w/win_h (то же самое окно,
 * что и у всех остальных программ) - один источник истины
 * для отрисовки И для определения клика по клетке/смайлику.
 */
typedef struct {
    INTN  field_x, field_y;
    UINTN field_w, field_h;

    INTN  head_x, head_y;
    UINTN head_w, head_h;

    INTN  grid_x, grid_y;
    UINTN cell;

    INTN  smile_x, smile_y;
    UINTN smile_size;

    INTN  led1_x, led1_y;
    INTN  led2_x, led2_y;
    UINTN led_w, led_h;
} GUI_MS_LAYOUT;


static BOOLEAN gui_point_in_rect(
    INTN px, INTN py,
    INTN x, INTN y, UINTN w, UINTN h
)
{
    return px >= x && px < x + (INTN)w &&
           py >= y && py < y + (INTN)h;
}


/* ============================================================
 * Сапёр: генератор случайных чисел, логика поля, геометрия
 * ============================================================ */

/* Простой LCG. Точная криптостойкость тут не нужна - только
   чтобы мины не лежали каждый раз одинаково. */
static UINT32 g_ms_rng = 12345;

static UINT32 gui_ms_rand(void)
{
    g_ms_rng = g_ms_rng * 1103515245u + 12345u;
    return (g_ms_rng >> 16) & 0x7fffu;
}

/* Затравка от текущего времени (RTC), чтобы расклад мин
   отличался между запусками, а не только между партиями
   внутри одного запуска. */
static void gui_ms_seed(EFI_SYSTEM_TABLE *st)
{
    UINT32 seed = 0x9e3779b9u;

    EFI_TIME t;

    if (
        st->RuntimeServices->GetTime &&
        st->RuntimeServices->GetTime(&t, NULL) == EFI_SUCCESS
    ) {
        seed ^= ((UINT32)t.Year     << 20) ^
                ((UINT32)t.Month    << 16) ^
                ((UINT32)t.Day      << 11) ^
                ((UINT32)t.Hour     << 22) ^
                ((UINT32)t.Minute   <<  6) ^
                ((UINT32)t.Second)         ^
                t.Nanosecond;
    }

    if (seed == 0)
        seed = 12345;

    g_ms_rng = seed;
}


static void gui_ms_reset(GUI_MS_STATE *ms)
{
    for (UINTN r = 0; r < GUI_MS_ROWS; r++) {
        for (UINTN c = 0; c < GUI_MS_COLS; c++) {
            ms->mine[r][c]     = 0;
            ms->adj[r][c]      = 0;
            ms->revealed[r][c] = 0;
            ms->flagged[r][c]  = 0;
        }
    }

    ms->generated      = FALSE;
    ms->over           = FALSE;
    ms->won            = FALSE;
    ms->flags_used     = 0;
    ms->revealed_count = 0;
    ms->timer          = 0;
    ms->boom_r         = -1;
    ms->boom_c         = -1;
}


/*
 * Расставить мины, избегая клетки первого клика и её
 * соседей (классическое поведение "первый клик всегда
 * безопасен"), затем посчитать числа-подсказки.
 */
static void gui_ms_generate(
    GUI_MS_STATE *ms, int safe_r, int safe_c
)
{
    UINTN placed = 0;

    while (placed < GUI_MS_MINES) {

        UINT32 rv = gui_ms_rand();

        int r = (int)(rv % GUI_MS_ROWS);
        int c = (int)((rv / GUI_MS_ROWS) % GUI_MS_COLS);

        int dr = r - safe_r;
        int dc = c - safe_c;

        if (dr < 0) dr = -dr;
        if (dc < 0) dc = -dc;

        if (dr <= 1 && dc <= 1)
            continue;

        if (ms->mine[r][c])
            continue;

        ms->mine[r][c] = 1;
        placed++;
    }

    for (int r = 0; r < GUI_MS_ROWS; r++) {

        for (int c = 0; c < GUI_MS_COLS; c++) {

            if (ms->mine[r][c])
                continue;

            UINTN n = 0;

            for (int dr = -1; dr <= 1; dr++) {

                for (int dc = -1; dc <= 1; dc++) {

                    if (dr == 0 && dc == 0)
                        continue;

                    int rr = r + dr;
                    int cc = c + dc;

                    if (rr < 0 || rr >= GUI_MS_ROWS ||
                        cc < 0 || cc >= GUI_MS_COLS)
                        continue;

                    if (ms->mine[rr][cc])
                        n++;
                }
            }

            ms->adj[r][c] = (UINT8)n;
        }
    }

    ms->generated = TRUE;
}


/*
 * Открыть клетку (r,c). Мины ещё нет на поле до первого
 * клика - она расставляется прямо тут, в первый раз.
 * Клетка с 0 соседних мин "заливает" соседей рекурсивно
 * (итеративно, через явный стек - без настоящей рекурсии).
 */
static void gui_ms_reveal(GUI_MS_STATE *ms, int start_r, int start_c)
{
    if (ms->over)
        return;

    if (!ms->generated)
        gui_ms_generate(ms, start_r, start_c);

    if (ms->flagged[start_r][start_c] ||
        ms->revealed[start_r][start_c])
        return;

    if (ms->mine[start_r][start_c]) {

        ms->revealed[start_r][start_c] = 1;
        ms->over   = TRUE;
        ms->won    = FALSE;
        ms->boom_r = start_r;
        ms->boom_c = start_c;

        /* Проигрыш: показать все мины на поле. */
        for (int r = 0; r < GUI_MS_ROWS; r++)
            for (int c = 0; c < GUI_MS_COLS; c++)
                if (ms->mine[r][c])
                    ms->revealed[r][c] = 1;

        return;
    }

    UINT8 queued[GUI_MS_ROWS][GUI_MS_COLS];

    for (UINTN r = 0; r < GUI_MS_ROWS; r++)
        for (UINTN c = 0; c < GUI_MS_COLS; c++)
            queued[r][c] = 0;

    int stack_r[GUI_MS_ROWS * GUI_MS_COLS];
    int stack_c[GUI_MS_ROWS * GUI_MS_COLS];
    int sp = 0;

    stack_r[sp] = start_r;
    stack_c[sp] = start_c;
    sp++;
    queued[start_r][start_c] = 1;

    while (sp > 0) {

        sp--;

        int r = stack_r[sp];
        int c = stack_c[sp];

        if (ms->revealed[r][c] || ms->flagged[r][c])
            continue;

        ms->revealed[r][c] = 1;
        ms->revealed_count++;

        if (ms->adj[r][c] != 0)
            continue;

        for (int dr = -1; dr <= 1; dr++) {

            for (int dc = -1; dc <= 1; dc++) {

                if (dr == 0 && dc == 0)
                    continue;

                int rr = r + dr;
                int cc = c + dc;

                if (rr < 0 || rr >= GUI_MS_ROWS ||
                    cc < 0 || cc >= GUI_MS_COLS)
                    continue;

                if (ms->revealed[rr][cc] ||
                    ms->flagged[rr][cc] ||
                    ms->mine[rr][cc] ||
                    queued[rr][cc])
                    continue;

                queued[rr][cc] = 1;
                stack_r[sp] = rr;
                stack_c[sp] = cc;
                sp++;
            }
        }
    }

    if (
        ms->revealed_count ==
        (UINTN)(GUI_MS_ROWS * GUI_MS_COLS) - GUI_MS_MINES
    ) {
        ms->won  = TRUE;
        ms->over = TRUE;

        /* Красиво доставить флажки на оставшиеся мины. */
        for (int r = 0; r < GUI_MS_ROWS; r++) {
            for (int c = 0; c < GUI_MS_COLS; c++) {

                if (ms->mine[r][c] && !ms->flagged[r][c]) {
                    ms->flagged[r][c] = 1;
                    ms->flags_used++;
                }
            }
        }
    }
}


static void gui_ms_toggle_flag(GUI_MS_STATE *ms, int r, int c)
{
    if (ms->over || ms->revealed[r][c])
        return;

    if (ms->flagged[r][c]) {

        ms->flagged[r][c] = 0;
        ms->flags_used--;

    } else {

        if (ms->flags_used >= GUI_MS_MINES)
            return;

        ms->flagged[r][c] = 1;
        ms->flags_used++;
    }
}


/*
 * Единственное место, где считается геометрия окна Сапёра -
 * и отрисовка, и обработка кликов берут клетки/кнопки строго
 * отсюда, чтобы никогда не разъехаться друг с другом.
 */
static void gui_ms_compute_layout(
    INTN win_x, INTN win_y, UINTN win_w, UINTN win_h,
    GUI_MS_LAYOUT *L
)
{
    L->field_x = win_x + 9;
    L->field_y = win_y + 31 + (INTN)GUI_MENU_H;
    L->field_w = win_w - 18;
    L->field_h = win_h - 42 - GUI_MENU_H;

    L->head_x = L->field_x + 6;
    L->head_y = L->field_y + 6;
    L->head_w = (L->field_w > 12) ? L->field_w - 12 : L->field_w;
    L->head_h = 32;

    L->led_w = 40;
    L->led_h = 20;

    L->led1_x = L->head_x + 6;
    L->led1_y = L->head_y + ((INTN)L->head_h - (INTN)L->led_h) / 2;

    L->led2_x = L->head_x + (INTN)L->head_w - 6 - (INTN)L->led_w;
    L->led2_y = L->led1_y;

    L->smile_size = 24;
    L->smile_x = L->head_x + (INTN)L->head_w / 2 - (INTN)L->smile_size / 2;
    L->smile_y = L->head_y + ((INTN)L->head_h - (INTN)L->smile_size) / 2;

    INTN  grid_area_x = L->head_x;
    INTN  grid_area_y = L->head_y + (INTN)L->head_h + 9;
    UINTN grid_area_w = L->head_w;

    INTN grid_area_bottom = L->field_y + (INTN)L->field_h - 6;
    UINTN grid_area_h =
        (grid_area_bottom > grid_area_y)
            ? (UINTN)(grid_area_bottom - grid_area_y)
            : 1;

    UINTN cell_w = grid_area_w / GUI_MS_COLS;
    UINTN cell_h = grid_area_h / GUI_MS_ROWS;
    UINTN cell   = (cell_w < cell_h) ? cell_w : cell_h;

    if (cell > 24)
        cell = 24;

    if (cell < 10)
        cell = 10;

    L->cell = cell;

    UINTN grid_w_px = GUI_MS_COLS * cell;
    UINTN grid_h_px = GUI_MS_ROWS * cell;

    L->grid_x = grid_area_x + (INTN)(grid_area_w - grid_w_px) / 2;
    L->grid_y = grid_area_y + (INTN)(grid_area_h - grid_h_px) / 2;
}


/* Цвет цифры-подсказки - как в оригинальном Сапёре
   (1 синий, 2 зелёный, 3 красный, 4 тёмно-синий, ...). */
static UINT32 gui_ms_number_color(
    EFI_GRAPHICS_PIXEL_FORMAT fmt, UINT8 n
)
{
    switch (n) {
        case 1: return gui_pack(fmt,   0,   0, 255);
        case 2: return gui_pack(fmt,   0, 128,   0);
        case 3: return gui_pack(fmt, 255,   0,   0);
        case 4: return gui_pack(fmt,   0,   0, 128);
        case 5: return gui_pack(fmt, 128,   0,   0);
        case 6: return gui_pack(fmt,   0, 128, 128);
        case 7: return gui_pack(fmt,   0,   0,   0);
        default: return gui_pack(fmt, 128, 128, 128);
    }
}


/*
 * Значок мины: закрашенный кружок с 8 "усиками" и белым
 * бликом, посчитанный от расстояния до центра - не нужно
 * хранить готовый битмап руками.
 */
static void gui_draw_icon_mine(
    volatile UINT32 *fb,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    INTN x, INTN y,
    UINT32 body, UINT32 hi
)
{
    const INTN c0 = 5;

    for (INTN r = 0; r < 11; r++) {

        for (INTN c = 0; c < 11; c++) {

            INTN dx = c - c0;
            INTN dy = r - c0;
            INTN ax = (dx < 0) ? -dx : dx;
            INTN ay = (dy < 0) ? -dy : dy;

            BOOLEAN on = FALSE;

            if (dx * dx + dy * dy <= 9)
                on = TRUE;
            else if (dx == 0 && ay >= 4)
                on = TRUE;
            else if (dy == 0 && ax >= 4)
                on = TRUE;
            else if (ax == 4 && ay == 4)
                on = TRUE;

            if (!on)
                continue;

            UINT32 col = (dx == -1 && dy == -1) ? hi : body;

            gui_fill_rect(
                fb, stride, fb_w, fb_h,
                x + c, y + r, 1, 1, col
            );
        }
    }
}


/* Значок флажка: флагшток + треугольный флаг + подставка. */
static void gui_draw_icon_flag(
    volatile UINT32 *fb,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    INTN x, INTN y,
    UINT32 pole, UINT32 flag, UINT32 base
)
{
    gui_fill_rect(fb, stride, fb_w, fb_h, x + 5, y + 1, 1, 8, pole);

    gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 1, 4, 1, flag);
    gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 2, 3, 1, flag);
    gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 3, 2, 1, flag);
    gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 4, 1, 1, flag);

    gui_fill_rect(fb, stride, fb_w, fb_h, x + 3, y + 9, 5, 1, base);
}


/*
 * Лицо кнопки "рестарт": mode 0 = обычное, 1 = победа
 * (тёмные очки), 2 = проигрыш (крестики вместо глаз).
 */
static void gui_draw_face(
    volatile UINT32 *fb,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN x, INTN y, int mode
)
{
    UINT32 yellow = gui_pack(fmt, 255, 216, 0);
    UINT32 black  = gui_pack(fmt,   0,   0, 0);

    const INTN c0 = 8;
    const INTN R  = 8;

    for (INTN r = 0; r < 17; r++) {

        for (INTN c = 0; c < 17; c++) {

            INTN dx = c - c0;
            INTN dy = r - c0;
            INTN d2 = dx * dx + dy * dy;

            if (d2 > R * R)
                continue;

            UINT32 col =
                (d2 >= (R - 1) * (R - 1)) ? black : yellow;

            gui_fill_rect(
                fb, stride, fb_w, fb_h,
                x + c, y + r, 1, 1, col
            );
        }
    }

    if (mode == 2) {

        /* проигрыш: крестики вместо глаз */
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 4, y + 5, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 5, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 5, y + 6, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 4, y + 7, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 6, y + 7, 1, 1, black);

        gui_fill_rect(fb, stride, fb_w, fb_h, x + 10, y + 5, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 12, y + 5, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 11, y + 6, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 10, y + 7, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 12, y + 7, 1, 1, black);

        gui_fill_rect(fb, stride, fb_w, fb_h, x + 5, y + 12, 7, 1, black);

    } else if (mode == 1) {

        /* победа: тёмные очки */
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 3, y + 6, 4, 2, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 10, y + 6, 4, 2, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 7, y + 6, 3, 1, black);

        gui_fill_rect(fb, stride, fb_w, fb_h, x + 4, y + 11, 9, 2, black);

    } else {

        /* обычное лицо: два глаза-точки и улыбка */
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 5, y + 6, 2, 2, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 10, y + 6, 2, 2, black);

        gui_fill_rect(fb, stride, fb_w, fb_h, x + 4, y + 10, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 12, y + 10, 1, 1, black);
        gui_fill_rect(fb, stride, fb_w, fb_h, x + 5, y + 11, 7, 1, black);
    }
}


/*
 * Экран рабочего стола: фон, иконки (с подсветкой
 * той, над которой сейчас курсор), панель задач
 * с часами и курсор поверх всего.
 */
/*
 * Курсор мыши рисуется ОТДЕЛЬНО от остальной картинки.
 *
 * Раньше любое движение мыши означало "перерисовать весь экран":
 * весь рабочий стол/окно заново в задний буфер (около миллиона
 * пикселей) и потом весь буфер в видеопамять (ещё миллион). Мышь
 * присылает десятки-сотни движений в секунду, а такой кадр
 * стоит миллисекунды - отсюда ощущение "5-15 FPS на мышке".
 *
 * Теперь задний буфер хранит картинку БЕЗ курсора, а курсор
 * рисуется только в видеопамяти, поверх. Когда сдвинулся ТОЛЬКО
 * курсор (и под ним ничего не должно поменяться - например,
 * подсветка пункта меню), достаточно: вернуть из заднего буфера
 * кусочек 12x12 там, где курсор был, и нарисовать курсор на новом
 * месте. Это ~300 пикселей вместо двух миллионов.
 *
 * g_gui_draw_cursor = TRUE - старое поведение (курсор рисуют сами
 * функции кадра); используется, если заднего буфера нет вообще.
 */
static BOOLEAN g_gui_draw_cursor = TRUE;

static void gui_draw_cursor_at(
    volatile UINT32 *fb,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN x, INTN y
)
{
    /* те же цвета, что у курсора внутри функций кадра:
       чёрная рамка + белая заливка */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        x, y, GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
        gui_pack(fmt, 0, 0, 0)
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        x + 2, y + 2, GUI_CURSOR_SIZE - 4, GUI_CURSOR_SIZE - 4,
        gui_pack(fmt, 255, 255, 255)
    );
}

/* Скопировать прямоугольник из заднего буфера в видеопамять */
static void gui_blit_rect(
    volatile UINT32 *dst,
    volatile UINT32 *src,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    INTN x, INTN y, UINTN w, UINTN h
)
{
    if (x < 0) { if ((UINTN)(-x) >= w) return; w -= (UINTN)(-x); x = 0; }
    if (y < 0) { if ((UINTN)(-y) >= h) return; h -= (UINTN)(-y); y = 0; }

    if (x >= (INTN)fb_w || y >= (INTN)fb_h)
        return;

    if ((UINTN)x + w > fb_w)
        w = fb_w - (UINTN)x;

    if ((UINTN)y + h > fb_h)
        h = fb_h - (UINTN)y;

    for (UINTN row = 0; row < h; row++) {

        UINTN base = (UINTN)(y + (INTN)row) * stride + (UINTN)x;

        for (UINTN col = 0; col < w; col++)
            dst[base + col] = src[base + col];
    }
}


static void gui_draw_desktop(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    const GUI_ICON *icons,
    UINTN icon_count,
    BOOLEAN menu_open,
    INTN btn_x, INTN btn_y, UINTN btn_w, UINTN btn_h,
    INTN cur_x, INTN cur_y,
    const char *clock_text
)
{
    /* Классическая "объёмная" серо-бирюзовая палитра */
    UINT32 col_bg       = gui_pack(fmt, 0, 128, 128);   /* бирюза рабочего стола */
    UINT32 col_border   = gui_pack(fmt, 0, 0, 0);
    UINT32 col_face     = gui_pack(fmt, 192, 192, 192); /* серая "поверхность" */
    UINT32 col_hi       = gui_pack(fmt, 255, 255, 255);
    UINT32 col_light    = gui_pack(fmt, 223, 223, 223);
    UINT32 col_shadow   = gui_pack(fmt, 128, 128, 128);
    UINT32 col_sel      = gui_pack(fmt, 0, 0, 128);     /* тёмно-синее выделение */
    UINT32 col_text     = gui_pack(fmt, 0, 0, 0);
    UINT32 col_text_sel = gui_pack(fmt, 255, 255, 255);
    UINT32 col_cursor   = gui_pack(fmt, 255, 255, 255);

    INTN cx = cur_x + GUI_CURSOR_SIZE / 2;
    INTN cy = cur_y + GUI_CURSOR_SIZE / 2;

    /* Рабочий стол: теперь пустой, все программы
       запускаются из меню "Start", как на референсе. */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        0, 0, fb_w, fb_h,
        col_bg
    );

    /* Панель задач: приподнятая серая панель во всю
       ширину экрана СВЕРХУ, слева - кнопка "Start",
       справа - вдавленные "часы". */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        0, 0,
        fb_w, GUI_TASKBAR_H,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        0, 0,
        fb_w, GUI_TASKBAR_H,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    {
        /* Пока меню открыто, кнопка выглядит "вдавленной" -
           так же, как в Windows 95. */
        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            btn_x, btn_y, btn_w, btn_h,
            col_face
        );

        gui_draw_bevel(
            fb, stride, fb_w, fb_h,
            btn_x, btn_y, btn_w, btn_h,
            col_hi, col_light, col_shadow, col_border,
            !menu_open
        );

        INTN txt_x = btn_x + (menu_open ? 8 : 7);

        /* имитация "жирного" текста двойной прорисовкой */
        gui_draw_text(
            fb, stride, fb_w, fb_h,
            txt_x, btn_y + (INTN)btn_h / 2 - 3,
            1, col_text,
            "START"
        );

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            txt_x + 1, btn_y + (INTN)btn_h / 2 - 3,
            1, col_text,
            "START"
        );
    }

    {
        UINTN clock_w = gui_text_width(clock_text, 1) + 12;
        UINTN clock_h = (UINTN)GUI_TASKBAR_H - 6;
        INTN  clock_x = (INTN)fb_w - (INTN)clock_w - 3;
        INTN  clock_y = 3;

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            clock_x, clock_y, clock_w, clock_h,
            col_face
        );

        gui_draw_bevel(
            fb, stride, fb_w, fb_h,
            clock_x, clock_y, clock_w, clock_h,
            col_hi, col_light, col_shadow, col_border,
            FALSE
        );

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            clock_x + 6, clock_y + (INTN)clock_h / 2 - 3,
            1, col_text,
            clock_text
        );
    }

    /* Меню "Start": выпадающая панель со списком программ,
       раскрывается вниз прямо под кнопкой. Каждый пункт -
       строка с подсветкой под курсором, как на референсе. */
    if (menu_open && icon_count > 0) {

        INTN  menu_x = icons[0].x;
        INTN  menu_y = icons[0].y;
        UINTN menu_w = icons[0].w;
        UINTN menu_h = icon_count * icons[0].h;

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            menu_x, menu_y, menu_w, menu_h,
            col_face
        );

        gui_draw_bevel(
            fb, stride, fb_w, fb_h,
            menu_x, menu_y, menu_w, menu_h,
            col_hi, col_light, col_shadow, col_border,
            TRUE
        );

        for (UINTN i = 0; i < icon_count; i++) {

            const GUI_ICON *ic = &icons[i];

            BOOLEAN hover = gui_point_in_rect(
                cx, cy, ic->x, ic->y, ic->w, ic->h
            );

            if (hover) {
                gui_fill_rect(
                    fb, stride, fb_w, fb_h,
                    ic->x + 2, ic->y + 1,
                    ic->w - 4, ic->h - 2,
                    col_sel
                );
            }

            gui_draw_text(
                fb, stride, fb_w, fb_h,
                ic->x + 8, ic->y + (INTN)ic->h / 2 - 3,
                1, hover ? col_text_sel : col_text,
                ic->label
            );
        }
    }

    /* Курсор: чёрная рамка + белая заливка */
    if (g_gui_draw_cursor) {

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            cur_x, cur_y,
            GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
            col_border
        );

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            cur_x + 2, cur_y + 2,
            GUI_CURSOR_SIZE - 4, GUI_CURSOR_SIZE - 4,
            col_cursor
        );
    }
}


/*
 * Окно приложения: заголовок, кнопка закрытия "X",
 * несколько строк текста и курсор поверх всего.
 */
static void gui_draw_window(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN win_x, INTN win_y,
    UINTN win_w, UINTN win_h,
    INTN btn_x, INTN btn_y,
    UINTN btn_size,
    const GUI_WINDOW_CONTENT *content,
    INTN cur_x, INTN cur_y
)
{
    UINT32 col_bg      = gui_pack(fmt, 0, 128, 128);
    UINT32 col_border  = gui_pack(fmt, 0, 0, 0);
    UINT32 col_hi      = gui_pack(fmt, 255, 255, 255);
    UINT32 col_light   = gui_pack(fmt, 223, 223, 223);
    UINT32 col_shadow  = gui_pack(fmt, 128, 128, 128);
    UINT32 col_title   = gui_pack(fmt, 0, 0, 128);   /* тёмно-синий заголовок */
    UINT32 col_ttext   = gui_pack(fmt, 255, 255, 255);
    UINT32 col_face    = gui_pack(fmt, 192, 192, 192);
    UINT32 col_win     = gui_pack(fmt, 255, 255, 255); /* белая "бумага" внутри */
    UINT32 col_text    = gui_pack(fmt, 0, 0, 0);
    UINT32 col_btn_tx  = gui_pack(fmt, 0, 0, 0);
    UINT32 col_cursor  = gui_pack(fmt, 255, 255, 255);

    /* Рабочий стол под окном */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        0, 0, fb_w, fb_h,
        col_bg
    );

    /* Корпус окна: серая объёмная рамка */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x, win_y, win_w, win_h,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        win_x, win_y, win_w, win_h,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    /* Заголовок окна */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 3,
        win_w - 6, 22,
        col_title
    );

    gui_draw_text(
        fb, stride, fb_w, fb_h,
        win_x + 9, win_y + 9,
        1, col_ttext,
        content->title
    );

    /* Строка меню под заголовком: плоская серая полоса
       с пунктами и "протравленной" (etched) линией-разделителем
       снизу - тем самым характерным приёмом Win9x, когда тонкая
       тёмная линия сразу сопровождается тонкой светлой под ней.
       Пункты декоративные (без реального меню), но оформлены
       в общем стиле остального интерфейса. */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25,
        win_w - 6, GUI_MENU_H,
        col_face
    );

    {
        static const char *menu_items[] = { "FILE", "EDIT", "VIEW", "HELP" };
        INTN mx = win_x + 9;

        for (UINTN i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); i++) {

            UINTN mw = gui_text_width(menu_items[i], 1);

            gui_draw_text(
                fb, stride, fb_w, fb_h,
                mx, win_y + 25 + (INTN)(GUI_MENU_H - 7) / 2,
                1, col_text,
                menu_items[i]
            );

            mx += (INTN)mw + 10;
        }
    }

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25 + (INTN)GUI_MENU_H - 2,
        win_w - 6, 1,
        col_shadow
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25 + (INTN)GUI_MENU_H - 1,
        win_w - 6, 1,
        col_hi
    );

    /* Тело окна: серое поле со вдавленной "бумагой" */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25 + (INTN)GUI_MENU_H,
        win_w - 6, win_h - 30 - GUI_MENU_H,
        col_face
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 9, win_y + 31 + (INTN)GUI_MENU_H,
        win_w - 18, win_h - 42 - GUI_MENU_H,
        col_win
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        win_x + 9, win_y + 31 + (INTN)GUI_MENU_H,
        win_w - 18, win_h - 42 - GUI_MENU_H,
        col_hi, col_light, col_shadow, col_border,
        FALSE
    );

    INTN line_y = win_y + 40 + (INTN)GUI_MENU_H;

    for (UINTN i = 0; i < content->line_count; i++) {

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            win_x + 16, line_y,
            1, col_text,
            content->lines[i]
        );

        line_y += 14;
    }

    /* Кнопка закрытия "X": серая объёмная кнопка,
       как у остальных элементов интерфейса, а не
       отдельная "иконка крестика" стороннего стиля. */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        btn_x, btn_y, btn_size, btn_size,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        btn_x, btn_y, btn_size, btn_size,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    gui_draw_text(
        fb, stride, fb_w, fb_h,
        btn_x + 6, btn_y + 5,
        1, col_btn_tx,
        "X"
    );

    /* Курсор: чёрная рамка + белая заливка */
    if (g_gui_draw_cursor) {

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            cur_x, cur_y,
            GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
            col_border
        );

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            cur_x + 2, cur_y + 2,
            GUI_CURSOR_SIZE - 4, GUI_CURSOR_SIZE - 4,
            col_cursor
        );
    }
}


/*
 * Сравнение двух обычных char-строк (не CHAR16, как
 * основной streq()) - нужно для команд, набранных
 * прямо в GUI-терминале.
 */
static int gui_streq(const char *a, const char *b)
{
    while (*a && *b) {

        if (*a != *b)
            return 0;

        a++;
        b++;
    }

    return *a == 0 && *b == 0;
}


/*
 * Добавить строку в скроллбек терминала. Если буфер
 * уже заполнен, самая старая строка "уезжает" вверх -
 * обычное поведение прокрутки в любом терминале.
 */
static void gui_term_push(
    char lines[][GUI_TERM_LINE_LEN + 1],
    UINTN *count,
    const char *text
)
{
    UINTN n = *count;

    if (n >= GUI_TERM_MAX_LINES) {

        for (UINTN i = 1; i < GUI_TERM_MAX_LINES; i++) {

            for (UINTN j = 0; j < GUI_TERM_LINE_LEN + 1; j++)
                lines[i - 1][j] = lines[i][j];
        }

        n = GUI_TERM_MAX_LINES - 1;
    }

    UINTN i = 0;

    while (text[i] != '\0' && i < GUI_TERM_LINE_LEN) {
        lines[n][i] = text[i];
        i++;
    }

    lines[n][i] = '\0';

    *count = n + 1;
}


/*
 * Сравнение начала обычной char-строки с префиксом
 * (аналог starts_with(), но для команд терминала).
 */
static int gui_starts_with(const char *s, const char *prefix)
{
    while (*prefix) {

        if (*s != *prefix)
            return 0;

        s++;
        prefix++;
    }

    return 1;
}


/*
 * Взять первое "слово" (до пробела или конца строки)
 * из char-строки. Возвращает длину слова.
 */
static UINTN gui_take_word(const char *s, char *out, UINTN max)
{
    UINTN i = 0;

    while (
        s[i] != '\0' &&
        s[i] != ' ' &&
        i < max - 1
    ) {
        out[i] = s[i];
        i++;
    }

    out[i] = '\0';

    return i;
}


/*
 * Простая char -> CHAR16 копия (имена файлов в
 * терминале - обычный char, но RAM-FS хранит CHAR16).
 */
static void gui_char_to_char16(
    const char *src,
    CHAR16 *dst,
    UINTN max
)
{
    UINTN i = 0;

    while (src[i] != '\0' && i < max - 1) {
        dst[i] = (CHAR16)(unsigned char)src[i];
        i++;
    }

    dst[i] = 0;
}


/*
 * И обратно: CHAR16 -> char, для вывода содержимого
 * файлов и списков в терминал (символы вне ASCII
 * заменяются на '?', как заглушку не-ASCII текста).
 */
static UINTN gui_char16_to_char(
    const CHAR16 *src,
    char *dst,
    UINTN max
)
{
    UINTN i = 0;

    while (src[i] != 0 && i < max - 1) {

        CHAR16 wc = src[i];

        dst[i] = (wc < 128) ? (char)wc : '?';
        i++;
    }

    dst[i] = '\0';

    return i;
}


/*
 * Простое копирование обычной char-строки с ограничением
 * по размеру (аналог char16_copy(), но для char).
 */
static void gui_str_copy8(
    char *dst,
    const char *src,
    UINTN max
)
{
    UINTN i = 0;

    while (src[i] != '\0' && i < max - 1) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}


/* ============================================================
 * "Поддельный" ConOut для вывода ПОСЛЕ ExitBootServices
 * ============================================================
 *
 * Весь xHCI-код (xhci_reset_controller, xhci_wait_for_event,
 * xhci_control_transfer, xhci_address_device_and_get_descriptor
 * и часть команды "ebs" ниже) устроен так, что печатает через
 * print()/print16()/print_uint()/print_hex() в параметр
 * "SIMPLE_TEXT_OUTPUT_INTERFACE *out" - а те, в свою очередь,
 * вызывают единственную вещь: out->OutputString(out, buf).
 * Больше никаких других полей структуры out нигде в этом коде
 * не используется (проверено).
 *
 * После ExitBootServices настоящий st->ConOut (он держится на
 * коде прошивки) больше не годится. Вместо переписывания
 * сотен вызовов print()/print_uint()/print_hex() по всему
 * xHCI-коду - подменяем "out" на собственную структуру того же
 * типа, у которой OutputString указывает на функцию ниже,
 * рисующую символы прямо в framebuffer собственным пиксельным
 * шрифтом (gui_draw_char). Весь остальной код xHCI-драйвера
 * остаётся дословно тем же самым, что и раньше, до
 * ExitBootServices - работает и после, без единой строчки
 * изменений в самой логике печати.
 *
 * scrollback_char()/g_scrollback_* (вызываются внутри print()/
 * print16() до OutputString) - чистая работа с обычной памятью,
 * никакого отношения к прошивке не имеют, поэтому тоже спокойно
 * продолжают работать после ExitBootServices.
 */

#define EBS_CONSOLE_SCALE  2
#define EBS_CONSOLE_CHAR_W (6 * EBS_CONSOLE_SCALE)
#define EBS_CONSOLE_CHAR_H (9 * EBS_CONSOLE_SCALE)
#define EBS_CONSOLE_MARGIN 10

static volatile UINT32 *g_ebsout_fb = NULL;
static UINT32 g_ebsout_stride = 0;
static UINT32 g_ebsout_w = 0;
static UINT32 g_ebsout_h = 0;
static UINT32 g_ebsout_fg = 0;
static UINT32 g_ebsout_bg = 0;
static INTN   g_ebsout_col = EBS_CONSOLE_MARGIN;
static INTN   g_ebsout_row = EBS_CONSOLE_MARGIN;

static EFI_STATUS EFIAPI ebs_console_output_string(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    CHAR16 *str
)
{
    /* this_out не используется - у нас на весь пиксельный
       "терминал" ровно один экземпляр, весь его реальный
       "объект" - глобальные переменные g_ebsout_* выше */
    (void)this_out;

    if (g_ebsout_fb == NULL || str == NULL)
        return EFI_SUCCESS;

    while (*str != 0) {

        CHAR16 c = *str;

        if (c == L'\r') {

            /* print() всегда шлёт '\r' и '\n' отдельной парой
               символов для перевода строки - сам перевод строки
               делает ветка '\n' ниже, здесь просто игнорируем */
            str++;
            continue;
        }

        if (c == L'\n') {

            g_ebsout_col = EBS_CONSOLE_MARGIN;
            g_ebsout_row += (INTN)EBS_CONSOLE_CHAR_H;

        } else {

            /* Наш пиксельный шрифт (gui_font) знает только ASCII
               (и то не весь набор - строчных букв там вообще
               нет). Символы вне таблицы gui_draw_char просто
               не рисует, но место под них всё равно резервирует
               - крашей не будет, часть текста молча превратится
               в пробелы. */
            /* Наш пиксельный шрифт (gui_font) знает только
               ASCII-цифры, ЗАГЛАВНЫЕ буквы и небольшой набор
               знаков препинания - строчных букв там нет вообще.
               Весь текст драйвера (print()/print_uint()/...)
               написан обычным регистром, поэтому строчные буквы
               без преобразования просто пропадали бы, оставляя
               дыры в тексте. Приводим к верхнему регистру перед
               отрисовкой - для латиницы это просто -32 к коду
               символа. Символы, которых в таблице всё равно нет
               (например большинство остальных знаков
               препинания), gui_draw_char по-прежнему molча
               пропускает - это ожидаемо и не крашится. */
            char ascii = (c < 128) ? (char)c : '?';

            if (ascii >= 'a' && ascii <= 'z')
                ascii = (char)(ascii - 'a' + 'A');

            gui_draw_char(
                g_ebsout_fb, g_ebsout_stride,
                g_ebsout_w, g_ebsout_h,
                g_ebsout_col, g_ebsout_row,
                EBS_CONSOLE_SCALE, g_ebsout_fg, ascii
            );

            g_ebsout_col += (INTN)EBS_CONSOLE_CHAR_W;

            if (
                g_ebsout_col + (INTN)EBS_CONSOLE_CHAR_W >
                (INTN)g_ebsout_w - EBS_CONSOLE_MARGIN
            ) {

                g_ebsout_col = EBS_CONSOLE_MARGIN;
                g_ebsout_row += (INTN)EBS_CONSOLE_CHAR_H;
            }
        }

        /* Дошли до низа экрана - настоящей прокрутки тут нет
           (framebuffer - просто массив пикселей в памяти,
           можно было бы сдвигать его memmove'ом, но это лишняя
           сложность ради истории, которая всё равно уже никому
           не нужна). Вместо этого просто очищаем экран и
           начинаем заново сверху - "листаем страницу". Важен
           только последний результат на экране, не вся история
           вывода. */
        if (
            g_ebsout_row + (INTN)EBS_CONSOLE_CHAR_H >
            (INTN)g_ebsout_h - EBS_CONSOLE_MARGIN
        ) {

            gui_fill_rect(
                g_ebsout_fb, g_ebsout_stride,
                g_ebsout_w, g_ebsout_h,
                0, 0, g_ebsout_w, g_ebsout_h,
                g_ebsout_bg
            );

            g_ebsout_col = EBS_CONSOLE_MARGIN;
            g_ebsout_row = EBS_CONSOLE_MARGIN;
        }

        str++;
    }

    return EFI_SUCCESS;
}

/* Сам "поддельный" объект ConOut - заполняется полями один раз
   в ebs_console_start() ниже. Только OutputString указывает на
   настоящую функцию, остальные указатели - NULL: весь код,
   который через этот out когда-либо проходит (print* семейство),
   их не вызывает. */
static SIMPLE_TEXT_OUTPUT_INTERFACE g_ebs_pixel_out;

/*
 * Готовит framebuffer и сам "поддельный" ConOut к работе после
 * ExitBootServices: очищает экран заданным цветом фона и ставит
 * курсор пиксельного "терминала" в левый верхний угол.
 * Параметры fb/stride/w/h должны быть закэшированы ДО
 * ExitBootServices (см. команду "ebs") - после него
 * GraphicsOutputProtocol уже не найти через LocateProtocol,
 * а сам framebuffer как область памяти продолжает работать
 * как обычно.
 */
static void ebs_console_start(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 w,
    UINT32 h,
    UINT32 fg,
    UINT32 bg
)
{
    g_ebsout_fb = fb;
    g_ebsout_stride = stride;
    g_ebsout_w = w;
    g_ebsout_h = h;
    g_ebsout_fg = fg;
    g_ebsout_bg = bg;
    g_ebsout_col = EBS_CONSOLE_MARGIN;
    g_ebsout_row = EBS_CONSOLE_MARGIN;

    gui_fill_rect(fb, stride, w, h, 0, 0, w, h, bg);

    g_ebs_pixel_out.Reset = NULL;
    g_ebs_pixel_out.OutputString = ebs_console_output_string;
    g_ebs_pixel_out.TestString = NULL;
    g_ebs_pixel_out.QueryMode = NULL;
    g_ebs_pixel_out.SetMode = NULL;
    g_ebs_pixel_out.SetAttribute = NULL;
    g_ebs_pixel_out.ClearScreen = NULL;
    g_ebs_pixel_out.SetCursorPosition = NULL;
    g_ebs_pixel_out.EnableCursor = NULL;
    g_ebs_pixel_out.Mode = NULL;
}


/*
 * Небольшой набор встроенных команд для терминала
 * внутри GUI. Это отдельная, упрощённая реализация:
 * основной run_command() пишет прямо в
 * EFI_SIMPLE_TEXT_OUTPUT, а тут нужен вывод в свой
 * скроллбек-буфер окна. Шрифт GUI умеет только
 * заглавные буквы, поэтому весь ввод здесь тоже
 * в верхнем регистре.
 *
 * Возвращает TRUE, если команда должна закрыть окно
 * терминала (EXIT/CLOSE/QUIT), иначе FALSE.
 */
static BOOLEAN gui_term_exec(
    EFI_SYSTEM_TABLE *st,
    const char *cmd,
    char lines[][GUI_TERM_LINE_LEN + 1],
    UINTN *count
)
{
    char prompt[GUI_TERM_LINE_LEN + 1];

    prompt[0] = '>';
    prompt[1] = ' ';

    UINTN i = 0;

    while (cmd[i] != '\0' && i < GUI_TERM_LINE_LEN - 2) {
        prompt[2 + i] = cmd[i];
        i++;
    }

    prompt[2 + i] = '\0';

    gui_term_push(lines, count, prompt);

    if (gui_streq(cmd, "")) {

        return FALSE;

    } else if (
        gui_streq(cmd, "EXIT") ||
        gui_streq(cmd, "CLOSE") ||
        gui_streq(cmd, "QUIT")
    ) {

        return TRUE;

    } else if (gui_streq(cmd, "HELP")) {

        gui_term_push(lines, count, "HELP ABOUT VER TIME DATE UPTIME");
        gui_term_push(lines, count, "WHOAMI CLEAR ECHO TEXT");
        gui_term_push(lines, count, "CALC A OP B");
        gui_term_push(lines, count, "LS TOUCH N CAT N SIZE N RM N");
        gui_term_push(lines, count, "WRITE N TEXT");
        gui_term_push(lines, count, "APPEND N TEXT");
        gui_term_push(lines, count, "MV A B CP A B");
        gui_term_push(lines, count, "REBOOT SHUTDOWN");
        gui_term_push(lines, count, "EXIT CLOSES THIS WINDOW");

    } else if (gui_streq(cmd, "ABOUT")) {

        gui_term_push(
            lines, count,
            "MYOS 0.1 - MINIMAL UEFI OS FROM SCRATCH."
        );

    } else if (gui_streq(cmd, "VER")) {

        gui_term_push(lines, count, "MYOS 0.1");

    } else if (gui_streq(cmd, "WHOAMI")) {

        gui_term_push(lines, count, "ROOT AT MYOS");

    } else if (
        gui_streq(cmd, "CLEAR") ||
        gui_streq(cmd, "CLS")
    ) {

        *count = 0;

    } else if (gui_streq(cmd, "TIME")) {

        EFI_TIME now;

        if (
            st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now, NULL
            ) == EFI_SUCCESS
        ) {

            char buf[9];

            gui_uint2_to_str(now.Hour, buf);
            buf[2] = ':';
            gui_uint2_to_str(now.Minute, buf + 3);
            buf[5] = ':';
            gui_uint2_to_str(now.Second, buf + 6);

            gui_term_push(lines, count, buf);

        } else {

            gui_term_push(lines, count, "TIME UNAVAILABLE");
        }

    } else if (gui_streq(cmd, "DATE")) {

        EFI_TIME now;

        if (
            st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now, NULL
            ) == EFI_SUCCESS
        ) {

            char buf[16];
            UINTN n = 0;

            n += gui_uint2_to_str(now.Day, buf + n);
            buf[n++] = '-';
            n += gui_uint2_to_str(now.Month, buf + n);
            buf[n++] = '-';
            n += gui_uint_to_str(now.Year, buf + n);

            gui_term_push(lines, count, buf);

        } else {

            gui_term_push(lines, count, "DATE UNAVAILABLE");
        }

    } else if (gui_streq(cmd, "UPTIME")) {

        if (
            !g_have_boot_time ||
            !st->RuntimeServices->GetTime
        ) {

            gui_term_push(lines, count, "UPTIME UNAVAILABLE");

        } else {

            EFI_TIME now;

            if (
                st->RuntimeServices->GetTime(
                    &now, NULL
                ) == EFI_SUCCESS
            ) {

                INT64 secs =
                    (INT64)now.Hour * 3600 +
                    (INT64)now.Minute * 60 +
                    now.Second
                    -
                    (
                        (INT64)g_boot_time.Hour * 3600 +
                        (INT64)g_boot_time.Minute * 60 +
                        g_boot_time.Second
                    );

                if (secs < 0)
                    secs += 86400;

                char buf[32];
                UINTN n = 0;

                buf[n++] = 'U';
                buf[n++] = 'P';
                buf[n++] = ' ';

                n += gui_uint_to_str(
                    (UINT64)secs / 3600, buf + n
                );
                buf[n++] = 'H';
                buf[n++] = ' ';

                n += gui_uint_to_str(
                    ((UINT64)secs / 60) % 60, buf + n
                );
                buf[n++] = 'M';
                buf[n++] = ' ';

                n += gui_uint_to_str(
                    (UINT64)secs % 60, buf + n
                );
                buf[n++] = 'S';
                buf[n] = '\0';

                gui_term_push(lines, count, buf);
            }
        }

    } else if (
        cmd[0] == 'E' && cmd[1] == 'C' &&
        cmd[2] == 'H' && cmd[3] == 'O' &&
        (cmd[4] == ' ' || cmd[4] == '\0')
    ) {

        gui_term_push(
            lines, count,
            cmd[4] == ' ' ? cmd + 5 : ""
        );

    } else if (
        gui_streq(cmd, "LS") ||
        gui_streq(cmd, "DIR") ||
        gui_streq(cmd, "FILES")
    ) {

        int any = 0;

        for (int fi = 0; fi < FS_MAX_FILES; fi++) {

            if (!g_fs[fi].used)
                continue;

            any = 1;

            char row[GUI_TERM_LINE_LEN + 1];
            UINTN n =
                gui_char16_to_char(
                    g_fs[fi].name, row,
                    GUI_TERM_LINE_LEN - 10
                );

            row[n++] = ' ';
            row[n++] = '-';
            row[n++] = ' ';

            n += gui_uint_to_str(
                g_fs[fi].size, row + n
            );

            row[n++] = 'B';
            row[n] = '\0';

            gui_term_push(lines, count, row);
        }

        if (!any)
            gui_term_push(lines, count, "EMPTY - NO FILES");

    } else if (gui_starts_with(cmd, "TOUCH ")) {

        char name8[FS_NAME_MAX];
        gui_take_word(cmd + 6, name8, sizeof(name8));

        if (name8[0] == '\0') {

            gui_term_push(lines, count, "USAGE: TOUCH NAME");

        } else {

            CHAR16 name16[FS_NAME_MAX];
            gui_char_to_char16(name8, name16, FS_NAME_MAX);

            if (fs_find(name16) >= 0) {

                gui_term_push(lines, count, "FILE ALREADY EXISTS");

            } else {

                int idx = fs_find_free();

                if (idx < 0) {

                    gui_term_push(lines, count, "FILESYSTEM FULL");

                } else {

                    g_fs[idx].used = TRUE;
                    char16_copy(g_fs[idx].name, name16, FS_NAME_MAX);
                    g_fs[idx].data[0] = 0;
                    g_fs[idx].size = 0;

                    gui_term_push(lines, count, "CREATED");
                }
            }
        }

    } else if (gui_starts_with(cmd, "CAT ")) {

        char name8[FS_NAME_MAX];
        gui_take_word(cmd + 4, name8, sizeof(name8));

        if (name8[0] == '\0') {

            gui_term_push(lines, count, "USAGE: CAT NAME");

        } else {

            CHAR16 name16[FS_NAME_MAX];
            gui_char_to_char16(name8, name16, FS_NAME_MAX);

            int idx = fs_find(name16);

            if (idx < 0) {

                gui_term_push(lines, count, "NO SUCH FILE");

            } else if (g_fs[idx].size == 0) {

                gui_term_push(lines, count, "EMPTY FILE");

            } else {

                char body[FS_DATA_MAX];
                UINTN blen =
                    gui_char16_to_char(
                        g_fs[idx].data, body, FS_DATA_MAX
                    );

                UINTN p = 0;

                while (p < blen) {

                    char row[GUI_TERM_LINE_LEN + 1];
                    UINTN n = 0;

                    while (
                        p < blen &&
                        body[p] != '\n' &&
                        n < GUI_TERM_LINE_LEN
                    ) {
                        row[n++] = body[p++];
                    }

                    row[n] = '\0';

                    if (p < blen && body[p] == '\n')
                        p++;

                    gui_term_push(lines, count, row);
                }
            }
        }

    } else if (gui_starts_with(cmd, "SIZE ")) {

        char name8[FS_NAME_MAX];
        gui_take_word(cmd + 5, name8, sizeof(name8));

        if (name8[0] == '\0') {

            gui_term_push(lines, count, "USAGE: SIZE NAME");

        } else {

            CHAR16 name16[FS_NAME_MAX];
            gui_char_to_char16(name8, name16, FS_NAME_MAX);

            int idx = fs_find(name16);

            if (idx < 0) {

                gui_term_push(lines, count, "NO SUCH FILE");

            } else {

                char buf[24];
                UINTN n =
                    gui_uint_to_str(g_fs[idx].size, buf);

                buf[n++] = 'B';
                buf[n] = '\0';

                gui_term_push(lines, count, buf);
            }
        }

    } else if (gui_starts_with(cmd, "RM ")) {

        char name8[FS_NAME_MAX];
        gui_take_word(cmd + 3, name8, sizeof(name8));

        if (name8[0] == '\0') {

            gui_term_push(lines, count, "USAGE: RM NAME");

        } else {

            CHAR16 name16[FS_NAME_MAX];
            gui_char_to_char16(name8, name16, FS_NAME_MAX);

            int idx = fs_find(name16);

            if (idx < 0) {

                gui_term_push(lines, count, "NO SUCH FILE");

            } else {

                g_fs[idx].used = FALSE;
                gui_term_push(lines, count, "DELETED");
            }
        }

    } else if (
        gui_starts_with(cmd, "WRITE ") ||
        gui_starts_with(cmd, "APPEND ")
    ) {
        int is_append = gui_starts_with(cmd, "APPEND ");

        const char *rest =
            cmd + (is_append ? 7 : 6);

        char name8[FS_NAME_MAX];
        UINTN nlen = gui_take_word(rest, name8, sizeof(name8));

        rest += nlen;

        if (*rest == ' ')
            rest++;

        if (name8[0] == '\0') {

            gui_term_push(
                lines, count,
                is_append ? "USAGE: APPEND NAME TEXT"
                          : "USAGE: WRITE NAME TEXT"
            );

        } else {

            CHAR16 name16[FS_NAME_MAX];
            gui_char_to_char16(name8, name16, FS_NAME_MAX);

            int idx = fs_find(name16);

            if (idx < 0)
                idx = fs_find_free();

            if (idx < 0) {

                gui_term_push(lines, count, "FILESYSTEM FULL");

            } else {

                if (!g_fs[idx].used) {

                    g_fs[idx].used = TRUE;
                    char16_copy(g_fs[idx].name, name16, FS_NAME_MAX);
                    g_fs[idx].data[0] = 0;
                    g_fs[idx].size = 0;
                }

                CHAR16 text16[GUI_TERM_LINE_LEN + 1];
                gui_char_to_char16(
                    rest, text16, GUI_TERM_LINE_LEN + 1
                );

                UINTN need = char16_len(text16);

                if (!is_append) {

                    char16_copy(
                        g_fs[idx].data, text16, FS_DATA_MAX
                    );

                    g_fs[idx].size = char16_len(g_fs[idx].data);

                } else {

                    UINTN cur = g_fs[idx].size;
                    UINTN sep = (cur > 0) ? 1 : 0;

                    if (cur + sep + need >= FS_DATA_MAX) {

                        gui_term_push(
                            lines, count,
                            "FILE TOO LARGE"
                        );

                        need = 0;

                    } else {

                        if (sep)
                            g_fs[idx].data[cur++] = L'\n';

                        for (UINTN k = 0; k < need; k++)
                            g_fs[idx].data[cur++] = text16[k];

                        g_fs[idx].data[cur] = 0;
                        g_fs[idx].size = cur;
                    }
                }

                if (need > 0 || !is_append) {

                    char buf[32];
                    UINTN n = 0;

                    buf[n++] = 'O';
                    buf[n++] = 'K';
                    buf[n++] = ' ';
                    buf[n++] = '-';
                    buf[n++] = ' ';

                    n += gui_uint_to_str(
                        g_fs[idx].size, buf + n
                    );

                    buf[n++] = 'B';
                    buf[n] = '\0';

                    gui_term_push(lines, count, buf);
                }
            }
        }

    } else if (
        gui_starts_with(cmd, "MV ") ||
        gui_starts_with(cmd, "CP ")
    ) {
        int is_copy = gui_starts_with(cmd, "CP ");

        const char *rest = cmd + 3;

        char a8[FS_NAME_MAX];
        UINTN alen = gui_take_word(rest, a8, sizeof(a8));

        rest += alen;

        if (*rest == ' ')
            rest++;

        char b8[FS_NAME_MAX];
        gui_take_word(rest, b8, sizeof(b8));

        if (a8[0] == '\0' || b8[0] == '\0') {

            gui_term_push(
                lines, count,
                is_copy ? "USAGE: CP A B" : "USAGE: MV A B"
            );

        } else {

            CHAR16 a16[FS_NAME_MAX];
            CHAR16 b16[FS_NAME_MAX];
            gui_char_to_char16(a8, a16, FS_NAME_MAX);
            gui_char_to_char16(b8, b16, FS_NAME_MAX);

            int ai = fs_find(a16);

            if (ai < 0) {

                gui_term_push(lines, count, "NO SUCH FILE");

            } else if (fs_find(b16) >= 0) {

                gui_term_push(lines, count, "TARGET ALREADY EXISTS");

            } else if (is_copy) {

                int bi = fs_find_free();

                if (bi < 0) {

                    gui_term_push(lines, count, "FILESYSTEM FULL");

                } else {

                    g_fs[bi].used = TRUE;
                    char16_copy(g_fs[bi].name, b16, FS_NAME_MAX);
                    char16_copy(g_fs[bi].data, g_fs[ai].data, FS_DATA_MAX);
                    g_fs[bi].size = g_fs[ai].size;

                    gui_term_push(lines, count, "COPIED");
                }

            } else {

                char16_copy(g_fs[ai].name, b16, FS_NAME_MAX);
                gui_term_push(lines, count, "RENAMED");
            }
        }

    } else if (gui_starts_with(cmd, "CALC ")) {

        UINTN a = 0, b = 0;
        char op = '+';
        UINTN p = 5;

        while (cmd[p] >= '0' && cmd[p] <= '9') {
            a = a * 10 + (UINTN)(cmd[p] - '0');
            p++;
        }

        while (cmd[p] == ' ')
            p++;

        if (
            cmd[p] == '+' || cmd[p] == '-' ||
            cmd[p] == '*' || cmd[p] == '/'
        ) {
            op = cmd[p];
            p++;
        }

        while (cmd[p] == ' ')
            p++;

        while (cmd[p] >= '0' && cmd[p] <= '9') {
            b = b * 10 + (UINTN)(cmd[p] - '0');
            p++;
        }

        BOOLEAN ok = TRUE;
        UINTN res = 0;

        if (op == '+') res = a + b;
        else if (op == '-') res = (a >= b) ? a - b : 0;
        else if (op == '*') res = a * b;
        else if (op == '/') {
            if (b == 0) ok = FALSE;
            else res = a / b;
        }

        if (!ok) {

            gui_term_push(lines, count, "DIVISION BY ZERO");

        } else {

            char buf[24];
            gui_uint_to_str(res, buf);
            gui_term_push(lines, count, buf);
        }

    } else if (gui_streq(cmd, "REBOOT")) {

        gui_term_push(lines, count, "REBOOTING");

        st->RuntimeServices->ResetSystem(
            EfiResetCold, EFI_SUCCESS, 0, NULL
        );

    } else if (gui_streq(cmd, "SHUTDOWN")) {

        gui_term_push(lines, count, "SHUTTING DOWN");

        st->RuntimeServices->ResetSystem(
            EfiResetShutdown, EFI_SUCCESS, 0, NULL
        );

    } else {

        gui_term_push(lines, count, "UNKNOWN COMMAND. TRY HELP.");
    }

    return FALSE;
}


/*
 * Окно терминала: то же самое оформление рамки/шапки,
 * что и у остальных программ, но внутри - чёрный
 * viewport с живым вводом, как у отдельного
 * приложения-терминала (kitty и подобные), а не
 * встроенная в ОС консоль поверх всего экрана.
 */
static void gui_draw_terminal(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN win_x, INTN win_y,
    UINTN win_w, UINTN win_h,
    INTN btn_x, INTN btn_y,
    UINTN btn_size,
    const char lines[][GUI_TERM_LINE_LEN + 1],
    UINTN line_count,
    const char *input
)
{
    UINT32 col_bg      = gui_pack(fmt, 0, 128, 128);
    UINT32 col_border  = gui_pack(fmt, 0, 0, 0);
    UINT32 col_hi      = gui_pack(fmt, 255, 255, 255);
    UINT32 col_light   = gui_pack(fmt, 223, 223, 223);
    UINT32 col_shadow  = gui_pack(fmt, 128, 128, 128);
    UINT32 col_title   = gui_pack(fmt, 0, 0, 128);
    UINT32 col_ttext   = gui_pack(fmt, 255, 255, 255);
    UINT32 col_face    = gui_pack(fmt, 192, 192, 192);
    UINT32 col_btn_tx  = gui_pack(fmt, 0, 0, 0);

    /* Тёмный "стеклянный" фон терминала - как у kitty
       и большинства отдельных терминальных приложений,
       а не белая "бумага" остальных окон. */
    UINT32 col_term_bg = gui_pack(fmt, 12, 12, 12);
    UINT32 col_term_fg = gui_pack(fmt, 220, 220, 220);
    UINT32 col_prompt  = gui_pack(fmt, 0, 220, 120);

    /* Рабочий стол под окном */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        0, 0, fb_w, fb_h,
        col_bg
    );

    /* Корпус окна: та же серая объёмная рамка, что и
       у остальных программ - терминал выглядит своим
       окном на общем столе, а не отдельным экраном. */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x, win_y, win_w, win_h,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        win_x, win_y, win_w, win_h,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    /* Заголовок окна */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 3,
        win_w - 6, 22,
        col_title
    );

    gui_draw_text(
        fb, stride, fb_w, fb_h,
        win_x + 9, win_y + 9,
        1, col_ttext,
        "TERMINAL"
    );

    /* Чёрный viewport вместо белой "бумаги" - без
       декоративного меню FILE/EDIT/VIEW, которое
       настоящему терминалу не нужно. */
    INTN  term_x = win_x + 9;
    INTN  term_y = win_y + 31;
    UINTN term_w = win_w - 18;
    UINTN term_h = win_h - 42;

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        term_x, term_y, term_w, term_h,
        col_term_bg
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        term_x, term_y, term_w, term_h,
        col_hi, col_light, col_shadow, col_border,
        FALSE
    );

    UINTN row_h = 9;
    UINTN max_rows = (UINTN)term_h / row_h;

    if (max_rows < 2)
        max_rows = 2;

    UINTN visible =
        (line_count < max_rows - 1) ? line_count : max_rows - 1;

    UINTN first = line_count - visible;

    INTN line_y = term_y + 4;

    for (UINTN i = 0; i < visible; i++) {

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            term_x + 4, line_y,
            1, col_term_fg,
            lines[first + i]
        );

        line_y += (INTN)row_h;
    }

    /* Строка ввода внизу viewport'а: зелёное
       приглашение, набранный текст и блочный курсор -
       как в настоящем терминале. */
    INTN prompt_y =
        term_y + (INTN)term_h - (INTN)row_h - 3;

    INTN px = term_x + 4;

    px += (INTN)gui_draw_text(
        fb, stride, fb_w, fb_h,
        px, prompt_y,
        1, col_prompt,
        "> "
    );

    px += (INTN)gui_draw_text(
        fb, stride, fb_w, fb_h,
        px, prompt_y,
        1, col_term_fg,
        input
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        px, prompt_y, 6, 7,
        col_term_fg
    );

    /* Кнопка закрытия "X" - того же стиля, что у
       остальных окон (закрывается также по Esc). */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        btn_x, btn_y, btn_size, btn_size,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        btn_x, btn_y, btn_size, btn_size,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    gui_draw_text(
        fb, stride, fb_w, fb_h,
        btn_x + 6, btn_y + 5,
        1, col_btn_tx,
        "X"
    );
}


/*
 * Окно "Сапёра": та же рамка/заголовок/крестик, что и у
 * остальных программ, а внутри - панель со счётчиком мин,
 * улыбающейся кнопкой рестарта, таймером и самим полем.
 */
static void gui_draw_minesweeper(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN win_x, INTN win_y,
    UINTN win_w, UINTN win_h,
    INTN btn_x, INTN btn_y,
    UINTN btn_size,
    const GUI_MS_STATE *ms,
    INTN cur_x, INTN cur_y
)
{
    UINT32 col_bg      = gui_pack(fmt, 0, 128, 128);
    UINT32 col_border  = gui_pack(fmt, 0, 0, 0);
    UINT32 col_hi      = gui_pack(fmt, 255, 255, 255);
    UINT32 col_light   = gui_pack(fmt, 223, 223, 223);
    UINT32 col_shadow  = gui_pack(fmt, 128, 128, 128);
    UINT32 col_title   = gui_pack(fmt, 0, 0, 128);
    UINT32 col_ttext   = gui_pack(fmt, 255, 255, 255);
    UINT32 col_face    = gui_pack(fmt, 192, 192, 192);
    UINT32 col_text    = gui_pack(fmt, 0, 0, 0);
    UINT32 col_led_bg  = gui_pack(fmt, 20, 20, 20);
    UINT32 col_led_fg  = gui_pack(fmt, 220, 0, 0);
    UINT32 col_sel     = gui_pack(fmt, 0, 0, 128);
    UINT32 col_red_bg  = gui_pack(fmt, 255, 0, 0);
    UINT32 col_flag    = gui_pack(fmt, 200, 0, 0);
    UINT32 col_cursor  = gui_pack(fmt, 255, 255, 255);

    GUI_MS_LAYOUT L;
    gui_ms_compute_layout(win_x, win_y, win_w, win_h, &L);

    /* Рабочий стол под окном */
    gui_fill_rect(fb, stride, fb_w, fb_h, 0, 0, fb_w, fb_h, col_bg);

    /* Корпус окна */
    gui_fill_rect(fb, stride, fb_w, fb_h, win_x, win_y, win_w, win_h, col_face);

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        win_x, win_y, win_w, win_h,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    /* Заголовок */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 3, win_w - 6, 22,
        col_title
    );

    gui_draw_text(
        fb, stride, fb_w, fb_h,
        win_x + 9, win_y + 9,
        1, col_ttext,
        "MINESWEEPER"
    );

    /* Строка "меню" GAME / HELP - декоративная, как на
       референсе, в общем стиле остальных окон программы. */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25, win_w - 6, GUI_MENU_H,
        col_face
    );

    {
        static const char *ms_menu_items[] = { "GAME", "HELP" };
        INTN mx = win_x + 9;

        for (UINTN i = 0; i < 2; i++) {

            UINTN mw = gui_text_width(ms_menu_items[i], 1);

            gui_draw_text(
                fb, stride, fb_w, fb_h,
                mx, win_y + 25 + (INTN)(GUI_MENU_H - 7) / 2,
                1, col_text,
                ms_menu_items[i]
            );

            mx += (INTN)mw + 10;
        }
    }

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25 + (INTN)GUI_MENU_H - 2,
        win_w - 6, 1, col_shadow
    );

    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        win_x + 3, win_y + 25 + (INTN)GUI_MENU_H - 1,
        win_w - 6, 1, col_hi
    );

    /* Вдавленная общая рамка игрового поля */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        L.field_x, L.field_y, L.field_w, L.field_h,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.field_x, L.field_y, L.field_w, L.field_h,
        col_hi, col_light, col_shadow, col_border,
        FALSE
    );

    /* Панель шапки: счётчик мин / смайлик / таймер */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        L.head_x, L.head_y, L.head_w, L.head_h,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.head_x, L.head_y, L.head_w, L.head_h,
        col_hi, col_light, col_shadow, col_border,
        FALSE
    );

    /* Счётчик оставшихся мин (слева) */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        L.led1_x, L.led1_y, L.led_w, L.led_h,
        col_led_bg
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.led1_x, L.led1_y, L.led_w, L.led_h,
        col_shadow, col_border, col_hi, col_light,
        FALSE
    );

    {
        UINTN left =
            (ms->flags_used >= GUI_MS_MINES)
                ? 0 : GUI_MS_MINES - ms->flags_used;

        if (left > 999)
            left = 999;

        char buf[4];
        buf[0] = (char)('0' + (left / 100) % 10);
        buf[1] = (char)('0' + (left / 10) % 10);
        buf[2] = (char)('0' + left % 10);
        buf[3] = '\0';

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            L.led1_x + 5, L.led1_y + (INTN)(L.led_h - 14) / 2,
            2, col_led_fg, buf
        );
    }

    /* Таймер (справа) */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        L.led2_x, L.led2_y, L.led_w, L.led_h,
        col_led_bg
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.led2_x, L.led2_y, L.led_w, L.led_h,
        col_shadow, col_border, col_hi, col_light,
        FALSE
    );

    {
        UINTN t = ms->timer;

        if (t > 999)
            t = 999;

        char buf[4];
        buf[0] = (char)('0' + (t / 100) % 10);
        buf[1] = (char)('0' + (t / 10) % 10);
        buf[2] = (char)('0' + t % 10);
        buf[3] = '\0';

        gui_draw_text(
            fb, stride, fb_w, fb_h,
            L.led2_x + 5, L.led2_y + (INTN)(L.led_h - 14) / 2,
            2, col_led_fg, buf
        );
    }

    /* Кнопка-смайлик (рестарт) */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        L.smile_x, L.smile_y, L.smile_size, L.smile_size,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.smile_x, L.smile_y, L.smile_size, L.smile_size,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    {
        int mode = ms->won ? 1 : (ms->over ? 2 : 0);

        gui_draw_face(
            fb, stride, fb_w, fb_h, fmt,
            L.smile_x + 3, L.smile_y + 3, mode
        );
    }

    /* Вдавленная рамка вокруг сетки клеток */
    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        L.grid_x - 3, L.grid_y - 3,
        GUI_MS_COLS * L.cell + 6, GUI_MS_ROWS * L.cell + 6,
        col_hi, col_light, col_shadow, col_border,
        FALSE
    );

    INTN cx = cur_x + GUI_CURSOR_SIZE / 2;
    INTN cy = cur_y + GUI_CURSOR_SIZE / 2;

    for (int r = 0; r < GUI_MS_ROWS; r++) {

        for (int c = 0; c < GUI_MS_COLS; c++) {

            INTN cell_x = L.grid_x + (INTN)((UINTN)c * L.cell);
            INTN cell_y = L.grid_y + (INTN)((UINTN)r * L.cell);

            BOOLEAN hovered =
                !ms->over &&
                gui_point_in_rect(
                    cx, cy, cell_x, cell_y, L.cell, L.cell
                );

            if (ms->revealed[r][c]) {

                UINT32 bg =
                    (ms->mine[r][c] &&
                     r == ms->boom_r && c == ms->boom_c)
                        ? col_red_bg : col_face;

                gui_fill_rect(
                    fb, stride, fb_w, fb_h,
                    cell_x, cell_y, L.cell, L.cell, bg
                );

                gui_draw_border(
                    fb, stride, fb_w, fb_h,
                    cell_x, cell_y, L.cell, L.cell, col_shadow
                );

                if (ms->mine[r][c]) {

                    gui_draw_icon_mine(
                        fb, stride, fb_w, fb_h,
                        cell_x + ((INTN)L.cell - 11) / 2,
                        cell_y + ((INTN)L.cell - 11) / 2,
                        col_border, col_hi
                    );

                } else if (ms->adj[r][c] > 0) {

                    char buf[2];
                    buf[0] = (char)('0' + ms->adj[r][c]);
                    buf[1] = '\0';

                    UINT32 numcol =
                        gui_ms_number_color(fmt, ms->adj[r][c]);

                    gui_draw_text(
                        fb, stride, fb_w, fb_h,
                        cell_x + ((INTN)L.cell - 6) / 2,
                        cell_y + ((INTN)L.cell - 7) / 2,
                        1, numcol, buf
                    );
                }

            } else {

                gui_fill_rect(
                    fb, stride, fb_w, fb_h,
                    cell_x + 1, cell_y + 1,
                    L.cell - 2, L.cell - 2,
                    col_face
                );

                gui_draw_bevel(
                    fb, stride, fb_w, fb_h,
                    cell_x + 1, cell_y + 1,
                    L.cell - 2, L.cell - 2,
                    col_hi, col_light, col_shadow, col_border,
                    TRUE
                );

                if (ms->flagged[r][c]) {

                    gui_draw_icon_flag(
                        fb, stride, fb_w, fb_h,
                        cell_x + ((INTN)L.cell - 11) / 2,
                        cell_y + ((INTN)L.cell - 11) / 2,
                        col_border, col_flag, col_border
                    );
                }

                if (hovered) {

                    gui_draw_border(
                        fb, stride, fb_w, fb_h,
                        cell_x, cell_y, L.cell, L.cell, col_sel
                    );
                }
            }
        }
    }

    /* Кнопка закрытия "X" */
    gui_fill_rect(
        fb, stride, fb_w, fb_h,
        btn_x, btn_y, btn_size, btn_size,
        col_face
    );

    gui_draw_bevel(
        fb, stride, fb_w, fb_h,
        btn_x, btn_y, btn_size, btn_size,
        col_hi, col_light, col_shadow, col_border,
        TRUE
    );

    gui_draw_text(
        fb, stride, fb_w, fb_h,
        btn_x + 6, btn_y + 5,
        1, col_text,
        "X"
    );

    /* Курсор поверх всего */
    if (g_gui_draw_cursor) {

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            cur_x, cur_y, GUI_CURSOR_SIZE, GUI_CURSOR_SIZE,
            col_border
        );

        gui_fill_rect(
            fb, stride, fb_w, fb_h,
            cur_x + 2, cur_y + 2,
            GUI_CURSOR_SIZE - 4, GUI_CURSOR_SIZE - 4,
            col_cursor
        );
    }
}


/*
 * Собрать окно "просмотра файла" (открывается кликом
 * по строке в Проводнике): заголовок = имя файла,
 * тело = содержимое, порезанное на строки под ширину
 * окна. idx - индекс файла в g_fs.
 */
static UINTN gui_open_fileview(
    int idx,
    char fileview_buf[][GUI_FILEVIEW_LINE_LEN],
    const char **fileview_lines,
    char *title_buf
)
{
    gui_char16_to_char(
        g_fs[idx].name, title_buf, FS_NAME_MAX + 1
    );

    if (g_fs[idx].size == 0) {

        gui_str_copy8(
            fileview_buf[0], "EMPTY FILE", GUI_FILEVIEW_LINE_LEN
        );

        fileview_lines[0] = fileview_buf[0];

        return 1;
    }

    char body[FS_DATA_MAX];
    UINTN blen =
        gui_char16_to_char(
            g_fs[idx].data, body, FS_DATA_MAX
        );

    UINTN row_count = 0;
    UINTN p = 0;

    while (p < blen && row_count < GUI_FILEVIEW_MAX_ROWS) {

        char *row = fileview_buf[row_count];
        UINTN n = 0;

        while (
            p < blen &&
            body[p] != '\n' &&
            n < GUI_FILEVIEW_LINE_LEN - 1
        ) {
            row[n++] = body[p++];
        }

        row[n] = '\0';

        if (p < blen && body[p] == '\n')
            p++;

        fileview_lines[row_count] = row;
        row_count++;
    }

    if (p < blen && row_count == GUI_FILEVIEW_MAX_ROWS) {

        /* Не влезло целиком - подменяем последнюю
           показанную строку отметкой обрезки. */
        gui_str_copy8(
            fileview_buf[row_count - 1],
            "...",
            GUI_FILEVIEW_LINE_LEN
        );

        fileview_lines[row_count - 1] =
            fileview_buf[row_count - 1];
    }

    return row_count;
}


/*
 * "Что сейчас под курсором" в виде одного числа - для решения,
 * хватит ли при движении мыши быстрого пути (только курсор) или
 * картинка под ним тоже меняется и нужен полный кадр: пункт меню
 * Start подсвечивается под курсором, клетка Сапёра - тоже.
 * Число поменялось -> полный кадр; нет -> двигаем только курсор.
 */
static INTN gui_hover_key(
    BOOLEAN in_minesweeper,
    BOOLEAN menu_open_on_desktop,
    GUI_ICON *icons,
    INTN win_x, INTN win_y, UINTN win_w, UINTN win_h,
    INTN cur_x, INTN cur_y
)
{
    INTN cx = cur_x + GUI_CURSOR_SIZE / 2;
    INTN cy = cur_y + GUI_CURSOR_SIZE / 2;

    if (in_minesweeper) {

        GUI_MS_LAYOUT L;
        gui_ms_compute_layout(win_x, win_y, win_w, win_h, &L);

        UINTN gw = GUI_MS_COLS * L.cell;
        UINTN gh = GUI_MS_ROWS * L.cell;

        if (gui_point_in_rect(cx, cy, L.grid_x, L.grid_y, gw, gh)) {

            INTN col = (INTN)((UINTN)(cx - L.grid_x) / L.cell);
            INTN row = (INTN)((UINTN)(cy - L.grid_y) / L.cell);

            return 1000 + row * GUI_MS_COLS + col;
        }

        return 0;
    }

    if (menu_open_on_desktop) {

        for (UINTN i = 0; i < GUI_ICON_COUNT; i++) {

            if (
                gui_point_in_rect(
                    cx, cy,
                    icons[i].x, icons[i].y, icons[i].w, icons[i].h
                )
            ) {
                return 1 + (INTN)i;
            }
        }
    }

    return 0;
}


static void gui_start(EFI_SYSTEM_TABLE *st)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out = st->ConOut;

    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    if (
        !st->BootServices->LocateProtocol ||
        st->BootServices->LocateProtocol(
            &gop_guid, NULL, (VOID **)&gop
        ) != EFI_SUCCESS ||
        !gop || !gop->Mode || !gop->Mode->Info
    ) {

        print(
            out,
            "GUI unavailable: no Graphics Output Protocol.\n"
        );

        return;
    }

    UINT32 fb_w    = gop->Mode->Info->HorizontalResolution;
    UINT32 fb_h    = gop->Mode->Info->VerticalResolution;
    UINT32 stride  = gop->Mode->Info->PixelsPerScanLine;
    EFI_GRAPHICS_PIXEL_FORMAT fmt = gop->Mode->Info->PixelFormat;

    volatile UINT32 *fb =
        (volatile UINT32 *)gop->Mode->FrameBufferBase;

    /*
     * Мышь: EFI_SIMPLE_POINTER_PROTOCOL.
     *
     * ВАЖНО про "фундаментальное ограничение": эта ОС никогда
     * не вызывает ExitBootServices - она всё время работает как
     * обычное UEFI-приложение поверх прошивки. Значит, доступ
     * к устройствам возможен только через протоколы, которые
     * САМА прошивка успела опубликовать в DXE-фазе, - своего
     * USB-стека и HID-драйвера у ОС нет и без выхода из Boot
     * Services быть не может.
     *
     * Для beспроводной мыши (в т.ч. Onikuma) это работает так:
     * USB-приёмник (донгл) на шине выглядит как обычная
     * USB HID-мышь - сама беспроводная часть скрыта внутри
     * донгла. Поэтому прошивке, а значит и нам, безразлично,
     * беспроводная мышь или нет: если у прошивки есть
     * встроенный USB HID mouse driver (как в OVMF/QEMU - см.
     * UsbMouseDxe), он публикует EFI_SIMPLE_POINTER_PROTOCOL,
     * и LocateProtocol ниже её найдёт.
     *
     * Собственно ограничение: на РЕАЛЬНОМ железе (не в QEMU)
     * многие прошивки ноутбуков/материнских плат вообще не
     * публикуют этот протокол для USB-мышей - либо потому что
     * USB HID driver в DXE не подключён вендором прошивки,
     * либо потому что подключён только PS/2. В таком случае
     * LocateProtocol честно вернёт "не найдено", и это не баг
     * в коде ниже - это значит, что у конкретной прошивки нет
     * поддержки мыши на уровне DXE. Единственный настоящий
     * выход в такой ситуации - писать собственный драйвер
     * USB-контроллера (xHCI) и HID-класса поверх него, что
     * требует уже выхода из Boot Services (ExitBootServices)
     * и заметно больше кода. Поэтому ниже сделан аккуратный
     * fallback: если протокола нет, GUI остаётся полностью
     * рабочим на стрелках/Enter.
     */
    EFI_GUID pointer_guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    EFI_SIMPLE_POINTER_PROTOCOL *pointer = NULL;
    BOOLEAN have_mouse = FALSE;

    if (
        st->BootServices->LocateProtocol &&
        st->BootServices->LocateProtocol(
            &pointer_guid, NULL, (VOID **)&pointer
        ) == EFI_SUCCESS &&
        pointer != NULL &&
        pointer->GetState != NULL
    ) {

        if (pointer->Reset != NULL)
            pointer->Reset(pointer, FALSE);

        have_mouse = TRUE;

        print(
            out,
            "Mouse found (EFI_SIMPLE_POINTER_PROTOCOL).\n"
        );

    } else {

        print(
            out,
            "No mouse exposed by firmware "
            "(EFI_SIMPLE_POINTER_PROTOCOL not found) - "
            "this UEFI does not publish USB HID mouse input "
            "at boot-services level. Falling back to keyboard "
            "cursor (arrows + Enter).\n"
        );

        if (!g_kernel_mode) {

            print(
                out,
                "TIP: type 'ebs' first - then MyOS uses its OWN USB "
                "mouse driver and the mouse works in the GUI even "
                "on machines like this one.\n"
            );

        } else {

            print(
                out,
                "MyOS's USB driver found no mouse - see 'usb'.\n"
            );
        }
    }

    /*
     * Двойная буферизация: без неё каждый кадр
     * собирался прямо в видимой памяти GOP по частям
     * (фон, иконки, панель задач, курсор отдельными
     * вызовами) — глаз успевал заметить промежуточные
     * состояния, отсюда и мерцание при каждом движении
     * курсора. Теперь кадр целиком собирается в
     * невидимом буфере в обычной RAM, и в видеопамять
     * копируется уже готовым, одним проходом.
     */
    volatile UINT32 *back_buf = NULL;

    {
        GUI_ALLOCATE_POOL AllocatePool =
            (GUI_ALLOCATE_POOL)st->BootServices->AllocatePool;

        UINTN back_size =
            (UINTN)stride * (UINTN)fb_h * sizeof(UINT32);

        VOID *back_raw = NULL;

        if (
            AllocatePool != NULL &&
            AllocatePool(
                GUI_EFI_BOOT_SERVICES_DATA,
                back_size,
                &back_raw
            ) == EFI_SUCCESS
        ) {
            back_buf = (volatile UINT32 *)back_raw;
        }
    }

    /* Буфер, в который реально рисуем каждый кадр. */
    volatile UINT32 *draw_buf =
        (back_buf != NULL) ? back_buf : fb;

    /* Есть задний буфер - курсор живёт отдельно от картинки
       (см. комментарий у g_gui_draw_cursor) */
    g_gui_draw_cursor = (back_buf == NULL);

    BOOLEAN cursor_moved = FALSE;   /* сдвинулся только курсор */
    INTN drawn_cur_x = 0;           /* где курсор нарисован сейчас */
    INTN drawn_cur_y = 0;
    INTN last_hover = -1;

    if (back_buf == NULL) {

        print(
            out,
            "Note: no back buffer, drawing may flicker.\n"
        );
    }

    if (have_mouse) {

        print(
            out,
            "Controls: move the mouse or use arrows, click or "
            "Enter to click, Esc to go back/exit.\n"
        );

    } else {

        print(
            out,
            "Controls: arrows to move cursor, Enter to click, Esc to go back/exit.\n"
        );
    }

    print(
        out,
        "Click Start (top-left) for About, Fetch, Help, Notepad, Explorer, Terminal, Minesweeper, Exit.\n"
    );

    print(
        out,
        "Press any key to enter the GUI...\n"
    );

    /*
     * Реальная пауза перед переходом в графику,
     * иначе это сообщение мгновенно перекроется
     * первым же кадром и его не увидеть.
     */
    {
        EFI_INPUT_KEY key;

        while (
            st->ConIn->ReadKeyStroke(
                st->ConIn, &key
            ) != EFI_SUCCESS
        ) {
            st->BootServices->Stall(10000);
        }
    }

    /* Геометрия модального окна приложений */
    UINTN win_w = fb_w / 2;
    UINTN win_h = fb_h / 2;
    INTN  win_x = ((INTN)fb_w - (INTN)win_w) / 2;
    INTN  win_y = ((INTN)fb_h - (INTN)win_h) / 2;

    UINTN btn_size = 18;
    INTN  btn_x = win_x + (INTN)win_w - (INTN)btn_size - 4;
    INTN  btn_y = win_y + 3;

    /* Геометрия кнопки "Start" на панели задач (сверху слева) */
    INTN  sbtn_x = 3;
    INTN  sbtn_y = 3;
    UINTN sbtn_w = 56;
    UINTN sbtn_h = (UINTN)GUI_TASKBAR_H - 6;

    /* Пункты меню "Start" - все программы, которые раньше
       лежали значками на столе, теперь только здесь. */
    GUI_ICON icons[GUI_ICON_COUNT];

    icons[0].label = "ABOUT";
    icons[0].action = GUI_ACT_ABOUT;

    icons[1].label = "FETCH";
    icons[1].action = GUI_ACT_FETCH;

    icons[2].label = "HELP";
    icons[2].action = GUI_ACT_HELP;

    icons[3].label = "NOTEPAD";
    icons[3].action = GUI_ACT_NOTEPAD;

    icons[4].label = "TERMINAL";
    icons[4].action = GUI_ACT_TERMINAL;

    icons[5].label = "EXPLORER";
    icons[5].action = GUI_ACT_EXPLORER;

    icons[6].label = "MINESWEEPER";
    icons[6].action = GUI_ACT_MINESWEEPER;

    icons[7].label = "EXIT";
    icons[7].action = GUI_ACT_EXIT;

    /* Список выпадает вниз прямо из-под кнопки "Start" */
    for (UINTN i = 0; i < GUI_ICON_COUNT; i++) {

        icons[i].x = sbtn_x;
        icons[i].y = GUI_TASKBAR_H + (INTN)(i * GUI_ICON_H);
        icons[i].w = GUI_ICON_W;
        icons[i].h = GUI_ICON_H;
    }

    BOOLEAN menu_open = FALSE;

    /* Содержимое окон About / Fetch / Help */
    static const char *about_lines[] = {
        "MYOS 0.1",
        "MINIMAL UEFI OS BUILT",
        "FROM SCRATCH.",
        "NO LINUX NO WINDOWS.",
        "ARROWS MOVE CURSOR,",
        "ENTER CLICKS."
    };

    static const char *help_lines[] = {
        "CONSOLE COMMANDS:",
        "HELP ABOUT FETCH VER",
        "TIME DATE BANNER LS",
        "CAT SIZE REBOOT",
        "TYPE THEM IN THE",
        "SHELL, NOT HERE."
    };

    char fetch_res[32];
    char fetch_fmt[24];

    {
        UINTN n = gui_uint_to_str(fb_w, fetch_res);

        fetch_res[n++] = ' ';
        fetch_res[n++] = 'X';
        fetch_res[n++] = ' ';

        n += gui_uint_to_str(fb_h, fetch_res + n);

        fetch_res[n] = '\0';
    }

    {
        const char *fname;

        if (fmt == PixelRedGreenBlueReserved8BitPerColor)
            fname = "FORMAT: RGB";
        else
            fname = "FORMAT: BGR";

        UINTN i = 0;

        while (fname[i] != '\0' && i < sizeof(fetch_fmt) - 1) {
            fetch_fmt[i] = fname[i];
            i++;
        }

        fetch_fmt[i] = '\0';
    }

    const char *fetch_lines[3];
    fetch_lines[0] = "GOP FRAMEBUFFER:";
    fetch_lines[1] = fetch_res;
    fetch_lines[2] = fetch_fmt;

    static const char *notepad_lines[] = {
        "NOTEPAD",
        "",
        "SIMPLE TEXT VIEWER.",
        "TYPING IS NOT YET",
        "SUPPORTED IN GUI MODE,",
        "ONLY ARROWS + ENTER."
    };

    GUI_WINDOW_CONTENT windows[9];

    windows[GUI_ACT_ABOUT].title = "ABOUT";
    windows[GUI_ACT_ABOUT].lines = about_lines;
    windows[GUI_ACT_ABOUT].line_count =
        sizeof(about_lines) / sizeof(about_lines[0]);

    windows[GUI_ACT_FETCH].title = "FETCH";
    windows[GUI_ACT_FETCH].lines = fetch_lines;
    windows[GUI_ACT_FETCH].line_count = 3;

    windows[GUI_ACT_HELP].title = "HELP";
    windows[GUI_ACT_HELP].lines = help_lines;
    windows[GUI_ACT_HELP].line_count =
        sizeof(help_lines) / sizeof(help_lines[0]);

    windows[GUI_ACT_NOTEPAD].title = "NOTEPAD";
    windows[GUI_ACT_NOTEPAD].lines = notepad_lines;
    windows[GUI_ACT_NOTEPAD].line_count =
        sizeof(notepad_lines) / sizeof(notepad_lines[0]);

    /* "Проводник": показывает содержимое той же RAM-FS,
       что и команды ls/cat/write в терминале. Строки
       собираются в этот буфер заново каждый раз, когда
       окно открывают, - чтобы список не отставал от
       файлов, созданных/изменённых через терминал.
       explorer_row_fs[i] - индекс файла в g_fs для
       i-й строки списка (или -1 для строк-заглушек
       вроде "(EMPTY)"/"..."), чтобы клик по строке
       открывал именно этот файл. */
    char explorer_buf[GUI_EXPLORER_MAX_ROWS + 1][GUI_EXPLORER_LINE_LEN];
    const char *explorer_lines[GUI_EXPLORER_MAX_ROWS + 1];
    int explorer_row_fs[GUI_EXPLORER_MAX_ROWS + 1];
    UINTN explorer_line_count = 0;

    windows[GUI_ACT_EXPLORER].title = "FILE EXPLORER";
    windows[GUI_ACT_EXPLORER].lines = explorer_lines;
    windows[GUI_ACT_EXPLORER].line_count = 0;

    /* Окно просмотра файла, открываемое кликом по строке
       в Проводнике. Как и explorer_buf, пересобирается
       заново при каждом открытии файла. */
    char fileview_buf[GUI_FILEVIEW_MAX_ROWS][GUI_FILEVIEW_LINE_LEN];
    const char *fileview_lines[GUI_FILEVIEW_MAX_ROWS];
    char fileview_title[FS_NAME_MAX + 1];

    windows[GUI_ACT_FILEVIEW].title = fileview_title;
    windows[GUI_ACT_FILEVIEW].lines = fileview_lines;
    windows[GUI_ACT_FILEVIEW].line_count = 0;

    INTN cur_x = (INTN)fb_w / 2;
    INTN cur_y = (INTN)fb_h / 2;

    /* Накопитель дробного остатка движения мыши (чтобы не
       терять медленные/мелкие смещения при делении на
       разрешение устройства) и состояние левой кнопки на
       предыдущем опросе (для детектирования "нажал только
       что", а не "зажата уже давно"). */
    INTN mouse_rem_x = 0;
    INTN mouse_rem_y = 0;
    BOOLEAN mouse_left_prev = FALSE;
    BOOLEAN mouse_click_pending = FALSE;

    BOOLEAN in_window = FALSE;
    UINTN open_action = 0;
    BOOLEAN want_cmd = FALSE;

    /* Терминал - отдельное состояние окна: своё окно
       на столе, как приложение (kitty), а не режим,
       выбрасывающий из GUI обратно в консоль. */
    BOOLEAN in_terminal = FALSE;
    BOOLEAN term_started = FALSE;

    /* Сапёр - тоже отдельное состояние окна, как терминал:
       своё окно на столе, а не запись в windows[]. Поле
       заводится один раз при первом открытии и живёт дальше,
       пока не нажат смайлик (рестарт) или Esc (просто закрыть
       окно, партия не сбрасывается). */
    BOOLEAN in_minesweeper = FALSE;
    BOOLEAN ms_inited = FALSE;
    GUI_MS_STATE ms;

    char  term_lines[GUI_TERM_MAX_LINES][GUI_TERM_LINE_LEN + 1];
    UINTN term_line_count = 0;

    char  term_input[GUI_TERM_LINE_LEN + 1];
    UINTN term_input_len = 0;

    term_input[0] = '\0';

    BOOLEAN dirty = TRUE;
    UINTN clock_tick = 0;
    char clock_text[9];

    clock_text[0] = '-';
    clock_text[1] = '-';
    clock_text[2] = ':';
    clock_text[3] = '-';
    clock_text[4] = '-';
    clock_text[5] = ':';
    clock_text[6] = '-';
    clock_text[7] = '-';
    clock_text[8] = '\0';

    for (;;) {

        /*
         * Раз примерно в секунду обновляем часы
         * в панели задач (даже без нажатий клавиш).
         */
        if (clock_tick == 0) {

            EFI_TIME now;

            if (
                st->RuntimeServices->GetTime &&
                st->RuntimeServices->GetTime(
                    &now, NULL
                ) == EFI_SUCCESS
            ) {

                gui_uint2_to_str(now.Hour, clock_text);
                gui_uint2_to_str(now.Minute, clock_text + 3);
                gui_uint2_to_str(now.Second, clock_text + 6);

                if (!in_window)
                    dirty = TRUE;
            }

            /* Таймер Сапёра тикает раз в секунду тем же
               способом, что и часы на панели задач - только
               пока партия идёт (мины уже расставлены и игра
               ещё не закончена). */
            if (in_minesweeper && ms.generated && !ms.over) {
                ms.timer++;
                dirty = TRUE;
            }
        }

        clock_tick++;

        if (clock_tick >= 250)
            clock_tick = 0;

        /* Опрос мыши - каждый кадр, независимо от клавиатуры.
           GetState неблокирующий: если новых данных с донгла/
           устройства не было, он просто вернёт последнее же
           состояние, поэтому дельта окажется нулевой и ничего
           лишнего не произойдёт. */
        if (have_mouse) {

            EFI_SIMPLE_POINTER_STATE pst;

            if (pointer->GetState(pointer, &pst) == EFI_SUCCESS) {

                INT64 res_x =
                    (pointer->Mode != NULL &&
                     pointer->Mode->ResolutionX != 0)
                        ? (INT64)pointer->Mode->ResolutionX
                        : 1;

                INT64 res_y =
                    (pointer->Mode != NULL &&
                     pointer->Mode->ResolutionY != 0)
                        ? (INT64)pointer->Mode->ResolutionY
                        : 1;

                /* RelativeMovement* приходит в "счётчиках",
                   ResolutionX/Y - счётчиков на мм (по спеке
                   UEFI). Переводим в пиксели через условную
                   чувствительность в пикселях на мм, а остаток
                   от деления копим, чтобы медленные движения
                   мыши не "съедались" округлением. */
                INT64 dx_num =
                    (INT64)pst.RelativeMovementX *
                        GUI_MOUSE_PIXELS_PER_MM +
                    (INT64)mouse_rem_x;

                INT64 dy_num =
                    (INT64)pst.RelativeMovementY *
                        GUI_MOUSE_PIXELS_PER_MM +
                    (INT64)mouse_rem_y;

                INTN dx = (INTN)(dx_num / res_x);
                INTN dy = (INTN)(dy_num / res_y);

                mouse_rem_x = (INTN)(dx_num - (INT64)dx * res_x);
                mouse_rem_y = (INTN)(dy_num - (INT64)dy * res_y);

                if (dx != 0 || dy != 0) {

                    cur_x += dx;
                    cur_y += dy;

                    if (cur_x < 0)
                        cur_x = 0;

                    if (cur_y < 0)
                        cur_y = 0;

                    if (cur_x > (INTN)fb_w - GUI_CURSOR_SIZE)
                        cur_x = (INTN)fb_w - GUI_CURSOR_SIZE;

                    if (cur_y > (INTN)fb_h - GUI_CURSOR_SIZE)
                        cur_y = (INTN)fb_h - GUI_CURSOR_SIZE;

                    if (g_gui_draw_cursor) {

                        /* нет заднего буфера - только полный кадр */
                        dirty = TRUE;

                    } else {

                        INTN hk = gui_hover_key(
                            in_minesweeper,
                            !in_window && !in_terminal &&
                                !in_minesweeper && menu_open,
                            icons,
                            win_x, win_y, win_w, win_h,
                            cur_x, cur_y
                        );

                        if (hk != last_hover)
                            dirty = TRUE;
                        else
                            cursor_moved = TRUE;
                    }
                }

                /* Клик засчитываем по фронту нажатия (кнопка
                   была отпущена, теперь нажата), а не по факту
                   "кнопка сейчас зажата" - иначе удержание кнопки
                   спамило бы кликами каждый кадр. */
                if (pst.LeftButton && !mouse_left_prev)
                    mouse_click_pending = TRUE;

                mouse_left_prev = pst.LeftButton;
            }
        }

        EFI_INPUT_KEY key;

        BOOLEAN got_event = (
            st->ConIn->ReadKeyStroke(
                st->ConIn, &key
            ) == EFI_SUCCESS
        );

        /* Если в этом кадре не было клавиши, но есть неотданный
           клик мышью - превращаем его в тот же самый Enter,
           который весь код ниже уже умеет обрабатывать как клик.
           Так не пришлось дублировать все хит-тесты под мышь
           отдельно. */
        if (!got_event && mouse_click_pending) {

            key.ScanCode = 0x00;
            key.UnicodeChar = CHAR_CARRIAGE_RETURN;
            got_event = TRUE;
            mouse_click_pending = FALSE;
        }

        if (got_event) {

            /* Esc: закрыть окно, либо меню Start,
               либо выйти из GUI */
            if (key.ScanCode == 0x17) {

                if (in_terminal) {
                    in_terminal = FALSE;
                    dirty = TRUE;
                    continue;
                }

                if (in_minesweeper) {
                    in_minesweeper = FALSE;
                    dirty = TRUE;
                    continue;
                }

                if (in_window) {
                    in_window = FALSE;
                    dirty = TRUE;
                    continue;
                }

                if (menu_open) {
                    menu_open = FALSE;
                    dirty = TRUE;
                    continue;
                }

                break;
            }

            /* Пока открыт терминал, клавиши идут не на
               управление курсором, а прямо в строку
               ввода - как в обычном приложении-терминале,
               а не в режиме "стрелки+Enter" остального GUI. */
            if (in_terminal) {

                if (key.UnicodeChar == CHAR_BACKSPACE) {

                    if (term_input_len > 0) {
                        term_input_len--;
                        term_input[term_input_len] = '\0';
                        dirty = TRUE;
                    }

                    continue;
                }

                if (
                    key.ScanCode == 0x00 &&
                    key.UnicodeChar == CHAR_CARRIAGE_RETURN
                ) {

                    BOOLEAN want_close =
                        gui_term_exec(
                            st, term_input,
                            term_lines, &term_line_count
                        );

                    term_input_len = 0;
                    term_input[0] = '\0';
                    dirty = TRUE;

                    if (want_close) {
                        in_terminal = FALSE;
                        term_started = FALSE;
                    }

                    continue;
                }

                if (
                    key.UnicodeChar != 0 &&
                    term_input_len < GUI_TERM_LINE_LEN
                ) {

                    char c = (char)key.UnicodeChar;

                    if (c >= 'a' && c <= 'z')
                        c = (char)(c - 'a' + 'A');

                    if (gui_find_glyph(c) != NULL) {

                        term_input[term_input_len++] = c;
                        term_input[term_input_len] = '\0';
                        dirty = TRUE;
                    }
                }

                continue;
            }

            /* Пока открыт Сапёр, обрабатываем клавиши сами -
               те же стрелки+Enter, что и везде, плюс F для
               флажка. Свой блок, а не общий ниже, потому что
               тут нужны свои хит-тесты (клетки/смайлик), а не
               иконки меню Start. */
            if (in_minesweeper) {

                if (key.ScanCode == 0x01) {
                    cur_y -= GUI_CURSOR_STEP;
                    dirty = TRUE;

                } else if (key.ScanCode == 0x02) {
                    cur_y += GUI_CURSOR_STEP;
                    dirty = TRUE;

                } else if (key.ScanCode == 0x03) {
                    cur_x += GUI_CURSOR_STEP;
                    dirty = TRUE;

                } else if (key.ScanCode == 0x04) {
                    cur_x -= GUI_CURSOR_STEP;
                    dirty = TRUE;
                }

                if (cur_x < 0)
                    cur_x = 0;

                if (cur_y < 0)
                    cur_y = 0;

                if (cur_x > (INTN)fb_w - GUI_CURSOR_SIZE)
                    cur_x = (INTN)fb_w - GUI_CURSOR_SIZE;

                if (cur_y > (INTN)fb_h - GUI_CURSOR_SIZE)
                    cur_y = (INTN)fb_h - GUI_CURSOR_SIZE;

                INTN mcx = cur_x + GUI_CURSOR_SIZE / 2;
                INTN mcy = cur_y + GUI_CURSOR_SIZE / 2;

                GUI_MS_LAYOUT L;
                gui_ms_compute_layout(win_x, win_y, win_w, win_h, &L);

                UINTN grid_w_px = GUI_MS_COLS * L.cell;
                UINTN grid_h_px = GUI_MS_ROWS * L.cell;

                char kc = (char)key.UnicodeChar;

                if (kc >= 'a' && kc <= 'z')
                    kc = (char)(kc - 'a' + 'A');

                if (
                    kc == 'F' &&
                    gui_point_in_rect(
                        mcx, mcy, L.grid_x, L.grid_y,
                        grid_w_px, grid_h_px
                    )
                ) {
                    int col = (int)((UINTN)(mcx - L.grid_x) / L.cell);
                    int row = (int)((UINTN)(mcy - L.grid_y) / L.cell);

                    gui_ms_toggle_flag(&ms, row, col);
                    dirty = TRUE;
                }

                if (
                    key.ScanCode == 0x00 &&
                    key.UnicodeChar == CHAR_CARRIAGE_RETURN
                ) {

                    if (
                        gui_point_in_rect(
                            mcx, mcy, btn_x, btn_y, btn_size, btn_size
                        )
                    ) {

                        in_minesweeper = FALSE;
                        dirty = TRUE;

                    } else if (
                        gui_point_in_rect(
                            mcx, mcy, L.smile_x, L.smile_y,
                            L.smile_size, L.smile_size
                        )
                    ) {

                        gui_ms_reset(&ms);
                        dirty = TRUE;

                    } else if (
                        !ms.over &&
                        gui_point_in_rect(
                            mcx, mcy, L.grid_x, L.grid_y,
                            grid_w_px, grid_h_px
                        )
                    ) {

                        int col =
                            (int)((UINTN)(mcx - L.grid_x) / L.cell);
                        int row =
                            (int)((UINTN)(mcy - L.grid_y) / L.cell);

                        gui_ms_reveal(&ms, row, col);
                        dirty = TRUE;
                    }
                }

                continue;
            }

            /* Стрелки двигают курсор */
            if (key.ScanCode == 0x01) {
                cur_y -= GUI_CURSOR_STEP;
                dirty = TRUE;

            } else if (key.ScanCode == 0x02) {
                cur_y += GUI_CURSOR_STEP;
                dirty = TRUE;

            } else if (key.ScanCode == 0x03) {
                cur_x += GUI_CURSOR_STEP;
                dirty = TRUE;

            } else if (key.ScanCode == 0x04) {
                cur_x -= GUI_CURSOR_STEP;
                dirty = TRUE;
            }

            if (cur_x < 0)
                cur_x = 0;

            if (cur_y < 0)
                cur_y = 0;

            if (cur_x > (INTN)fb_w - GUI_CURSOR_SIZE)
                cur_x = (INTN)fb_w - GUI_CURSOR_SIZE;

            if (cur_y > (INTN)fb_h - GUI_CURSOR_SIZE)
                cur_y = (INTN)fb_h - GUI_CURSOR_SIZE;

            /* Enter работает как клик левой кнопкой мыши */
            if (
                key.ScanCode == 0x00 &&
                key.UnicodeChar == CHAR_CARRIAGE_RETURN
            ) {

                INTN cx = cur_x + GUI_CURSOR_SIZE / 2;
                INTN cy = cur_y + GUI_CURSOR_SIZE / 2;

                if (!in_window && menu_open) {

                    /* Клик по кнопке "Start" пока меню
                       открыто - просто закрыть его. */
                    if (
                        gui_point_in_rect(
                            cx, cy,
                            sbtn_x, sbtn_y, sbtn_w, sbtn_h
                        )
                    ) {

                        menu_open = FALSE;
                        dirty = TRUE;

                    } else {

                        for (UINTN i = 0; i < GUI_ICON_COUNT; i++) {

                            if (
                                gui_point_in_rect(
                                    cx, cy,
                                    icons[i].x, icons[i].y,
                                    icons[i].w, icons[i].h
                                )
                            ) {

                                if (icons[i].action == GUI_ACT_EXIT) {

                                    in_window = FALSE;
                                    want_cmd = FALSE;
                                    goto gui_exit_loop;
                                }

                                if (icons[i].action == GUI_ACT_TERMINAL) {

                                    in_window = FALSE;

                                    if (!term_started) {

                                        term_started = TRUE;

                                        gui_term_push(
                                            term_lines,
                                            &term_line_count,
                                            "MYOS TERMINAL. TYPE HELP."
                                        );
                                    }

                                    in_terminal = TRUE;
                                    menu_open = FALSE;
                                    dirty = TRUE;
                                    break;
                                }

                                if (icons[i].action == GUI_ACT_MINESWEEPER) {

                                    in_window = FALSE;

                                    if (!ms_inited) {

                                        gui_ms_seed(st);
                                        gui_ms_reset(&ms);
                                        ms_inited = TRUE;
                                    }

                                    in_minesweeper = TRUE;
                                    menu_open = FALSE;
                                    dirty = TRUE;
                                    break;
                                }

                                if (icons[i].action == GUI_ACT_EXPLORER) {

                                    /* Пересобрать список файлов
                                       из RAM-FS прямо перед
                                       открытием окна. */
                                    explorer_line_count = 0;

                                    for (int fi = 0;
                                         fi < FS_MAX_FILES &&
                                         explorer_line_count <
                                             GUI_EXPLORER_MAX_ROWS;
                                         fi++) {

                                        if (!g_fs[fi].used)
                                            continue;

                                        char *row =
                                            explorer_buf[
                                                explorer_line_count
                                            ];
                                        UINTN p = 0;

                                        for (UINTN c = 0;
                                             g_fs[fi].name[c] != 0 &&
                                             p < GUI_EXPLORER_LINE_LEN
                                                 - 16;
                                             c++) {

                                            CHAR16 wc =
                                                g_fs[fi].name[c];

                                            row[p++] =
                                                (wc < 128)
                                                    ? (char)wc
                                                    : ' ';
                                        }

                                        row[p++] = ' ';
                                        row[p++] = '-';
                                        row[p++] = ' ';

                                        p += gui_uint_to_str(
                                            g_fs[fi].size,
                                            row + p
                                        );

                                        row[p++] = 'B';
                                        row[p] = '\0';

                                        explorer_lines[
                                            explorer_line_count
                                        ] = row;

                                        explorer_row_fs[
                                            explorer_line_count
                                        ] = fi;

                                        explorer_line_count++;
                                    }

                                    if (explorer_line_count == 0) {

                                        explorer_lines[0] =
                                            "EMPTY - NO FILES";
                                        explorer_row_fs[0] = -1;
                                        explorer_line_count = 1;

                                    } else {

                                        int more = 0;

                                        for (int fi = 0;
                                             fi < FS_MAX_FILES;
                                             fi++) {

                                            if (g_fs[fi].used)
                                                more++;
                                        }

                                        if ((UINTN)more >
                                            explorer_line_count &&
                                            explorer_line_count <
                                                GUI_EXPLORER_MAX_ROWS
                                                    + 1) {

                                            char *row =
                                                explorer_buf[
                                                    explorer_line_count
                                                ];

                                            UINTN p = 0;
                                            row[p++] = '.';
                                            row[p++] = '.';
                                            row[p++] = '.';
                                            row[p] = '\0';

                                            explorer_lines[
                                                explorer_line_count
                                            ] = row;

                                            explorer_row_fs[
                                                explorer_line_count
                                            ] = -1;

                                            explorer_line_count++;
                                        }
                                    }

                                    windows[GUI_ACT_EXPLORER]
                                        .line_count =
                                        explorer_line_count;
                                }

                                open_action = icons[i].action;
                                in_window = TRUE;
                                break;
                            }
                        }

                        /* Клик по пункту или мимо меню -
                           в любом случае закрыть список. */
                        menu_open = FALSE;
                        dirty = TRUE;
                    }

                } else if (!in_window) {

                    /* Меню закрыто: единственное, на что
                       можно кликнуть на столе - кнопка Start. */
                    if (
                        gui_point_in_rect(
                            cx, cy,
                            sbtn_x, sbtn_y, sbtn_w, sbtn_h
                        )
                    ) {

                        menu_open = TRUE;
                        dirty = TRUE;
                    }

                } else if (
                    cx >= btn_x &&
                    cx <  btn_x + (INTN)btn_size &&
                    cy >= btn_y &&
                    cy <  btn_y + (INTN)btn_size
                ) {

                    /* "клик" по "X" — закрыть окно */
                    in_window = FALSE;
                    dirty = TRUE;

                } else if (open_action == GUI_ACT_EXPLORER) {

                    /* Клик по строке файла в Проводнике -
                       открыть его в окне просмотра. Та же
                       геометрия строк, что и в gui_draw_window:
                       первая строка на win_y+40+GUI_MENU_H,
                       дальше каждая следующая на +14. */
                    INTN rows_y0 =
                        win_y + 40 + (INTN)GUI_MENU_H;

                    if (
                        cx >= win_x + 9 &&
                        cx <  win_x + (INTN)win_w - 9 &&
                        cy >= rows_y0
                    ) {

                        UINTN row =
                            (UINTN)(cy - rows_y0) / 14;

                        if (row < explorer_line_count &&
                            explorer_row_fs[row] >= 0) {

                            windows[GUI_ACT_FILEVIEW]
                                .line_count =
                                gui_open_fileview(
                                    explorer_row_fs[row],
                                    fileview_buf,
                                    fileview_lines,
                                    fileview_title
                                );

                            open_action = GUI_ACT_FILEVIEW;
                            dirty = TRUE;
                        }
                    }
                }
            }
        }

        if (dirty) {

            if (in_terminal) {

                gui_draw_terminal(
                    draw_buf, stride, fb_w, fb_h, fmt,
                    win_x, win_y, win_w, win_h,
                    btn_x, btn_y, btn_size,
                    (const char (*)[GUI_TERM_LINE_LEN + 1])
                        term_lines,
                    term_line_count,
                    term_input
                );

            } else if (in_minesweeper) {

                gui_draw_minesweeper(
                    draw_buf, stride, fb_w, fb_h, fmt,
                    win_x, win_y, win_w, win_h,
                    btn_x, btn_y, btn_size,
                    &ms,
                    cur_x, cur_y
                );

            } else if (in_window) {

                gui_draw_window(
                    draw_buf, stride, fb_w, fb_h, fmt,
                    win_x, win_y, win_w, win_h,
                    btn_x, btn_y, btn_size,
                    &windows[open_action],
                    cur_x, cur_y
                );

            } else {

                gui_draw_desktop(
                    draw_buf, stride, fb_w, fb_h, fmt,
                    icons, GUI_ICON_COUNT,
                    menu_open,
                    sbtn_x, sbtn_y, sbtn_w, sbtn_h,
                    cur_x, cur_y,
                    clock_text
                );
            }

            /*
             * Готовый кадр из невидимого буфера —
             * одним проходом в реальную видеопамять.
             */
            if (back_buf != NULL) {

                UINTN total = (UINTN)stride * (UINTN)fb_h;

                for (UINTN i = 0; i < total; i++)
                    fb[i] = back_buf[i];

                /* курсор - только в видеопамяти, поверх */
                gui_draw_cursor_at(fb, stride, fb_w, fb_h, fmt, cur_x, cur_y);
                drawn_cur_x = cur_x;
                drawn_cur_y = cur_y;

                last_hover = gui_hover_key(
                    in_minesweeper,
                    !in_window && !in_terminal &&
                        !in_minesweeper && menu_open,
                    icons,
                    win_x, win_y, win_w, win_h,
                    cur_x, cur_y
                );
            }

            dirty = FALSE;
            cursor_moved = FALSE;

        } else if (cursor_moved) {

            /* Быстрый путь: вернуть картинку под старым местом
               курсора из заднего буфера и нарисовать курсор на
               новом. Два квадратика 12x12 вместо всего экрана. */
            gui_blit_rect(
                fb, back_buf, stride, fb_w, fb_h,
                drawn_cur_x, drawn_cur_y,
                GUI_CURSOR_SIZE, GUI_CURSOR_SIZE
            );

            gui_draw_cursor_at(fb, stride, fb_w, fb_h, fmt, cur_x, cur_y);

            drawn_cur_x = cur_x;
            drawn_cur_y = cur_y;
            cursor_moved = FALSE;
        }

        /*
         * Пауза между итерациями. Раньше - 4 мс всегда. Теперь
         * меньше, если мышь двигается прямо сейчас (быстрый путь
         * стоит копейки, а чем чаще опрос - тем плавнее курсор),
         * и по-прежнему 4 мс в покое, чтобы не молотить впустую.
         */
        st->BootServices->Stall(have_mouse ? 1000 : 4000);
    }

gui_exit_loop:

    g_gui_draw_cursor = TRUE;

    if (back_buf != NULL) {

        GUI_FREE_POOL FreePool =
            (GUI_FREE_POOL)st->BootServices->FreePool;

        if (FreePool != NULL)
            FreePool((VOID *)back_buf);
    }

    /* Возврат в текстовый режим */
    out->ClearScreen(out);
    set_color(out, g_color);

    if (want_cmd) {

        print(
            out,
            "Command line. Type 'start' to return to the GUI.\n"
        );

    } else {

        print(
            out,
            "Left GUI shell.\n"
        );
    }
}



/*
 * Опрос Event Ring в поисках события конкретного типа, с
 * пропуском посторонних событий.
 *
 * ВАЖНОЕ ОТКРЫТИЕ (по скриншоту первого прогона Шага 5): после
 * Port Reset контроллер сам, асинхронно, кладёт в Event Ring
 * Port Status Change Event (TRB Type=34) - это отдельное,
 * ожидаемое по спеке xHCI событие ("порт сменил состояние"),
 * никак не связанное с Command Completion Event, которого мы
 * ждём после Enable Slot/Address Device. Раньше код читал
 * СТРОГО первый попавшийся TRB в Event Ring и считал его тем,
 * чего ждал - из-за этого он принимал Port Status Change
 * Event (34) за Command Completion Event (33) и вся дальнейшая
 * логика (проверка `trb_type==33`) просто тихо не срабатывала,
 * SlotID читался с мусорным нулём, дальше по коду ничего не
 * происходило.
 *
 * Эта функция вместо чтения ровно одного TRB - идёт по кольцу
 * TRB за TRB (используя *io_slot как "текущий индекс", который
 * сохраняется между вызовами - Event Ring общий на весь
 * контроллер, а не привязан к одной команде), и для каждого
 * встреченного TRB: если это не тот тип, который мы ждём -
 * подтверждает его (сдвигает ERDP) и идёт дальше; если тот -
 * возвращает указатель на него, НЕ трогая ERDP (подтверждение
 * этого конкретного события - как и раньше, дело вызывающего
 * кода, после того как он его разобрал и напечатал).
 */

/*
 * Размер Event Ring в TRB. У нас под него выделена ровно одна
 * страница (4096 байт), TRB занимает 16 байт => 256 штук.
 * ДОЛЖНО совпадать с тем, что реально записано в ERSTSZ при
 * создании кольца (см. код чуть выше, где выделяются страницы
 * под Event Ring/ERST) - если когда-нибудь решим сделать кольцо
 * больше одной страницы, это число тоже надо будет поменять.
 */
#define XHCI_EVENT_RING_TRBS 256u

/*
 * ev_slot у нас - это "сквозной", вечно растущий номер по счёту
 * обработанного события (0, 1, 2, 3, ...), а не индекс внутри
 * кольца - так удобнее передавать между функциями (Address
 * Device, GET_DESCRIPTOR, Configure Endpoint, опрос отчётов
 * мыши - все они просто продолжают считать оттуда, где
 * остановился предыдущий шаг). Но физический адрес TRB в памяти
 * и ожидаемое значение его Cycle Bit нужно каждый раз вычислять
 * заново из этого сквозного номера - вот эти две функции.
 *
 * НАЙДЕННЫЙ БАГ (по скриншоту - опрос отчётов мыши "останавливался"
 * ровно около 249-256 отчёта, а перед остановкой печатались три
 * подозрительные строки "skipping unrelated event, TRB Type=9/11/12"
 * - а это же типы самих КОМАНД (Enable Slot/Address Device/
 * Configure Endpoint), а не событий (у событий Command Completion
 * Event всегда TRB Type=33!). Причина: раньше адрес TRB считался
 * просто как evring_phys + ev_slot*16, без остатка от деления на
 * размер кольца - то есть уже после первых 256 обработанных
 * событий код преспокойно "уезжал" за пределы выделенной под
 * Event Ring страницы и начинал читать СОСЕДНЮЮ страницу памяти
 * (которая оказалась Command Ring'ом - там как раз и лежат TRB
 * с типами 9/11/12, ещё с выставленным Cycle=1, поэтому код
 * принимал их за "новые события"). Дальше, за Command Ring'ом,
 * шла обнулённая память (Cycle=0) - вот и всё, опрос "замирал",
 * решив, что событий больше нет.
 *
 * Правильное поведение xHCI: Event Ring - кольцевой буфер.
 * Контроллер, записав TRB в последний слот кольца, на следующем
 * событии возвращается к слоту 0 - но при этом переворачивает
 * Cycle Bit (первый круг - пишет туда 1, второй круг - 0, третий
 * - снова 1, и так далее). Это единственный способ software
 * отличить "ещё не обработанное новое событие второго круга" от
 * "старого мусора первого круга, оставшегося по тому же адресу".
 * Поэтому здесь мы: (1) берём остаток от деления ev_slot на
 * размер кольца - получаем настоящий адрес внутри кольца;
 * (2) считаем, сколько полных кругов кольца уже пройдено
 * (ev_slot / размер кольца), и если это число нечётное - значит
 * сейчас ожидаем Cycle=0, если чётное (включая 0) - ожидаем
 * Cycle=1.
 */
static UINT64 xhci_event_ring_trb_addr(
    UINT64 evring_phys,
    UINTN  ev_slot
)
{
    UINTN slot_in_ring =
        ev_slot % (UINTN)XHCI_EVENT_RING_TRBS;

    return evring_phys + (UINT64)slot_in_ring * 16u;
}

static UINT32 xhci_event_ring_expected_cycle(
    UINTN ev_slot
)
{
    UINTN laps_done =
        ev_slot / (UINTN)XHCI_EVENT_RING_TRBS;

    if ((laps_done % 2u) == 0u)
        return 1u;

    return 0u;
}

/*
 * То же самое, но для Transfer Ring конечной точки (используется
 * пока только для Interrupt IN endpoint'а мыши, но по спеке любой
 * Transfer Ring устроен одинаково) - здесь МЫ производитель
 * (production side), а контроллер - потребитель, поэтому кольцо
 * организовано иначе, чем Event Ring: явным Link TRB в последнем
 * слоте страницы (см. подробный комментарий на месте его записи),
 * а не подразумеваемым остатком от деления. Из 256 TRB страницы
 * последний слот - служебный Link TRB, поэтому "полезных" слотов
 * под настоящие данные - 255.
 */
#define XHCI_XFER_RING_TRBS 256u
#define XHCI_XFER_RING_USABLE_TRBS (XHCI_XFER_RING_TRBS - 1u)

static UINT64 xhci_xfer_ring_trb_addr(
    UINT64 ring_phys,
    UINTN  seq
)
{
    UINTN slot_in_ring =
        seq % (UINTN)XHCI_XFER_RING_USABLE_TRBS;

    return ring_phys + (UINT64)slot_in_ring * 16u;
}

static UINT32 xhci_xfer_ring_pcs(
    UINTN seq
)
{
    UINTN laps_done =
        seq / (UINTN)XHCI_XFER_RING_USABLE_TRBS;

    if ((laps_done % 2u) == 0u)
        return 1u;

    return 0u;
}

static BOOLEAN xhci_wait_for_event(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 evring_phys,
    UINT64 intr0,
    UINTN *io_slot,
    UINT8 want_trb_type,
    volatile UINT32 **out_trb
)
{
    for (;;) {

        UINT64 trb_addr =
            xhci_event_ring_trb_addr(evring_phys, *io_slot);

        UINT32 expected_cycle =
            xhci_event_ring_expected_cycle(*io_slot);

        volatile UINT32 *trb =
            (volatile UINT32 *)(UINTN)trb_addr;

        BOOLEAN got = FALSE;

        for (UINTN i = 0; i < 500; i++) {

            /* Раньше здесь стоял BootServices->Stall(1000) -
               эта функция теперь вызывается только после
               ExitBootServices (см. команду "ebs"), поэтому
               задержка firmware-независимая. */
            busy_wait_ms(1);

            if ((trb[3] & 0x1u) == expected_cycle) {
                got = TRUE;
                break;
            }
        }

        if (!got)
            return FALSE;

        UINT8 t = (UINT8)((trb[3] >> 10) & 0x3Fu);

        if (t == want_trb_type) {
            *out_trb = trb;
            return TRUE;
        }

        /* постороннее событие (например Port Status Change,
           TRB Type=34, которое генерит сам Port Reset) - не
           то, чего мы ждём именно в этой точке кода, но оно
           уже заняло место в Event Ring, и его нужно
           подтвердить (сдвинуть ERDP), иначе контроллер будет
           считать, что мы его ещё не обработали, и рано или
           поздно решит, что кольцо переполнено. Пропускаем и
           идём дальше. */

        print(out, "  (skipping unrelated event, TRB Type=");
        print_uint(out, t);
        print(out, ")\n");

        (*io_slot) = (*io_slot) + 1;

        UINT64 next_addr =
            xhci_event_ring_trb_addr(evring_phys, *io_slot);

        mmio_write32(
            intr0 + 0x18,
            (UINT32)(next_addr & 0xFFFFFFFFu)
        );
        mmio_write32(
            intr0 + 0x1C,
            (UINT32)(next_addr >> 32)
        );
    }
}



/*
 * Универсальный USB Control Transfer через Endpoint 0 (Setup +
 * опционально Data + Status), поверх уже настроенного Transfer
 * Ring конкретного устройства. Раньше (в самой первой версии
 * GET_DESCRIPTOR) все три TRB собирались вручную прямо в теле
 * xhci_address_device_and_get_descriptor - именно там и нашлась
 * ошибка с перепутанными битами IDT/IOC. Вынесено сюда одной
 * функцией: во-первых, чтобы больше не дублировать этот код
 * (следующие шаги - SET_CONFIGURATION, HID SET_PROTOCOL и
 * т.п. - тоже control transfers), во-вторых, чтобы саму логику
 * битов было где один раз перепроверить и больше не трогать.
 *
 * io_trb_slot - "текущий свободный слот" на Transfer Ring
 * Endpoint 0, в TRB (по 16 байт), сохраняется между вызовами -
 * кольцо общее на весь Endpoint 0, каждый новый control transfer
 * просто пишется дальше по кольцу.
 * io_ev_slot - аналогично, текущий слот Event Ring (общий на
 * весь контроллер, разделяется и с Command Ring).
 *
 * wLength==0 означает запрос без стадии данных (например
 * SET_CONFIGURATION) - тогда Data Stage TRB не создаётся вообще,
 * и Status Stage TRB идёт сразу после Setup, с направлением IN
 * (по спеке USB: если нет стадии данных, Status всегда IN).
 * Если стадия данных есть - направление Status противоположно
 * направлению Data (это тоже требование спеки).
 *
 * Возвращает FALSE только если событие вообще не появилось
 * (таймаут) - код завершения при этом не проверяется, это
 * решает вызывающий код через out_compl_code (1=Success,
 * 13=Short Packet - тоже нормальный исход для IN-запросов,
 * если устройство прислало меньше данных, чем мы запросили).
 */
static BOOLEAN xhci_control_transfer(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 xmmio,
    XHCI_CAP_INFO *cap,
    UINT64 evring_phys,
    UINT64 intr0,
    UINT64 ep0_ring_phys,
    UINTN *io_trb_slot,
    UINT8 slot_id,
    UINT8 bm_request_type,
    UINT8 b_request,
    UINT16 w_value,
    UINT16 w_index,
    UINT16 w_length,
    UINT64 data_buf_phys,
    UINTN *io_ev_slot,
    UINT8 *out_compl_code
)
{
    /* Setup Stage TRB */

    volatile UINT32 *setup_trb =
        (volatile UINT32 *)
            (UINTN)(ep0_ring_phys + (UINT64)(*io_trb_slot) * 16);

    setup_trb[0] =
        (UINT32)bm_request_type |
        ((UINT32)b_request << 8) |
        ((UINT32)w_value << 16);
    setup_trb[1] =
        (UINT32)w_index | ((UINT32)w_length << 16);
    setup_trb[2] = 8u;

    UINT32 trt; /* Transfer Type: 0=нет данных,
                   2=Data Stage OUT, 3=Data Stage IN */

    if (w_length == 0)
        trt = 0u;
    else if (bm_request_type & 0x80u)
        trt = 3u;
    else
        trt = 2u;

    /* IDT - бит 6 (не 5! бит5 - это IOC, разные вещи -
       см. подробный комментарий на месте, где эта ошибка
       раньше пряталась) */
    setup_trb[3] =
        0x1u | (1u << 6) | (2u << 10) | (trt << 16);

    (*io_trb_slot) = (*io_trb_slot) + 1;

    BOOLEAN has_data = (w_length != 0);

    if (has_data) {

        volatile UINT32 *data_trb =
            (volatile UINT32 *)
                (UINTN)(ep0_ring_phys +
                        (UINT64)(*io_trb_slot) * 16);

        data_trb[0] =
            (UINT32)(data_buf_phys & 0xFFFFFFFFu);
        data_trb[1] =
            (UINT32)(data_buf_phys >> 32);
        data_trb[2] = w_length;

        UINT32 dir =
            (bm_request_type & 0x80u) ? 1u : 0u;

        data_trb[3] =
            0x1u | (3u << 10) | (dir << 16);

        (*io_trb_slot) = (*io_trb_slot) + 1;
    }

    volatile UINT32 *status_trb =
        (volatile UINT32 *)
            (UINTN)(ep0_ring_phys + (UINT64)(*io_trb_slot) * 16);

    /* если данных не было - Status всегда IN; если были -
       направление противоположно направлению Data Stage */
    UINT32 status_dir;

    if (!has_data)
        status_dir = 1u;
    else
        status_dir =
            (bm_request_type & 0x80u) ? 0u : 1u;

    status_trb[0] = 0;
    status_trb[1] = 0;
    status_trb[2] = 0;
    status_trb[3] =
        0x1u | (1u << 5) | (4u << 10) | (status_dir << 16);

    (*io_trb_slot) = (*io_trb_slot) + 1;

    /* Doorbell SlotID, Target=1 (Endpoint 0) */
    mmio_write32(
        xmmio + cap->DbOff + (UINT64)slot_id * 4, 1u
    );

    volatile UINT32 *ev = NULL;

    BOOLEAN got =
        xhci_wait_for_event(
            out, evring_phys, intr0,
            io_ev_slot, 32, &ev
        );

    if (!got)
        return FALSE;

    UINT8 cc = (UINT8)((ev[2] >> 24) & 0xFFu);

    if (out_compl_code)
        *out_compl_code = cc;

    (*io_ev_slot) = (*io_ev_slot) + 1;

    UINT64 next_ev_addr =
        xhci_event_ring_trb_addr(evring_phys, *io_ev_slot);

    mmio_write32(
        intr0 + 0x18,
        (UINT32)(next_ev_addr & 0xFFFFFFFFu)
    );
    mmio_write32(
        intr0 + 0x1C,
        (UINT32)(next_ev_addr >> 32)
    );

    return TRUE;
}



/*
 * --------------------------------------------------------------
 * Разбор настоящего HID Report Descriptor (вместо угаданного
 * "boot protocol" byte0=кнопки/byte1=dX/byte2=dY).
 * --------------------------------------------------------------
 *
 * До этого места формат отчёта мыши был просто предположением -
 * самым частым, но НЕ гарантированным спекой HID форматом (он
 * называется "boot protocol" и обязателен только тогда, когда
 * хост явно попросил его через SET_PROTOCOL(Boot) - мы этого
 * никогда не делали, просто повезло, что эмулированная
 * `-device usb-mouse` в QEMU отвечает именно в этом формате).
 * Настоящий, всегда правильный способ - прочитать Report
 * Descriptor (сам HID Class Descriptor с типом 0x22, длину
 * которого мы уже нашли в HID Class Descriptor'е с типом 0x21
 * внутри Configuration Descriptor, см. выше) и разобрать его
 * по-настоящему: он описывает, ГДЕ именно (с какого бита, какой
 * ширины) в отчёте лежат кнопки/dX/dY/колесо у КОНКРЕТНОГО
 * устройства - это может отличаться от мыши к мыши.
 *
 * Report Descriptor - НЕ поток байт с фиксированными полями (как
 * Device/Configuration Descriptor), а маленький байт-код: поток
 * "item'ов" (элементов), каждый начинается с одного префиксного
 * байта, дальше 0/1/2/4 байта данных. Префиксный байт:
 *   биты [1:0] - bSize: 0,1,2 - столько байт данных, значение
 *                3 - ОСОБЫЙ случай, означает 4 байта (а не 3!).
 *   биты [3:2] - bType: 0=Main, 1=Global, 2=Local, 3=зарезервим
 *                (Long item, префикс тогда всегда 0xFE - другой
 *                формат целиком, у простых мышей не встречается,
 *                но на всякий случай тоже пропускаем корректно).
 *   биты [7:4] - bTag - какой конкретно это item, смысл зависит
 *                от bType (см. switch ниже).
 *
 * Три вида item'ов:
 *   Global - меняют "текущие настройки", которые действуют на
 *            все следующие item'ы, пока не переопределены снова
 *            (Usage Page, Report Size, Report Count, Report ID,
 *            Logical Minimum/Maximum).
 *   Local  - тоже "текущие настройки", но живут только ДО
 *            следующего Main item'а, потом автоматически
 *            сбрасываются (Usage, Usage Minimum, Usage Maximum -
 *            описывают "назначение" конкретных бит: X? Y?
 *            кнопка?).
 *   Main   - собственно описывают поле в отчёте (Input - для
 *            данных, которые устройство ПРИСЫЛАЕТ нам, именно
 *            это нам и нужно; есть ещё Output и Feature - не
 *            относятся к опросу мыши, пропускаем их не трогая
 *            счётчик битов Input-отчёта). Когда встречаем Input,
 *            "текущие" Global+Local настройки говорят: следующие
 *            (Report Count) полей по (Report Size) бит каждое -
 *            и если перед этим были отдельные Usage-теги (Usage
 *            X, Usage Y, ...), i-е поле получает i-й Usage по
 *            порядку; если вместо этого были Usage Minimum/
 *            Maximum (диапазон, так обычно описывают кнопки -
 *            "кнопки с 1 по 3"), i-е поле получает Usage =
 *            Minimum+i.
 *
 * Bit-пакинг отчёта (важно): биты полей идут подряд начиная с
 * БИТА 0 БАЙТА 0 отчёта, младшим битом вперёд (LSB first) - то
 * есть если первое поле шириной 3 бита, оно занимает биты 0-2
 * байта 0, а следующее поле сразу продолжается с бита 3 того же
 * байта, а не с начала следующего байта (см. hid_extract_bits
 * ниже - именно поэтому там арифметика "абсолютный номер бита
 * / 8" и "% 8", а не просто индексация по байтам).
 */

typedef struct {

    BOOLEAN valid;      /* TRUE, если удалось найти хотя бы X и Y
                            - без них "мышиный" отчёт не имеет
                            смысла, даже если кнопки нашлись */

    BOOLEAN has_report_id;
    UINT8   report_id;   /* если TRUE - самый первый байт КАЖДОГО
                             реального отчёта это Report ID, а не
                             данные; все смещения ниже уже
                             включают эти +8 бит, см. конец
                             hid_parse_report_descriptor */

    BOOLEAN has_buttons;
    UINT32  button_bit_offset;
    UINT8   button_count; /* сколько битов-кнопок, бит0 этого
                              поля = button_bit_offset (обычно
                              бит0=левая, бит1=правая,
                              бит2=средняя - но, в отличие от
                              старого кода, это теперь не
                              предположение, а порядок Usage
                              Minimum..Maximum из самого
                              дескриптора) */

    UINT32  x_bit_offset;
    UINT8   x_bit_size;
    BOOLEAN x_is_relative; /* TRUE = смещение от предыдущего
                               отчёта (обычная мышь), FALSE =
                               абсолютная координата (планшет/
                               тачпад - для дельта-курсора это
                               уже другой сценарий использования,
                               просто не путаем формат) */

    UINT32  y_bit_offset;
    UINT8   y_bit_size;
    BOOLEAN y_is_relative;

    BOOLEAN has_wheel;
    UINT32  wheel_bit_offset;
    UINT8   wheel_bit_size;
    BOOLEAN wheel_is_relative;

    /* Logical Maximum полей X/Y - нужен только для АБСОЛЮТНЫХ
       устройств (планшет, QEMU usb-tablet): чтобы перевести
       координату 0..max в пиксели экрана */
    INT32   x_logical_max;
    INT32   y_logical_max;

} HID_MOUSE_REPORT_LAYOUT;


/*
 * Достаёт bit_size битов из буфера отчёта, начиная с абсолютного
 * bit_offset (считая от бита 0 байта 0), младшим битом вперёд
 * (см. объяснение bit-пакинга в комментарии выше). Возвращает
 * СЫРОЕ беззнаковое значение до 32 битов - знак (для
 * относительных dX/dY) навешивает отдельно hid_sign_extend ниже,
 * здесь мы его ещё не знаем (это решает Logical Minimum, а не
 * сам факт извлечения битов).
 */
static UINT32 hid_extract_bits(
    volatile UINT8 *report,
    UINTN report_len_bytes,
    UINT32 bit_offset,
    UINT8 bit_size
)
{
    UINT32 value = 0;

    if (bit_size > 32)
        bit_size = 32; /* защита - в реальных мышиных дескрипторах
                           такого не бывает, но на всякий случай
                           не переполняем UINT32 сдвигом >=32 */

    for (UINT8 i = 0; i < bit_size; i++) {

        UINT32 abs_bit = bit_offset + i;
        UINT32 byte_index = abs_bit / 8u;
        UINT8  bit_in_byte = (UINT8)(abs_bit % 8u);

        if (byte_index >= report_len_bytes) {

            /* реальный отчёт оказался короче, чем обещал
               дескриптор (короткий пакет) - дальше считаем
               недостающие биты нулями, а не читаем за пределы
               буфера */
            break;
        }

        UINT8 byte_val = report[byte_index];
        UINT8 bit_val = (UINT8)((byte_val >> bit_in_byte) & 0x1u);

        if (bit_val)
            value = value | (1u << i);
    }

    return value;
}


/*
 * Знаковое расширение значения шириной bit_size битов до полных
 * 32 битов (дополнительный код) - нужно для dX/dY, которые
 * почти всегда описаны как Logical Minimum < 0 (типично -127..
 * 127 при Report Size=8), то есть являются знаковыми, а
 * hid_extract_bits выше отдаёт их просто как биты без знака.
 */
static INT32 hid_sign_extend(UINT32 raw_value, UINT8 bit_size)
{
    if (bit_size == 0 || bit_size >= 32)
        return (INT32)raw_value;

    UINT32 sign_bit_mask = 1u << (bit_size - 1u);

    if (raw_value & sign_bit_mask) {

        UINT32 extend_mask = ~((1u << bit_size) - 1u);

        return (INT32)(raw_value | extend_mask);
    }

    return (INT32)raw_value;
}


/*
 * Интерпретирует raw как знаковое число шириной ровно
 * item_size_bytes байт (0,1,2 или 4 - см. bSize в комментарии
 * выше) - нужно для Global item'ов Logical Minimum/Maximum,
 * которые по спеке HID хранятся в ровно стольких байтах, сколько
 * реально нужно, и это ЗНАКОВЫЕ величины (Logical Minimum мыши
 * почти всегда отрицательный).
 */
static INT32 hid_item_signed_value(
    UINT32 raw,
    UINT8 item_size_bytes
)
{
    if (item_size_bytes == 1) {

        return (INT32)(INT8)(raw & 0xFFu);

    } else if (item_size_bytes == 2) {

        /* efi.h не объявляет INT16 (только INT8/UINT8/UINT16),
           поэтому знаковое расширение 2-байтового значения
           делаем вручную через ту же битовую маску, что и
           hid_sign_extend ниже, а не приведением типа к
           несуществующему INT16 */
        UINT32 v = raw & 0xFFFFu;

        if (v & 0x8000u)
            return (INT32)(v | 0xFFFF0000u);

        return (INT32)v;
    }

    /* item_size_bytes == 4 (или 0, тогда raw и так 0) */
    return (INT32)raw;
}


/*
 * Сам разбор Report Descriptor - проходит по всем item'ам подряд
 * (см. общее объяснение формата в большом комментарии перед этим
 * блоком) и ищет Input-поля с Usage Page/Usage, похожими на
 * обычную мышь: Generic Desktop (0x01) / X (0x30), Y (0x31),
 * Wheel (0x38), и Button Page (0x09) для кнопок. Найденные битовые
 * смещения/ширины складывает в *layout. Печатает в out
 * человеко-читаемый итог - чтобы пользователь мог убедиться
 * скриншотом, что разбор прошёл осмысленно (а не просто "молча
 * не нашёл ничего").
 *
 * ОГРАНИЧЕНИЯ (осознанные, явно, чтобы не забыть):
 *  - Не поддержаны Push/Pop (Global item'ы 0xA4/0xB4) - редкость
 *    у простых мышей, сохранение/восстановление стека Global-
 *    состояния не реализовано; если встретится - разбор просто
 *    пойдёт по item'ам дальше, результат для необычных
 *    дескрипторов может быть неверным.
 *  - Отдельные счётчики бит-смещения для Output/Feature НЕ
 *    ведутся вообще (нам не нужны их данные) - но и не мешают:
 *    их Report Size/Report Count просто не трогают тот
 *    единственный счётчик, что мы ведём (input_bit_offset), это
 *    и есть корректное поведение (у каждого типа отчёта, Input/
 *    Output/Feature, отдельная своя нумерация бит по спеке).
 *  - Составные устройства с НЕСКОЛЬКИМИ разными Report ID для
 *    разных наборов данных (например, отдельно мышь и отдельно
 *    медиа-клавиши в одном интерфейсе) - report_id запоминается
 *    просто "последний встреченный", а не привязывается к
 *    конкретному найденному полю. Для одиночного HID-интерфейса
 *    мыши (typичный случай, в т.ч. у большинства USB-донглов) это
 *    не проблема - там обычно вообще нет Report ID или он один.
 */
static void hid_parse_report_descriptor(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    volatile UINT8 *desc,
    UINT16 desc_len,
    HID_MOUSE_REPORT_LAYOUT *layout
)
{
    /* обнуляем результат вручную - libc/memset нет */
    layout->valid = FALSE;
    layout->has_report_id = FALSE;
    layout->report_id = 0;
    layout->has_buttons = FALSE;
    layout->button_bit_offset = 0;
    layout->button_count = 0;
    layout->x_bit_offset = 0;
    layout->x_bit_size = 0;
    layout->x_is_relative = FALSE;
    layout->y_bit_offset = 0;
    layout->y_bit_size = 0;
    layout->y_is_relative = FALSE;
    layout->has_wheel = FALSE;
    layout->wheel_bit_offset = 0;
    layout->wheel_bit_size = 0;
    layout->wheel_is_relative = FALSE;
    layout->x_logical_max = 0;
    layout->y_logical_max = 0;

    /* "текущие" Global-настройки */
    UINT16 usage_page = 0;
    UINT32 report_size = 0;
    UINT32 report_count = 0;
    INT32  logical_min = 0;
    INT32  logical_max = 0;

    /*
     * Несколько Report ID в одном дескрипторе (частый случай у
     * беспроводных донглов: в одном интерфейсе и мышь, и
     * мультимедиа-клавиши). По спеке HID у КАЖДОГО Report ID
     * своя нумерация бит, начиная с нуля (сразу после байта ID).
     * Раньше счётчик бит был один на весь дескриптор - поля
     * второго отчёта получали неверные смещения. Теперь для
     * каждого встреченного ID хранится свой счётчик, а у
     * найденных полей запоминается, в каком отчёте они живут -
     * в итоге layout->report_id = ID того отчёта, где X.
     */
    UINT8  rid_ids[8];
    UINT32 rid_offs[8];
    UINT8  rid_count = 0;
    UINT8  cur_report_id = 0;
    UINT8  buttons_report_id = 0;
    UINT8  x_report_id = 0;
    UINT8  wheel_report_id = 0;

    /* "текущие" Local-настройки - сбрасываются после КАЖДОГО
       Main item'а (см. конец цикла) */
    UINT16  usage_stack[16];
    UINT8   usage_stack_count = 0;
    UINT16  usage_min = 0;
    UINT16  usage_max = 0;
    BOOLEAN seen_usage_min = FALSE;
    BOOLEAN seen_usage_max = FALSE;

    /* Счётчик бит только для Input-отчётов (см. ограничение про
       Output/Feature в комментарии выше) */
    UINT32 input_bit_offset = 0;

    UINT16 pos = 0;

    while (pos < desc_len) {

        UINT8 prefix = desc[pos];

        if (prefix == 0xFEu) {

            /* Long item - у простых HID-мышей практически не
               встречается, но формат другой (следующий байт -
               размер данных, затем байт tag'а, затем сами
               данные) - корректно пропускаем целиком, не пытаясь
               разобрать как обычный item. */

            if ((UINT32)pos + 2u > (UINT32)desc_len)
                break;

            UINT8 long_data_size = desc[pos + 1];

            pos = (UINT16)(pos + 3 + long_data_size);
            continue;
        }

        UINT8 b_size_code = (UINT8)(prefix & 0x3u);
        UINT8 b_type = (UINT8)((prefix >> 2) & 0x3u);
        UINT8 b_tag = (UINT8)((prefix >> 4) & 0xFu);

        /* bSize=3 - особый случай, означает 4 байта данных, а
           не 3 (см. большой комментарий выше) */
        UINT8 item_size =
            (b_size_code == 3u) ? 4u : b_size_code;

        pos = (UINT16)(pos + 1);

        if ((UINT32)pos + (UINT32)item_size > (UINT32)desc_len)
            break; /* битый/обрезанный дескриптор (короткий
                       пакет) - останавливаемся, не читаем за
                       пределы буфера */

        UINT32 raw = 0;

        for (UINT8 i = 0; i < item_size; i++) {
            raw = raw | ((UINT32)desc[pos + i] << (8u * i));
        }

        pos = (UINT16)(pos + item_size);

        if (b_type == 1u) {

            /* --- Global item --- */

            if (b_tag == 0u) {

                /* Usage Page */
                usage_page = (UINT16)(raw & 0xFFFFu);

            } else if (b_tag == 1u) {

                /* Logical Minimum - знаковое, ширина=item_size */
                logical_min =
                    hid_item_signed_value(raw, item_size);

            } else if (b_tag == 2u) {

                /* Logical Maximum - знаковое, как и Minimum */
                logical_max =
                    hid_item_signed_value(raw, item_size);

            } else if (b_tag == 7u) {

                /* Report Size - ширина ОДНОГО элемента поля,
                   в битах */
                report_size = raw;

            } else if (b_tag == 8u) {

                /* Report ID - раз он вообще встретился в
                   дескрипторе, значит каждый реальный отчёт
                   с устройства начинается с байта Report ID
                   (см. has_report_id ниже, в конце функции) */
                layout->has_report_id = TRUE;

                UINT8 new_id = (UINT8)(raw & 0xFFu);

                /* сохранить счётчик бит текущего отчёта и
                   переключиться на счётчик нового (0, если этот
                   ID встретился впервые) */
                BOOLEAN saved = FALSE;

                for (UINT8 k = 0; k < rid_count; k++) {
                    if (rid_ids[k] == cur_report_id) {
                        rid_offs[k] = input_bit_offset;
                        saved = TRUE;
                    }
                }

                if (!saved && rid_count < 8u) {
                    rid_ids[rid_count] = cur_report_id;
                    rid_offs[rid_count] = input_bit_offset;
                    rid_count++;
                }

                input_bit_offset = 0;

                for (UINT8 k = 0; k < rid_count; k++) {
                    if (rid_ids[k] == new_id)
                        input_bit_offset = rid_offs[k];
                }

                cur_report_id = new_id;

            } else if (b_tag == 9u) {

                /* Report Count - сколько ОДИНАКОВЫХ по ширине
                   элементов подряд в следующем Main item'е */
                report_count = raw;
            }

            /* остальные Global item'ы (Logical Maximum,
               Physical Minimum/Maximum, Unit Exponent, Unit,
               Push, Pop) нам не нужны - намеренно игнорируем */

        } else if (b_type == 2u) {

            /* --- Local item --- */

            if (b_tag == 0u) {

                /* Usage - "назначение" одного конкретного поля
                   (например, X, Y или Wheel по отдельности) */
                if (usage_stack_count < 16u) {
                    usage_stack[usage_stack_count] =
                        (UINT16)(raw & 0xFFFFu);
                    usage_stack_count =
                        (UINT8)(usage_stack_count + 1u);
                }

            } else if (b_tag == 1u) {

                /* Usage Minimum - начало ДИАПАЗОНА назначений
                   (так почти всегда описывают кнопки: "кнопки
                   с Minimum по Maximum") */
                usage_min = (UINT16)(raw & 0xFFFFu);
                seen_usage_min = TRUE;

            } else if (b_tag == 2u) {

                /* Usage Maximum - конец того же диапазона */
                usage_max = (UINT16)(raw & 0xFFFFu);
                seen_usage_max = TRUE;
            }

        } else if (b_type == 0u) {

            /* --- Main item --- */

            if (b_tag == 8u) {

                /* Input - собственно поле данных, которое
                   устройство нам ПРИСЫЛАЕТ (то, что мы опрашиваем
                   через Interrupt IN endpoint) */

                UINT32 flags = raw;
                BOOLEAN is_const = (flags & 0x1u) != 0;
                BOOLEAN is_relative = (flags & 0x4u) != 0;

                if (
                    !is_const &&
                    usage_page == 0x09u &&
                    !layout->has_buttons
                ) {

                    /* Button Page - по спеке USB HID кнопки
                       мыши описываются ОДНИМ полем-диапазоном
                       (Usage Minimum=1..Usage Maximum=N кнопок,
                       Report Size=1 бит на кнопку, Report
                       Count=N) - значит всё это Input item'а
                       целиком и есть блок кнопок, bit0 этого
                       блока = кнопка с номером Usage Minimum
                       (почти всегда 1 = левая). Берём только
                       первое такое поле, если их несколько -
                       см. ограничение про несколько Report ID
                       в комментарии перед функцией. */

                    layout->has_buttons = TRUE;
                    layout->button_bit_offset = input_bit_offset;
                    layout->button_count = (UINT8)report_count;
                    buttons_report_id = cur_report_id;
                }

                /* X/Y/Wheel описываются ОТДЕЛЬНЫМИ Usage-тегами
                   (не диапазоном) - i-й Usage из usage_stack
                   соответствует i-му полю по порядку внутри
                   этого Input item'а (см. общий комментарий про
                   Main item выше). */

                /* Некоторые (особенно дешёвые) устройства описывают
                   X/Y не отдельными Usage, а диапазоном "Usage
                   Minimum=X(0x30) .. Usage Maximum=Y(0x31)" - тогда
                   i-е поле получает Usage = Minimum + i. */
                BOOLEAN xy_by_range =
                    (usage_stack_count == 0u) &&
                    seen_usage_min && seen_usage_max &&
                    usage_max >= usage_min;

                UINT32 xy_fields =
                    xy_by_range ?
                        (UINT32)(usage_max - usage_min) + 1u :
                        (UINT32)usage_stack_count;

                if (
                    !is_const &&
                    usage_page == 0x01u &&
                    xy_fields > 0u
                ) {

                    for (
                        UINT8 i = 0;
                        i < report_count && i < xy_fields;
                        i++
                    ) {

                        UINT16 field_usage =
                            xy_by_range ?
                                (UINT16)(usage_min + i) :
                                usage_stack[i];

                        UINT32 field_bit_offset =
                            input_bit_offset +
                            (UINT32)i * report_size;

                        if (
                            field_usage == 0x30u &&
                            layout->x_bit_size == 0
                        ) {

                            /* Generic Desktop / X */
                            layout->x_bit_offset =
                                field_bit_offset;
                            layout->x_bit_size =
                                (UINT8)report_size;
                            layout->x_is_relative =
                                is_relative;
                            layout->x_logical_max =
                                logical_max;
                            x_report_id = cur_report_id;

                        } else if (
                            field_usage == 0x31u &&
                            layout->y_bit_size == 0
                        ) {

                            /* Generic Desktop / Y */
                            layout->y_bit_offset =
                                field_bit_offset;
                            layout->y_bit_size =
                                (UINT8)report_size;
                            layout->y_is_relative =
                                is_relative;
                            layout->y_logical_max =
                                logical_max;

                        } else if (
                            field_usage == 0x38u &&
                            !layout->has_wheel
                        ) {

                            /* Generic Desktop / Wheel */
                            layout->has_wheel = TRUE;
                            layout->wheel_bit_offset =
                                field_bit_offset;
                            layout->wheel_bit_size =
                                (UINT8)report_size;
                            layout->wheel_is_relative =
                                is_relative;
                            wheel_report_id = cur_report_id;
                        }
                    }
                }

                /* только Input-поля двигают наш счётчик - см.
                   ограничение про Output/Feature в комментарии
                   перед функцией */
                input_bit_offset =
                    input_bit_offset +
                    report_count * report_size;
            }

            /* Local-состояние (Usage/Usage Minimum/Usage
               Maximum) по спеке HID живёт ровно до следующего
               Main item'а (Input/Output/Feature/Collection/End
               Collection - вообще любого) - сбрасываем после
               ЛЮБОГО Main item'а, не только Input. */
            usage_stack_count = 0;
            usage_min = 0;
            usage_max = 0;
            seen_usage_min = FALSE;
            seen_usage_max = FALSE;

            /* usage_min/usage_max выше пока нигде не
               используются для полей мыши (кнопки определяются
               по Usage Page целиком, см. Input выше) - они
               оставлены разобранными на будущее (например, если
               понадобится точно знать номер первой кнопки,
               logical_min тоже уже разобран и лежит рядом) */
            (void)usage_min;
            (void)usage_max;
            (void)seen_usage_min;
            (void)seen_usage_max;
            (void)logical_min;
        }
    }

    /* Если в дескрипторе был Report ID - реальный байт 0
       каждого отчёта занят им, а не данными; все найденные
       смещения выше считались от начала "полезной" части
       отчёта (как будто Report ID не было), нужно сдвинуть их
       на 8 бит вперёд ровно один раз, здесь. */

    if (layout->has_report_id) {

        /* отчёт мыши - тот, где нашёлся X; кнопки/колесо из
           ДРУГОГО отчёта (например, "кнопки" мультимедиа-блока)
           к мыши отношения не имеют - лучше без них, чем
           с чужими битами */
        layout->report_id = x_report_id;

        if (layout->has_buttons && buttons_report_id != x_report_id)
            layout->has_buttons = FALSE;

        if (layout->has_wheel && wheel_report_id != x_report_id)
            layout->has_wheel = FALSE;
    }

    UINT32 id_shift = layout->has_report_id ? 8u : 0u;

    layout->button_bit_offset =
        layout->button_bit_offset + id_shift;
    layout->x_bit_offset = layout->x_bit_offset + id_shift;
    layout->y_bit_offset = layout->y_bit_offset + id_shift;
    layout->wheel_bit_offset =
        layout->wheel_bit_offset + id_shift;

    layout->valid =
        (layout->x_bit_size != 0) &&
        (layout->y_bit_size != 0);

    /* --- печатаем человеко-читаемый итог разбора --- */

    print(out, "\nReport Descriptor parsed (");
    print_uint(out, desc_len);
    print(out, " bytes):\n");

    if (layout->has_report_id) {
        print(out, "  Report ID = ");
        print_uint(out, layout->report_id);
        print(out, " (present in every report as byte 0)\n");
    } else {
        print(out, "  no Report ID (reports start with data "
                    "directly)\n");
    }

    if (layout->has_buttons) {
        print(out, "  Buttons: ");
        print_uint(out, layout->button_count);
        print(out, " bit(s) at bit offset ");
        print_uint(out, layout->button_bit_offset);
        print(out, "\n");
    } else {
        print(out, "  Buttons: not found\n");
    }

    if (layout->x_bit_size != 0) {
        print(out, "  X: ");
        print_uint(out, layout->x_bit_size);
        print(out, "-bit, offset ");
        print_uint(out, layout->x_bit_offset);
        print(out, ", ");
        print(out, layout->x_is_relative ? "relative" :
                                            "ABSOLUTE");
        print(out, "\n");
    } else {
        print(out, "  X: not found\n");
    }

    if (layout->y_bit_size != 0) {
        print(out, "  Y: ");
        print_uint(out, layout->y_bit_size);
        print(out, "-bit, offset ");
        print_uint(out, layout->y_bit_offset);
        print(out, ", ");
        print(out, layout->y_is_relative ? "relative" :
                                            "ABSOLUTE");
        print(out, "\n");
    } else {
        print(out, "  Y: not found\n");
    }

    if (layout->has_wheel) {
        print(out, "  Wheel: ");
        print_uint(out, layout->wheel_bit_size);
        print(out, "-bit, offset ");
        print_uint(out, layout->wheel_bit_offset);
        print(out, "\n");
    } else {
        print(out, "  Wheel: not present\n");
    }

    if (!layout->valid) {

        print(
            out,
            "  WARNING: no X and/or Y field found - this "
            "does not look like a pointing device, falling "
            "back to the old guessed byte0/1/2 layout below.\n"
        );
    }
}



/*
 * Шаг 6 — Address Device + первый настоящий USB-запрос
 * (GET_DESCRIPTOR, Device Descriptor) через Default Control Pipe
 * (Endpoint 0).
 *
 * На входе уже готово (сделано раньше, в команде "xhci"):
 *   - контроллер сброшен, DCBAA и Command Ring настроены и
 *     работают (см. Enable Slot Command),
 *   - Event Ring настроен и мы уже один раз успешно прочитали
 *     из него Command Completion Event (для Enable Slot),
 *   - у устройства есть SlotID (получен Enable Slot Command),
 *   - порт прошёл Port Reset (PED=1).
 *
 * Что делает эта функция:
 *   1. Читает Port Speed из PORTSC уже сброшенного порта - нужно
 *      для двух вещей: поля Speed в Slot Context и стартового
 *      Max Packet Size для Endpoint 0 (у control endpoint это
 *      значение заранее неизвестно точно, пока не прочитан сам
 *      Device Descriptor - поэтому по спеке USB для старта
 *      берётся стандартное значение по скорости порта: 8 байт
 *      для Low/Full Speed, 64 для High Speed, 512 для SuperSpeed).
 *   2. Выделяет и заполняет Input Context (Input Control Context +
 *      Slot Context + Endpoint 0 Context) - "заявка" контроллеру
 *      на то, каким должен стать Device Context.
 *   3. Выделяет пустой Device Context (уйдёт в DCBAA[SlotID] -
 *      это то, что контроллер реально будет использовать и
 *      обновлять сам, в отличие от Input Context, который нужен
 *      только на момент самой команды).
 *   4. Выделяет Transfer Ring для Endpoint 0 - отдельное кольцо
 *      TRB (того же формата, что Command Ring, но для передачи
 *      данных конкретной конечной точке конкретного устройства,
 *      не команд самому контроллеру).
 *   5. Кладёт в Command Ring TRB Address Device (Type 11), звонит
 *      в Doorbell 0, ждёт Command Completion Event - после этого
 *      устройству реально назначен USB-адрес и Endpoint 0 готов
 *      к работе.
 *   6. Строит Control Transfer из трёх TRB на Transfer Ring
 *      Endpoint 0 (Setup Stage + Data Stage IN + Status Stage
 *      OUT) - классический 3-стадийный USB control-запрос,
 *      здесь конкретно GET_DESCRIPTOR(Device), запрашиваем все
 *      18 байт Device Descriptor. Звонит в Doorbell SlotID (не
 *      0! у Command Ring и у Transfer Ring конкретного
 *      устройства разные doorbell'ы), ждёт Transfer Event.
 *   7. Если всё получилось - печатает содержимое Device
 *      Descriptor человеко-читаемо, в первую очередь
 *      Vendor ID/Product ID - то, ради чего всё затевалось: это
 *      настоящие данные с настоящего USB-устройства, добытые
 *      без единого обращения к прошивке.
 *
 * Всё выделение памяти - через AllocatePages (не AllocatePool),
 * по той же причине, что и раньше: xHCI требует выравнивания
 * (Input/Device Context - 64 байта, Transfer Ring - 16 байт),
 * страница (4096, выровнена по странице) даёт выравнивание с
 * большим запасом, а размер тут всё равно намного меньше
 * страницы, так что переплата памятью не важна на этом этапе.
 *
 * ОГРАНИЧЕНИЕ этого шага (явно, чтобы не забыть): Context Size
 * (CSZ, бит 2 HCCPARAMS1) здесь читается и учитывается - если
 * контроллер требует 64-байтные контексты вместо 32-байтных,
 * это меняет только размер шага между Slot Context/EP Context
 * внутри Input Context и Device Context, сам код это учитывает
 * через переменную ctx_size. Дальше по коду отдельно НЕ
 * поддержаны: Multi-TT хабы, Streams, и любые EP кроме
 * Endpoint 0 - это всё будущие шаги (Endpoint 0 достаточно для
 * GET_DESCRIPTOR, но не для реального опроса координат мыши -
 * для этого понадобится ещё и Interrupt IN Endpoint, см.
 * SET_CONFIGURATION в планах дальше).
 */
static void xhci_address_device_and_get_descriptor(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 xmmio,
    XHCI_CAP_INFO *cap,
    UINT64 dcbaa_phys,
    UINT64 cmdring_phys,
    UINT64 evring_phys,
    UINT64 intr0,
    UINT64 port_base,
    UINTN  root_port,
    UINT8  slot_id,
    UINTN  start_ev_slot,
    /*
     * Раньше эти четыре страницы выделялись прямо здесь через
     * AllocatePages. Теперь эта функция вызывается уже ПОСЛЕ
     * ExitBootServices (см. команду "ebs"), где AllocatePages
     * недоступен - поэтому все страницы выделяются заранее, ещё
     * при живых Boot Services, и передаются сюда готовыми
     * физическими адресами. Функция только обнуляет и заполняет
     * их содержимое, память под них уже есть.
     */
    UINT64 input_ctx_phys,
    UINT64 dev_ctx_phys,
    UINT64 ep0_ring_phys,
    UINT64 desc_buf_phys,
    UINT64 int_ring_phys
)
{
    /* --- 1. скорость порта --- */

    UINT32 portsc_now = mmio_read32(port_base);

    /* Port Speed - биты [13:10] PORTSC. Значения по спеке xHCI
       (таблица Protocol Speed ID по умолчанию, USB2/3 root hub):
       1=Full Speed, 2=Low Speed, 3=High Speed, 4=SuperSpeed. */
    UINT32 port_speed = (portsc_now >> 10) & 0xFu;

    UINT16 ep0_max_packet;

    if (port_speed == 3) {
        ep0_max_packet = 64;   /* High Speed */
    } else if (port_speed == 4) {
        ep0_max_packet = 512;  /* SuperSpeed */
    } else {
        ep0_max_packet = 8;    /* Low/Full Speed - стандартный
                                   стартовый минимум по спеке USB,
                                   настоящее значение придёт в
                                   самом Device Descriptor
                                   (bMaxPacketSize0), для первого
                                   запроса используем его позже,
                                   если захотим перечитать точнее */
    }

    print(out, "Port speed code = ");
    print_uint(out, port_speed);
    print(out, " (1=FS 2=LS 3=HS 4=SS), EP0 MaxPacket = ");
    print_uint(out, ep0_max_packet);
    print(out, "\n");

    /* --- размер контекста (CSZ, HCCPARAMS1 бит 2) --- */

    UINT32 hccparams1 = mmio_read32(xmmio + 0x10);
    BOOLEAN csz64 = (hccparams1 & 0x4u) != 0;
    UINT32 ctx_size = csz64 ? 64u : 32u;

    print(out, "Context size = ");
    print_uint(out, ctx_size);
    print(out, " bytes per context (CSZ=");
    print_uint(out, csz64 ? 1 : 0);
    print(out, ")\n");

    /* --- 2/3/4. Input Context, Device Context и Transfer Ring
       под Endpoint 0 - страницы уже выделены вызывающим кодом
       (до ExitBootServices), тут только обнуляем их --- */

    raw_zero_mem((volatile UINT8 *)(UINTN)input_ctx_phys, 4096);
    raw_zero_mem((volatile UINT8 *)(UINTN)dev_ctx_phys, 4096);
    raw_zero_mem((volatile UINT8 *)(UINTN)ep0_ring_phys, 4096);
    raw_zero_mem((volatile UINT8 *)(UINTN)desc_buf_phys, 4096);

    /* --- заполняем Input Control Context (первый контекст в
       Input Context): Add Context Flags A0 (Slot) и A1 (EP0) --- */

    volatile UINT32 *input_ctrl =
        (volatile UINT32 *)(UINTN)input_ctx_phys;

    input_ctrl[0] = 0;      /* Drop Context flags - ничего не
                                убираем, устройство новое */
    input_ctrl[1] = 0x3u;   /* Add Context flags: бит0=Slot
                                Context, бит1=EP0 Context */

    /* --- Slot Context, второй контекст, смещение ctx_size --- */

    volatile UINT32 *slot_ctx =
        (volatile UINT32 *)
            (UINTN)(input_ctx_phys + ctx_size);

    /* DW0: Route String=0 (устройство напрямую в root hub,
       без промежуточных хабов), Speed (биты 20:23),
       Context Entries (биты 27:31) = 1 - валиден пока только
       EP0 */
    slot_ctx[0] =
        (port_speed << 20) | (1u << 27);

    /* DW1: Root Hub Port Number - биты [23:16] - номер
       физического порта root hub, куда воткнуто устройство
       (тот же номер, что печатала команда 'xhci' при дампе
       PORTSC и что использовался для Port Reset) */
    slot_ctx[1] = (UINT32)root_port << 16;

    /* DW2/DW3 пока 0 - Interrupter Target=0 (используем
       Interrupter 0, тот же, что уже настроен), USB Device
       Address=0 (ещё не назначен - назначит сам контроллер по
       Address Device Command), Slot State=0 (Disabled/Enabled,
       контроллер сам выставит после команды) */
    slot_ctx[2] = 0;
    slot_ctx[3] = 0;

    /* --- Endpoint 0 Context, третий контекст, смещение
       2*ctx_size --- */

    volatile UINT32 *ep0_ctx =
        (volatile UINT32 *)
            (UINTN)(input_ctx_phys + 2u * ctx_size);

    ep0_ctx[0] = 0; /* Mult/MaxPStreams/LSA/Interval - все 0
                        для control endpoint */

    /* DW1: CErr (Error Count, биты 1:2) = 3 - максимум,
       контроллер сам остановит endpoint после 3 подряд
       неудачных попыток; EP Type (биты 3:5) = 4 = Control;
       Max Packet Size (биты 16:31) */
    ep0_ctx[1] =
        (3u << 1) | (4u << 3) |
        ((UINT32)ep0_max_packet << 16);

    /* DW2/DW3: TR Dequeue Pointer (64-бит, 16-байтное
       выравнивание гарантировано страницей) + Dequeue Cycle
       State=1 (бит0 DW2) - начальное состояние кольца EP0,
       кольцо только что создано, Cycle Bit контроллера должен
       совпадать с тем, что мы пишем в наши TRB (тоже 1, см.
       ниже) */
    ep0_ctx[2] =
        (UINT32)(ep0_ring_phys & 0xFFFFFFFFu) | 0x1u;
    ep0_ctx[3] = (UINT32)(ep0_ring_phys >> 32);

    /* DW4: Average TRB Length (биты 0:15) - не критично для
       простого случая, пишем разумное ненулевое значение (8,
       размер Setup-пакета) - по спеке 0 недопустим */
    ep0_ctx[4] = 8;

    print(
        out,
        "Input Context / Device Context / EP0 Transfer "
        "Ring allocated and filled.\n"
    );

    /* --- DCBAA[SlotID] = Device Context (не Input Context!
       в DCBAA идёт "чистый" Device Context, который контроллер
       будет сам обновлять; Input Context используется только
       один раз, как параметр самой команды Address Device) --- */

    volatile UINT64 *dcbaa =
        (volatile UINT64 *)(UINTN)dcbaa_phys;

    dcbaa[slot_id] = dev_ctx_phys;

    /* --- 5. Address Device Command (TRB Type 11) во второй
       слот Command Ring - первый (offset 0) уже занят
       Enable Slot Command, отправленной раньше --- */

    volatile UINT32 *cmd_trb2 =
        (volatile UINT32 *)
            (UINTN)(cmdring_phys + 16);

    cmd_trb2[0] =
        (UINT32)(input_ctx_phys & 0xFFFFFFFFu);
    cmd_trb2[1] =
        (UINT32)(input_ctx_phys >> 32);
    cmd_trb2[2] = 0;
    /* DW3: SlotID (биты 24:31), TRB Type=11 (биты 10:15),
       BSR=0 (бит 9, значит контроллер сам пошлёт SET_ADDRESS
       устройству - не просим только "заблокировать" адрес),
       Cycle=1 (бит0) - тот же Producer Cycle State, что и у
       первой команды, кольцо ещё не переворачивалось */
    cmd_trb2[3] =
        ((UINT32)slot_id << 24) | (11u << 10) | 0x1u;

    /* Doorbell 0, Target=0 - опять Command Ring (это тот же
       "звонок", что и для Enable Slot - Command Ring общий на
       весь контроллер, не per-device) */
    mmio_write32(xmmio + cap->DbOff, 0);

    UINTN ev_slot = start_ev_slot;

    volatile UINT32 *ev_trb2 = NULL;

    BOOLEAN got_event2 =
        xhci_wait_for_event(
            out, evring_phys, intr0,
            &ev_slot, 33, &ev_trb2
        );

    if (!got_event2) {

        print(
            out,
            "No Command Completion Event for Address "
            "Device - stopping here.\n"
        );
        return;
    }

    UINT8 trb_type2 =
        (UINT8)((ev_trb2[3] >> 10) & 0x3Fu);
    UINT8 compl_code2 =
        (UINT8)((ev_trb2[2] >> 24) & 0xFFu);

    print(out, "Address Device event: TRB Type=");
    print_uint(out, trb_type2);
    print(out, " CompletionCode=");
    print_uint(out, compl_code2);
    print(out, " (1=Success)\n");

    /* подтверждаем событие - сдвигаем ERDP на следующий TRB
       и продвигаем ev_slot, чтобы GET_DESCRIPTOR ниже начал
       опрос с правильного места */
    ev_slot = ev_slot + 1;

    UINT64 addr_dev_next_ev =
        xhci_event_ring_trb_addr(evring_phys, ev_slot);

    mmio_write32(
        intr0 + 0x18,
        (UINT32)(addr_dev_next_ev & 0xFFFFFFFFu)
    );
    mmio_write32(
        intr0 + 0x1C,
        (UINT32)(addr_dev_next_ev >> 32)
    );

    if (!(trb_type2 == 33 && compl_code2 == 1)) {

        print(
            out,
            "Address Device did not succeed - not "
            "attempting GET_DESCRIPTOR.\n"
        );
        return;
    }

    print(
        out,
        "\nAddress Device succeeded - device now has a "
        "real USB address. Sending GET_DESCRIPTOR "
        "(Device) over the Default Control Pipe...\n"
    );

    /* --- 6. GET_DESCRIPTOR(Device) - первый control transfer,
       через универсальную xhci_control_transfer() --- */

    UINTN trb_slot = 0; /* текущий свободный TRB на EP0 Transfer
                            Ring - у этой функции кольцо только
                            что создано, начинаем с нуля */

    UINT8 compl_code3 = 0;

    BOOLEAN got_event3 =
        xhci_control_transfer(
            out, xmmio, cap,
            evring_phys, intr0,
            ep0_ring_phys, &trb_slot,
            slot_id,
            0x80u,   /* bmRequestType: Device-to-Host/
                        Standard/Device */
            0x06u,   /* bRequest: GET_DESCRIPTOR */
            0x0100u, /* wValue: тип=1 (Device), индекс=0 */
            0,       /* wIndex */
            18,      /* wLength - весь Device Descriptor */
            desc_buf_phys,
            &ev_slot,
            &compl_code3
        );

    if (!got_event3) {

        print(
            out,
            "No Transfer Event for GET_DESCRIPTOR - "
            "stopping here.\n"
        );
        return;
    }

    print(
        out,
        "GET_DESCRIPTOR event: CompletionCode="
    );
    print_uint(out, compl_code3);
    print(out, " (1=Success, 13=ShortPacket)\n");

    if (!(compl_code3 == 1 || compl_code3 == 13)) {

        print(
            out,
            "GET_DESCRIPTOR did not complete "
            "successfully.\n"
        );
        return;
    }

    /* --- 7. печатаем Device Descriptor --- */

    volatile UINT8 *d =
        (volatile UINT8 *)(UINTN)desc_buf_phys;

    UINT16 id_vendor =
        (UINT16)d[8] | ((UINT16)d[9] << 8);
    UINT16 id_product =
        (UINT16)d[10] | ((UINT16)d[11] << 8);

    print(
        out,
        "\n=== Device Descriptor (real data from the "
        "USB device, no firmware involved) ===\n"
    );

    print(out, "bLength            = ");
    print_uint(out, d[0]);
    print(out, "\nbDescriptorType    = ");
    print_uint(out, d[1]);
    print(out, "  (1=Device)\nbcdUSB             = 0x");
    print_hex(
        out,
        (UINT32)d[2] | ((UINT32)d[3] << 8),
        4
    );
    print(out, "\nbDeviceClass       = ");
    print_uint(out, d[4]);
    print(out, "\nbDeviceSubClass    = ");
    print_uint(out, d[5]);
    print(out, "\nbDeviceProtocol    = ");
    print_uint(out, d[6]);
    print(out, "\nbMaxPacketSize0    = ");
    print_uint(out, d[7]);
    print(out, "\nidVendor           = 0x");
    print_hex(out, id_vendor, 4);
    print(out, "\nidProduct          = 0x");
    print_hex(out, id_product, 4);
    print(out, "\nbcdDevice          = 0x");
    print_hex(
        out,
        (UINT32)d[12] | ((UINT32)d[13] << 8),
        4
    );
    print(out, "\nbNumConfigurations = ");
    print_uint(out, d[17]);
    print(
        out,
        "\n\nGot it - real Vendor ID / Product ID read "
        "straight from the USB device over our own "
        "xHCI driver.\n"
    );

    /*
     * --- Шаг 7 — Configuration Descriptor + поиск HID
     * Interrupt IN endpoint + SET_CONFIGURATION ---
     *
     * Configuration Descriptor устроен "матрёшкой": сам
     * GET_DESCRIPTOR(Configuration) возвращает не один
     * дескриптор, а сразу целую пачку подряд - сначала сам
     * Configuration Descriptor (9 байт, в т.ч. wTotalLength -
     * сколько байт всего вернулось и bNumInterfaces), потом
     * для каждого интерфейса - Interface Descriptor (9 байт,
     * в т.ч. bInterfaceClass - у мыши это 3 = HID) и следом
     * его Endpoint Descriptor'ы (7 байт каждый). Нас интересует
     * конкретно Interrupt IN endpoint внутри HID-интерфейса -
     * именно на него мышь будет сама, без опроса, присылать
     * пакеты с dx/dy/кнопками, когда мы его настроим (это уже
     * следующий шаг, Configure Endpoint Command - тут мы его
     * только находим и запоминаем адрес/MaxPacketSize/Interval).
     *
     * Буфер под ответ - та же страница, что и Device Descriptor
     * (desc_buf_phys), но со сдвигом +256 байт, чтобы не
     * затирать уже прочитанные 18 байт (страница у нас 4096
     * байт, с большим запасом на оба буфера).
     */

    UINT64 cfg_buf_phys = desc_buf_phys + 256;
    UINTN  cfg_buf_len  = 512; /* с запасом - у мыши обычно

                                   в разы меньше */

    UINT8 compl_code4 = 0;

    BOOLEAN got_event4 =
        xhci_control_transfer(
            out, xmmio, cap,
            evring_phys, intr0,
            ep0_ring_phys, &trb_slot,
            slot_id,
            0x80u,               /* Device-to-Host/Standard/
                                     Device */
            0x06u,               /* GET_DESCRIPTOR */
            0x0200u,             /* тип=2 (Configuration),
                                     индекс=0 */
            0,
            (UINT16)cfg_buf_len,
            cfg_buf_phys,
            &ev_slot,
            &compl_code4
        );

    if (!got_event4) {

        print(
            out,
            "\nNo Transfer Event for "
            "GET_DESCRIPTOR(Configuration) - stopping "
            "here.\n"
        );
        return;
    }

    print(
        out,
        "\nGET_DESCRIPTOR(Configuration) event: "
        "CompletionCode="
    );
    print_uint(out, compl_code4);
    print(out, " (1=Success, 13=ShortPacket)\n");

    if (!(compl_code4 == 1 || compl_code4 == 13)) {

        print(
            out,
            "GET_DESCRIPTOR(Configuration) did not "
            "complete successfully - stopping here.\n"
        );
        return;
    }

    volatile UINT8 *cfg =
        (volatile UINT8 *)(UINTN)cfg_buf_phys;

    UINT16 cfg_total_len =
        (UINT16)cfg[2] | ((UINT16)cfg[3] << 8);
    UINT8  cfg_num_interfaces = cfg[4];
    UINT8  cfg_value = cfg[5];

    print(out, "bNumInterfaces     = ");
    print_uint(out, cfg_num_interfaces);
    print(out, "\nbConfigurationValue= ");
    print_uint(out, cfg_value);
    print(out, "\nwTotalLength       = ");
    print_uint(out, cfg_total_len);
    print(out, "\n");

    /* устройство не обязано прислать больше, чем реально
       есть - на случай короткого пакета не читаем за пределы
       того, что реально уместилось в буфер */
    UINTN scan_len = cfg_total_len;

    if (scan_len > cfg_buf_len)
        scan_len = cfg_buf_len;

    UINTN off = 0;
    UINT8 cur_iface_class = 0xFFu;
    UINT8 cur_iface_num = 0xFFu;
    BOOLEAN found_ep = FALSE;
    UINT8  ep_addr = 0;
    UINT16 ep_maxpkt = 0;
    UINT8  ep_interval = 0;

    /* Найдено ли устройство HID Class Descriptor (тип 0x21) -
       он лежит внутри Configuration Descriptor сразу после
       Interface Descriptor HID-интерфейса и ДО его Endpoint
       Descriptor'ов, и это единственное место, откуда можно
       узнать реальную длину Report Descriptor'а (поле
       wDescriptorLength) и номер интерфейса, которому его
       адресовать (wIndex у GET_DESCRIPTOR(Report) - это номер
       интерфейса, не устройства) - см. следующий шаг ниже,
       отдельный control transfer за самим Report Descriptor'ом. */
    BOOLEAN found_hid_desc = FALSE;
    UINT8   hid_iface_num = 0;
    UINT16  hid_report_desc_len = 0;

    while (off + 2 <= scan_len) {

        UINT8 d_len = cfg[off];
        UINT8 d_type = cfg[off + 1];

        if (d_len == 0)
            break; /* защита от зацикливания на битом
                       дескрипторе */

        if (d_type == 4 && off + 9 <= scan_len) {

            /* Interface Descriptor: байт 2 - bInterfaceNumber,
               байт 5 - bInterfaceClass (3 = HID) */
            cur_iface_num = cfg[off + 2];
            cur_iface_class = cfg[off + 5];

        } else if (
            d_type == 0x21u && off + 9 <= scan_len &&
            cur_iface_class == 3 && !found_hid_desc
        ) {

            /* HID Class Descriptor (не путать с Report
               Descriptor - это отдельная маленькая "обёртка"
               внутри Configuration Descriptor, которая просто
               ОПИСЫВАЕТ Report Descriptor, но не содержит его):
               байты[2:3]=bcdHID, байт4=bCountryCode,
               байт5=bNumDescriptors (обычно 1 - один Report
               Descriptor на интерфейс), байт6=bDescriptorType
               следующего вложенного дескриптора (должно быть
               0x22 = Report), байты[7:8]=wDescriptorLength - его
               длина в байтах, ровно то число, которое нужно
               запросить через GET_DESCRIPTOR ниже. Берём только
               первый найденный (на случай нескольких HID-
               интерфейсов в составном устройстве, нас интересует
               именно тот, у которого чуть выше уже нашли
               Interrupt IN endpoint). */

            found_hid_desc = TRUE;
            hid_iface_num = cur_iface_num;
            hid_report_desc_len =
                (UINT16)cfg[off + 7] |
                ((UINT16)cfg[off + 8] << 8);

        } else if (
            d_type == 5 && off + 7 <= scan_len &&
            !found_ep
        ) {

            /* Endpoint Descriptor: байт2=bEndpointAddress
               (бит7=1 значит IN), байт3=bmAttributes (биты
               [1:0]=3 значит Interrupt), байты[4:5]=
               wMaxPacketSize, байт6=bInterval */
            UINT8 addr = cfg[off + 2];
            UINT8 attr = cfg[off + 3];

            if (
                cur_iface_class == 3 &&
                (addr & 0x80u) &&
                (attr & 0x3u) == 3u
            ) {

                found_ep = TRUE;
                ep_addr = addr;
                ep_maxpkt =
                    (UINT16)cfg[off + 4] |
                    ((UINT16)cfg[off + 5] << 8);
                ep_interval = cfg[off + 6];
            }
        }

        off = off + d_len;
    }

    if (!found_ep) {

        print(
            out,
            "\nNo HID Interrupt IN endpoint found in "
            "the Configuration Descriptor - this "
            "device may not be a plain HID mouse, or "
            "the descriptor set did not fully fit in "
            "the buffer.\n"
        );

    } else {

        print(out, "\nHID Interrupt IN endpoint found:\n");
        print(out, "  bEndpointAddress = 0x");
        print_hex(out, ep_addr, 2);
        print(out, "  (EP");
        print_uint(out, ep_addr & 0x0Fu);
        print(out, " IN)\n  wMaxPacketSize   = ");
        print_uint(out, ep_maxpkt);
        print(out, "\n  bInterval        = ");
        print_uint(out, ep_interval);
        print(
            out,
            " (polling interval, used below for the "
            "Configure Endpoint Command)\n"
        );
    }

    /* --- SET_CONFIGURATION - без стадии данных (wLength=0),
       переводит устройство из состояния Addressed в
       Configured. bmRequestType=0x00 (Host-to-Device/
       Standard/Device), bRequest=0x09 */

    print(out, "\nSending SET_CONFIGURATION(");
    print_uint(out, cfg_value);
    print(out, ")...\n");

    UINT8 compl_code5 = 0;

    BOOLEAN got_event5 =
        xhci_control_transfer(
            out, xmmio, cap,
            evring_phys, intr0,
            ep0_ring_phys, &trb_slot,
            slot_id,
            0x00u,
            0x09u,
            cfg_value,
            0,
            0,    /* wLength=0 - без Data Stage */
            0,    /* data_buf_phys не используется */
            &ev_slot,
            &compl_code5
        );

    if (!got_event5) {

        print(
            out,
            "No Transfer Event for SET_CONFIGURATION - "
            "stopping here.\n"
        );
        return;
    }

    print(out, "SET_CONFIGURATION event: CompletionCode=");
    print_uint(out, compl_code5);
    print(out, " (1=Success)\n");

    if (compl_code5 != 1) {

        return;
    }

    if (!found_ep) {

        print(
            out,
            "\nDevice is now Configured, but no HID "
            "Interrupt IN endpoint was found earlier - "
            "cannot poll for mouse reports.\n"
        );
        return;
    }

    /* --------------------------------------------------------
     * Шаг 7.5 — настоящий Report Descriptor (GET_DESCRIPTOR,
     * тип 0x22) вместо угаданного boot-protocol формата
     * --------------------------------------------------------
     *
     * bmRequestType=0x81: Device-to-Host (бит7=1) / Standard
     * (биты[6:5]=00 - это ещё обычный, не класс-специфичный
     * запрос, несмотря на то, что адресован он HID-дескриптору) /
     * Recipient=Interface (биты[4:0]=00001) - GET_DESCRIPTOR
     * Report ОБЯЗАН идти с Recipient=Interface, а не Device
     * (в отличие от Device/Configuration Descriptor выше), и
     * wIndex - это номер интерфейса (hid_iface_num, найденный
     * выше при разборе Configuration Descriptor), а НЕ 0.
     * wValue = (тип=0x22 << 8) | индекс=0 (первый, и почти
     * всегда единственный, Report Descriptor интерфейса).
     *
     * Длину берём из HID Class Descriptor'а (hid_report_desc_len,
     * см. выше) - если по какой-то причине он не нашёлся
     * (found_hid_desc == FALSE, дескриптор в кадр не влез или
     * был не там, где ожидали), подстраховываемся разумным
     * запасом в 256 байт - обычные мышиные Report Descriptor'ы
     * укладываются в 30-80 байт, этого с большим запасом хватит,
     * а лишние незаполненные байты GET_DESCRIPTOR просто не
     * пришлёт (Short Packet, CompletionCode=13 - тоже
     * обрабатываем как успех, как и везде выше).
     *
     * Буфер - та же страница, что Device/Configuration
     * Descriptor и сам буфер под отчёты (desc_buf_phys), сдвиг
     * +2048 - после Device Descriptor (0..18), Configuration
     * Descriptor (256..768) и буфера под сами отчёты
     * (1024..1088) остаётся больше 3000 байт свободного места в
     * странице, +2048 - с запасом от всех них.
     */

    UINT16 report_desc_len =
        found_hid_desc ? hid_report_desc_len : 256u;

    if (report_desc_len > 2048u)
        report_desc_len = 2048u; /* защита - не должно случаться
                                     у обычной мыши, но не
                                     позволяем запросу вылезти за
                                     пределы страницы */

    UINT64 report_desc_buf_phys = desc_buf_phys + 2048u;

    raw_zero_mem(
        (volatile UINT8 *)(UINTN)report_desc_buf_phys, 2048
    );

    print(out, "\nRequesting HID Report Descriptor (");
    print_uint(out, report_desc_len);
    print(out, " bytes, interface ");
    print_uint(out, hid_iface_num);
    print(out, ")...\n");

    UINT8 compl_code_rd = 0;

    BOOLEAN got_event_rd =
        xhci_control_transfer(
            out, xmmio, cap,
            evring_phys, intr0,
            ep0_ring_phys, &trb_slot,
            slot_id,
            0x81u,               /* Device-to-Host/Standard/
                                     Interface */
            0x06u,               /* GET_DESCRIPTOR */
            (UINT16)(0x2200u),   /* тип=0x22 (Report), индекс=0 */
            hid_iface_num,
            report_desc_len,
            report_desc_buf_phys,
            &ev_slot,
            &compl_code_rd
        );

    HID_MOUSE_REPORT_LAYOUT mouse_layout;

    /* если хоть что-то пошло не так - mouse_layout.valid
       останется FALSE (см. явную инициализацию ниже), и цикл
       опроса ниже сам заметит это и вернётся к старому
       угаданному byte0/1/2 формату, ничего специально
       обрабатывать тут не нужно */
    mouse_layout.valid = FALSE;
    mouse_layout.has_buttons = FALSE;
    mouse_layout.x_bit_size = 0;
    mouse_layout.y_bit_size = 0;
    mouse_layout.has_wheel = FALSE;
    mouse_layout.has_report_id = FALSE;

    if (!got_event_rd) {

        print(
            out,
            "No Transfer Event for GET_DESCRIPTOR(Report) - "
            "keeping the old guessed byte0/1/2 layout.\n"
        );

    } else {

        print(out, "GET_DESCRIPTOR(Report) event: "
                    "CompletionCode=");
        print_uint(out, compl_code_rd);
        print(out, " (1=Success, 13=ShortPacket)\n");

        if (compl_code_rd == 1u || compl_code_rd == 13u) {

            hid_parse_report_descriptor(
                out,
                (volatile UINT8 *)(UINTN)report_desc_buf_phys,
                report_desc_len,
                &mouse_layout
            );

        } else {

            print(
                out,
                "GET_DESCRIPTOR(Report) did not complete "
                "successfully - keeping the old guessed "
                "byte0/1/2 layout.\n"
            );
        }
    }

    /* --------------------------------------------------------
     * Шаг 8 — Configure Endpoint Command для Interrupt IN
     * endpoint + опрос реальных отчётов мыши (dX/dY/кнопки)
     * --------------------------------------------------------
     *
     * Устройство сейчас в состоянии Configured, но реально
     * работает пока только Endpoint 0 (Default Control Pipe) -
     * контроллер ничего не знает про Interrupt IN endpoint,
     * найденный выше в Configuration Descriptor, пока мы явно
     * не опишем его в Input Context и не пошлём Configure
     * Endpoint Command (TRB Type 12) - тот же механизм, что и
     * Address Device (Type 11) выше, только теперь добавляем
     * контекст ОДНОЙ конкретной конечной точки, а не Endpoint 0.
     *
     * Input Context здесь переиспользуется (та же страница
     * input_ctx_phys, что была под Address Device) - она нужна
     * только на момент самой команды, контроллер её не хранит
     * после обработки, поэтому спокойно перезаписываем.
     */

    print(
        out,
        "\nDevice is now Configured. Setting up the "
        "Interrupt IN endpoint (Configure Endpoint "
        "Command)...\n"
    );

    /* Device Context Index (DCI): по спеке xHCI конечные точки
       нумеруются в Device Context не по bEndpointAddress
       напрямую, а как DCI = 2*(номер endpoint) + направление
       (0=OUT, 1=IN). Endpoint 0 всегда DCI=1 (уже занят, EP0
       Context выше). Для нашего Interrupt IN endpoint номер N -
       DCI = 2*N + 1. */

    UINT8 ep_num = ep_addr & 0x0Fu;
    UINT8 dci = (UINT8)(ep_num * 2u + 1u);

    print(out, "Endpoint number = ");
    print_uint(out, ep_num);
    print(out, ", Device Context Index (DCI) = ");
    print_uint(out, dci);
    print(out, "\n");

    /* Interval: xHCI хранит период опроса как степень двойки в
       единицах 125мкс (реальный период = 2^Interval * 125мкс).
       USB-дескриптор хранит bInterval по-разному в зависимости
       от скорости порта:
       - High/SuperSpeed: bInterval УЖЕ показатель степени
         (1..16), период = 2^(bInterval-1) * 125мкс, поэтому
         Interval = bInterval - 1.
       - Low/Full Speed: bInterval - это число целых кадров по
         1мс (1..255) буквально. xHCI всё равно не умеет хранить
         произвольный период, только степень двойки от 125мкс,
         поэтому берём ближайшую снизу степень двойки от
         (bInterval мс, переведённых в те же единицы 125мкс,
         т.е. bInterval*8) через floor(log2(bInterval))+3 - это
         тот же приближённый пересчёт, что использует и Linux
         в своём xHCI-драйвере. */

    UINT8 interval_field;

    if (port_speed == 3 || port_speed == 4) {

        interval_field =
            (ep_interval >= 1) ? (UINT8)(ep_interval - 1) : 0;

    } else {

        UINT8 v = (ep_interval == 0) ? 1 : ep_interval;
        UINT8 log2_val = 0;

        while ((v >> 1) != 0) {
            v = (UINT8)(v >> 1);
            log2_val = (UINT8)(log2_val + 1);
        }

        interval_field = (UINT8)(log2_val + 3);
    }

    if (interval_field > 15)
        interval_field = 15;

    print(out, "xHCI Interval field = ");
    print_uint(out, interval_field);
    print(out, " (period = 2^");
    print_uint(out, interval_field);
    print(out, " * 125us)\n");

    /* --- Transfer Ring для этой конечной точки + буфер под
       отчёт (переиспользуем ту же страницу, что и под
       Device/Configuration Descriptor - там ещё много свободного
       места после байта 768). int_ring_phys теперь тоже приходит
       уже выделенным аргументом функции (см. комментарий выше про
       AllocatePages) - тут только обнуляем. --- */

    raw_zero_mem(
        (volatile UINT8 *)(UINTN)int_ring_phys, 4096
    );

    /*
     * Link TRB (TRB Type=6) в последнем слоте страницы (слот 255
     * из 256 - страница 4096 байт, TRB по 16 байт).
     *
     * ЭТО ТА ЖЕ САМАЯ ПРОБЛЕМА, что мы только что нашли и
     * починили на Event Ring (см. xhci_event_ring_trb_addr выше
     * по файлу) - только здесь наоборот: не мы читаем события
     * контроллера, а контроллер читает TRB, которые пишем МЫ
     * (Transfer Ring для Interrupt IN endpoint'а). Опрос отчётов
     * мыши раньше писал TRB по адресу int_ring_phys +
     * int_trb_slot*16, где int_trb_slot рос без остановки - то
     * есть ровно так же "уезжал" за пределы выделенной под кольцо
     * страницы после 256 отчётов, и мышь (точнее, xHC) переставала
     * присылать Transfer Event вообще - отчёты просто "кончались"
     * без единого сообщения об ошибке.
     *
     * Правильное решение по спеке xHCI - НЕ "остаток от деления"
     * (это работало для Event Ring, потому что там читаем МЫ),
     * а настоящий Link TRB: последний слот страницы навсегда
     * отдаётся под TRB Type=6, который указывает контроллеру
     * "дальше кольцо продолжается по адресу int_ring_phys" (то
     * есть с начала той же страницы) и содержит бит Toggle Cycle
     * (бит1) - это указание контроллеру, что при переходе по
     * этой ссылке нужно перевернуть его внутреннее понимание
     * Cycle Bit (ровно как и Event Ring, кольцо "разворачивается"
     * начиная со второго круга - только тут разворот явно
     * прописан TRB-указателем, а не подразумевается остатком от
     * деления). Cycle этого TRB тоже нужно обновлять на каждом
     * круге (см. xhci_xfer_ring_pcs ниже) - обновляем его сразу,
     * как только начинаем писать первый Normal TRB нового круга,
     * чтобы контроллер успел увидеть верное значение к тому
     * моменту, как дойдёт до этого слота.
     */
    volatile UINT32 *int_ring_link_trb =
        (volatile UINT32 *)
            (UINTN)(int_ring_phys +
                    (UINT64)(XHCI_XFER_RING_TRBS - 1u) * 16u);

    int_ring_link_trb[0] =
        (UINT32)(int_ring_phys & 0xFFFFFFFFu);
    int_ring_link_trb[1] =
        (UINT32)(int_ring_phys >> 32);
    int_ring_link_trb[2] = 0;
    /* DW3: Cycle=1 (для самого первого круга, совпадает с
       DCS=1, который мы пропишем в Endpoint Context ниже),
       Toggle Cycle (бит1), TRB Type=6 (Link, биты 10:15) */
    int_ring_link_trb[3] =
        0x1u | (1u << 1) | (6u << 10);

    UINT64 report_buf_phys = desc_buf_phys + 1024;

    raw_zero_mem(
        (volatile UINT8 *)(UINTN)report_buf_phys, 64
    );

    /* --- перезаписываем Input Context под Configure Endpoint
       Command: Add Context Flags A0 (Slot, обязательно всегда)
       и A(dci) (наша конечная точка). Slot Context копируем с
       теми же Route String/Speed/Root Port, что и раньше, но
       Context Entries теперь = dci (самый старший используемый
       индекс контекста, а не 1) - иначе контроллер не будет
       знать, что контекст endpoint'а вообще валиден. --- */

    raw_zero_mem(
        (volatile UINT8 *)(UINTN)input_ctx_phys, 4096
    );

    volatile UINT32 *input_ctrl2 =
        (volatile UINT32 *)(UINTN)input_ctx_phys;

    input_ctrl2[0] = 0;
    input_ctrl2[1] = 0x1u | (1u << dci);

    volatile UINT32 *slot_ctx2 =
        (volatile UINT32 *)
            (UINTN)(input_ctx_phys + ctx_size);

    slot_ctx2[0] =
        (port_speed << 20) | ((UINT32)dci << 27);
    slot_ctx2[1] = (UINT32)root_port << 16;
    slot_ctx2[2] = 0;
    slot_ctx2[3] = 0;

    volatile UINT32 *int_ep_ctx =
        (volatile UINT32 *)
            (UINTN)(input_ctx_phys +
                    (UINT64)(dci + 1u) * ctx_size);

    /* DW0: Interval - биты 16:23 (НЕ 8:15! Раньше тут была
       ошибка - Interval попадал в биты Mult/MaxPStreams/LSA,
       а для Interrupt-конечной точки это невалидные поля,
       контроллер мог принять команду (CompletionCode=1), но
       дальше просто не планировать реальный опрос устройства -
       см. историю отладки: с этим багом Configure Endpoint
       "успешно" завершался, но ни одного отчёта с мыши так и
       не приходило). Mult/MaxPStreams(биты 8:14)/LSA(бит15) = 0
       - обычная одиночная конечная точка без streams. */
    int_ep_ctx[0] = (UINT32)interval_field << 16;

    /* DW1: CErr=3, EP Type=7 (Interrupt IN), Max Burst Size=0,
       Max Packet Size (биты 16:31) */
    int_ep_ctx[1] =
        (3u << 1) | (7u << 3) |
        ((UINT32)ep_maxpkt << 16);

    /* DW2/DW3: TR Dequeue Pointer + DCS=1 - кольцо только что
       создано */
    int_ep_ctx[2] =
        (UINT32)(int_ring_phys & 0xFFFFFFFFu) | 0x1u;
    int_ep_ctx[3] = (UINT32)(int_ring_phys >> 32);

    /* DW4: Average TRB Length - размер отчёта (ep_maxpkt) - по
       спеке не должен быть 0; Max ESIT Payload Lo оставляем 0
       (не критично для простого прерывания без Streams/SS) */
    int_ep_ctx[4] = ep_maxpkt;

    /* --- Configure Endpoint Command (TRB Type 12) - третий
       слот Command Ring (offset 32, после Enable Slot и
       Address Device) --- */

    volatile UINT32 *cmd_trb3 =
        (volatile UINT32 *)
            (UINTN)(cmdring_phys + 32);

    cmd_trb3[0] =
        (UINT32)(input_ctx_phys & 0xFFFFFFFFu);
    cmd_trb3[1] =
        (UINT32)(input_ctx_phys >> 32);
    cmd_trb3[2] = 0;
    cmd_trb3[3] =
        ((UINT32)slot_id << 24) | (12u << 10) | 0x1u;

    mmio_write32(xmmio + cap->DbOff, 0);

    volatile UINT32 *ev_trb3 = NULL;

    BOOLEAN got_event6 =
        xhci_wait_for_event(
            out, evring_phys, intr0,
            &ev_slot, 33, &ev_trb3
        );

    if (!got_event6) {

        print(
            out,
            "No Command Completion Event for Configure "
            "Endpoint - stopping here.\n"
        );
        return;
    }

    UINT8 compl_code6 =
        (UINT8)((ev_trb3[2] >> 24) & 0xFFu);

    print(out, "Configure Endpoint event: CompletionCode=");
    print_uint(out, compl_code6);
    print(out, " (1=Success)\n");

    ev_slot = ev_slot + 1;

    UINT64 cfgep_next_ev =
        xhci_event_ring_trb_addr(evring_phys, ev_slot);

    mmio_write32(
        intr0 + 0x18,
        (UINT32)(cfgep_next_ev & 0xFFFFFFFFu)
    );
    mmio_write32(
        intr0 + 0x1C,
        (UINT32)(cfgep_next_ev >> 32)
    );

    if (compl_code6 != 1) {

        print(
            out,
            "Configure Endpoint did not succeed - not "
            "polling for reports.\n"
        );
        return;
    }

    print(
        out,
        "\nInterrupt IN endpoint configured. Polling "
        "for real mouse reports now - move the mouse.\n"
    );

    /* --------------------------------------------------------
     * Опрос отчётов: кладём один Normal TRB (буфер под отчёт)
     * на Transfer Ring endpoint'а, звоним в его Doorbell
     * (Target=dci, НЕ 1 - это отдельная конечная точка, не
     * Endpoint 0), и ждём Transfer Event.
     *
     * РАНЬШЕ здесь между короткими порциями ожидания опрашивалась
     * клавиатура (ConIn->ReadKeyStroke) неблокирующим образом,
     * чтобы можно было прервать опрос нажатием клавиши. Эта
     * функция теперь вызывается уже ПОСЛЕ ExitBootServices (см.
     * команду "ebs") - клавиатуры (как и всех остальных Boot
     * Services) больше гарантированно нет, поэтому "выход по
     * клавише" заменён на выход по ограничению числа отчётов
     * (XHCI_POLL_MAX_REPORTS) - опрос идёт заданное число раз и
     * останавливается сам, без участия пользователя.
     * -------------------------------------------------------- */

    UINTN int_trb_slot = 0;
    UINTN reports_seen = 0;

    for (;;) {

        if (reports_seen >= XHCI_POLL_MAX_REPORTS)
            break;

        /* Обновляем Cycle Bit в Link TRB на конец страницы -
           НО не в момент записи первого TRB нового круга (как
           было раньше - и это была ошибка), а на ВТОРОМ TRB
           нового круга.
           Почему: получив Transfer Event для первого TRB нового
           круга (int_trb_slot % USABLE == 0), мы точно знаем,
           что контроллер уже реально прошёл через Link TRB со
           СТАРЫМ значением Cycle Bit (иначе он не смог бы
           признать валидным TRB, который лежит сразу после
           Link TRB, и событие бы не пришло) - то есть именно
           сейчас, при записи второго TRB, обновлять Link TRB уже
           безопасно, старое значение ему больше не понадобится
           вплоть до конца ЭТОГО круга (ещё 253 TRB впереди).
           Если обновлять на первом TRB (как было) - есть гонка:
           контроллер может проверять Link TRB "лениво", только
           когда реально пытается дойти до следующего TRB после
           него, и наша слишком ранняя перезапись подменяет
           значение прямо в момент, когда он его ожидает старым -
           он видит "невалидный" Link TRB и зависает навсегда
           (ровно то, что и случилось на отчёте #256). */

        if (
            (int_trb_slot % (UINTN)XHCI_XFER_RING_USABLE_TRBS)
                == 1
        ) {

            volatile UINT32 *link_upd =
                (volatile UINT32 *)
                    (UINTN)(int_ring_phys +
                            (UINT64)(XHCI_XFER_RING_TRBS - 1u) *
                                16u);

            UINT32 link_cycle =
                xhci_xfer_ring_pcs(int_trb_slot);

            link_upd[3] =
                link_cycle | (1u << 1) | (6u << 10);
        }

        UINT32 xfer_cycle =
            xhci_xfer_ring_pcs(int_trb_slot);

        volatile UINT32 *xfer_trb =
            (volatile UINT32 *)(UINTN)
                xhci_xfer_ring_trb_addr(
                    int_ring_phys, int_trb_slot
                );

        xfer_trb[0] =
            (UINT32)(report_buf_phys & 0xFFFFFFFFu);
        xfer_trb[1] =
            (UINT32)(report_buf_phys >> 32);
        /* DW2: TRB Transfer Length (биты 0:16) - сколько байт
           готовы принять */
        xfer_trb[2] = (UINT32)ep_maxpkt;
        /* DW3: Cycle - см. xhci_xfer_ring_pcs (переворачивается
           каждый круг кольца, иначе контроллер после первого же
           оборота решит, что новых TRB больше нет), IOC (бит5),
           TRB Type=1 (Normal, биты 10:15) */
        xfer_trb[3] =
            xfer_cycle | (1u << 5) | (1u << 10);

        /* Doorbell SlotID, Target=dci - у каждой конечной точки
           свой Target в её собственном doorbell-регистре, не
           путать с Target=1 у Endpoint 0 (control transfer'ы
           выше) и Target=0 (Command Ring, отдельный doorbell
           0) */
        mmio_write32(
            xmmio + cap->DbOff + (UINT64)slot_id * 4, dci
        );

        volatile UINT32 *ev_trb4 =
            (volatile UINT32 *)(UINTN)
                xhci_event_ring_trb_addr(evring_phys, ev_slot);

        UINT32 expected_cycle4 =
            xhci_event_ring_expected_cycle(ev_slot);

        BOOLEAN got_report = FALSE;

        for (
            UINTN attempt = 0;
            attempt < XHCI_POLL_MAX_IDLE_ATTEMPTS &&
                !got_report;
            attempt++
        ) {

            for (UINTN i = 0; i < 50; i++) {

                /* Раньше - BootServices->Stall(1000).
                   Firmware-независимая задержка, см.
                   busy_wait_ms() выше по файлу. */
                busy_wait_ms(1);

                if ((ev_trb4[3] & 0x1u) == expected_cycle4) {
                    got_report = TRUE;
                    break;
                }
            }
        }

        if (!got_report) {

            /* мышь просто молчала это окно времени - не ошибка,
               тот же TRB ещё не потреблён контроллером (раз
               события не было), идём на следующую итерацию и
               пробуем ещё */
            continue;
        }

        UINT8 t4 = (UINT8)((ev_trb4[3] >> 10) & 0x3Fu);

        if (t4 != 32) {

            /* постороннее событие - подтверждаем (сдвигаем
               ERDP) и продолжаем опрос тем же TRB */
            print(
                out,
                "  (skipping unrelated event, TRB Type="
            );
            print_uint(out, t4);
            print(out, ")\n");

            ev_slot = ev_slot + 1;

            UINT64 skip_addr =
                xhci_event_ring_trb_addr(evring_phys, ev_slot);

            mmio_write32(
                intr0 + 0x18,
                (UINT32)(skip_addr & 0xFFFFFFFFu)
            );
            mmio_write32(
                intr0 + 0x1C,
                (UINT32)(skip_addr >> 32)
            );

            continue;
        }

        UINT8 compl_code7 =
            (UINT8)((ev_trb4[2] >> 24) & 0xFFu);

        ev_slot = ev_slot + 1;

        UINT64 rep_next_ev =
            xhci_event_ring_trb_addr(evring_phys, ev_slot);

        mmio_write32(
            intr0 + 0x18,
            (UINT32)(rep_next_ev & 0xFFFFFFFFu)
        );
        mmio_write32(
            intr0 + 0x1C,
            (UINT32)(rep_next_ev >> 32)
        );

        if (!(compl_code7 == 1 || compl_code7 == 13)) {

            print(
                out,
                "  (transfer event CompletionCode="
            );
            print_uint(out, compl_code7);
            print(out, " - skipping this report)\n");

            int_trb_slot = int_trb_slot + 1;
            continue;
        }

        reports_seen = reports_seen + 1;

        volatile UINT8 *rep =
            (volatile UINT8 *)(UINTN)report_buf_phys;

        print(out, "Report #");
        print_uint(out, reports_seen);
        print(out, ": ");

        if (mouse_layout.valid) {

            /* --- настоящий формат, разобранный из Report
               Descriptor этого конкретного устройства (см.
               hid_parse_report_descriptor выше) - никаких
               предположений про byte0/1/2 тут больше нет,
               каждое поле читается ровно с того бита и той
               ширины, которые прислало само устройство. --- */

            UINT32 buttons = 0;

            if (mouse_layout.has_buttons) {

                buttons =
                    hid_extract_bits(
                        rep, ep_maxpkt,
                        mouse_layout.button_bit_offset,
                        mouse_layout.button_count
                    );
            }

            UINT32 x_raw =
                hid_extract_bits(
                    rep, ep_maxpkt,
                    mouse_layout.x_bit_offset,
                    mouse_layout.x_bit_size
                );
            UINT32 y_raw =
                hid_extract_bits(
                    rep, ep_maxpkt,
                    mouse_layout.y_bit_offset,
                    mouse_layout.y_bit_size
                );

            /* знак имеет смысл только для относительных полей -
               абсолютные координаты (планшет/тачпад) печатаем
               как есть, без знакового расширения */
            INT32 dx =
                mouse_layout.x_is_relative ?
                    hid_sign_extend(
                        x_raw, mouse_layout.x_bit_size
                    ) :
                    (INT32)x_raw;
            INT32 dy =
                mouse_layout.y_is_relative ?
                    hid_sign_extend(
                        y_raw, mouse_layout.y_bit_size
                    ) :
                    (INT32)y_raw;

            print(out, "buttons=0x");
            print_hex(out, buttons, 2);
            print(out, "  dX=");
            print_int(out, dx);
            print(out, "  dY=");
            print_int(out, dy);

            if (mouse_layout.has_wheel) {

                UINT32 wheel_raw =
                    hid_extract_bits(
                        rep, ep_maxpkt,
                        mouse_layout.wheel_bit_offset,
                        mouse_layout.wheel_bit_size
                    );
                INT32 wheel =
                    mouse_layout.wheel_is_relative ?
                        hid_sign_extend(
                            wheel_raw,
                            mouse_layout.wheel_bit_size
                        ) :
                        (INT32)wheel_raw;

                print(out, "  wheel=");
                print_int(out, wheel);
            }

            print(out, "\n");

        } else {

            /* --- Report Descriptor не прочитался или не
               распознался (см. предупреждение чуть выше по
               логу) - тот же угаданный "boot protocol" формат,
               что использовался раньше: байт0 - битовая маска
               кнопок, байт1 - dX со знаком, байт2 - dY со
               знаком. Оставлен как подстраховка, а не основной
               путь. --- */

            UINT8 buttons = rep[0];
            INT8  dx = (INT8)rep[1];
            INT8  dy = (INT8)rep[2];

            print(out, "(fallback layout) buttons=0x");
            print_hex(out, buttons, 2);
            print(out, "  dX=");
            print_int(out, dx);
            print(out, "  dY=");
            print_int(out, dy);
            print(out, "\n");
        }

        int_trb_slot = int_trb_slot + 1;
    }

    print(out, "\nStopped polling after ");
    print_uint(out, reports_seen);
    print(out, " report(s).\n");
}


/*
 * Весь путь от уже включённого/отсоединённого от прошивки
 * контроллера (шаги 0-3, см. команду "xhci") до реального опроса
 * отчётов мыши: сброс, DCBAA + Command Ring + Event Ring, запуск,
 * сканирование портов, Port Reset, Enable Slot, Address Device +
 * GET_DESCRIPTOR + SET_CONFIGURATION + опрос.
 *
 * Эта функция вызывается командой "ebs" (см. ниже) ЦЕЛИКОМ ПОСЛЕ
 * ExitBootServices - "out" здесь уже не настоящий st->ConOut, а
 * наш собственный пиксельный "терминал" (см. ebs_console_start
 * выше), а все физические страницы под DCBAA/кольца/контексты
 * переданы уже готовыми - их выделяли заранее, пока ещё были живы
 * Boot Services (AllocatePages после ExitBootServices недоступен).
 * Единственное, чем эта функция вообще пользуется из "внешнего
 * мира" - чтение/запись MMIO-регистров контроллера через порты
 * PCI и обычную память - к прошивке никакого отношения не имеет.
 */
static void xhci_run_post_ebs(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 xmmio,
    XHCI_CAP_INFO *cap,
    UINT64 dcbaa_phys,
    UINT64 cmdring_phys,
    UINT64 evring_phys,
    UINT64 erst_phys,
    UINT64 input_ctx_phys,
    UINT64 dev_ctx_phys,
    UINT64 ep0_ring_phys,
    UINT64 desc_buf_phys,
    UINT64 int_ring_phys
)
{
    UINT64 op_base = xmmio + cap->CapLength;

    print(out, "\n--- reset + minimal init ---\n");

    if (!xhci_reset_controller(op_base)) {

        print(
            out,
            "Controller did not come out of reset within "
            "the timeout - stopping here.\n"
        );
        return;
    }

    print(out, "Host Controller Reset: done.\n");

    raw_zero_mem((volatile UINT8 *)(UINTN)dcbaa_phys, 4096);
    raw_zero_mem((volatile UINT8 *)(UINTN)cmdring_phys, 4096);

    print(out, "DCBAA   at 0x");
    print_hex(out, dcbaa_phys, 16);
    print(out, "\nCmdRing at 0x");
    print_hex(out, cmdring_phys, 16);
    print(out, "\n");

    /* DCBAAP - Device Context Base Address Array Pointer */
    mmio_write32(
        op_base + 0x30, (UINT32)(dcbaa_phys & 0xFFFFFFFFu)
    );
    mmio_write32(op_base + 0x34, (UINT32)(dcbaa_phys >> 32));

    /* CRCR - Command Ring Control Register: указатель + Ring
       Cycle State = 1 (стартовое значение по спеке) */
    UINT64 crcr = (cmdring_phys & ~0x3Full) | 0x1u;

    mmio_write32(op_base + 0x18, (UINT32)(crcr & 0xFFFFFFFFu));
    mmio_write32(op_base + 0x1C, (UINT32)(crcr >> 32));

    /* CONFIG - сколько Device Slot'ов реально включаем */
    mmio_write32(op_base + 0x38, cap->MaxSlots);

    /*
     * Event Ring (регистры интеррапера ERSTSZ/ERSTBA/ERDP)
     * обязаны быть настроены ДО того, как ставится RS=1
     * (Run/Stop) - иначе контроллер может записать событие
     * (например Port Status Change) по ещё не инициализированным
     * регистрам, то есть буквально куда попало в память.
     */
    UINT64 rt_base = xmmio + cap->RtsOff;
    UINT64 intr0 = rt_base + 0x20;

    raw_zero_mem((volatile UINT8 *)(UINTN)evring_phys, 4096);
    raw_zero_mem((volatile UINT8 *)(UINTN)erst_phys, 4096);

    /* ERST[0]: адрес сегмента + число TRB в нём (по 16 байт
       каждый) */
    volatile UINT32 *erst = (volatile UINT32 *)(UINTN)erst_phys;

    erst[0] = (UINT32)(evring_phys & 0xFFFFFFFFu);
    erst[1] = (UINT32)(evring_phys >> 32);
    erst[2] = 256;
    erst[3] = 0;

    /* ERSTSZ = 1 сегмент */
    mmio_write32(intr0 + 0x08, 1);

    /* ERSTBA */
    mmio_write32(
        intr0 + 0x10, (UINT32)(erst_phys & 0xFFFFFFFFu)
    );
    mmio_write32(intr0 + 0x14, (UINT32)(erst_phys >> 32));

    /* ERDP = начало кольца (мы ещё ничего не читали) */
    mmio_write32(
        intr0 + 0x18, (UINT32)(evring_phys & 0xFFFFFFFFu)
    );
    mmio_write32(intr0 + 0x1C, (UINT32)(evring_phys >> 32));

    /* Run/Stop = 1 - запускаем контроллер */
    UINT32 usbcmd = mmio_read32(op_base + 0x00);
    usbcmd |= 0x1u;
    mmio_write32(op_base + 0x00, usbcmd);

    BOOLEAN started = FALSE;

    for (UINTN i = 0; i < 200; i++) {

        /* Раньше - BootServices->Stall(1000), см. busy_wait_ms()
           выше по файлу. */
        busy_wait_ms(1);

        UINT32 sts = mmio_read32(op_base + 0x04);

        if ((sts & 0x1u) == 0) {
            started = TRUE;
            break;
        }
    }

    if (!started) {

        print(
            out,
            "Controller did not leave Halted state after "
            "Run/Stop=1 - stopping here.\n"
        );
        return;
    }

    print(
        out, "Controller running (HCHalted=0). Port status:\n\n"
    );

    UINTN reset_target_port = 0;

    for (UINTN p = 1; p <= cap->MaxPorts; p++) {

        UINT64 port_base =
            op_base + 0x400 + (UINT64)(p - 1) * 0x10;

        UINT32 portsc = mmio_read32(port_base);

        BOOLEAN ccs = (portsc & 0x1u) != 0;
        BOOLEAN ped = (portsc & 0x2u) != 0;

        print(out, "  Port ");
        print_uint(out, p);
        print(out, ": PORTSC=0x");
        print_hex(out, portsc, 8);
        print(out, "  ");

        if (ccs) {

            print(out, "CONNECTED");

            if (ped) {
                print(out, ", enabled");
            } else if (reset_target_port == 0) {
                reset_target_port = p;
            }

        } else {

            print(out, "empty");
        }

        print(out, "\n");
    }

    /*
     * Порт подключён, но не включён (обычное дело для
     * не-SuperSpeed портов - им нужен явный Port Reset, только
     * после него PED станет 1). Дальше - Enable Slot Command
     * через Command Ring, чтобы получить Slot ID для этого
     * устройства.
     */
    if (reset_target_port == 0) {

        print(
            out,
            "\nNo port needs a reset (nothing new connected) - "
            "nothing further to do.\n"
        );
        return;
    }

    print(out, "\n--- port reset + Enable Slot Command ---\n");

    UINT64 port_base =
        op_base + 0x400 + (UINT64)(reset_target_port - 1) * 0x10;

    UINT32 cur = mmio_read32(port_base);

    mmio_write32(
        port_base,
        portsc_base_for_write(cur) | PORTSC_BIT_PR
    );

    BOOLEAN reset_done = FALSE;

    for (UINTN i = 0; i < 500; i++) {

        busy_wait_ms(1);

        UINT32 s = mmio_read32(port_base);

        if (s & PORTSC_BIT_PRC) {
            reset_done = TRUE;
            cur = s;
            break;
        }
    }

    if (!reset_done) {

        print(
            out,
            "Port Reset did not complete within the timeout - "
            "stopping here.\n"
        );
        return;
    }

    /* подтвердить (очистить) PRC, не трогая PED и остальные
       RW1CS-биты */
    mmio_write32(
        port_base,
        portsc_base_for_write(cur) | PORTSC_BIT_PRC
    );

    print(out, "Port ");
    print_uint(out, reset_target_port);
    print(out, ": reset complete, now enabled (PED=1).\n");

    /* Command TRB: Enable Slot (TRB Type 9), Cycle=1 */
    volatile UINT32 *cmd_trb =
        (volatile UINT32 *)(UINTN)cmdring_phys;

    cmd_trb[0] = 0;
    cmd_trb[1] = 0;
    cmd_trb[2] = 0;
    cmd_trb[3] = (9u << 10) | 0x1u;

    /* Doorbell 0, Target=0 - звонок в Command Ring */
    mmio_write32(xmmio + cap->DbOff, 0);

    volatile UINT32 *ev_trb = NULL;
    UINTN ev_slot = 0;

    BOOLEAN got_event =
        xhci_wait_for_event(
            out, evring_phys, intr0,
            &ev_slot, 33, /* Command Completion Event */
            &ev_trb
        );

    if (!got_event) {

        print(
            out,
            "No Command Completion Event showed up on the "
            "Event Ring - stopping here.\n"
        );
        return;
    }

    UINT8 trb_type = (UINT8)((ev_trb[3] >> 10) & 0x3Fu);
    UINT8 compl_code = (UINT8)((ev_trb[2] >> 24) & 0xFFu);
    UINT8 slot_id = (UINT8)((ev_trb[3] >> 24) & 0xFFu);

    print(out, "Event: TRB Type=");
    print_uint(out, trb_type);
    print(
        out,
        " (33=CommandCompletionEvent), CompletionCode="
    );
    print_uint(out, compl_code);
    print(out, " (1=Success), SlotID=");
    print_uint(out, slot_id);
    print(out, "\n");

    /* подтвердить это событие - сдвинуть ERDP на следующий TRB,
       и запомнить индекс для дальнейшего опроса (Address
       Device) */
    ev_slot = ev_slot + 1;

    UINT64 next_ev_addr =
        xhci_event_ring_trb_addr(evring_phys, ev_slot);

    mmio_write32(
        intr0 + 0x18, (UINT32)(next_ev_addr & 0xFFFFFFFFu)
    );
    mmio_write32(intr0 + 0x1C, (UINT32)(next_ev_addr >> 32));

    if (!(trb_type == 33 && compl_code == 1)) {
        return;
    }

    print(
        out,
        "\nEnable Slot succeeded - device has a Slot ID now.\n"
    );

    /*
     * Address Device + GET_DESCRIPTOR + SET_CONFIGURATION +
     * опрос отчётов. port_base тут ещё указывает на PORTSC
     * reset_target_port (мы его не трогали с момента Port Reset
     * выше). ev_slot уже указывает на первый ещё не занятый слот
     * Event Ring - передаём его как стартовую точку опроса.
     */
    xhci_address_device_and_get_descriptor(
        out, xmmio, cap,
        dcbaa_phys, cmdring_phys, evring_phys, intr0,
        port_base, reset_target_port, slot_id, ev_slot,
        input_ctx_phys, dev_ctx_phys, ep0_ring_phys,
        desc_buf_phys, int_ring_phys
    );
}





/* ################################################################
 * ################################################################
 *
 *   KERNEL MODE: ОС продолжает жить ПОСЛЕ ExitBootServices
 *
 * ################################################################
 * ################################################################
 *
 * До этого места вся ОС (шелл, GUI, "Сапёр", терминал) работала
 * как обычное UEFI-приложение: печатала через st->ConOut, читала
 * клавиши через st->ConIn, ждала через BootServices->Stall,
 * брала память через AllocatePool, а мышь - через
 * EFI_SIMPLE_POINTER_PROTOCOL. Всё это - код прошивки, и всё это
 * исчезает в момент ExitBootServices. Старое демо "ebs" поэтому
 * могло только нарисовать что-то и перезагрузиться.
 *
 * Этот блок - собственные замены каждой из этих вещей:
 *
 *   1. Текстовая консоль (kcon_*) - рисует символы нормальным
 *      шрифтом 8x16 прямо в framebuffer, с цветами, прокруткой и
 *      курсором. Выглядит для остального кода как обычный
 *      SIMPLE_TEXT_OUTPUT_INTERFACE.
 *
 *   2. Процессор (kx_cpu_*) - своя GDT, своя IDT с обработчиками
 *      всех 256 векторов, экран "паники" для исключений процессора
 *      (вместо тихого зависания/перезагрузки).
 *
 *   3. Время (kx_time_*) - частота TSC калибруется по PIT
 *      (микросхема 8254, порты 0x40-0x43), а настоящий
 *      периодический таймер - Local APIC timer, 1000 прерываний в
 *      секунду. Вместо неоткалиброванного busy_wait_ms.
 *
 *   4. Физическая память (pmm_*) - битовая карта страниц,
 *      построенная по ИТОГОВОЙ карте памяти от GetMemoryMap
 *      (той самой, с которой вызывался ExitBootServices). Поверх
 *      неё - простейший "пул" (kpool_*) под AllocatePool.
 *
 *   5. Ввод (kbd_*, kx_*, ps2_*) - неблокирующий xHCI-драйвер,
 *      который перечисляет ВСЕ устройства на корневых портах
 *      (а не одно), поднимает на них HID-клавиатуры (boot
 *      protocol) и HID-мыши (по настоящему Report Descriptor), и
 *      дальше работает "в фоне": опрашивается при каждом чтении
 *      клавиши/состояния мыши. Плюс запасной PS/2-драйвер
 *      клавиатуры (порты 0x60/0x64) - встроенные клавиатуры
 *      ноутбуков почти всегда подключены именно так, а не по USB.
 *
 *   6. "Прокладка" (kbs_*, g_kst) - собственная EFI_SYSTEM_TABLE,
 *      в которой ConIn/ConOut/BootServices указывают на наши
 *      функции. Шелл и GUI написаны поверх st->ConOut/
 *      st->BootServices->Stall/LocateProtocol и т.п. - после
 *      подмены таблицы они продолжают работать БЕЗ ЕДИНОЙ правки,
 *      только теперь под ними наши драйверы, а не прошивка. Это
 *      обычный приём настоящих ОС (слой совместимости), а не
 *      обман: ни одна функция таблицы g_kst не вызывает код
 *      прошивки (кроме RuntimeServices - они по спецификации UEFI
 *      обязаны работать и после выхода, и используются только
 *      для часов реального времени и перезагрузки).
 */


/* Коды ошибок UEFI, которых нет в efi.h - нужны нашим
   собственным реализациям сервисов (верхний бит = ошибка) */
#define K_EFI_ERR(n)             (0x8000000000000000ull | (UINT64)(n))
#define K_EFI_INVALID_PARAMETER  K_EFI_ERR(2)
#define K_EFI_UNSUPPORTED        K_EFI_ERR(3)
#define K_EFI_NOT_READY          K_EFI_ERR(6)
#define K_EFI_OUT_OF_RESOURCES   K_EFI_ERR(9)
#define K_EFI_NOT_FOUND          K_EFI_ERR(14)

/* g_kernel_mode (TRUE после "ebs") объявлен в начале файла */

/*
 * Системная таблица, которой пользуется главный цикл шелла в
 * efi_main. До "ebs" - настоящая таблица прошивки, после - наша
 * g_kst (см. конец блока).
 */
static EFI_SYSTEM_TABLE *g_st = NULL;

/* Framebuffer, закэшированный до ExitBootServices */
static volatile UINT32 *g_kfb = NULL;
static UINT32 g_kfb_w = 0;
static UINT32 g_kfb_h = 0;
static UINT32 g_kfb_stride = 0;
static EFI_GRAPHICS_PIXEL_FORMAT g_kfb_fmt =
    PixelBlueGreenRedReserved8BitPerColor;


/* ================================================================
 * 1. Текстовая консоль в framebuffer
 * ================================================================
 *
 * Шрифт: Spleen 8x16 (только ASCII 32..126), автор Frederic
 * Cambus, https://github.com/fcambus/spleen - лицензия BSD-2
 * (её текст обязателен рядом с данными шрифта, см. ниже).
 * Пиксельный шрифт GUI (gui_font) для текстового шелла не годится
 * - в нём нет строчных букв, а шелл печатает в основном ими.
 *
 * Каждый символ - 16 байт, байт = одна строка из 8 пикселей,
 * старший бит = левый пиксель.
 *
 * ---- Spleen license ----
 * Copyright (c) 2018-2026, Frederic Cambus
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the
 * following conditions are met:
 *
 *   * Redistributions of source code must retain the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ---- end of Spleen license ----
 */
static const UINT8 g_kfont[95][16] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 32 space */
    { 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00 }, /* 33 ! */
    { 0x00, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 34 " */
    { 0x00, 0x00, 0x6C, 0x6C, 0xFE, 0x6C, 0x6C, 0x6C, 0x6C, 0xFE, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00 }, /* 35 # */
    { 0x00, 0x10, 0x7E, 0xD0, 0xD0, 0xD0, 0x7C, 0x16, 0x16, 0x16, 0x16, 0xFC, 0x10, 0x00, 0x00, 0x00 }, /* 36 $ */
    { 0x00, 0x00, 0x06, 0x66, 0x6C, 0x0C, 0x18, 0x18, 0x30, 0x36, 0x66, 0x60, 0x00, 0x00, 0x00, 0x00 }, /* 37 % */
    { 0x00, 0x00, 0x38, 0x6C, 0x6C, 0x6C, 0x38, 0x70, 0xDA, 0xCC, 0xCC, 0x7A, 0x00, 0x00, 0x00, 0x00 }, /* 38 & */
    { 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 39 ' */
    { 0x00, 0x0E, 0x18, 0x30, 0x30, 0x60, 0x60, 0x60, 0x60, 0x30, 0x30, 0x18, 0x0E, 0x00, 0x00, 0x00 }, /* 40 ( */
    { 0x00, 0x70, 0x18, 0x0C, 0x0C, 0x06, 0x06, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x70, 0x00, 0x00, 0x00 }, /* 41 ) */
    { 0x00, 0x00, 0x00, 0x00, 0x66, 0x3C, 0x18, 0xFF, 0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 42 asterisk */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 43 + */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00 }, /* 44 , */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 45 - */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00 }, /* 46 . */
    { 0x00, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0xC0, 0xC0, 0x00, 0x00, 0x00 }, /* 47 slash */
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xCE, 0xDE, 0xF6, 0xE6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00 }, /* 48 0 */
    { 0x00, 0x00, 0x18, 0x38, 0x78, 0x58, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 49 1 */
    { 0x00, 0x00, 0x7C, 0xC6, 0x06, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC6, 0xFE, 0x00, 0x00, 0x00, 0x00 }, /* 50 2 */
    { 0x00, 0x00, 0x7C, 0xC6, 0x06, 0x06, 0x3C, 0x06, 0x06, 0x06, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00 }, /* 51 3 */
    { 0x00, 0x00, 0xC0, 0xC0, 0xCC, 0xCC, 0xCC, 0xCC, 0xFE, 0x0C, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0x00 }, /* 52 4 */
    { 0x00, 0x00, 0xFE, 0xC6, 0xC0, 0xC0, 0xFC, 0x06, 0x06, 0x06, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00 }, /* 53 5 */
    { 0x00, 0x00, 0x7C, 0xC6, 0xC0, 0xC0, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00 }, /* 54 6 */
    { 0x00, 0x00, 0xFE, 0xC6, 0x06, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00 }, /* 55 7 */
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00 }, /* 56 8 */
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0x06, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00 }, /* 57 9 */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00 }, /* 58 : */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00 }, /* 59 ; */
    { 0x00, 0x00, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x00, 0x00, 0x00, 0x00 }, /* 60 < */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 61 = */
    { 0x00, 0x00, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00 }, /* 62 > */
    { 0x00, 0x00, 0x7C, 0xC6, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00 }, /* 63 ? */
    { 0x00, 0x00, 0x00, 0x7C, 0xC2, 0xDA, 0xDA, 0xDA, 0xDA, 0xDE, 0xC0, 0x7C, 0x00, 0x00, 0x00, 0x00 }, /* 64 @ */
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 65 A */
    { 0x00, 0x00, 0xFC, 0xC6, 0xC6, 0xC6, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0xFC, 0x00, 0x00, 0x00, 0x00 }, /* 66 B */
    { 0x00, 0x00, 0x7E, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 67 C */
    { 0x00, 0x00, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xFC, 0x00, 0x00, 0x00, 0x00 }, /* 68 D */
    { 0x00, 0x00, 0x7E, 0xC0, 0xC0, 0xC0, 0xF8, 0xC0, 0xC0, 0xC0, 0xC0, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 69 E */
    { 0x00, 0x00, 0x7E, 0xC0, 0xC0, 0xC0, 0xF8, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00 }, /* 70 F */
    { 0x00, 0x00, 0x7E, 0xC0, 0xC0, 0xC0, 0xDE, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 71 G */
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 72 H */
    { 0x00, 0x00, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 73 I */
    { 0x00, 0x00, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF0, 0x00, 0x00, 0x00, 0x00 }, /* 74 J */
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xCC, 0xF8, 0xCC, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 75 K */
    { 0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 76 L */
    { 0x00, 0x00, 0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 77 M */
    { 0x00, 0x00, 0xC6, 0xC6, 0xE6, 0xE6, 0xD6, 0xD6, 0xCE, 0xCE, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 78 N */
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00 }, /* 79 O */
    { 0x00, 0x00, 0xFC, 0xC6, 0xC6, 0xC6, 0xFC, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00 }, /* 80 P */
    { 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xD6, 0x7C, 0x18, 0x0C, 0x00, 0x00 }, /* 81 Q */
    { 0x00, 0x00, 0xFC, 0xC6, 0xC6, 0xC6, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 82 R */
    { 0x00, 0x00, 0x7E, 0xC0, 0xC0, 0xC0, 0x7C, 0x06, 0x06, 0x06, 0x06, 0xFC, 0x00, 0x00, 0x00, 0x00 }, /* 83 S */
    { 0x00, 0x00, 0xFF, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00 }, /* 84 T */
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 85 U */
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00 }, /* 86 V */
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 87 W */
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 88 X */
    { 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0x06, 0x06, 0x06, 0xFC, 0x00, 0x00, 0x00, 0x00 }, /* 89 Y */
    { 0x00, 0x00, 0xFE, 0x06, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0xC0, 0xFE, 0x00, 0x00, 0x00, 0x00 }, /* 90 Z */
    { 0x00, 0x3E, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3E, 0x00, 0x00, 0x00 }, /* 91 [ */
    { 0x00, 0xC0, 0xC0, 0x60, 0x60, 0x30, 0x30, 0x18, 0x18, 0x0C, 0x0C, 0x06, 0x06, 0x00, 0x00, 0x00 }, /* 92 backslash */
    { 0x00, 0x7C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x7C, 0x00, 0x00, 0x00 }, /* 93 ] */
    { 0x00, 0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 94 ^ */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x00 }, /* 95 _ */
    { 0x00, 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 96 ` */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x06, 0x7E, 0xC6, 0xC6, 0xC6, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 97 a */
    { 0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xFC, 0x00, 0x00, 0x00, 0x00 }, /* 98 b */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 99 c */
    { 0x00, 0x00, 0x06, 0x06, 0x06, 0x7E, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 100 d */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0xC6, 0xC6, 0xFE, 0xC0, 0xC0, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 101 e */
    { 0x00, 0x00, 0x1E, 0x30, 0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00 }, /* 102 f */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x06, 0x06, 0xFC, 0x00 }, /* 103 g */
    { 0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 104 h */
    { 0x00, 0x00, 0x18, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1C, 0x00, 0x00, 0x00, 0x00 }, /* 105 i */
    { 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x70, 0x00 }, /* 106 j */
    { 0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xCC, 0xD8, 0xF0, 0xF0, 0xD8, 0xCC, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 107 k */
    { 0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1C, 0x00, 0x00, 0x00, 0x00 }, /* 108 l */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xEC, 0xD6, 0xD6, 0xD6, 0xD6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 109 m */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 110 n */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00 }, /* 111 o */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xFC, 0xC0, 0xC0, 0xC0, 0x00 }, /* 112 p */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0x06, 0x06, 0x00 }, /* 113 q */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0xC6, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00 }, /* 114 r */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0xC0, 0xC0, 0x7C, 0x06, 0x06, 0xFC, 0x00, 0x00, 0x00, 0x00 }, /* 115 s */
    { 0x00, 0x00, 0x30, 0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1E, 0x00, 0x00, 0x00, 0x00 }, /* 116 t */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* 117 u */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00 }, /* 118 v */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xD6, 0xD6, 0xD6, 0xD6, 0x6E, 0x00, 0x00, 0x00, 0x00 }, /* 119 w */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0x6C, 0x38, 0x38, 0x6C, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00 }, /* 120 x */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0x06, 0xFC, 0x00 }, /* 121 y */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFE, 0x00, 0x00, 0x00, 0x00 }, /* 122 z */
    { 0x00, 0x0E, 0x18, 0x18, 0x18, 0x18, 0x70, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0E, 0x00, 0x00, 0x00 }, /* 123 { */
    { 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00 }, /* 124 | */
    { 0x00, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0E, 0x0E, 0x18, 0x18, 0x18, 0x18, 0x70, 0x00, 0x00, 0x00 }, /* 125 } */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x7E, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 126 ~ */
};

/*
 * Размер "экрана" консоли в символах ограничен сверху, чтобы
 * массивы ниже были статическими (своего malloc на момент
 * инициализации консоли ещё нет - она поднимается САМОЙ ПЕРВОЙ
 * после ExitBootServices, чтобы было куда печатать лог).
 */
#define KCON_MAX_COLS 240
#define KCON_MAX_ROWS 100

/* Что ДОЛЖНО быть на экране (символ + атрибут цвета в формате
   UEFI: биты 0-3 цвет текста, биты 4-6 цвет фона) */
static UINT8 g_kcon_ch[KCON_MAX_ROWS][KCON_MAX_COLS];
static UINT8 g_kcon_at[KCON_MAX_ROWS][KCON_MAX_COLS];

/*
 * Что РЕАЛЬНО нарисовано сейчас ("теневая" копия). Нужна для
 * быстрой прокрутки: вместо перерисовки всех пикселей экрана
 * на каждый перевод строки перерисовываем только те клетки,
 * содержимое которых после сдвига реально поменялось (в тексте
 * много пустого места - это в разы быстрее). Читать сам
 * framebuffer назад для сравнения нельзя: на реальном железе
 * чтение видеопамяти очень медленное.
 */
static UINT8 g_kcon_sch[KCON_MAX_ROWS][KCON_MAX_COLS];
static UINT8 g_kcon_sat[KCON_MAX_ROWS][KCON_MAX_COLS];
static BOOLEAN g_kcon_shadow_valid = FALSE;

static UINTN g_kcon_cols = 0;
static UINTN g_kcon_rows = 0;
static UINTN g_kcon_scale = 1;
static UINTN g_kcon_x0 = 0;
static UINTN g_kcon_y0 = 0;

static UINTN g_kcon_col = 0;
static UINTN g_kcon_row = 0;
static UINT8 g_kcon_attr = 0x07;

static BOOLEAN g_kcon_cursor_on = TRUE;
static UINTN g_kcon_cur_drawn_r = (UINTN)-1;
static UINTN g_kcon_cur_drawn_c = (UINTN)-1;

static UINT32 g_kcon_palette[16];

static SIMPLE_TEXT_OUTPUT_MODE g_kcon_mode;
static SIMPLE_TEXT_OUTPUT_INTERFACE g_kcon_out;


/*
 * Нарисовать одну клетку (r, c) в framebuffer. with_cursor -
 * поверх символа нарисовать курсор (подчёркивание, две нижние
 * строки клетки).
 */
static void kcon_draw_cell(UINTN r, UINTN c, BOOLEAN with_cursor)
{
    if (g_kfb == NULL || r >= g_kcon_rows || c >= g_kcon_cols)
        return;

    UINT8 ch = g_kcon_ch[r][c];
    UINT8 at = g_kcon_at[r][c];

    UINT32 fg = g_kcon_palette[at & 0x0Fu];
    UINT32 bg = g_kcon_palette[(at >> 4) & 0x07u];

    const UINT8 *glyph;

    if (ch >= 32 && ch < 127)
        glyph = g_kfont[ch - 32];
    else
        glyph = g_kfont['?' - 32];

    UINTN s = g_kcon_scale;

    for (UINTN gy = 0; gy < 16; gy++) {

        UINT8 bits = glyph[gy];

        if (with_cursor && gy >= 14)
            bits = 0xFFu;

        for (UINTN sy = 0; sy < s; sy++) {

            UINTN py =
                g_kcon_y0 + (r * 16u + gy) * s + sy;

            volatile UINT32 *dst =
                g_kfb + py * g_kfb_stride +
                g_kcon_x0 + c * 8u * s;

            for (UINTN gx = 0; gx < 8; gx++) {

                UINT32 color =
                    (bits & (0x80u >> gx)) ? fg : bg;

                for (UINTN sx = 0; sx < s; sx++) {
                    *dst = color;
                    dst++;
                }
            }
        }
    }
}


/* Нарисовать клетку и запомнить её в теневой копии */
static void kcon_commit_cell(UINTN r, UINTN c)
{
    kcon_draw_cell(r, c, FALSE);

    g_kcon_sch[r][c] = g_kcon_ch[r][c];
    g_kcon_sat[r][c] = g_kcon_at[r][c];
}


/* Привести экран к g_kcon_ch/at, перерисовав только изменённые
   клетки (или все, если теневая копия недействительна) */
static void kcon_sync(void)
{
    for (UINTN r = 0; r < g_kcon_rows; r++) {

        for (UINTN c = 0; c < g_kcon_cols; c++) {

            if (
                !g_kcon_shadow_valid ||
                g_kcon_sch[r][c] != g_kcon_ch[r][c] ||
                g_kcon_sat[r][c] != g_kcon_at[r][c]
            ) {
                kcon_commit_cell(r, c);
            }
        }
    }

    g_kcon_shadow_valid = TRUE;
}


/*
 * Курсор: стереть его со старой позиции (перерисовать клетку без
 * подчёркивания) и нарисовать на новой. Одна-две клетки, дёшево.
 * Колонка может временно быть == g_kcon_cols ("отложенный
 * перенос", см. kcon_put_char) - тогда курсор рисуется в
 * последней колонке.
 */
static void kcon_update_cursor(void)
{
    UINTN cc = g_kcon_col;

    if (cc >= g_kcon_cols && g_kcon_cols > 0)
        cc = g_kcon_cols - 1;

    if (
        g_kcon_cur_drawn_r != (UINTN)-1 &&
        (g_kcon_cur_drawn_r != g_kcon_row ||
         g_kcon_cur_drawn_c != cc)
    ) {
        kcon_draw_cell(g_kcon_cur_drawn_r, g_kcon_cur_drawn_c, FALSE);
    }

    g_kcon_cur_drawn_r = (UINTN)-1;
    g_kcon_cur_drawn_c = (UINTN)-1;

    if (g_kcon_cursor_on) {

        kcon_draw_cell(g_kcon_row, cc, TRUE);

        g_kcon_cur_drawn_r = g_kcon_row;
        g_kcon_cur_drawn_c = cc;
    }

    g_kcon_mode.CursorColumn = (INTN)cc;
    g_kcon_mode.CursorRow = (INTN)g_kcon_row;
}


/*
 * ОТЛОЖЕННАЯ ОТРИСОВКА. print() шлёт в консоль по одному
 * символу, и если рисовать каждый сразу, то каждый перевод
 * строки внизу экрана (прокрутка) перерисовывал бы почти весь
 * экран - длинный вывод (help, fetch, лог USB) полз бы
 * секундами. Поэтому вывод только меняет массив клеток и ставит
 * флаг "грязно", а в пиксели это переводит kcon_flush():
 *   - когда шелл/GUI начинают ждать ввод или паузу (наши
 *     ReadKeyStroke / GetState / Stall зовут kcon_flush) -
 *     это и есть момент, когда человек смотрит на экран;
 *   - и не реже чем раз в ~30 мс во время длинного вывода,
 *     чтобы был виден прогресс.
 * Десять прокруток подряд между двумя flush стоят как одна.
 */
static BOOLEAN g_kcon_dirty = FALSE;
static UINT64  g_kcon_last_flush = 0;

static void kcon_flush(void)
{
    if (!g_kcon_dirty || g_kfb == NULL)
        return;

    kcon_sync();
    kcon_update_cursor();

    g_kcon_dirty = FALSE;
    g_kcon_last_flush = rdtsc();
}


/* Прокрутка на одну строку вверх (только массив клеток -
   пиксели догонит kcon_flush) */
static void kcon_scroll(void)
{
    for (UINTN r = 1; r < g_kcon_rows; r++) {

        for (UINTN c = 0; c < g_kcon_cols; c++) {

            g_kcon_ch[r - 1][c] = g_kcon_ch[r][c];
            g_kcon_at[r - 1][c] = g_kcon_at[r][c];
        }
    }

    for (UINTN c = 0; c < g_kcon_cols; c++) {

        g_kcon_ch[g_kcon_rows - 1][c] = ' ';
        g_kcon_at[g_kcon_rows - 1][c] = g_kcon_attr;
    }
}


static void kcon_newline(void)
{
    g_kcon_row++;

    if (g_kcon_row >= g_kcon_rows) {

        g_kcon_row = g_kcon_rows - 1;
        kcon_scroll();
    }
}


/*
 * Один символ - та же семантика, что у консоли прошивки:
 * '\r' - в начало строки, '\n' - на строку ниже БЕЗ возврата в
 * начало (поэтому print() всегда шлёт пару "\r\n"), '\b' - на
 * символ влево (стирание делает сам шелл последовательностью
 * "\b \b").
 *
 * Перенос длинной строки - "отложенный", как в терминалах VT100:
 * после символа в последней колонке курсор остаётся за краем, а
 * на новую строку переходит только следующий ПЕЧАТНЫЙ символ.
 * Иначе строка ровно во всю ширину экрана + "\r\n" давала бы
 * лишнюю пустую строку.
 */
static void kcon_put_char(CHAR16 c)
{
    g_kcon_dirty = TRUE;

    if (c == L'\r') {
        g_kcon_col = 0;
        return;
    }

    if (c == L'\n') {
        kcon_newline();
        return;
    }

    if (c == L'\b') {
        if (g_kcon_col >= g_kcon_cols && g_kcon_cols > 0)
            g_kcon_col = g_kcon_cols - 1;
        else if (g_kcon_col > 0)
            g_kcon_col--;
        return;
    }

    if (c == L'\t')
        c = L' ';

    if (c < 32)
        return;

    if (g_kcon_col >= g_kcon_cols) {
        g_kcon_col = 0;
        kcon_newline();
    }

    g_kcon_ch[g_kcon_row][g_kcon_col] =
        (c < 128) ? (UINT8)c : (UINT8)'?';
    g_kcon_at[g_kcon_row][g_kcon_col] = g_kcon_attr;

    g_kcon_col++;
}


static EFI_STATUS EFIAPI kcon_output_string(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    CHAR16 *str
)
{
    (void)this_out;

    if (g_kfb == NULL || str == NULL)
        return EFI_SUCCESS;

    while (*str != 0) {
        kcon_put_char(*str);
        str++;
    }

    /* пока TSC не откалиброван (самое начало лога) - рисуем
       сразу; потом - не чаще раза в ~30 мс */
    if (
        g_tsc_hz == 0 ||
        (rdtsc() - g_kcon_last_flush) > (g_tsc_hz / 1000u) * 30u
    ) {
        kcon_flush();
    }

    return EFI_SUCCESS;
}


static EFI_STATUS EFIAPI kcon_set_attribute(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    UINTN attr
)
{
    (void)this_out;

    g_kcon_attr = (UINT8)(attr & 0x7Fu);
    g_kcon_mode.Attribute = (INTN)g_kcon_attr;

    return EFI_SUCCESS;
}


static EFI_STATUS EFIAPI kcon_clear_screen(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out
)
{
    (void)this_out;

    if (g_kfb == NULL)
        return EFI_SUCCESS;

    for (UINTN r = 0; r < g_kcon_rows; r++) {

        for (UINTN c = 0; c < g_kcon_cols; c++) {

            g_kcon_ch[r][c] = ' ';
            g_kcon_at[r][c] = g_kcon_attr;
        }
    }

    /*
     * Полная перерисовка, а не только изменившихся клеток: сюда
     * мы попадаем, например, после выхода из GUI - GUI рисовал
     * в тот же framebuffer поверх консоли, и теневая копия
     * больше не соответствует реальным пикселям.
     */
    gui_fill_rect(
        g_kfb, g_kfb_stride, g_kfb_w, g_kfb_h,
        0, 0, g_kfb_w, g_kfb_h,
        g_kcon_palette[(g_kcon_attr >> 4) & 0x07u]
    );

    g_kcon_shadow_valid = FALSE;
    g_kcon_cur_drawn_r = (UINTN)-1;
    g_kcon_cur_drawn_c = (UINTN)-1;

    g_kcon_col = 0;
    g_kcon_row = 0;

    g_kcon_dirty = TRUE;
    kcon_flush();

    return EFI_SUCCESS;
}


static EFI_STATUS EFIAPI kcon_reset(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    BOOLEAN extended
)
{
    (void)extended;

    return kcon_clear_screen(this_out);
}


static EFI_STATUS EFIAPI kcon_test_string(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    CHAR16 *str
)
{
    (void)this_out;
    (void)str;

    return EFI_SUCCESS;
}


static EFI_STATUS EFIAPI kcon_query_mode(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    UINTN mode_number,
    UINTN *columns,
    UINTN *rows
)
{
    (void)this_out;

    if (mode_number != 0)
        return K_EFI_UNSUPPORTED;

    if (columns)
        *columns = g_kcon_cols;

    if (rows)
        *rows = g_kcon_rows;

    return EFI_SUCCESS;
}


static EFI_STATUS EFIAPI kcon_set_mode(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    UINTN mode_number
)
{
    if (mode_number != 0)
        return K_EFI_UNSUPPORTED;

    return kcon_clear_screen(this_out);
}


static EFI_STATUS EFIAPI kcon_set_cursor_position(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    UINTN column,
    UINTN row
)
{
    (void)this_out;

    if (column >= g_kcon_cols || row >= g_kcon_rows)
        return K_EFI_UNSUPPORTED;

    g_kcon_col = column;
    g_kcon_row = row;

    g_kcon_dirty = TRUE;
    kcon_flush();

    return EFI_SUCCESS;
}


static EFI_STATUS EFIAPI kcon_enable_cursor(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    BOOLEAN visible
)
{
    (void)this_out;

    g_kcon_cursor_on = visible ? TRUE : FALSE;
    g_kcon_mode.CursorVisible = g_kcon_cursor_on;

    g_kcon_dirty = TRUE;
    kcon_flush();

    return EFI_SUCCESS;
}


/*
 * Поднять консоль поверх уже закэшированного framebuffer
 * (g_kfb и т.п. должны быть заполнены до вызова).
 */
static void kcon_init(void)
{
    /* Стандартная 16-цветная палитра UEFI/VGA по номерам
       EFI_BLACK..EFI_WHITE */
    static const UINT8 pal_rgb[16][3] = {
        {   0,   0,   0 }, {   0,   0, 170 },
        {   0, 170,   0 }, {   0, 170, 170 },
        { 170,   0,   0 }, { 170,   0, 170 },
        { 170,  85,   0 }, { 170, 170, 170 },
        {  85,  85,  85 }, {  85,  85, 255 },
        {  85, 255,  85 }, {  85, 255, 255 },
        { 255,  85,  85 }, { 255,  85, 255 },
        { 255, 255,  85 }, { 255, 255, 255 }
    };

    for (UINTN i = 0; i < 16; i++) {

        g_kcon_palette[i] =
            gui_pack(
                g_kfb_fmt,
                pal_rgb[i][0], pal_rgb[i][1], pal_rgb[i][2]
            );
    }

    /* На очень больших экранах (4K и т.п.) 8x16 - микроскопический
       текст, увеличиваем вдвое */
    g_kcon_scale = (g_kfb_w >= 2560) ? 2 : 1;

    g_kcon_cols = g_kfb_w / (8u * g_kcon_scale);
    g_kcon_rows = g_kfb_h / (16u * g_kcon_scale);

    if (g_kcon_cols > KCON_MAX_COLS)
        g_kcon_cols = KCON_MAX_COLS;

    if (g_kcon_rows > KCON_MAX_ROWS)
        g_kcon_rows = KCON_MAX_ROWS;

    /* остаток по краям делим пополам - текст по центру */
    g_kcon_x0 =
        (g_kfb_w - g_kcon_cols * 8u * g_kcon_scale) / 2u;
    g_kcon_y0 =
        (g_kfb_h - g_kcon_rows * 16u * g_kcon_scale) / 2u;

    g_kcon_attr = 0x07;
    g_kcon_cursor_on = TRUE;

    g_kcon_mode.MaxMode = 1;
    g_kcon_mode.Mode = 0;
    g_kcon_mode.Attribute = g_kcon_attr;
    g_kcon_mode.CursorColumn = 0;
    g_kcon_mode.CursorRow = 0;
    g_kcon_mode.CursorVisible = TRUE;

    g_kcon_out.Reset = kcon_reset;
    g_kcon_out.OutputString = kcon_output_string;
    g_kcon_out.TestString = (VOID *)kcon_test_string;
    g_kcon_out.QueryMode = (VOID *)kcon_query_mode;
    g_kcon_out.SetMode = (VOID *)kcon_set_mode;
    g_kcon_out.SetAttribute = kcon_set_attribute;
    g_kcon_out.ClearScreen = kcon_clear_screen;
    g_kcon_out.SetCursorPosition = (VOID *)kcon_set_cursor_position;
    g_kcon_out.EnableCursor = (VOID *)kcon_enable_cursor;
    g_kcon_out.Mode = &g_kcon_mode;

    kcon_clear_screen(&g_kcon_out);
}


/* Мелкие помощники для печати без print() (экран паники) */
static UINTN kx_hex_str(UINT64 v, UINTN digits, char *buf)
{
    static const char hx[] = "0123456789ABCDEF";

    for (UINTN i = 0; i < digits; i++)
        buf[i] = hx[(v >> ((digits - 1 - i) * 4)) & 0xFu];

    buf[digits] = '\0';

    return digits;
}


/* Прямой вывод строки шрифтом Spleen в пиксели (x, y) - без
   консоли вообще (для экрана паники, где консоли доверять
   уже нельзя) */
static void kx_raw_text(
    UINTN x, UINTN y,
    const char *s,
    UINT32 fg, UINT32 bg
)
{
    while (*s != '\0') {

        UINT8 ch = (UINT8)*s;
        const UINT8 *glyph =
            (ch >= 32 && ch < 127) ? g_kfont[ch - 32]
                                   : g_kfont['?' - 32];

        for (UINTN gy = 0; gy < 16; gy++) {

            if (y + gy >= g_kfb_h)
                break;

            for (UINTN gx = 0; gx < 8; gx++) {

                if (x + gx >= g_kfb_w)
                    break;

                g_kfb[(y + gy) * g_kfb_stride + x + gx] =
                    (glyph[gy] & (0x80u >> gx)) ? fg : bg;
            }
        }

        x += 8;
        s++;
    }
}


/* ================================================================
 * 2. Процессор: порты, MSR, GDT, IDT, исключения
 * ================================================================ */

static inline void io_out8(UINT16 port, UINT8 value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline UINT8 io_in8(UINT16 port)
{
    UINT8 v;

    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));

    return v;
}

/* Короткая пауза после записи в "медленные" старые микросхемы
   (8259 PIC, 8042) - запись в неиспользуемый порт 0x80, так
   делают все ОС (Linux: io_delay) */
static inline void io_wait(void)
{
    io_out8(0x80, 0);
}

static inline UINT64 kx_rdmsr(UINT32 msr)
{
    UINT32 lo, hi;

    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));

    return ((UINT64)hi << 32) | lo;
}

static inline void kx_wrmsr(UINT32 msr, UINT64 v)
{
    __asm__ __volatile__(
        "wrmsr" : : "c"(msr),
        "a"((UINT32)(v & 0xFFFFFFFFu)),
        "d"((UINT32)(v >> 32))
    );
}

static inline UINT64 kx_read_cr2(void)
{
    UINT64 v;

    __asm__ __volatile__("mov %%cr2, %0" : "=r"(v));

    return v;
}

static inline void kx_cli(void)
{
    __asm__ __volatile__("cli" ::: "memory");
}

static inline void kx_sti(void)
{
    __asm__ __volatile__("sti" ::: "memory");
}

static inline void kx_hlt(void)
{
    __asm__ __volatile__("hlt" ::: "memory");
}


/*
 * GDT. После ExitBootServices мы всё ещё сидим на GDT, которую
 * когда-то загрузила прошивка - и она лежит в памяти прошивки
 * (EfiBootServicesData), которая по правилам UEFI теперь
 * принадлежит ОС и может быть переиспользована. Своя GDT в
 * нашем собственном образе - первое, что делает любая ОС.
 *
 * 64-битному режиму нужны всего два дескриптора (сегментация в
 * long mode почти не работает - базы и лимиты игнорируются):
 *   0x08 - код:   L=1 (64-битный), P=1, DPL=0, исполняемый
 *   0x10 - данные: P=1, DPL=0, запись разрешена
 * TSS (нужен для отдельного стека на double fault и для
 * перехода в ring 3) - следующий шаг, пока не заводим.
 */
static UINT64 g_kgdt[3] __attribute__((aligned(16))) = {
    0x0000000000000000ull,
    0x00AF9A000000FFFFull,
    0x00CF92000000FFFFull
};

typedef struct __attribute__((packed)) {
    UINT16 limit;
    UINT64 base;
} KX_DTR;

typedef struct __attribute__((packed)) {
    UINT16 off_lo;
    UINT16 selector;
    UINT8  ist;
    UINT8  type_attr;
    UINT16 off_mid;
    UINT32 off_hi;
    UINT32 zero;
} KX_IDT_ENTRY;

static KX_IDT_ENTRY g_kidt[256] __attribute__((aligned(16)));

#define KX_VEC_TIMER     0x40u
#define KX_VEC_SPURIOUS  0xFFu

static void kx_load_gdt(void)
{
    KX_DTR gdtr;

    gdtr.limit = (UINT16)(sizeof(g_kgdt) - 1);
    gdtr.base = (UINT64)(UINTN)&g_kgdt[0];

    /*
     * lgdt сам по себе не меняет уже загруженные сегментные
     * регистры - их надо перезагрузить. CS нельзя загрузить
     * обычным mov, только дальним переходом/возвратом:
     * кладём в стек новый селектор кода и адрес метки "1:" и
     * делаем lretq - процессор "возвращается" на следующую же
     * инструкцию, но уже с CS=0x08 из нашей GDT.
     */
    __asm__ __volatile__(
        "lgdt %0\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        :
        : "m"(gdtr)
        : "rax", "memory"
    );
}


/*
 * Точки входа прерываний (ISR stubs) - на ассемблере, потому
 * что процессор при прерывании кладёт в стек свою рамку и
 * прыгает по адресу из IDT, а обычная C-функция так вызываться
 * не умеет (не знает, что нужно сохранить ВСЕ регистры и
 * вернуться через iretq).
 *
 * 256 одинаковых кусочков, каждый ровно по 16 байт (.balign 16)
 * - поэтому адрес заглушки вектора N = начало + 16*N, и таблицу
 * адресов не нужно хранить отдельно. Каждый кладёт в стек код
 * ошибки (для векторов, где процессор его НЕ кладёт сам, -
 * фиктивный 0, чтобы рамка всегда была одинаковой) и номер
 * вектора, и прыгает в общий kx_isr_common.
 *
 * kx_isr_common сохраняет все регистры общего назначения и
 * состояние SSE (fxsave, 512 байт) - C-код, который мы
 * вызываем, компилятор волен собирать с SSE-инструкциями, и без
 * этого прерывание таймера посреди, например, копирования
 * памяти в шелле тихо портило бы xmm-регистры прерванного кода.
 */
__asm__(
    ".text\n"
    ".macro KX_ISR_NOERR v\n"
    "  .balign 16\n"
    "  pushq $0\n"
    "  pushq $\\v\n"
    "  jmp kx_isr_common\n"
    ".endm\n"
    ".macro KX_ISR_ERR v\n"
    "  .balign 16\n"
    "  pushq $\\v\n"
    "  jmp kx_isr_common\n"
    ".endm\n"
    ".balign 16\n"
    ".globl kx_isr_stubs\n"
    ".hidden kx_isr_stubs\n"
    "kx_isr_stubs:\n"
    "  KX_ISR_NOERR 0\n"
    "  KX_ISR_NOERR 1\n"
    "  KX_ISR_NOERR 2\n"
    "  KX_ISR_NOERR 3\n"
    "  KX_ISR_NOERR 4\n"
    "  KX_ISR_NOERR 5\n"
    "  KX_ISR_NOERR 6\n"
    "  KX_ISR_NOERR 7\n"
    "  KX_ISR_ERR 8\n"
    "  KX_ISR_NOERR 9\n"
    "  KX_ISR_ERR 10\n"
    "  KX_ISR_ERR 11\n"
    "  KX_ISR_ERR 12\n"
    "  KX_ISR_ERR 13\n"
    "  KX_ISR_ERR 14\n"
    "  KX_ISR_NOERR 15\n"
    "  KX_ISR_NOERR 16\n"
    "  KX_ISR_ERR 17\n"
    "  KX_ISR_NOERR 18\n"
    "  KX_ISR_NOERR 19\n"
    "  KX_ISR_NOERR 20\n"
    "  KX_ISR_ERR 21\n"
    "  KX_ISR_NOERR 22\n"
    "  KX_ISR_NOERR 23\n"
    "  KX_ISR_NOERR 24\n"
    "  KX_ISR_NOERR 25\n"
    "  KX_ISR_NOERR 26\n"
    "  KX_ISR_NOERR 27\n"
    "  KX_ISR_NOERR 28\n"
    "  KX_ISR_ERR 29\n"
    "  KX_ISR_ERR 30\n"
    "  KX_ISR_NOERR 31\n"
    "  KX_ISR_NOERR 32\n"
    "  KX_ISR_NOERR 33\n"
    "  KX_ISR_NOERR 34\n"
    "  KX_ISR_NOERR 35\n"
    "  KX_ISR_NOERR 36\n"
    "  KX_ISR_NOERR 37\n"
    "  KX_ISR_NOERR 38\n"
    "  KX_ISR_NOERR 39\n"
    "  KX_ISR_NOERR 40\n"
    "  KX_ISR_NOERR 41\n"
    "  KX_ISR_NOERR 42\n"
    "  KX_ISR_NOERR 43\n"
    "  KX_ISR_NOERR 44\n"
    "  KX_ISR_NOERR 45\n"
    "  KX_ISR_NOERR 46\n"
    "  KX_ISR_NOERR 47\n"
    "  KX_ISR_NOERR 48\n"
    "  KX_ISR_NOERR 49\n"
    "  KX_ISR_NOERR 50\n"
    "  KX_ISR_NOERR 51\n"
    "  KX_ISR_NOERR 52\n"
    "  KX_ISR_NOERR 53\n"
    "  KX_ISR_NOERR 54\n"
    "  KX_ISR_NOERR 55\n"
    "  KX_ISR_NOERR 56\n"
    "  KX_ISR_NOERR 57\n"
    "  KX_ISR_NOERR 58\n"
    "  KX_ISR_NOERR 59\n"
    "  KX_ISR_NOERR 60\n"
    "  KX_ISR_NOERR 61\n"
    "  KX_ISR_NOERR 62\n"
    "  KX_ISR_NOERR 63\n"
    "  KX_ISR_NOERR 64\n"
    "  KX_ISR_NOERR 65\n"
    "  KX_ISR_NOERR 66\n"
    "  KX_ISR_NOERR 67\n"
    "  KX_ISR_NOERR 68\n"
    "  KX_ISR_NOERR 69\n"
    "  KX_ISR_NOERR 70\n"
    "  KX_ISR_NOERR 71\n"
    "  KX_ISR_NOERR 72\n"
    "  KX_ISR_NOERR 73\n"
    "  KX_ISR_NOERR 74\n"
    "  KX_ISR_NOERR 75\n"
    "  KX_ISR_NOERR 76\n"
    "  KX_ISR_NOERR 77\n"
    "  KX_ISR_NOERR 78\n"
    "  KX_ISR_NOERR 79\n"
    "  KX_ISR_NOERR 80\n"
    "  KX_ISR_NOERR 81\n"
    "  KX_ISR_NOERR 82\n"
    "  KX_ISR_NOERR 83\n"
    "  KX_ISR_NOERR 84\n"
    "  KX_ISR_NOERR 85\n"
    "  KX_ISR_NOERR 86\n"
    "  KX_ISR_NOERR 87\n"
    "  KX_ISR_NOERR 88\n"
    "  KX_ISR_NOERR 89\n"
    "  KX_ISR_NOERR 90\n"
    "  KX_ISR_NOERR 91\n"
    "  KX_ISR_NOERR 92\n"
    "  KX_ISR_NOERR 93\n"
    "  KX_ISR_NOERR 94\n"
    "  KX_ISR_NOERR 95\n"
    "  KX_ISR_NOERR 96\n"
    "  KX_ISR_NOERR 97\n"
    "  KX_ISR_NOERR 98\n"
    "  KX_ISR_NOERR 99\n"
    "  KX_ISR_NOERR 100\n"
    "  KX_ISR_NOERR 101\n"
    "  KX_ISR_NOERR 102\n"
    "  KX_ISR_NOERR 103\n"
    "  KX_ISR_NOERR 104\n"
    "  KX_ISR_NOERR 105\n"
    "  KX_ISR_NOERR 106\n"
    "  KX_ISR_NOERR 107\n"
    "  KX_ISR_NOERR 108\n"
    "  KX_ISR_NOERR 109\n"
    "  KX_ISR_NOERR 110\n"
    "  KX_ISR_NOERR 111\n"
    "  KX_ISR_NOERR 112\n"
    "  KX_ISR_NOERR 113\n"
    "  KX_ISR_NOERR 114\n"
    "  KX_ISR_NOERR 115\n"
    "  KX_ISR_NOERR 116\n"
    "  KX_ISR_NOERR 117\n"
    "  KX_ISR_NOERR 118\n"
    "  KX_ISR_NOERR 119\n"
    "  KX_ISR_NOERR 120\n"
    "  KX_ISR_NOERR 121\n"
    "  KX_ISR_NOERR 122\n"
    "  KX_ISR_NOERR 123\n"
    "  KX_ISR_NOERR 124\n"
    "  KX_ISR_NOERR 125\n"
    "  KX_ISR_NOERR 126\n"
    "  KX_ISR_NOERR 127\n"
    "  KX_ISR_NOERR 128\n"
    "  KX_ISR_NOERR 129\n"
    "  KX_ISR_NOERR 130\n"
    "  KX_ISR_NOERR 131\n"
    "  KX_ISR_NOERR 132\n"
    "  KX_ISR_NOERR 133\n"
    "  KX_ISR_NOERR 134\n"
    "  KX_ISR_NOERR 135\n"
    "  KX_ISR_NOERR 136\n"
    "  KX_ISR_NOERR 137\n"
    "  KX_ISR_NOERR 138\n"
    "  KX_ISR_NOERR 139\n"
    "  KX_ISR_NOERR 140\n"
    "  KX_ISR_NOERR 141\n"
    "  KX_ISR_NOERR 142\n"
    "  KX_ISR_NOERR 143\n"
    "  KX_ISR_NOERR 144\n"
    "  KX_ISR_NOERR 145\n"
    "  KX_ISR_NOERR 146\n"
    "  KX_ISR_NOERR 147\n"
    "  KX_ISR_NOERR 148\n"
    "  KX_ISR_NOERR 149\n"
    "  KX_ISR_NOERR 150\n"
    "  KX_ISR_NOERR 151\n"
    "  KX_ISR_NOERR 152\n"
    "  KX_ISR_NOERR 153\n"
    "  KX_ISR_NOERR 154\n"
    "  KX_ISR_NOERR 155\n"
    "  KX_ISR_NOERR 156\n"
    "  KX_ISR_NOERR 157\n"
    "  KX_ISR_NOERR 158\n"
    "  KX_ISR_NOERR 159\n"
    "  KX_ISR_NOERR 160\n"
    "  KX_ISR_NOERR 161\n"
    "  KX_ISR_NOERR 162\n"
    "  KX_ISR_NOERR 163\n"
    "  KX_ISR_NOERR 164\n"
    "  KX_ISR_NOERR 165\n"
    "  KX_ISR_NOERR 166\n"
    "  KX_ISR_NOERR 167\n"
    "  KX_ISR_NOERR 168\n"
    "  KX_ISR_NOERR 169\n"
    "  KX_ISR_NOERR 170\n"
    "  KX_ISR_NOERR 171\n"
    "  KX_ISR_NOERR 172\n"
    "  KX_ISR_NOERR 173\n"
    "  KX_ISR_NOERR 174\n"
    "  KX_ISR_NOERR 175\n"
    "  KX_ISR_NOERR 176\n"
    "  KX_ISR_NOERR 177\n"
    "  KX_ISR_NOERR 178\n"
    "  KX_ISR_NOERR 179\n"
    "  KX_ISR_NOERR 180\n"
    "  KX_ISR_NOERR 181\n"
    "  KX_ISR_NOERR 182\n"
    "  KX_ISR_NOERR 183\n"
    "  KX_ISR_NOERR 184\n"
    "  KX_ISR_NOERR 185\n"
    "  KX_ISR_NOERR 186\n"
    "  KX_ISR_NOERR 187\n"
    "  KX_ISR_NOERR 188\n"
    "  KX_ISR_NOERR 189\n"
    "  KX_ISR_NOERR 190\n"
    "  KX_ISR_NOERR 191\n"
    "  KX_ISR_NOERR 192\n"
    "  KX_ISR_NOERR 193\n"
    "  KX_ISR_NOERR 194\n"
    "  KX_ISR_NOERR 195\n"
    "  KX_ISR_NOERR 196\n"
    "  KX_ISR_NOERR 197\n"
    "  KX_ISR_NOERR 198\n"
    "  KX_ISR_NOERR 199\n"
    "  KX_ISR_NOERR 200\n"
    "  KX_ISR_NOERR 201\n"
    "  KX_ISR_NOERR 202\n"
    "  KX_ISR_NOERR 203\n"
    "  KX_ISR_NOERR 204\n"
    "  KX_ISR_NOERR 205\n"
    "  KX_ISR_NOERR 206\n"
    "  KX_ISR_NOERR 207\n"
    "  KX_ISR_NOERR 208\n"
    "  KX_ISR_NOERR 209\n"
    "  KX_ISR_NOERR 210\n"
    "  KX_ISR_NOERR 211\n"
    "  KX_ISR_NOERR 212\n"
    "  KX_ISR_NOERR 213\n"
    "  KX_ISR_NOERR 214\n"
    "  KX_ISR_NOERR 215\n"
    "  KX_ISR_NOERR 216\n"
    "  KX_ISR_NOERR 217\n"
    "  KX_ISR_NOERR 218\n"
    "  KX_ISR_NOERR 219\n"
    "  KX_ISR_NOERR 220\n"
    "  KX_ISR_NOERR 221\n"
    "  KX_ISR_NOERR 222\n"
    "  KX_ISR_NOERR 223\n"
    "  KX_ISR_NOERR 224\n"
    "  KX_ISR_NOERR 225\n"
    "  KX_ISR_NOERR 226\n"
    "  KX_ISR_NOERR 227\n"
    "  KX_ISR_NOERR 228\n"
    "  KX_ISR_NOERR 229\n"
    "  KX_ISR_NOERR 230\n"
    "  KX_ISR_NOERR 231\n"
    "  KX_ISR_NOERR 232\n"
    "  KX_ISR_NOERR 233\n"
    "  KX_ISR_NOERR 234\n"
    "  KX_ISR_NOERR 235\n"
    "  KX_ISR_NOERR 236\n"
    "  KX_ISR_NOERR 237\n"
    "  KX_ISR_NOERR 238\n"
    "  KX_ISR_NOERR 239\n"
    "  KX_ISR_NOERR 240\n"
    "  KX_ISR_NOERR 241\n"
    "  KX_ISR_NOERR 242\n"
    "  KX_ISR_NOERR 243\n"
    "  KX_ISR_NOERR 244\n"
    "  KX_ISR_NOERR 245\n"
    "  KX_ISR_NOERR 246\n"
    "  KX_ISR_NOERR 247\n"
    "  KX_ISR_NOERR 248\n"
    "  KX_ISR_NOERR 249\n"
    "  KX_ISR_NOERR 250\n"
    "  KX_ISR_NOERR 251\n"
    "  KX_ISR_NOERR 252\n"
    "  KX_ISR_NOERR 253\n"
    "  KX_ISR_NOERR 254\n"
    "  KX_ISR_NOERR 255\n"
    ".balign 16\n"
    "kx_isr_common:\n"
    "  pushq %rax\n"
    "  pushq %rbx\n"
    "  pushq %rcx\n"
    "  pushq %rdx\n"
    "  pushq %rsi\n"
    "  pushq %rdi\n"
    "  pushq %rbp\n"
    "  pushq %r8\n"
    "  pushq %r9\n"
    "  pushq %r10\n"
    "  pushq %r11\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  movq %rsp, %rbx\n"
    "  subq $512, %rsp\n"
    "  andq $-16, %rsp\n"
    "  fxsave (%rsp)\n"
    "  movq %rbx, %rdi\n"
    "  cld\n"
    "  call kx_isr_dispatch\n"
    "  fxrstor (%rsp)\n"
    "  movq %rbx, %rsp\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %r11\n"
    "  popq %r10\n"
    "  popq %r9\n"
    "  popq %r8\n"
    "  popq %rbp\n"
    "  popq %rdi\n"
    "  popq %rsi\n"
    "  popq %rdx\n"
    "  popq %rcx\n"
    "  popq %rbx\n"
    "  popq %rax\n"
    "  addq $16, %rsp\n"
    "  iretq\n"
);

extern void kx_isr_stubs(void) __attribute__((visibility("hidden")));

/* Рамка стека в момент вызова kx_isr_dispatch - ровно в
   обратном порядке относительно push'ей выше */
typedef struct {
    UINT64 r15, r14, r13, r12, r11, r10, r9, r8;
    UINT64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    UINT64 vector, error;
    UINT64 rip, cs, rflags, rsp, ss;
} KX_ISR_FRAME;

/* Счётчики для команды kinfo */
static volatile UINT64 g_kticks = 0;        /* миллисекунды от старта
                                                таймера */
static volatile UINT64 g_kspurious = 0;
static volatile UINT64 g_kstray = 0;
static volatile UINT64 g_kstray_last = 0;
static volatile UINT64 g_kbreakpoints = 0;
static volatile UINT64 g_kbp_rip = 0;

static void kx_lapic_eoi(void);


static const char *kx_exception_name(UINT64 v)
{
    static const char *names[32] = {
        "#DE Divide Error",
        "#DB Debug",
        "NMI",
        "#BP Breakpoint",
        "#OF Overflow",
        "#BR BOUND Range",
        "#UD Invalid Opcode",
        "#NM Device Not Available",
        "#DF Double Fault",
        "Coprocessor Segment Overrun",
        "#TS Invalid TSS",
        "#NP Segment Not Present",
        "#SS Stack-Segment Fault",
        "#GP General Protection",
        "#PF Page Fault",
        "(reserved 15)",
        "#MF x87 FPU Error",
        "#AC Alignment Check",
        "#MC Machine Check",
        "#XM SIMD Exception",
        "#VE Virtualization",
        "#CP Control Protection",
        "(reserved 22)", "(reserved 23)", "(reserved 24)",
        "(reserved 25)", "(reserved 26)", "(reserved 27)",
        "#HV Hypervisor Injection",
        "#VC VMM Communication",
        "#SX Security",
        "(reserved 31)"
    };

    if (v < 32)
        return names[v];

    return "?";
}


/*
 * Экран "паники" - исключение процессора (деление на ноль,
 * обращение по неверному адресу и т.п.) в нашем коде. Раньше в
 * такой ситуации машина либо зависала, либо молча
 * перезагружалась (triple fault) - теперь видно, ЧТО и ГДЕ
 * случилось: номер исключения, адрес инструкции (RIP), для
 * Page Fault - адрес, к которому обращались (CR2), и регистры.
 * Этого достаточно, чтобы по скриншоту найти ошибку.
 */
static void kx_panic(KX_ISR_FRAME *f)
{
    kx_cli();

    if (g_kfb == NULL) {
        for (;;)
            kx_hlt();
    }

    UINT32 bg = gui_pack(g_kfb_fmt, 120, 0, 0);
    UINT32 fg = gui_pack(g_kfb_fmt, 255, 255, 255);
    UINT32 hl = gui_pack(g_kfb_fmt, 255, 220, 90);

    UINTN bw = 8u * 64u;
    UINTN bh = 16u * 20u;

    if (bw > g_kfb_w)
        bw = g_kfb_w;

    if (bh > g_kfb_h)
        bh = g_kfb_h;

    gui_fill_rect(
        g_kfb, g_kfb_stride, g_kfb_w, g_kfb_h,
        0, 0, bw, bh, bg
    );

    UINTN x = 16;
    UINTN y = 12;

    kx_raw_text(x, y, "*** MyOS KERNEL PANIC: CPU EXCEPTION ***", hl, bg);
    y += 24;

    char line[80];
    char num[20];
    UINTN p;

    /* "Vector N: имя" */
    p = 0;
    {
        const char *a = "Vector 0x";
        for (UINTN i = 0; a[i]; i++) line[p++] = a[i];
        kx_hex_str(f->vector, 2, num);
        for (UINTN i = 0; num[i]; i++) line[p++] = num[i];
        line[p++] = ':';
        line[p++] = ' ';
        const char *n = kx_exception_name(f->vector);
        for (UINTN i = 0; n[i] && p < 78; i++) line[p++] = n[i];
        line[p] = '\0';
    }
    kx_raw_text(x, y, line, fg, bg);
    y += 20;

    /* Пары "ИМЯ = значение" */
    const char *names[] = {
        "ERROR", "RIP  ", "CR2  ", "RSP  ", "RFLAGS",
        "CS   ", "RAX  ", "RBX  ", "RCX  ", "RDX  ",
        "RSI  ", "RDI  ", "RBP  "
    };

    UINT64 vals[13];

    vals[0] = f->error;
    vals[1] = f->rip;
    vals[2] = kx_read_cr2();
    vals[3] = f->rsp;
    vals[4] = f->rflags;
    vals[5] = f->cs;
    vals[6] = f->rax;
    vals[7] = f->rbx;
    vals[8] = f->rcx;
    vals[9] = f->rdx;
    vals[10] = f->rsi;
    vals[11] = f->rdi;
    vals[12] = f->rbp;

    for (UINTN k = 0; k < 13; k++) {

        p = 0;

        for (UINTN i = 0; names[k][i]; i++)
            line[p++] = names[k][i];

        line[p++] = ' ';
        line[p++] = '=';
        line[p++] = ' ';
        line[p++] = '0';
        line[p++] = 'x';

        kx_hex_str(vals[k], 16, num);

        for (UINTN i = 0; num[i]; i++)
            line[p++] = num[i];

        line[p] = '\0';

        kx_raw_text(x, y, line, fg, bg);
        y += 16;
    }

    y += 8;
    kx_raw_text(
        x, y,
        "System halted. Take a screenshot, then reset the machine.",
        hl, bg
    );

    for (;;)
        kx_hlt();
}


/*
 * Единая C-точка входа всех прерываний (вызывается из
 * kx_isr_common). Не static и с "used", потому что вызывается
 * только из ассемблера - иначе компилятор решил бы, что она
 * никому не нужна, и выбросил бы её.
 */
__attribute__((used, visibility("hidden")))
void kx_isr_dispatch(KX_ISR_FRAME *f)
{
    UINT64 v = f->vector;

    if (v == KX_VEC_TIMER) {

        g_kticks = g_kticks + 1;
        kx_lapic_eoi();
        return;
    }

    if (v == KX_VEC_SPURIOUS) {

        /* "ложное" прерывание Local APIC - по спеке на него
           EOI не посылается */
        g_kspurious = g_kspurious + 1;
        return;
    }

    if (v == 3) {

        /*
         * #BP (инструкция int3) - "ловушка": процессор кладёт в
         * RIP адрес СЛЕДУЮЩЕЙ инструкции, поэтому можно просто
         * вернуться и продолжить. Используется командой "int3"
         * как безопасная живая проверка, что наша IDT реально
         * работает.
         */
        g_kbreakpoints = g_kbreakpoints + 1;
        g_kbp_rip = f->rip;
        return;
    }

    if (v < 32) {

        kx_panic(f);
        return;
    }

    /* Неожиданное внешнее прерывание (все источники, о которых
       мы знаем, замаскированы - но на всякий случай не
       зависаем, а считаем и подтверждаем) */
    g_kstray = g_kstray + 1;
    g_kstray_last = v;
    kx_lapic_eoi();
}


static void kx_idt_set(UINTN vec, UINT64 handler)
{
    g_kidt[vec].off_lo = (UINT16)(handler & 0xFFFFu);
    g_kidt[vec].selector = 0x08;
    g_kidt[vec].ist = 0;
    /* 0x8E: P=1, DPL=0, тип 0xE = 64-битный interrupt gate
       (процессор сам сбрасывает IF при входе - обработчик не
       прерывается следующим прерыванием) */
    g_kidt[vec].type_attr = 0x8E;
    g_kidt[vec].off_mid = (UINT16)((handler >> 16) & 0xFFFFu);
    g_kidt[vec].off_hi = (UINT32)(handler >> 32);
    g_kidt[vec].zero = 0;
}


static void kx_load_idt(void)
{
    UINT64 base = (UINT64)(UINTN)kx_isr_stubs;

    for (UINTN v = 0; v < 256; v++)
        kx_idt_set(v, base + (UINT64)v * 16u);

    KX_DTR idtr;

    idtr.limit = (UINT16)(sizeof(g_kidt) - 1);
    idtr.base = (UINT64)(UINTN)&g_kidt[0];

    __asm__ __volatile__("lidt %0" : : "m"(idtr) : "memory");
}


/*
 * Старый контроллер прерываний 8259 (PIC). Мы им не
 * пользуемся (таймер - Local APIC), но он физически есть
 * (или эмулируется чипсетом), и после включения прерываний
 * мог бы прислать что-нибудь. По умолчанию его векторы 0x08-
 * 0x0F совпадают с векторами ИСКЛЮЧЕНИЙ процессора (например,
 * 0x08 = Double Fault) - поэтому даже замаскированный PIC
 * положено сначала перенастроить на безопасные 0x20-0x2F
 * (классическая последовательность ICW1-ICW4), и только потом
 * замаскировать все его линии.
 */
static void kx_pic_disable(void)
{
    io_out8(0x20, 0x11); io_wait();   /* ICW1: init + ICW4 */
    io_out8(0xA0, 0x11); io_wait();
    io_out8(0x21, 0x20); io_wait();   /* ICW2: master -> 0x20 */
    io_out8(0xA1, 0x28); io_wait();   /* ICW2: slave  -> 0x28 */
    io_out8(0x21, 0x04); io_wait();   /* ICW3: slave на IRQ2 */
    io_out8(0xA1, 0x02); io_wait();
    io_out8(0x21, 0x01); io_wait();   /* ICW4: режим 8086 */
    io_out8(0xA1, 0x01); io_wait();
    io_out8(0x21, 0xFF);              /* замаскировать все */
    io_out8(0xA1, 0xFF);
}


/*
 * I/O APIC - современный "распределитель" внешних прерываний
 * (от чипсета, ACPI, HPET и т.п.). Прошивка могла оставить в
 * нём включённые записи, нацеленные на свои векторы, - после
 * включения прерываний они прилетали бы к нам, а уровневые (как
 * ACPI SCI) прилетали бы бесконечно, пока их не обслужат.
 * Маскируем все записи. Стандартный адрес первого I/O APIC на
 * PC - 0xFEC00000 (строго говоря, его надо брать из ACPI-
 * таблицы MADT - это следующий шаг, вместе с разбором ACPI).
 * Возвращает число замаскированных записей, 0 если I/O APIC
 * по этому адресу не отвечает.
 */
static UINTN kx_ioapic_mask_all(void)
{
    UINT64 base = 0xFEC00000ull;

    mmio_write32(base + 0x00, 0x01);   /* IOREGSEL = версия */

    UINT32 ver = mmio_read32(base + 0x10);

    if (ver == 0xFFFFFFFFu)
        return 0;

    UINTN max_entry = (ver >> 16) & 0xFFu;

    if (max_entry > 239)
        return 0;

    for (UINTN i = 0; i <= max_entry; i++) {

        mmio_write32(base + 0x00, (UINT32)(0x10u + 2u * i));

        UINT32 lo = mmio_read32(base + 0x10);

        mmio_write32(base + 0x10, lo | (1u << 16));
    }

    return max_entry + 1;
}


/* ================================================================
 * 3. Время: TSC + PIT (калибровка) + Local APIC timer
 * ================================================================ */

static UINT64 g_tsc_hz_stall = 0;   /* замер через Stall прошивки
                                        (ещё до ExitBootServices) */
static UINT64 g_tsc_hz_pit = 0;     /* замер через PIT - уже без
                                        прошивки */
static const char *g_tsc_source = "none";

static BOOLEAN g_lapic_x2 = FALSE;
static UINT64  g_lapic_base = 0;
static UINT64  g_lapic_hz = 0;      /* частота счётчика LAPIC
                                        timer (после делителя) */
static BOOLEAN g_ktimer_ok = FALSE; /* прерывания таймера реально
                                        приходят */
static UINT64  g_kboot_tsc = 0;     /* TSC в момент перехода в
                                        kernel mode */


/*
 * Замер частоты TSC по PIT. PIT (Intel 8254) тикает с
 * фиксированной, известной с 1981 года частотой 1193182 Гц -
 * это и делает его эталоном. Канал 2 (исторически - для
 * PC-спикера) можно использовать без прерываний: заряжаем его
 * на N тиков в режиме 0 ("прерывание по окончании счёта") и
 * смотрим бит 5 порта 0x61 - это выход канала 2, он становится
 * 1, когда счёт дошёл до нуля. Параллельно читаем TSC до и
 * после - получаем, сколько тактов TSC уложилось в N/1193182
 * секунды. Тот же приём использует Linux (pit_calibrate_tsc).
 *
 * Возвращает 0, если PIT не отвечает (на некоторых новых
 * платформах прошивка отключает тактирование 8254 ради
 * экономии энергии).
 */
static UINT64 kx_pit_measure_tsc_hz(void)
{
    const UINT32 pit_hz = 1193182u;
    const UINT32 latch = 11932u;        /* ~10 мс */

    UINT64 best = 0;

    for (UINTN attempt = 0; attempt < 3; attempt++) {

        /* бит0 порта 0x61 = gate канала 2 (включаем),
           бит1 = сам динамик (выключаем, чтобы не пищал) */
        UINT8 p61 = io_in8(0x61);

        io_out8(0x61, (UINT8)((p61 & ~0x02u) | 0x01u));

        /* 0xB0 = канал 2, запись lo+hi байта, режим 0, двоичный */
        io_out8(0x43, 0xB0);
        io_out8(0x42, (UINT8)(latch & 0xFFu));
        io_out8(0x42, (UINT8)(latch >> 8));

        UINT64 t0 = rdtsc();
        UINT64 spins = 0;
        BOOLEAN done = FALSE;

        /*
         * Страховка на случай "мёртвого" PIT (на новых платформах
         * Intel прошивка может отключить его тактирование): ждём
         * не дольше ~100 мс по контрольному замеру через Stall.
         * Ограничивать просто числом итераций нельзя - каждое
         * чтение порта на реальном железе стоит около микросекунды,
         * и "200 миллионов попыток" растянулись бы на минуты.
         */
        UINT64 max_cycles =
            (g_tsc_hz_stall != 0) ? (g_tsc_hz_stall / 10u)
                                  : 400000000ull;

        for (;;) {

            if (io_in8(0x61) & 0x20u) {
                done = TRUE;
                break;
            }

            spins++;

            if ((spins & 0xFFu) == 0 && (rdtsc() - t0) > max_cycles)
                break;
        }

        UINT64 t1 = rdtsc();

        if (!done)
            return 0;

        UINT64 cycles = t1 - t0;
        UINT64 hz = (cycles * (UINT64)pit_hz) / (UINT64)latch;

        /* берём минимальный замер - любые задержки (например,
           SMI прошивки посреди замера) только увеличивают
           результат, никогда не уменьшают */
        if (best == 0 || hz < best)
            best = hz;
    }

    return best;
}


static void kx_lapic_write(UINT32 reg, UINT32 v)
{
    if (g_lapic_x2)
        kx_wrmsr(0x800u + (reg >> 4), v);
    else
        mmio_write32(g_lapic_base + reg, v);
}

static UINT32 kx_lapic_read(UINT32 reg)
{
    if (g_lapic_x2)
        return (UINT32)kx_rdmsr(0x800u + (reg >> 4));

    return mmio_read32(g_lapic_base + reg);
}

static void kx_lapic_eoi(void)
{
    if (g_lapic_base == 0 && !g_lapic_x2)
        return;

    kx_lapic_write(0xB0, 0);
}


/*
 * Local APIC - встроенный в каждое ядро процессора контроллер
 * прерываний; в нём есть свой таймер, который мы и используем
 * как системный "тик" (1000 раз в секунду). Регистры:
 *   0x0F0 SVR  - включение APIC + вектор "ложного" прерывания
 *   0x080 TPR  - порог приоритета (0 = принимать всё)
 *   0x320 LVT Timer - вектор таймера + режим (бит17=периодический)
 *                     + маска (бит16)
 *   0x3E0 делитель, 0x380 начальный счёт, 0x390 текущий счёт
 *   0x0B0 EOI  - "прерывание обработано"
 * Доступ - либо как к памяти (xAPIC, адрес из MSR 0x1B), либо
 * через MSR 0x800+ (режим x2APIC - его прошивка могла включить
 * на новых машинах, тогда MMIO-окно уже не работает).
 */
static BOOLEAN kx_lapic_timer_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT64 apic_msr = kx_rdmsr(0x1B);

    if (!(apic_msr & (1ull << 11))) {

        /* APIC глобально выключен - включаем */
        apic_msr |= (1ull << 11);
        kx_wrmsr(0x1B, apic_msr);
    }

    g_lapic_x2 = (apic_msr & (1ull << 10)) ? TRUE : FALSE;
    g_lapic_base = apic_msr & 0x000FFFFFFFFFF000ull;

    print(out, "  Local APIC: ");
    print(out, g_lapic_x2 ? "x2APIC mode (MSR access)" :
                            "xAPIC mode, MMIO at 0x");

    if (!g_lapic_x2)
        print_hex(out, g_lapic_base, 8);

    print(out, "\n");

    kx_lapic_write(0x0F0, 0x100u | KX_VEC_SPURIOUS);
    kx_lapic_write(0x080, 0);

    /* Калибровка: разовый счёт с маской, 50 мс по TSC (чем
       длиннее окно, тем меньше влияет случайная задержка в
       начале/конце замера) */
    kx_lapic_write(0x3E0, 0x3);                 /* делитель 16 */
    kx_lapic_write(0x320, (1u << 16) | KX_VEC_TIMER);
    kx_lapic_write(0x380, 0xFFFFFFFFu);

    tsc_delay_us(50000);

    UINT32 left = kx_lapic_read(0x390);

    kx_lapic_write(0x380, 0);

    UINT64 ticks = 0xFFFFFFFFull - (UINT64)left;

    g_lapic_hz = ticks * 20u;

    print(out, "  LAPIC timer: ");
    print_uint(out, g_lapic_hz / 1000u);
    print(out, " kHz after divide-by-16\n");

    if (g_lapic_hz < 1000u) {

        print(out, "  LAPIC timer does not count - no timer interrupts.\n");
        return FALSE;
    }

    /* Периодический режим, 1000 Гц */
    kx_lapic_write(0x320, (1u << 17) | KX_VEC_TIMER);
    kx_lapic_write(0x380, (UINT32)(g_lapic_hz / 1000u));

    return TRUE;
}


/* Микросекунды с момента перехода в kernel mode (по TSC) */
static UINT64 kx_uptime_us(void)
{
    if (g_tsc_hz == 0)
        return 0;

    UINT64 d = rdtsc() - g_kboot_tsc;

    /* без 128-битной арифметики (её поддержка - функции
       libgcc, которых у нас нет): целая часть секунд отдельно,
       остаток отдельно - так ничего не переполняется */
    return (d / g_tsc_hz) * 1000000ull +
           ((d % g_tsc_hz) * 1000000ull) / g_tsc_hz;
}


/*
 * Пауза. Если таймер реально тикает - процессор между тиками
 * спит на инструкции hlt (просыпается от каждого прерывания,
 * раз в миллисекунду), а не молотит впустую: в QEMU это видно
 * по загрузке CPU хоста. Точность всё равно по TSC.
 */
static void kx_sleep_us(UINT64 us)
{
    if (g_tsc_hz == 0) {
        busy_wait_ms((UINTN)((us + 999u) / 1000u));
        return;
    }

    UINT64 start = rdtsc();
    UINT64 cycles = (us * g_tsc_hz) / 1000000ull;

    while ((rdtsc() - start) < cycles) {

        if (g_ktimer_ok && us >= 2000u)
            kx_hlt();
        else
            cpu_pause();
    }
}



/* ================================================================
 * 4. Физическая память: карта от GetMemoryMap + битовая карта
 * ================================================================
 *
 * GetMemoryMap отдаёт список регионов физической памяти с типом
 * у каждого. После ExitBootServices ОС может свободно
 * пользоваться регионами типа EfiConventionalMemory (7) -
 * это просто свободная RAM. Остальные типы пока НЕ трогаем:
 *   - EfiLoaderCode/Data (1/2) - наш собственный образ и то,
 *     что мы сами выделяли до выхода;
 *   - EfiBootServicesCode/Data (3/4) - формально тоже наши после
 *     выхода, НО там до сих пор лежат таблицы страниц прошивки
 *     (по которым процессор прямо сейчас переводит адреса) и
 *     стек, на котором мы работаем. Забрать их можно будет
 *     только после того, как ОС построит свои таблицы страниц и
 *     свой стек - следующий шаг;
 *   - Runtime Services Code/Data (5/6) - нельзя никогда, это код
 *     часов/перезагрузки, который мы продолжаем вызывать;
 *   - ACPI, MMIO, Reserved - не RAM или чужая.
 *
 * Битовая карта: 1 бит на страницу 4 КиБ, 1 = занята. Сама
 * карта размещается в первом подходящем свободном регионе (и её
 * страницы помечаются занятыми). Первый мегабайт не выдаём
 * никогда (историческая "грязная" зона - BIOS-данные, будущий
 * trampoline для запуска остальных ядер процессора и т.п.).
 */

#define KMM_MAX_REGIONS 512
#define KMM_PAGE        4096ull

typedef struct {
    UINT32 type;
    UINT64 phys;
    UINT64 pages;
} KMM_REGION;

static KMM_REGION g_kmm_map[KMM_MAX_REGIONS];
static UINTN  g_kmm_map_count = 0;
static UINTN  g_kmm_map_dropped = 0;

static UINT64 *g_kmm_bitmap = NULL;
static UINT64 g_kmm_bitmap_phys = 0;
static UINT64 g_kmm_bitmap_pages = 0;
static UINT64 g_kmm_total_pages = 0;   /* сколько страниц покрывает
                                           битовая карта */
static UINT64 g_kmm_usable_pages = 0;  /* сколько из них было
                                           свободной RAM */
static UINT64 g_kmm_free_pages = 0;
static BOOLEAN g_kmm_ready = FALSE;

/* Смещения полей в EFI_MEMORY_DESCRIPTOR (в efi.h его нет) -
   читаем по байтовым смещениям, т.к. реальный размер записи
   (DescriptorSize) прошивка может сделать больше структуры */
#define KMM_DESC_TYPE   0
#define KMM_DESC_PHYS   8
#define KMM_DESC_PAGES  24


/* Скопировать итоговую карту памяти из буфера прошивки в наш
   статический массив (сразу после ExitBootServices) */
static void pmm_save_map(
    VOID *map_buf,
    UINTN map_size,
    UINTN desc_size
)
{
    g_kmm_map_count = 0;
    g_kmm_map_dropped = 0;

    if (map_buf == NULL || desc_size == 0)
        return;

    UINTN n = map_size / desc_size;

    for (UINTN i = 0; i < n; i++) {

        UINT8 *d = (UINT8 *)map_buf + i * desc_size;

        if (g_kmm_map_count >= KMM_MAX_REGIONS) {
            g_kmm_map_dropped++;
            continue;
        }

        g_kmm_map[g_kmm_map_count].type =
            *(UINT32 *)(d + KMM_DESC_TYPE);
        g_kmm_map[g_kmm_map_count].phys =
            *(UINT64 *)(d + KMM_DESC_PHYS);
        g_kmm_map[g_kmm_map_count].pages =
            *(UINT64 *)(d + KMM_DESC_PAGES);

        g_kmm_map_count++;
    }
}


static inline BOOLEAN pmm_test(UINT64 page)
{
    return (g_kmm_bitmap[page >> 6] >> (page & 63u)) & 1u;
}

static inline void pmm_set(UINT64 page)
{
    g_kmm_bitmap[page >> 6] |= (1ull << (page & 63u));
}

static inline void pmm_clear(UINT64 page)
{
    g_kmm_bitmap[page >> 6] &= ~(1ull << (page & 63u));
}


static BOOLEAN pmm_init(void)
{
    UINT64 max_end = 0;

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        if (g_kmm_map[i].type != 7)
            continue;

        UINT64 end =
            g_kmm_map[i].phys + g_kmm_map[i].pages * KMM_PAGE;

        if (end > max_end)
            max_end = end;
    }

    if (max_end == 0)
        return FALSE;

    g_kmm_total_pages = max_end / KMM_PAGE;

    UINT64 words = (g_kmm_total_pages + 63u) / 64u;
    UINT64 bytes = words * 8u;

    g_kmm_bitmap_pages = (bytes + KMM_PAGE - 1u) / KMM_PAGE;

    /* Где разместить саму битовую карту: первый свободный
       регион выше 1 МиБ, в который она целиком влезает */
    g_kmm_bitmap_phys = 0;

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        if (g_kmm_map[i].type != 7)
            continue;

        UINT64 start = g_kmm_map[i].phys;
        UINT64 end = start + g_kmm_map[i].pages * KMM_PAGE;

        if (start < 0x100000ull)
            start = 0x100000ull;

        if (start >= end)
            continue;

        if ((end - start) / KMM_PAGE >= g_kmm_bitmap_pages) {
            g_kmm_bitmap_phys = start;
            break;
        }
    }

    if (g_kmm_bitmap_phys == 0)
        return FALSE;

    g_kmm_bitmap = (UINT64 *)(UINTN)g_kmm_bitmap_phys;

    /* всё занято... */
    for (UINT64 w = 0; w < words; w++)
        g_kmm_bitmap[w] = ~0ull;

    /* ...кроме свободной RAM (EfiConventionalMemory) */
    g_kmm_usable_pages = 0;

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        if (g_kmm_map[i].type != 7)
            continue;

        UINT64 first = g_kmm_map[i].phys / KMM_PAGE;
        UINT64 count = g_kmm_map[i].pages;

        for (UINT64 p = first; p < first + count; p++) {

            if (p < 256u)       /* первый мегабайт не выдаём */
                continue;

            if (p >= g_kmm_total_pages)
                break;

            if (pmm_test(p)) {
                pmm_clear(p);
                g_kmm_usable_pages++;
            }
        }
    }

    /* страницы самой битовой карты - заняты */
    UINT64 bm_first = g_kmm_bitmap_phys / KMM_PAGE;

    for (UINT64 p = bm_first; p < bm_first + g_kmm_bitmap_pages; p++)
        pmm_set(p);

    g_kmm_free_pages = g_kmm_usable_pages - g_kmm_bitmap_pages;
    g_kmm_ready = TRUE;

    return TRUE;
}


/*
 * Выделить count подряд идущих страниц. limit - верхняя граница
 * физического адреса (0 = без ограничения): например, xHCI без
 * поддержки 64-битной адресации (бит AC64) может обращаться
 * только к памяти ниже 4 ГиБ. Первый подходящий участок (first
 * fit), полностью занятые 64-страничные слова пропускаются
 * целиком. Возвращает физический адрес или 0.
 */
static UINT64 pmm_alloc_pages(UINT64 count, UINT64 limit)
{
    if (!g_kmm_ready || count == 0 || count > g_kmm_free_pages)
        return 0;

    UINT64 limit_pages = g_kmm_total_pages;

    if (limit != 0 && limit / KMM_PAGE < limit_pages)
        limit_pages = limit / KMM_PAGE;

    UINT64 run = 0;
    UINT64 run_start = 0;

    for (UINT64 p = 256; p < limit_pages; p++) {

        if (
            (p & 63u) == 0 &&
            run == 0 &&
            g_kmm_bitmap[p >> 6] == ~0ull
        ) {
            p += 63;
            continue;
        }

        if (pmm_test(p)) {
            run = 0;
            continue;
        }

        if (run == 0)
            run_start = p;

        run++;

        if (run == count) {

            for (UINT64 q = run_start; q < run_start + count; q++)
                pmm_set(q);

            g_kmm_free_pages -= count;

            return run_start * KMM_PAGE;
        }
    }

    return 0;
}


static void pmm_free_pages(UINT64 phys, UINT64 count)
{
    if (!g_kmm_ready)
        return;

    UINT64 first = phys / KMM_PAGE;

    for (UINT64 p = first; p < first + count; p++) {

        if (p < 256u || p >= g_kmm_total_pages)
            continue;

        if (pmm_test(p)) {
            pmm_clear(p);
            g_kmm_free_pages++;
        }
    }
}


/* То же + обнулить (для DMA-структур xHCI это обязательно:
   контроллер трактует мусор в Cycle-битах как настоящие TRB) */
static UINT64 pmm_alloc_zeroed(UINT64 count, UINT64 limit)
{
    UINT64 phys = pmm_alloc_pages(count, limit);

    if (phys == 0)
        return 0;

    volatile UINT64 *q = (volatile UINT64 *)(UINTN)phys;
    UINT64 n = count * KMM_PAGE / 8u;

    for (UINT64 i = 0; i < n; i++)
        q[i] = 0;

    return phys;
}


/*
 * "Пул" - то, что прошивка отдавала через AllocatePool
 * (произвольный размер). Самый простой честный вариант поверх
 * страничного аллокатора: каждая аллокация - отдельный кусок
 * целых страниц, в первых 16 байтах - заголовок (метка +
 * число страниц), чтобы FreePool знал, сколько возвращать.
 * Расточительно для мелких кусков, но шелл и GUI просят пул
 * редко и крупно (например, задний буфер кадра GUI - мегабайты).
 * Настоящий heap с мелкими блоками - отдельный будущий шаг.
 */
#define KPOOL_MAGIC 0x4C4F4F50534F594Dull   /* "MYOSPOOL" */

static UINT64 g_kpool_allocs = 0;

static VOID *kpool_alloc(UINTN size)
{
    UINT64 pages = ((UINT64)size + 16u + KMM_PAGE - 1u) / KMM_PAGE;
    UINT64 phys = pmm_alloc_pages(pages, 0);

    if (phys == 0)
        return NULL;

    UINT64 *hdr = (UINT64 *)(UINTN)phys;

    hdr[0] = KPOOL_MAGIC;
    hdr[1] = pages;

    g_kpool_allocs++;

    return (VOID *)(UINTN)(phys + 16u);
}

static BOOLEAN kpool_free(VOID *ptr)
{
    if (ptr == NULL)
        return FALSE;

    UINT64 *hdr = (UINT64 *)((UINT8 *)ptr - 16);

    if (hdr[0] != KPOOL_MAGIC)
        return FALSE;

    UINT64 pages = hdr[1];

    hdr[0] = 0;

    pmm_free_pages((UINT64)(UINTN)hdr, pages);

    if (g_kpool_allocs > 0)
        g_kpool_allocs--;

    return TRUE;
}


static const char *kmm_type_name(UINT32 t)
{
    switch (t) {
    case 0:  return "Reserved";
    case 1:  return "LoaderCode";
    case 2:  return "LoaderData";
    case 3:  return "BootServicesCode";
    case 4:  return "BootServicesData";
    case 5:  return "RuntimeServicesCode";
    case 6:  return "RuntimeServicesData";
    case 7:  return "Conventional (free RAM)";
    case 8:  return "Unusable";
    case 9:  return "ACPIReclaim";
    case 10: return "ACPINVS";
    case 11: return "MMIO";
    case 12: return "MMIOPortSpace";
    case 13: return "PalCode";
    case 14: return "Persistent";
    default: return "Other";
    }
}


/* ================================================================
 * 5a. Клавиатура: очередь клавиш + перевод HID Usage -> EFI key
 * ================================================================
 *
 * И USB-клавиатура (boot protocol), и PS/2 сводятся к одному и
 * тому же: "нажата клавиша с кодом HID Usage X" (PS/2-коды
 * переводятся в HID Usage таблицей ниже). Дальше - один общий
 * код превращает Usage + Shift/CapsLock в EFI_INPUT_KEY (тот
 * самый формат, который шелл и GUI получали от ConIn прошивки:
 * UnicodeChar для обычных символов, ScanCode для стрелок/F1/Esc
 * и т.п.) и кладёт в очередь. ConIn->ReadKeyStroke (наш, см.
 * kbs_*) просто достаёт из этой очереди.
 *
 * Раскладка - US QWERTY (как и у консоли прошивки по умолчанию).
 */

#define KBD_QUEUE_SIZE 64

static EFI_INPUT_KEY g_kbd_queue[KBD_QUEUE_SIZE];
static UINTN g_kbd_q_head = 0;   /* откуда читать */
static UINTN g_kbd_q_tail = 0;   /* куда писать */

static BOOLEAN g_kbd_caps = FALSE;
static UINT8   g_kbd_usb_mods = 0;   /* байт модификаторов из
                                         последнего USB-отчёта */
static UINT8   g_kbd_ps2_mods = 0;   /* то же, собранное из PS/2
                                         make/break-кодов, в том же
                                         формате битов */
static UINT64  g_kbd_keys_total = 0;

/* Автоповтор (typematic): у USB-клавиатуры его нет в "железе",
   его обязан делать сам хост. У PS/2 - есть, там не нужен. */
static UINT8  g_kbd_rep_usage = 0;
static UINT64 g_kbd_rep_next_tsc = 0;

#define KBD_REPEAT_DELAY_MS 500u
#define KBD_REPEAT_RATE_MS  33u


static void kbd_enqueue(UINT16 scan, CHAR16 uc)
{
    UINTN next = (g_kbd_q_tail + 1u) % KBD_QUEUE_SIZE;

    if (next == g_kbd_q_head)
        return;              /* очередь полна - клавишу теряем */

    g_kbd_queue[g_kbd_q_tail].ScanCode = scan;
    g_kbd_queue[g_kbd_q_tail].UnicodeChar = uc;
    g_kbd_q_tail = next;

    g_kbd_keys_total++;
}

static BOOLEAN kbd_dequeue(EFI_INPUT_KEY *key)
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
static const char g_kbd_sym[0x39 - 0x1E][2] = {
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
static BOOLEAN kbd_shift_down(void)
{
    UINT8 m = (UINT8)(g_kbd_usb_mods | g_kbd_ps2_mods);

    return (m & 0x22u) != 0;
}


/* Главное: одно нажатие клавиши (HID Usage) -> EFI_INPUT_KEY
   в очередь */
static void kbd_press_usage(UINT8 u)
{
    BOOLEAN shift = kbd_shift_down();

    if (u >= 0x04 && u <= 0x1D) {

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

    /* цифровой блок (считаем, что NumLock включён) */
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
static BOOLEAN kbd_usage_repeats(UINT8 u)
{
    if (u == 0x39)                 /* Caps Lock */
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
static void kbd_usb_boot_report(
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


static void kbd_repeat_tick(void)
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

static BOOLEAN g_ps2_present = FALSE;
static BOOLEAN g_ps2_e0 = FALSE;
static UINTN   g_ps2_skip = 0;       /* сколько байт Pause/Break
                                         ещё пропустить */
static UINT64  g_ps2_bytes = 0;
static UINT8   g_ps2_config = 0;

/* Set 1 (без префикса) -> HID Usage. 0 = нет/не нужна */
static const UINT8 g_ps2_to_hid[0x59] = {
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
static UINT8 ps2_e0_to_hid(UINT8 code)
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


static void ps2_handle_byte(UINT8 b)
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


static BOOLEAN ps2_wait_input_empty(void)
{
    for (UINTN i = 0; i < 100000; i++) {
        if (!(io_in8(0x64) & 0x02u))
            return TRUE;
        cpu_pause();
    }
    return FALSE;
}

static BOOLEAN ps2_wait_output_full(void)
{
    for (UINTN i = 0; i < 100000; i++) {
        if (io_in8(0x64) & 0x01u)
            return TRUE;
        cpu_pause();
    }
    return FALSE;
}


static void ps2_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
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


static void ps2_poll(void)
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


/* ================================================================
 * 5c. Мышь: накопитель движения между опросами
 * ================================================================
 *
 * Драйвер складывает сюда dX/dY/колесо из каждого отчёта, а
 * наш EFI_SIMPLE_POINTER_PROTOCOL->GetState (которым пользуется
 * GUI) отдаёт накопленное с прошлого вызова и обнуляет - ровно
 * так же, как это делал драйвер мыши прошивки.
 */
static INT64  g_kmouse_dx = 0;
static INT64  g_kmouse_dy = 0;
static INT64  g_kmouse_dz = 0;
static UINT32 g_kmouse_buttons = 0;
static BOOLEAN g_kmouse_present = FALSE;
static UINT64 g_kmouse_reports = 0;



/* ================================================================
 * 6. xHCI + USB HID: неблокирующий драйвер для многих устройств
 * ================================================================
 *
 * Чем отличается от старого кода команды ebsdemo
 * (xhci_run_post_ebs/xhci_address_device_and_get_descriptor):
 *
 *   - Не одно устройство, а ВСЕ, что воткнуты в корневые порты
 *     контроллера (мышь + клавиатура + донгл с обоими сразу и
 *     т.п.). У беспроводного донгла часто несколько HID-
 *     интерфейсов в одном устройстве (клавиатура, мышь,
 *     мультимедиа-клавиши) - поднимаются все, что понимаем.
 *
 *   - Не "опросить 200 отчётов и выйти", а работать всегда:
 *     на каждую Interrupt IN конечную точку всегда стоит ровно
 *     один Normal TRB; когда контроллер кладёт в Event Ring
 *     Transfer Event (устройство прислало отчёт) - разбираем
 *     отчёт и сразу ставим следующий TRB. Разбор событий -
 *     kx_poll(), её зовут наш ConIn->ReadKeyStroke и
 *     SimplePointer->GetState, т.е. каждый раз, когда шелл или
 *     GUI хотят ввод. Ждать внутри не нужно ничего.
 *
 *   - Кольца (Command Ring, Transfer Ring'и) - одна общая
 *     реализация kx_ring_* с Link TRB и переворотом Cycle-бита,
 *     поэтому они могут крутиться бесконечно (раньше Command
 *     Ring писался по фиксированным смещениям 0/16/32).
 *
 *   - Любое ожидание (команда, control transfer) не выбрасывает
 *     "чужие" события, а передаёт их общему обработчику - так
 *     отчёт уже работающей мыши не потеряется, пока мы, например,
 *     настраиваем следующее устройство.
 *
 *   - Ошибки на конечной точке (STALL, ошибка передачи) не
 *     убивают драйвер: делается штатное восстановление по спеке
 *     xHCI (Reset Endpoint + Set TR Dequeue Pointer).
 *
 *   - Scratchpad Buffers: многие настоящие контроллеры (Intel)
 *     требуют отдать им немного RAM под внутренние нужды (поле
 *     Max Scratchpad Buffers в HCSPARAMS2) - без этого они
 *     работают неправильно. QEMU их не просит, поэтому старый
 *     код это не делал; здесь - делаем.
 *
 * Все DMA-структуры берутся из нашего pmm (ниже 4 ГиБ - так
 * работает любой контроллер, даже без 64-битной адресации),
 * а не заранее выделяются через AllocatePages прошивки.
 *
 * Ограничения (честно, чтобы не забыть):
 *   - USB-хабы не поддерживаются: только устройства, воткнутые
 *     прямо в порты контроллера (у ноутбука внешние порты
 *     обычно как раз такие);
 *   - "горячее" подключение пока не обрабатывается: устройства
 *     перечисляются один раз, в момент "ebs";
 *   - клавиатуры - только с boot protocol (subclass 1) - это
 *     практически все клавиатуры и донглы;
 *   - светодиоды CapsLock/NumLock на клавиатуре не зажигаются.
 */

#define KX_RING_TRBS    256u
#define KX_RING_USABLE  (KX_RING_TRBS - 1u)   /* последний - Link */
#define KX_EV_TRBS      256u
#define KX_MAX_DEVS     16
#define KX_MAX_HID      16
#define KX_MAX_IF_PER_DEV 4
#define KX_DMA_LIMIT    0x100000000ull        /* ниже 4 ГиБ */

#define KX_ROLE_NONE        0
#define KX_ROLE_KBD_BOOT    1
#define KX_ROLE_MOUSE_RPT   2
#define KX_ROLE_MOUSE_BOOT  3

#define KX_EP_RUN     0
#define KX_EP_RESET   1    /* ждём завершения Reset Endpoint */
#define KX_EP_SETDEQ  2    /* ждём завершения Set TR Dequeue */
#define KX_EP_DEAD    3

typedef struct {
    UINT64 phys;
    UINTN  seq;   /* сквозной номер следующего TRB (0,1,2,...) */
} KX_RING;

typedef struct {
    BOOLEAN used;
    UINT8   port;
    UINT8   speed;
    UINT8   slot;
    UINT16  vid;
    UINT16  pid;
    UINT8   dclass;
    UINT16  mps0;
    UINT64  dev_ctx;
    UINT64  in_ctx;
    UINT64  buf;       /* страница под дескрипторы */
    KX_RING ep0;
    const char *status;
} KX_DEV;

typedef struct {
    BOOLEAN used;
    UINT8   dev;          /* индекс в g_kx_devs */
    UINT8   iface;
    UINT8   subclass;
    UINT8   protocol;
    UINT8   role;
    UINT8   ep_addr;
    UINT8   dci;
    UINT16  maxpkt;
    UINT8   burst;
    UINT8   interval_raw;
    UINT8   interval_field;
    UINT16  rdesc_len;
    UINT32  req_len;
    KX_RING ring;
    UINT64  rep_buf;
    HID_MOUSE_REPORT_LAYOUT layout;
    UINT8   prev_keys[6];
    UINT8   state;
    UINT64  pending_cmd;  /* адрес TRB команды восстановления */
    UINT64  last_trb;     /* адрес последнего поставленного TRB */
    UINT64  reports;
    UINT64  errors;
    UINT8   last_err;
    UINT32  err_streak;    /* ошибок подряд, без успешного отчёта */
    UINT8   last_cmd_cc;   /* код завершения последней команды
                              восстановления */
    UINT64  recoveries;
    UINT64  recover_tsc;   /* когда началось восстановление */
    UINT8   last_rep[16];  /* последний отчёт "как есть" - для
                              диагностики (команда mousetest) */
    UINTN   last_len;
    const char *mode_note; /* как именно выбран формат отчётов -
                              показывается в usb/mousetest */
    UINT64  rejected;      /* отчёты мыши, которые пришли, но не
                              подошли под разобранный формат
                              (чужой Report ID, слишком короткие) */
    INT64   abs_last_x;
    INT64   abs_last_y;
} KX_HID;

static struct {
    BOOLEAN present;      /* контроллер найден до выхода */
    BOOLEAN running;      /* драйвер запущен и опрашивается */
    UINT8   bus, devn, func;
    UINT64  mmio;
    XHCI_CAP_INFO cap;
    UINT64  op;
    UINT64  rt;
    UINT64  db;
    UINT64  intr0;
    UINT32  ctx_size;
    UINT32  scratchpads;
    UINT64  dcbaa;
    UINT64  evring;
    UINT64  erst;
    KX_RING cmd;
    UINTN   ev_deq;       /* сквозной номер следующего события */
    UINT64  events;
    UINT64  port_events;
    UINT64  stray_events;
} g_kx;

static KX_DEV g_kx_devs[KX_MAX_DEVS];
static KX_HID g_kx_hid[KX_MAX_HID];


/* --- кольца, где МЫ производитель (Command/Transfer) --- */

static UINT32 kx_ring_pcs(UINTN seq)
{
    return ((seq / KX_RING_USABLE) % 2u == 0) ? 1u : 0u;
}

static UINT64 kx_ring_slot_addr(KX_RING *r, UINTN seq)
{
    return r->phys + (UINT64)(seq % KX_RING_USABLE) * 16u;
}

static void kx_ring_init(KX_RING *r, UINT64 phys)
{
    r->phys = phys;
    r->seq = 0;

    raw_zero_mem((volatile UINT8 *)(UINTN)phys, 4096);

    volatile UINT32 *link =
        (volatile UINT32 *)(UINTN)(phys + (KX_RING_TRBS - 1u) * 16u);

    link[0] = (UINT32)(phys & 0xFFFFFFFFu);
    link[1] = (UINT32)(phys >> 32);
    link[2] = 0;
    /* Link TRB (Type 6), Toggle Cycle (бит 1), Cycle=1 */
    link[3] = (6u << 10) | (1u << 1) | 1u;
}

/*
 * Положить TRB в кольцо (d3 - без Cycle-бита, его ставит сама
 * функция). Возвращает физический адрес TRB (по нему потом
 * находится Command Completion Event этой команды).
 *
 * Link TRB и Cycle-бит (важно, это ровно то место, где раньше
 * прятались баги с "замиранием" на 256-м отчёте): Cycle-бит -
 * это "флажок владения". TRB принадлежит контроллеру, только
 * если его Cycle совпадает с внутренним Cycle State контроллера,
 * который переворачивается каждый раз, когда контроллер проходит
 * по Link TRB в конце страницы. Значит, и Link TRB должен
 * "отдаваться" контроллеру так же, как обычный TRB - в момент,
 * когда МЫ переходим через конец кольца, с Cycle ТЕКУЩЕГО
 * (заканчивающегося) круга. До этого момента у Link TRB Cycle
 * от предыдущего круга, и контроллер, догнав нас, честно
 * останавливается перед ним. Так делает и Linux.
 */
static UINT64 kx_ring_push(
    KX_RING *r,
    UINT32 d0, UINT32 d1, UINT32 d2, UINT32 d3
)
{
    UINTN slot = r->seq % KX_RING_USABLE;
    UINT32 pcs = kx_ring_pcs(r->seq);

    if (slot == 0 && r->seq > 0) {

        volatile UINT32 *link =
            (volatile UINT32 *)(UINTN)
                (r->phys + (KX_RING_TRBS - 1u) * 16u);

        link[3] = (6u << 10) | (1u << 1) | (pcs ^ 1u);
    }

    UINT64 addr = r->phys + (UINT64)slot * 16u;
    volatile UINT32 *t = (volatile UINT32 *)(UINTN)addr;

    t[0] = d0;
    t[1] = d1;
    t[2] = d2;

    /* Cycle - строго последним: как только он записан, TRB
       может быть взят контроллером */
    __asm__ __volatile__("" ::: "memory");

    t[3] = (d3 & ~1u) | pcs;

    r->seq++;

    return addr;
}


/* --- Event Ring, где производитель - контроллер --- */

static void kx_erdp_update(void)
{
    UINT64 a = g_kx.evring + (UINT64)(g_kx.ev_deq % KX_EV_TRBS) * 16u;

    /* бит 3 (EHB, Event Handler Busy) - RW1C, пишем 1, чтобы
       сбросить */
    mmio_write32(g_kx.intr0 + 0x18, (UINT32)(a & 0xFFFFFFFFu) | 0x8u);
    mmio_write32(g_kx.intr0 + 0x1C, (UINT32)(a >> 32));
}

/* Взять следующее событие, если оно есть (копией - слот в
   кольце сразу освобождается) */
static BOOLEAN kx_ev_fetch(UINT32 ev[4])
{
    volatile UINT32 *t =
        (volatile UINT32 *)(UINTN)
            (g_kx.evring + (UINT64)(g_kx.ev_deq % KX_EV_TRBS) * 16u);

    UINT32 cyc = ((g_kx.ev_deq / KX_EV_TRBS) % 2u == 0) ? 1u : 0u;
    UINT32 d3 = t[3];

    if ((d3 & 1u) != cyc)
        return FALSE;

    ev[0] = t[0];
    ev[1] = t[1];
    ev[2] = t[2];
    ev[3] = d3;

    g_kx.ev_deq++;
    g_kx.events++;

    kx_erdp_update();

    return TRUE;
}


static void kx_doorbell(UINT8 slot, UINT32 target)
{
    mmio_write32(g_kx.db + (UINT64)slot * 4u, target);
}

static UINT64 kx_dma_page(void)
{
    return pmm_alloc_zeroed(1, KX_DMA_LIMIT);
}


static void kx_hid_queue(KX_HID *h);
static void kx_hid_report(KX_HID *h, UINTN len);


/*
 * Обработка события, которого сейчас никто синхронно не ждёт:
 * отчёты работающих устройств, шаги восстановления после ошибок,
 * изменения портов.
 */
static void kx_handle_async_event(UINT32 ev[4])
{
    UINT8 type = (UINT8)((ev[3] >> 10) & 0x3Fu);
    UINT8 cc = (UINT8)((ev[2] >> 24) & 0xFFu);

    if (type == 32) {

        /* Transfer Event */
        UINT8 slot = (UINT8)((ev[3] >> 24) & 0xFFu);
        UINT8 epid = (UINT8)((ev[3] >> 16) & 0x1Fu);

        KX_HID *h = NULL;

        for (UINTN i = 0; i < KX_MAX_HID; i++) {

            KX_HID *c = &g_kx_hid[i];

            if (
                c->used && c->role != KX_ROLE_NONE &&
                g_kx_devs[c->dev].slot == slot && c->dci == epid
            ) {
                h = c;
                break;
            }
        }

        UINT64 trb_ptr = (UINT64)ev[0] | ((UINT64)ev[1] << 32);

        if (h == NULL || h->state != KX_EP_RUN || trb_ptr != h->last_trb) {
            /* не наш/запоздавший/повторный - не трогаем кольцо,
               иначе на конечной точке оказалось бы два TRB */
            g_kx.stray_events++;
            return;
        }

        if (cc == 1 || cc == 13) {

            /* в младших 24 битах - сколько байт НЕ пришло */
            UINT32 residue = ev[2] & 0xFFFFFFu;
            UINTN len =
                (residue <= h->req_len) ? (h->req_len - residue) : 0;

            h->reports++;
            h->err_streak = 0;

            {
                volatile UINT8 *r = (volatile UINT8 *)(UINTN)h->rep_buf;

                h->last_len = len;

                for (UINTN k = 0; k < 16; k++)
                    h->last_rep[k] = (k < len) ? r[k] : 0;
            }

            kx_hid_report(h, len);
            kx_hid_queue(h);
            return;
        }

        h->errors++;
        h->last_err = cc;
        h->err_streak++;

        /*
         * РАНЬШЕ: после 200 ошибок ЗА ВСЁ ВРЕМЯ конечная точка
         * выключалась навсегда. У беспроводного донгла, который
         * шлёт отчёты до 1000 раз в секунду, редкие ошибки
         * передачи - норма, и 200 штук могли набежать за
         * секунды - мышь "немного двигалась и умирала". Теперь
         * сдаёмся только после многих ошибок ПОДРЯД, без единого
         * удачного отчёта между ними.
         */
        if (h->err_streak > 64) {
            h->state = KX_EP_DEAD;
            return;
        }

        if (cc == 21) {

            /* Missed Service Error: контроллер не успел
               обслужить конечную точку в её интервал. Она при
               этом НЕ останавливается (не Halted) - Reset
               Endpoint тут не нужен и даже вернул бы ошибку
               "неверное состояние". TRB уже списан - просто
               ставим следующий. */
            kx_hid_queue(h);
            return;
        }

        /* Остальные ошибки (Transaction Error, Babble, Stall...)
           останавливают конечную точку (Halted).
           Шаг 1 восстановления - Reset Endpoint Command */
        h->recoveries++;
        h->recover_tsc = rdtsc();
        h->state = KX_EP_RESET;
        h->pending_cmd =
            kx_ring_push(
                &g_kx.cmd, 0, 0, 0,
                ((UINT32)slot << 24) | ((UINT32)h->dci << 16) |
                    (14u << 10)
            );
        kx_doorbell(0, 0);
        return;
    }

    if (type == 33) {

        /* Command Completion Event - наше ли это восстановление? */
        UINT64 ptr = (UINT64)ev[0] | ((UINT64)ev[1] << 32);

        for (UINTN i = 0; i < KX_MAX_HID; i++) {

            KX_HID *h = &g_kx_hid[i];

            if (!h->used || h->pending_cmd != ptr || ptr == 0)
                continue;

            UINT8 slot = g_kx_devs[h->dev].slot;

            h->last_cmd_cc = cc;

            if (h->state == KX_EP_RESET && cc == 19) {

                /* Context State Error: конечная точка на самом деле
                   не была остановлена (ошибка оказалась из тех,
                   после которых контроллер продолжает сам) - ни
                   Reset, ни Set TR Dequeue ей не нужны */
                h->state = KX_EP_RUN;
                h->pending_cmd = 0;
                kx_hid_queue(h);

            } else if (h->state == KX_EP_RESET) {

                /* Шаг 2 - Set TR Dequeue Pointer: сказать
                   контроллеру, что кольцо продолжается с нашего
                   следующего свободного TRB (сбойный TRB
                   пропускаем) */
                UINT64 deq = kx_ring_slot_addr(&h->ring, h->ring.seq);
                UINT32 dcs = kx_ring_pcs(h->ring.seq);

                h->state = KX_EP_SETDEQ;
                h->pending_cmd =
                    kx_ring_push(
                        &g_kx.cmd,
                        (UINT32)(deq & 0xFFFFFFF0u) | dcs,
                        (UINT32)(deq >> 32),
                        0,
                        ((UINT32)slot << 24) |
                            ((UINT32)h->dci << 16) | (16u << 10)
                    );
                kx_doorbell(0, 0);

            } else if (h->state == KX_EP_SETDEQ) {

                /* Шаг 3 - снова работаем */
                h->state = KX_EP_RUN;
                h->pending_cmd = 0;
                kx_hid_queue(h);
            }

            return;
        }

        g_kx.stray_events++;
        return;
    }

    if (type == 34) {

        /* Port Status Change Event: номер порта в битах 31:24
           первого слова. Сбрасываем флаги изменений порта
           (иначе следующих событий по этому порту не будет) */
        UINT32 port = (ev[0] >> 24) & 0xFFu;

        g_kx.port_events++;

        if (port >= 1 && port <= g_kx.cap.MaxPorts) {

            UINT64 pb = g_kx.op + 0x400u + (UINT64)(port - 1u) * 0x10u;
            UINT32 cur = mmio_read32(pb);

            mmio_write32(
                pb,
                portsc_base_for_write(cur) | (cur & PORTSC_RW1CS_MASK)
            );
        }

        return;
    }

    g_kx.stray_events++;
}


/*
 * Синхронное ожидание конкретного события (с таймаутом по TSC).
 *   type 33 (Command Completion) - совпадение по адресу TRB
 *           команды (match_ptr);
 *   type 32 (Transfer) - по SlotID + номеру конечной точки.
 * Всё постороннее - в kx_handle_async_event.
 */
static BOOLEAN kx_wait_event(
    UINT8 type,
    UINT64 match_ptr,
    UINT8 slot,
    UINT8 epid,
    UINT32 out_ev[4],
    UINTN timeout_ms
)
{
    UINT64 start = rdtsc();
    UINT64 limit = (g_tsc_hz / 1000u) * (UINT64)timeout_ms;

    for (;;) {

        UINT32 ev[4];

        if (kx_ev_fetch(ev)) {

            UINT8 t = (UINT8)((ev[3] >> 10) & 0x3Fu);
            BOOLEAN match = FALSE;

            if (t == type && type == 33) {

                UINT64 ptr = (UINT64)ev[0] | ((UINT64)ev[1] << 32);
                match = (ptr == match_ptr);

            } else if (t == type && type == 32) {

                match =
                    ((UINT8)((ev[3] >> 24) & 0xFFu) == slot) &&
                    ((UINT8)((ev[3] >> 16) & 0x1Fu) == epid);
            }

            if (match) {

                out_ev[0] = ev[0];
                out_ev[1] = ev[1];
                out_ev[2] = ev[2];
                out_ev[3] = ev[3];
                return TRUE;
            }

            kx_handle_async_event(ev);
            continue;
        }

        if (rdtsc() - start > limit)
            return FALSE;

        cpu_pause();
    }
}


/* Команда контроллеру через Command Ring. Возвращает Completion
   Code (1 = Success, 0 = событие так и не пришло) */
static UINT8 kx_command(
    UINT32 d0, UINT32 d1, UINT32 d2, UINT32 d3,
    UINT8 *out_slot
)
{
    UINT64 trb = kx_ring_push(&g_kx.cmd, d0, d1, d2, d3);

    kx_doorbell(0, 0);

    UINT32 ev[4];

    if (!kx_wait_event(33, trb, 0, 0, ev, 1000))
        return 0;

    if (out_slot)
        *out_slot = (UINT8)((ev[3] >> 24) & 0xFFu);

    return (UINT8)((ev[2] >> 24) & 0xFFu);
}


/* Синхронное восстановление остановленной (Halted) конечной
   точки - используется для Endpoint 0 во время настройки
   устройства (например, если устройство ответило STALL на
   необязательный запрос вроде SET_IDLE) */
static void kx_recover_sync(UINT8 slot, UINT8 dci, KX_RING *r)
{
    kx_command(
        0, 0, 0,
        ((UINT32)slot << 24) | ((UINT32)dci << 16) | (14u << 10),
        NULL
    );

    UINT64 deq = kx_ring_slot_addr(r, r->seq);
    UINT32 dcs = kx_ring_pcs(r->seq);

    kx_command(
        (UINT32)(deq & 0xFFFFFFF0u) | dcs,
        (UINT32)(deq >> 32),
        0,
        ((UINT32)slot << 24) | ((UINT32)dci << 16) | (16u << 10),
        NULL
    );
}


/*
 * Control transfer на Endpoint 0 (Setup + [Data] + Status) -
 * та же схема, что xhci_control_transfer выше по файлу (там
 * подробно разобраны биты IDT/IOC/TRT), только поверх kx_ring и
 * с правильным ожиданием. Возвращает Completion Code: 1 или 13
 * (Short Packet) = успех, 0 = таймаут.
 */
static UINT8 kx_control(
    KX_DEV *d,
    UINT8 bm_request_type,
    UINT8 b_request,
    UINT16 w_value,
    UINT16 w_index,
    UINT16 w_length,
    UINT64 data_phys
)
{
    UINT32 trt;

    if (w_length == 0)
        trt = 0u;
    else if (bm_request_type & 0x80u)
        trt = 3u;
    else
        trt = 2u;

    kx_ring_push(
        &d->ep0,
        (UINT32)bm_request_type | ((UINT32)b_request << 8) |
            ((UINT32)w_value << 16),
        (UINT32)w_index | ((UINT32)w_length << 16),
        8u,
        (1u << 6) | (2u << 10) | (trt << 16)
    );

    if (w_length != 0) {

        UINT32 dir = (bm_request_type & 0x80u) ? 1u : 0u;

        kx_ring_push(
            &d->ep0,
            (UINT32)(data_phys & 0xFFFFFFFFu),
            (UINT32)(data_phys >> 32),
            w_length,
            (3u << 10) | (dir << 16)
        );
    }

    UINT32 status_dir;

    if (w_length == 0)
        status_dir = 1u;
    else
        status_dir = (bm_request_type & 0x80u) ? 0u : 1u;

    kx_ring_push(
        &d->ep0, 0, 0, 0,
        (1u << 5) | (4u << 10) | (status_dir << 16)
    );

    kx_doorbell(d->slot, 1);

    UINT32 ev[4];

    if (!kx_wait_event(32, 0, d->slot, 1, ev, 1000))
        return 0;

    UINT8 cc = (UINT8)((ev[2] >> 24) & 0xFFu);

    if (cc != 1 && cc != 13)
        kx_recover_sync(d->slot, 1, &d->ep0);

    return cc;
}


/* Поставить следующий Normal TRB на Interrupt IN конечную точку */
static void kx_hid_queue(KX_HID *h)
{
    UINT32 len = h->maxpkt;

    if (len > 512u)
        len = 512u;

    if (len == 0)
        len = 8u;

    h->req_len = len;

    /* IOC (бит 5) - событие по завершении (в т.ч. коротким
       пакетом - отчёты часто короче maxpkt, тогда в событии
       Completion Code 13 = Short Packet). Адрес TRB запоминаем:
       событие должно прийти именно по нему. */
    h->last_trb =
        kx_ring_push(
            &h->ring,
            (UINT32)(h->rep_buf & 0xFFFFFFFFu),
            (UINT32)(h->rep_buf >> 32),
            len,
            (1u << 5) | (1u << 10)
        );

    kx_doorbell(g_kx_devs[h->dev].slot, h->dci);
}


/* Отчёт мыши, формат которого разобран из Report Descriptor
   (см. hid_parse_report_descriptor выше по файлу) */
static void kx_mouse_report_layout(KX_HID *h, volatile UINT8 *rep, UINTN len)
{
    HID_MOUSE_REPORT_LAYOUT *L = &h->layout;

    /* у составных устройств в одном интерфейсе бывает несколько
       отчётов с разными Report ID (мышь + мультимедиа и т.п.) -
       чужие просто пропускаем */
    if (L->has_report_id) {

        if (len < 1 || rep[0] != L->report_id) {
            h->rejected++;
            return;
        }
    }

    UINT32 buttons = 0;

    if (L->has_buttons) {
        buttons =
            hid_extract_bits(
                rep, len, L->button_bit_offset, L->button_count
            );
    }

    UINT32 xr = hid_extract_bits(rep, len, L->x_bit_offset, L->x_bit_size);
    UINT32 yr = hid_extract_bits(rep, len, L->y_bit_offset, L->y_bit_size);

    if (L->x_is_relative) {

        g_kmouse_dx += hid_sign_extend(xr, L->x_bit_size);

    } else {

        /* абсолютная координата (планшет, QEMU usb-tablet):
           переводим в пиксели экрана и отдаём как разницу с
           прошлой позицией - GUI умеет только относительное */
        INT64 maxv = (L->x_logical_max > 0) ? L->x_logical_max : 32767;
        INT64 px = ((INT64)xr * (INT64)g_kfb_w) / (maxv + 1);

        g_kmouse_dx += px - h->abs_last_x;
        h->abs_last_x = px;
    }

    if (L->y_is_relative) {

        g_kmouse_dy += hid_sign_extend(yr, L->y_bit_size);

    } else {

        INT64 maxv = (L->y_logical_max > 0) ? L->y_logical_max : 32767;
        INT64 py = ((INT64)yr * (INT64)g_kfb_h) / (maxv + 1);

        g_kmouse_dy += py - h->abs_last_y;
        h->abs_last_y = py;
    }

    if (L->has_wheel) {

        UINT32 wr =
            hid_extract_bits(
                rep, len, L->wheel_bit_offset, L->wheel_bit_size
            );

        g_kmouse_dz += hid_sign_extend(wr, L->wheel_bit_size);
    }

    g_kmouse_buttons = buttons;
    g_kmouse_reports++;
}


static void kx_hid_report(KX_HID *h, UINTN len)
{
    volatile UINT8 *rep = (volatile UINT8 *)(UINTN)h->rep_buf;

    if (h->role == KX_ROLE_KBD_BOOT) {

        kbd_usb_boot_report(rep, len, h->prev_keys);

    } else if (h->role == KX_ROLE_MOUSE_RPT) {

        kx_mouse_report_layout(h, rep, len);

    } else if (h->role == KX_ROLE_MOUSE_BOOT) {

        /* boot protocol мыши: байт0 кнопки, байт1 dX, байт2 dY,
           [байт3 колесо] - все знаковые */
        if (len < 3)
            h->rejected++;

        if (len >= 3) {

            g_kmouse_buttons = rep[0];
            g_kmouse_dx += (INT8)rep[1];
            g_kmouse_dy += (INT8)rep[2];

            if (len >= 4)
                g_kmouse_dz += (INT8)rep[3];

            g_kmouse_reports++;
        }
    }
}


/* Разобрать все накопившиеся события (без ожидания) */
static void kx_poll(void)
{
    if (!g_kx.running)
        return;

    for (UINTN i = 0; i < 64; i++) {

        UINT32 ev[4];

        if (!kx_ev_fetch(ev))
            break;

        kx_handle_async_event(ev);
    }

    /*
     * Сторож восстановления: если команда Reset Endpoint / Set TR
     * Dequeue так и не завершилась за ~300 мс (событие потерялось
     * или контроллер повёл себя не по книжке), не ждём вечно -
     * ставим TRB заново. Иначе конечная точка молча "застряла" бы
     * в состоянии восстановления навсегда.
     */
    if (g_tsc_hz != 0) {

        UINT64 now = rdtsc();
        UINT64 limit = (g_tsc_hz / 1000u) * 300u;

        for (UINTN i = 0; i < KX_MAX_HID; i++) {

            KX_HID *h = &g_kx_hid[i];

            if (!h->used || h->role == KX_ROLE_NONE)
                continue;

            if (
                (h->state == KX_EP_RESET || h->state == KX_EP_SETDEQ) &&
                now - h->recover_tsc > limit
            ) {
                h->state = KX_EP_RUN;
                h->pending_cmd = 0;
                h->last_cmd_cc = 0xFF;   /* "не дождались" */
                kx_hid_queue(h);
            }
        }
    }
}


static const char *kx_speed_name(UINT8 s)
{
    switch (s) {
    case 1:  return "Full Speed (12 Mbit/s)";
    case 2:  return "Low Speed (1.5 Mbit/s)";
    case 3:  return "High Speed (480 Mbit/s)";
    case 4:  return "SuperSpeed (5 Gbit/s)";
    case 5:  return "SuperSpeed+ (10 Gbit/s)";
    default: return "unknown speed";
    }
}

static const char *kx_role_name(UINT8 r)
{
    switch (r) {
    case KX_ROLE_KBD_BOOT:   return "keyboard (boot protocol)";
    case KX_ROLE_MOUSE_RPT:  return "mouse (report descriptor)";
    case KX_ROLE_MOUSE_BOOT: return "mouse (boot protocol)";
    default:                 return "not used";
    }
}


/*
 * Перевести мышь в boot protocol И УБЕДИТЬСЯ, что она перешла.
 *
 * SET_PROTOCOL(Boot) - просьба, а не приказ: некоторые дешёвые
 * донглы заявляют поддержку boot protocol (subclass 1), отвечают
 * на SET_PROTOCOL "успешно" - и продолжают слать свой обычный
 * формат (часто с байтом Report ID в начале). Тогда мы читали бы
 * Report ID как кнопки, а кнопки как dX - курсор прыгал бы не туда
 * или "залипала" бы левая кнопка. Это и было бы угадывание.
 *
 * Поэтому после SET_PROTOCOL спрашиваем GET_PROTOCOL (bRequest
 * 0x03, ответ - 1 байт: 0 = boot, 1 = report):
 *   0            -> boot точно включён, берём фиксированный формат;
 *   1            -> мышь НЕ переключилась, возвращаем FALSE, и
 *                   вызывающий код разберёт её Report Descriptor;
 *   не ответила  -> (запрос обязателен по спеке для boot-устройств,
 *                   но встречаются и такие) доверяем SET_PROTOCOL.
 */
static BOOLEAN kx_mouse_switch_to_boot(KX_DEV *d, UINT8 iface, KX_HID *h)
{
    UINT8 cc = kx_control(d, 0x21, 0x0B, 0, iface, 0, 0);

    if (cc != 1) {
        h->mode_note = "SET_PROTOCOL(boot) refused";
        return FALSE;
    }

    UINT64 buf = d->buf + 512u;
    volatile UINT8 *b = (volatile UINT8 *)(UINTN)buf;

    b[0] = 0xEE;   /* заведомо не 0 и не 1 */

    cc = kx_control(d, 0xA1, 0x03, 0, iface, 1, buf);

    if (cc != 1 && cc != 13) {
        h->mode_note = "boot (GET_PROTOCOL not answered, trusting SET)";
        return TRUE;
    }

    if (b[0] == 0) {
        h->mode_note = "boot (confirmed by GET_PROTOCOL)";
        return TRUE;
    }

    h->mode_note = "device ignored SET_PROTOCOL, report mode";
    return FALSE;
}


/* Кандидат в HID-интерфейсы, найденный в Configuration
   Descriptor */
typedef struct {
    UINT8  iface;
    UINT8  subclass;
    UINT8  protocol;
    BOOLEAN has_ep;
    UINT8  ep_addr;
    UINT16 maxpkt_raw;
    UINT8  interval;
    UINT16 rdesc_len;
} KX_HID_CAND;


/*
 * Полная настройка одного устройства на корневом порту p:
 * Port Reset -> Enable Slot -> Address Device -> дескрипторы ->
 * SET_CONFIGURATION -> настройка HID-интерфейсов -> Configure
 * Endpoint -> первые TRB на опрос. Подробные объяснения каждого
 * шага - в старом коде (xhci_address_device_and_get_descriptor),
 * здесь - только отличия.
 */
static void kx_enum_port(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN p)
{
    UINT64 pb = g_kx.op + 0x400u + (UINT64)(p - 1u) * 0x10u;
    UINT32 sc = mmio_read32(pb);

    if (!(sc & PORTSC_BIT_CCS))
        return;

    print(out, "\n  Port ");
    print_uint(out, p);
    print(out, ": device connected\n");

    if (!(sc & PORTSC_BIT_PED)) {

        /* USB2-порту нужен явный Port Reset (USB3 включается
           сам после тренировки линии) */
        mmio_write32(pb, portsc_base_for_write(sc) | PORTSC_BIT_PR);

        BOOLEAN done = FALSE;

        for (UINTN i = 0; i < 500; i++) {

            busy_wait_ms(1);

            UINT32 s = mmio_read32(pb);

            if (s & PORTSC_BIT_PRC) {
                done = TRUE;
                mmio_write32(pb, portsc_base_for_write(s) | PORTSC_BIT_PRC);
                break;
            }
        }

        /* 10 мс "восстановления" после сброса - требование
           спеки USB 2.0 (TRSTRCY) */
        busy_wait_ms(20);

        sc = mmio_read32(pb);

        if (!done || !(sc & PORTSC_BIT_PED)) {
            print(out, "    port reset failed - skipped\n");
            return;
        }
    }

    UINT8 speed = (UINT8)((sc >> 10) & 0xFu);

    print(out, "    ");
    print(out, kx_speed_name(speed));
    print(out, "\n");

    KX_DEV *d = NULL;
    UINTN di = 0;

    for (di = 0; di < KX_MAX_DEVS; di++) {
        if (!g_kx_devs[di].used) {
            d = &g_kx_devs[di];
            break;
        }
    }

    if (d == NULL) {
        print(out, "    too many devices - skipped\n");
        return;
    }

    d->used = TRUE;
    d->port = (UINT8)p;
    d->speed = speed;
    d->status = "setup failed";

    /* --- Enable Slot --- */
    UINT8 slot = 0;
    UINT8 cc = kx_command(0, 0, 0, (9u << 10), &slot);

    if (cc != 1 || slot == 0) {
        print(out, "    Enable Slot failed, cc=");
        print_uint(out, cc);
        print(out, "\n");
        return;
    }

    d->slot = slot;

    d->dev_ctx = kx_dma_page();
    d->in_ctx = kx_dma_page();
    d->buf = kx_dma_page();

    UINT64 ep0_ring = kx_dma_page();

    if (!d->dev_ctx || !d->in_ctx || !d->buf || !ep0_ring) {
        print(out, "    out of memory\n");
        return;
    }

    kx_ring_init(&d->ep0, ep0_ring);

    ((volatile UINT64 *)(UINTN)g_kx.dcbaa)[slot] = d->dev_ctx;

    /* Стартовый Max Packet Size для EP0. Раньше для Full Speed
       брали 8 - это работает с QEMU, но на настоящем FS-
       устройстве с bMaxPacketSize0=64 ответ длиннее 8 байт
       пришёл бы одним "слишком большим" пакетом (Babble). Как в
       Linux: для FS стартуем с 64 и просим сначала только 8
       байт дескриптора - такой ответ влезает в любой пакет. */
    UINT16 mps0;

    if (speed == 2)
        mps0 = 8;
    else if (speed == 1 || speed == 3)
        mps0 = 64;
    else
        mps0 = 512;

    UINT32 cs = g_kx.ctx_size;
    volatile UINT32 *ictl = (volatile UINT32 *)(UINTN)d->in_ctx;
    volatile UINT32 *islot = (volatile UINT32 *)(UINTN)(d->in_ctx + cs);
    volatile UINT32 *iep0 = (volatile UINT32 *)(UINTN)(d->in_ctx + 2u * cs);

    ictl[0] = 0;
    ictl[1] = 0x3u;                          /* A0 Slot + A1 EP0 */
    islot[0] = ((UINT32)speed << 20) | (1u << 27);
    islot[1] = (UINT32)p << 16;
    islot[2] = 0;
    islot[3] = 0;
    iep0[0] = 0;
    iep0[1] = (3u << 1) | (4u << 3) | ((UINT32)mps0 << 16);
    iep0[2] = (UINT32)(ep0_ring & 0xFFFFFFFFu) | 1u;
    iep0[3] = (UINT32)(ep0_ring >> 32);
    iep0[4] = 8;

    /* --- Address Device --- */
    cc = kx_command(
        (UINT32)(d->in_ctx & 0xFFFFFFFFu),
        (UINT32)(d->in_ctx >> 32),
        0,
        ((UINT32)slot << 24) | (11u << 10),
        NULL
    );

    if (cc != 1) {
        print(out, "    Address Device failed, cc=");
        print_uint(out, cc);
        print(out, "\n");
        return;
    }

    busy_wait_ms(5);    /* SET_ADDRESS recovery (2 мс по спеке) */

    volatile UINT8 *b = (volatile UINT8 *)(UINTN)d->buf;

    /* --- первые 8 байт Device Descriptor (ради bMaxPacketSize0) --- */
    cc = kx_control(d, 0x80, 0x06, 0x0100, 0, 8, d->buf);

    if (cc != 1 && cc != 13) {
        print(out, "    GET_DESCRIPTOR(Device, 8) failed, cc=");
        print_uint(out, cc);
        print(out, "\n");
        return;
    }

    UINT16 real_mps0 = b[7];

    if (speed >= 4)
        real_mps0 = (UINT16)(1u << (b[7] & 0xFu));  /* у USB3 это
                                                        степень двойки */

    if (real_mps0 >= 8 && real_mps0 != mps0) {

        /* Evaluate Context: обновить MPS у EP0 на настоящий */
        raw_zero_mem((volatile UINT8 *)(UINTN)d->in_ctx, 4096);

        ictl[1] = 0x2u;                      /* только A1 = EP0 */
        iep0[1] = (3u << 1) | (4u << 3) | ((UINT32)real_mps0 << 16);
        iep0[2] = (UINT32)(ep0_ring & 0xFFFFFFFFu) | 1u;
        iep0[3] = (UINT32)(ep0_ring >> 32);
        iep0[4] = 8;

        cc = kx_command(
            (UINT32)(d->in_ctx & 0xFFFFFFFFu),
            (UINT32)(d->in_ctx >> 32),
            0,
            ((UINT32)slot << 24) | (13u << 10),
            NULL
        );

        if (cc != 1) {
            print(out, "    Evaluate Context failed, cc=");
            print_uint(out, cc);
            print(out, "\n");
        }

        mps0 = real_mps0;
    }

    d->mps0 = mps0;

    /* --- весь Device Descriptor --- */
    cc = kx_control(d, 0x80, 0x06, 0x0100, 0, 18, d->buf);

    if (cc != 1 && cc != 13) {
        print(out, "    GET_DESCRIPTOR(Device) failed\n");
        return;
    }

    d->vid = (UINT16)(b[8] | (b[9] << 8));
    d->pid = (UINT16)(b[10] | (b[11] << 8));
    d->dclass = b[4];

    print(out, "    VID:PID = ");
    print_hex(out, d->vid, 4);
    print(out, ":");
    print_hex(out, d->pid, 4);
    print(out, ", class ");
    print_uint(out, d->dclass);
    print(out, ", slot ");
    print_uint(out, slot);
    print(out, ", EP0 max packet ");
    print_uint(out, mps0);
    print(out, "\n");

    if (d->dclass == 9) {
        print(out, "    this is a USB hub - hubs are not supported yet\n");
        d->status = "hub (not supported yet)";
        return;
    }

    /* --- Configuration Descriptor: сначала 9 байт (узнать
       полную длину), потом целиком --- */
    UINT64 cfg_phys = d->buf + 1024u;
    volatile UINT8 *cfg = (volatile UINT8 *)(UINTN)cfg_phys;

    cc = kx_control(d, 0x80, 0x06, 0x0200, 0, 9, cfg_phys);

    if (cc != 1 && cc != 13) {
        print(out, "    GET_DESCRIPTOR(Configuration) failed\n");
        return;
    }

    UINT16 total = (UINT16)(cfg[2] | (cfg[3] << 8));
    UINT8 cfg_value = cfg[5];

    if (total > 1024u)
        total = 1024u;

    if (total < 9u)
        total = 9u;

    cc = kx_control(d, 0x80, 0x06, 0x0200, 0, total, cfg_phys);

    if (cc != 1 && cc != 13) {
        print(out, "    GET_DESCRIPTOR(Configuration, full) failed\n");
        return;
    }

    /* --- найти HID-интерфейсы --- */
    KX_HID_CAND cand[KX_MAX_IF_PER_DEV];
    UINTN ncand = 0;
    INTN cur = -1;
    UINTN off = 0;

    while (off + 2u <= total) {

        UINT8 dl = cfg[off];
        UINT8 dt = cfg[off + 1];

        if (dl < 2)
            break;

        if (dt == 4 && off + 9u <= total) {

            /* Interface: [2]=номер, [3]=alt setting, [5]=класс,
               [6]=подкласс, [7]=протокол */
            cur = -1;

            if (cfg[off + 3] == 0 && cfg[off + 5] == 3 &&
                ncand < KX_MAX_IF_PER_DEV) {

                cand[ncand].iface = cfg[off + 2];
                cand[ncand].subclass = cfg[off + 6];
                cand[ncand].protocol = cfg[off + 7];
                cand[ncand].has_ep = FALSE;
                cand[ncand].ep_addr = 0;
                cand[ncand].maxpkt_raw = 0;
                cand[ncand].interval = 0;
                cand[ncand].rdesc_len = 0;
                cur = (INTN)ncand;
                ncand++;
            }

        } else if (dt == 0x21 && cur >= 0 && off + 9u <= total) {

            if (cfg[off + 6] == 0x22)
                cand[cur].rdesc_len =
                    (UINT16)(cfg[off + 7] | (cfg[off + 8] << 8));

        } else if (dt == 5 && cur >= 0 && off + 7u <= total) {

            UINT8 a = cfg[off + 2];
            UINT8 at = cfg[off + 3];

            if (!cand[cur].has_ep && (a & 0x80u) && (at & 0x3u) == 3u) {

                cand[cur].has_ep = TRUE;
                cand[cur].ep_addr = a;
                cand[cur].maxpkt_raw =
                    (UINT16)(cfg[off + 4] | (cfg[off + 5] << 8));
                cand[cur].interval = cfg[off + 6];
            }
        }

        off += dl;
    }

    if (ncand == 0) {
        print(out, "    not a HID device - left unconfigured\n");
        d->status = "not HID (unused)";
        return;
    }

    /* --- SET_CONFIGURATION --- */
    cc = kx_control(d, 0x00, 0x09, cfg_value, 0, 0, 0);

    if (cc != 1) {
        print(out, "    SET_CONFIGURATION failed, cc=");
        print_uint(out, cc);
        print(out, "\n");
        return;
    }

    /* --- каждый HID-интерфейс --- */
    UINTN hid_idx[KX_MAX_IF_PER_DEV];
    UINTN nh = 0;
    UINT8 max_dci = 1;

    for (UINTN k = 0; k < ncand; k++) {

        KX_HID_CAND *c = &cand[k];

        print(out, "    HID interface ");
        print_uint(out, c->iface);
        print(out, " (subclass ");
        print_uint(out, c->subclass);
        print(out, ", protocol ");
        print_uint(out, c->protocol);
        print(out, "): ");

        if (!c->has_ep) {
            print(out, "no interrupt IN endpoint - skipped\n");
            continue;
        }

        KX_HID *h = NULL;

        for (UINTN i = 0; i < KX_MAX_HID; i++) {
            if (!g_kx_hid[i].used) {
                h = &g_kx_hid[i];
                hid_idx[nh] = i;
                break;
            }
        }

        if (h == NULL) {
            print(out, "too many HID interfaces - skipped\n");
            continue;
        }

        h->dev = (UINT8)di;
        h->iface = c->iface;
        h->subclass = c->subclass;
        h->protocol = c->protocol;
        h->role = KX_ROLE_NONE;
        h->ep_addr = c->ep_addr;
        h->dci = (UINT8)((c->ep_addr & 0x0Fu) * 2u + 1u);
        h->maxpkt = (UINT16)(c->maxpkt_raw & 0x7FFu);
        h->burst = (UINT8)((c->maxpkt_raw >> 11) & 0x3u);
        h->interval_raw = c->interval;
        h->rdesc_len = c->rdesc_len;
        h->state = KX_EP_RUN;
        h->pending_cmd = 0;
        h->reports = 0;
        h->errors = 0;
        h->last_err = 0;
        h->abs_last_x = (INT64)g_kfb_w / 2;
        h->abs_last_y = (INT64)g_kfb_h / 2;
        h->layout.valid = FALSE;

        for (UINTN i = 0; i < 6; i++)
            h->prev_keys[i] = 0;

        h->err_streak = 0;
        h->mode_note = "";
        h->rejected = 0;
        h->last_cmd_cc = 0;
        h->recoveries = 0;
        h->recover_tsc = 0;
        h->last_len = 0;

        /* Report Descriptor читаем у ВСЕХ HID-интерфейсов, даже
           у клавиатур (где он нам не нужен): так делает любая
           ОС, и некоторые устройства (особенно донглы) не
           начинают слать отчёты, пока его не прочитали */
        UINT16 rlen = c->rdesc_len ? c->rdesc_len : 256u;

        if (rlen > 2048u)
            rlen = 2048u;

        UINT64 rd_phys = d->buf + 2048u;

        raw_zero_mem((volatile UINT8 *)(UINTN)rd_phys, 2048);

        UINT8 rcc = kx_control(d, 0x81, 0x06, 0x2200, c->iface, rlen, rd_phys);

        if (c->subclass == 1 && c->protocol == 1) {

            /* Клавиатура: SET_PROTOCOL(Boot) - гарантированный
               8-байтный формат, одинаковый у всех клавиатур;
               SET_IDLE(0) - слать отчёт только при изменениях.
               Ошибки SET_IDLE не критичны (восстановление EP0
               делает kx_control) */
            kx_control(d, 0x21, 0x0B, 0, c->iface, 0, 0);
            kx_control(d, 0x21, 0x0A, 0, c->iface, 0, 0);

            h->role = KX_ROLE_KBD_BOOT;

        } else if (
            c->subclass == 1 && c->protocol == 2 &&
            kx_mouse_switch_to_boot(d, c->iface, h)
        ) {

            /*
             * Мышь с поддержкой boot protocol (subclass 1,
             * protocol 2 - это заявляет сама мышь): переключаем
             * её SET_PROTOCOL(Boot) в стандартный формат, одинаковый
             * у всех мышей: байт0 кнопки, байт1 dX, байт2 dY,
             * [байт3 колесо]. Так делают BIOS и загрузчики.
             *
             * РАНЬШЕ основным путём был разбор Report Descriptor.
             * В QEMU он работал, но на реальном донгле (Onikuma)
             * отчёты приходили, а курсор стоял на месте - значит,
             * дескриптор этого устройства разбирался неверно.
             * Boot protocol от дескриптора не зависит вообще.
             * Разбор дескриптора остаётся для устройств без boot
             * protocol (например, планшет с абсолютными
             * координатами) и на случай, если мышь отказалась
             * переключаться (SET_PROTOCOL вернул ошибку).
             */
            h->role = KX_ROLE_MOUSE_BOOT;

        } else {

            if (c->subclass == 1 && c->protocol == 2)
                print(out, "\n    mouse stayed in report mode - reading its descriptor");

            if (rcc == 1 || rcc == 13) {

                print(out, "\n");

                hid_parse_report_descriptor(
                    out,
                    (volatile UINT8 *)(UINTN)rd_phys,
                    rlen,
                    &h->layout
                );

                print(out, "    -> ");
            }

            if (h->layout.valid)
                h->role = KX_ROLE_MOUSE_RPT;
        }

        if (h->role == KX_ROLE_NONE) {
            print(out, "not a keyboard/mouse we understand - skipped\n");
            continue;
        }

        /* Interval в формате xHCI (см. подробный разбор в
           старом коде выше) */
        UINT8 iv;

        if (speed == 3 || speed >= 4) {

            iv = (c->interval >= 1) ? (UINT8)(c->interval - 1u) : 0;

            if (iv > 15)
                iv = 15;

        } else {

            UINT8 v = (c->interval == 0) ? 1 : c->interval;
            UINT8 lg = 0;

            while ((v >> 1) != 0) {
                v = (UINT8)(v >> 1);
                lg++;
            }

            iv = (UINT8)(lg + 3u);

            if (iv < 3)
                iv = 3;

            if (iv > 10)
                iv = 10;
        }

        h->interval_field = iv;

        UINT64 ring = kx_dma_page();
        h->rep_buf = kx_dma_page();

        if (!ring || !h->rep_buf) {
            print(out, "out of memory\n");
            h->role = KX_ROLE_NONE;
            continue;
        }

        kx_ring_init(&h->ring, ring);

        h->used = TRUE;
        nh++;

        if (h->dci > max_dci)
            max_dci = h->dci;

        print(out, kx_role_name(h->role));
        print(out, ", EP 0x");
        print_hex(out, h->ep_addr, 2);
        print(out, "\n");
    }

    if (nh == 0) {
        d->status = "HID, nothing usable";
        return;
    }

    /* --- Configure Endpoint: все найденные конечные точки
       устройства одной командой --- */
    raw_zero_mem((volatile UINT8 *)(UINTN)d->in_ctx, 4096);

    volatile UINT32 *oslot = (volatile UINT32 *)(UINTN)d->dev_ctx;

    ictl[0] = 0;
    ictl[1] = 0x1u;

    /* Slot Context - копия текущего (из Device Context, его
       заполнил контроллер после Address Device), с новым
       Context Entries = старший используемый DCI */
    islot[0] = (oslot[0] & ~(0x1Fu << 27)) | ((UINT32)max_dci << 27);
    islot[1] = oslot[1];
    islot[2] = oslot[2];
    islot[3] = 0;

    for (UINTN k = 0; k < nh; k++) {

        KX_HID *h = &g_kx_hid[hid_idx[k]];

        ictl[1] |= (1u << h->dci);

        volatile UINT32 *ep =
            (volatile UINT32 *)(UINTN)
                (d->in_ctx + (UINT64)(h->dci + 1u) * cs);

        UINT32 esit = (UINT32)h->maxpkt * (UINT32)(h->burst + 1u);

        ep[0] = (UINT32)h->interval_field << 16;
        ep[1] = (3u << 1) | (7u << 3) | ((UINT32)h->burst << 8) |
                ((UINT32)h->maxpkt << 16);
        ep[2] = (UINT32)(h->ring.phys & 0xFFFFFFFFu) | 1u;
        ep[3] = (UINT32)(h->ring.phys >> 32);
        /* Average TRB Length + Max ESIT Payload (некоторые
           контроллеры отвергают периодическую конечную точку с
           нулевым Max ESIT Payload) */
        ep[4] = (UINT32)h->maxpkt | ((esit & 0xFFFFu) << 16);
    }

    cc = kx_command(
        (UINT32)(d->in_ctx & 0xFFFFFFFFu),
        (UINT32)(d->in_ctx >> 32),
        0,
        ((UINT32)slot << 24) | (12u << 10),
        NULL
    );

    if (cc != 1) {

        print(out, "    Configure Endpoint failed, cc=");
        print_uint(out, cc);
        print(out, "\n");

        for (UINTN k = 0; k < nh; k++)
            g_kx_hid[hid_idx[k]].role = KX_ROLE_NONE;

        return;
    }

    /* --- поехали: первый TRB на каждую конечную точку --- */
    for (UINTN k = 0; k < nh; k++) {

        KX_HID *h = &g_kx_hid[hid_idx[k]];

        if (h->role == KX_ROLE_MOUSE_RPT || h->role == KX_ROLE_MOUSE_BOOT)
            g_kmouse_present = TRUE;

        kx_hid_queue(h);
    }

    d->status = "HID, active";

    print(out, "    ready.\n");
}


/*
 * Запуск драйвера целиком (после ExitBootServices): сброс
 * контроллера, структуры, запуск, питание портов, перечисление
 * всех устройств.
 */
static void kx_usb_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_kx.present) {
        print(out, "  No xHCI controller - USB input disabled.\n");
        return;
    }

    UINT64 mmio = g_kx.mmio;

    g_kx.op = mmio + g_kx.cap.CapLength;
    g_kx.rt = mmio + g_kx.cap.RtsOff;
    g_kx.db = mmio + g_kx.cap.DbOff;
    g_kx.intr0 = g_kx.rt + 0x20u;

    UINT32 hcc1 = mmio_read32(mmio + 0x10);
    g_kx.ctx_size = (hcc1 & 0x4u) ? 64u : 32u;

    UINT32 hcs2 = mmio_read32(mmio + 0x08);
    g_kx.scratchpads =
        (((hcs2 >> 21) & 0x1Fu) << 5) | ((hcs2 >> 27) & 0x1Fu);

    print(out, "  xHCI at ");
    print_uint(out, g_kx.bus);
    print(out, ":");
    print_uint(out, g_kx.devn);
    print(out, ".");
    print_uint(out, g_kx.func);
    print(out, ", ");
    print_uint(out, g_kx.cap.MaxPorts);
    print(out, " ports, context size ");
    print_uint(out, g_kx.ctx_size);
    print(out, ", scratchpad buffers ");
    print_uint(out, g_kx.scratchpads);
    print(out, "\n");

    if (!xhci_reset_controller(g_kx.op)) {
        print(out, "  xHCI reset failed - USB input disabled.\n");
        return;
    }

    g_kx.dcbaa = kx_dma_page();
    g_kx.evring = kx_dma_page();
    g_kx.erst = kx_dma_page();

    UINT64 cmd_page = kx_dma_page();

    if (!g_kx.dcbaa || !g_kx.evring || !g_kx.erst || !cmd_page) {
        print(out, "  out of memory for xHCI structures\n");
        return;
    }

    /* Scratchpad Buffer Array: DCBAA[0] -> массив адресов
       страниц, каждая страница - в распоряжении контроллера */
    if (g_kx.scratchpads > 0) {

        UINT64 arr = kx_dma_page();

        if (!arr) {
            print(out, "  out of memory for scratchpads\n");
            return;
        }

        volatile UINT64 *a = (volatile UINT64 *)(UINTN)arr;

        for (UINT32 i = 0; i < g_kx.scratchpads && i < 512u; i++) {

            UINT64 pg = kx_dma_page();

            if (!pg) {
                print(out, "  out of memory for scratchpads\n");
                return;
            }

            a[i] = pg;
        }

        ((volatile UINT64 *)(UINTN)g_kx.dcbaa)[0] = arr;
    }

    kx_ring_init(&g_kx.cmd, cmd_page);

    mmio_write32(g_kx.op + 0x30, (UINT32)(g_kx.dcbaa & 0xFFFFFFFFu));
    mmio_write32(g_kx.op + 0x34, (UINT32)(g_kx.dcbaa >> 32));

    UINT64 crcr = (cmd_page & ~0x3Full) | 0x1u;

    mmio_write32(g_kx.op + 0x18, (UINT32)(crcr & 0xFFFFFFFFu));
    mmio_write32(g_kx.op + 0x1C, (UINT32)(crcr >> 32));

    mmio_write32(g_kx.op + 0x38, g_kx.cap.MaxSlots);

    /* Event Ring: порядок по спеке - ERSTSZ, ERDP, ERSTBA
       (запись ERSTBA заставляет контроллер прочитать таблицу) */
    volatile UINT32 *erst = (volatile UINT32 *)(UINTN)g_kx.erst;

    erst[0] = (UINT32)(g_kx.evring & 0xFFFFFFFFu);
    erst[1] = (UINT32)(g_kx.evring >> 32);
    erst[2] = KX_EV_TRBS;
    erst[3] = 0;

    g_kx.ev_deq = 0;

    mmio_write32(g_kx.intr0 + 0x08, 1);
    kx_erdp_update();
    mmio_write32(g_kx.intr0 + 0x10, (UINT32)(g_kx.erst & 0xFFFFFFFFu));
    mmio_write32(g_kx.intr0 + 0x14, (UINT32)(g_kx.erst >> 32));

    /* Run */
    mmio_write32(g_kx.op + 0x00, mmio_read32(g_kx.op + 0x00) | 0x1u);

    BOOLEAN started = FALSE;

    for (UINTN i = 0; i < 200; i++) {

        busy_wait_ms(1);

        if ((mmio_read32(g_kx.op + 0x04) & 0x1u) == 0) {
            started = TRUE;
            break;
        }
    }

    if (!started) {
        print(out, "  xHCI did not start - USB input disabled.\n");
        return;
    }

    g_kx.running = TRUE;

    /* Питание портов: после сброса контроллера с Port Power
       Control (HCCPARAMS1 бит 3) порты могут быть обесточены */
    for (UINTN p = 1; p <= g_kx.cap.MaxPorts; p++) {

        UINT64 pb = g_kx.op + 0x400u + (UINT64)(p - 1u) * 0x10u;
        UINT32 s = mmio_read32(pb);

        if (!(s & (1u << 9)))
            mmio_write32(pb, portsc_base_for_write(s) | (1u << 9));
    }

    /* Дать устройствам время заново "появиться" после сброса
       (в QEMU мгновенно, на железе - десятки миллисекунд) */
    busy_wait_ms(200);

    print(out, "  xHCI running. Scanning root ports...\n");

    for (UINTN p = 1; p <= g_kx.cap.MaxPorts; p++)
        kx_enum_port(out, p);

    /* подобрать события, накопившиеся за перечисление */
    kx_poll();

    UINTN kbds = 0, mice = 0;

    for (UINTN i = 0; i < KX_MAX_HID; i++) {

        if (!g_kx_hid[i].used)
            continue;

        if (g_kx_hid[i].role == KX_ROLE_KBD_BOOT)
            kbds++;
        else if (g_kx_hid[i].role != KX_ROLE_NONE)
            mice++;
    }

    print(out, "\n  USB summary: ");
    print_uint(out, kbds);
    print(out, " keyboard(s), ");
    print_uint(out, mice);
    print(out, " mouse/pointer interface(s)\n");
}


/* Опросить ВСЕ источники ввода. Зовётся нашими ConIn/
   SimplePointer при каждом обращении шелла/GUI за вводом */
static void kernel_poll_input(void)
{
    kx_poll();
    ps2_poll();
    kbd_repeat_tick();
}



/* ================================================================
 * 7. "Прокладка": наши ConIn / BootServices / GOP / SimplePointer
 * ================================================================ */

static EFI_SYSTEM_TABLE      g_kst;
static EFI_BOOT_SERVICES     g_kbs;
static SIMPLE_INPUT_INTERFACE g_kconin;

static EFI_GRAPHICS_OUTPUT_PROTOCOL         g_kgop;
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE    g_kgop_mode;
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION g_kgop_info;

static EFI_SIMPLE_POINTER_PROTOCOL g_kptr;
static EFI_SIMPLE_POINTER_MODE     g_kptr_mode;


/* Всё, чего у нас нет, честно отвечает "не поддерживается".
   Вызывается с любым числом аргументов - в соглашении ms_abi
   лишние аргументы убирает вызывающий, это безопасно. */
static EFI_STATUS EFIAPI kbs_unsupported(void)
{
    return K_EFI_UNSUPPORTED;
}

static UINT64 EFIAPI kbs_stall(UINTN us)
{
    /* кто-то ждёт - самое время показать накопленный вывод */
    kcon_flush();

    kx_sleep_us(us);

    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI kbs_allocate_pool(
    UINTN pool_type, UINTN size, VOID **buffer
)
{
    (void)pool_type;

    if (buffer == NULL)
        return K_EFI_INVALID_PARAMETER;

    *buffer = kpool_alloc(size);

    return (*buffer != NULL) ? EFI_SUCCESS : K_EFI_OUT_OF_RESOURCES;
}

static EFI_STATUS EFIAPI kbs_free_pool(VOID *buffer)
{
    return kpool_free(buffer) ? EFI_SUCCESS : K_EFI_INVALID_PARAMETER;
}

/* Type: 0 = AllocateAnyPages, 1 = AllocateMaxAddress,
   2 = AllocateAddress (последнего у нас нет) */
static EFI_STATUS EFIAPI kbs_allocate_pages(
    UINTN type, UINTN mem_type, UINTN pages, UINT64 *memory
)
{
    (void)mem_type;

    if (memory == NULL)
        return K_EFI_INVALID_PARAMETER;

    UINT64 limit = 0;

    if (type == 1)
        limit = *memory + 1u;
    else if (type != 0)
        return K_EFI_UNSUPPORTED;

    UINT64 phys = pmm_alloc_pages(pages, limit);

    if (phys == 0)
        return K_EFI_OUT_OF_RESOURCES;

    *memory = phys;

    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI kbs_free_pages(UINT64 memory, UINTN pages)
{
    pmm_free_pages(memory, pages);

    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI kbs_set_watchdog(
    UINTN timeout, UINT64 code, UINTN size, CHAR16 *data
)
{
    (void)timeout;
    (void)code;
    (void)size;
    (void)data;

    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI kbs_get_next_monotonic_count(UINT64 *count)
{
    if (count == NULL)
        return K_EFI_INVALID_PARAMETER;

    *count = g_kticks;

    return EFI_SUCCESS;
}

static VOID EFIAPI kbs_copy_mem(VOID *dst, VOID *src, UINTN len)
{
    UINT8 *d = (UINT8 *)dst;
    UINT8 *s = (UINT8 *)src;

    if (d < s) {
        for (UINTN i = 0; i < len; i++)
            d[i] = s[i];
    } else {
        for (UINTN i = len; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
}

static VOID EFIAPI kbs_set_mem(VOID *buf, UINTN len, UINT8 value)
{
    UINT8 *d = (UINT8 *)buf;

    for (UINTN i = 0; i < len; i++)
        d[i] = value;
}


static BOOLEAN kx_guid_eq(EFI_GUID *a, EFI_GUID *b)
{
    UINT8 *x = (UINT8 *)a;
    UINT8 *y = (UINT8 *)b;

    for (UINTN i = 0; i < sizeof(EFI_GUID); i++) {
        if (x[i] != y[i])
            return FALSE;
    }

    return TRUE;
}

/* LocateProtocol: два протокола, которые реально нужны шеллу и
   GUI - экран (GOP) и мышь (Simple Pointer). Оба - наши. */
static EFI_STATUS EFIAPI kbs_locate_protocol(
    EFI_GUID *protocol,
    VOID *registration,
    VOID **iface
)
{
    (void)registration;

    if (protocol == NULL || iface == NULL)
        return K_EFI_INVALID_PARAMETER;

    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GUID ptr_guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;

    if (kx_guid_eq(protocol, &gop_guid) && g_kfb != NULL) {
        *iface = &g_kgop;
        return EFI_SUCCESS;
    }

    if (kx_guid_eq(protocol, &ptr_guid) && g_kmouse_present) {
        *iface = &g_kptr;
        return EFI_SUCCESS;
    }

    *iface = NULL;

    return K_EFI_NOT_FOUND;
}


/* --- ConIn --- */

static EFI_STATUS EFIAPI kconin_reset(
    SIMPLE_INPUT_INTERFACE *this_in,
    BOOLEAN extended
)
{
    (void)this_in;
    (void)extended;

    g_kbd_q_head = 0;
    g_kbd_q_tail = 0;

    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI kconin_read_key(
    SIMPLE_INPUT_INTERFACE *this_in,
    EFI_INPUT_KEY *key
)
{
    (void)this_in;

    kcon_flush();
    kernel_poll_input();

    if (key == NULL)
        return K_EFI_INVALID_PARAMETER;

    if (kbd_dequeue(key))
        return EFI_SUCCESS;

    return K_EFI_NOT_READY;
}


/* --- Simple Pointer (мышь для GUI) --- */

static EFI_STATUS EFIAPI kptr_reset(
    EFI_SIMPLE_POINTER_PROTOCOL *this_ptr,
    BOOLEAN extended
)
{
    (void)this_ptr;
    (void)extended;

    g_kmouse_dx = 0;
    g_kmouse_dy = 0;
    g_kmouse_dz = 0;

    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI kptr_get_state(
    EFI_SIMPLE_POINTER_PROTOCOL *this_ptr,
    EFI_SIMPLE_POINTER_STATE *state
)
{
    (void)this_ptr;

    kcon_flush();
    kernel_poll_input();

    if (state == NULL)
        return K_EFI_INVALID_PARAMETER;

    state->RelativeMovementX = (INT32)g_kmouse_dx;
    state->RelativeMovementY = (INT32)g_kmouse_dy;
    state->RelativeMovementZ = (INT32)g_kmouse_dz;
    state->LeftButton = (g_kmouse_buttons & 0x1u) ? TRUE : FALSE;
    state->RightButton = (g_kmouse_buttons & 0x2u) ? TRUE : FALSE;

    g_kmouse_dx = 0;
    g_kmouse_dy = 0;
    g_kmouse_dz = 0;

    return EFI_SUCCESS;
}


static void kx_install_shims(EFI_SYSTEM_TABLE *fw_st)
{
    /* Boot Services: сначала ВСЁ = "не поддерживается" (чтобы
       ни одно поле не осталось NULL - вызов по NULL был бы
       крахом), потом наши реализации поверх. Сами поля в efi.h
       - VOID* или указатели на функции, все по 8 байт подряд
       после заголовка. */
    VOID **slots =
        (VOID **)((UINT8 *)&g_kbs + sizeof(EFI_TABLE_HEADER));
    UINTN nslots =
        (sizeof(EFI_BOOT_SERVICES) - sizeof(EFI_TABLE_HEADER)) /
        sizeof(VOID *);

    for (UINTN i = 0; i < nslots; i++)
        slots[i] = (VOID *)kbs_unsupported;

    g_kbs.Hdr.Signature = 0x56524553544f4f42ull;  /* "BOOTSERV" */
    g_kbs.Hdr.Revision = 0;
    g_kbs.Hdr.HeaderSize = (UINT32)sizeof(EFI_BOOT_SERVICES);
    g_kbs.Hdr.CRC32 = 0;
    g_kbs.Hdr.Reserved = 0;

    g_kbs.AllocatePages = (VOID *)kbs_allocate_pages;
    g_kbs.FreePages = (VOID *)kbs_free_pages;
    g_kbs.AllocatePool = (VOID *)kbs_allocate_pool;
    g_kbs.FreePool = (VOID *)kbs_free_pool;
    g_kbs.Stall = kbs_stall;
    g_kbs.SetWatchdogTimer = (VOID *)kbs_set_watchdog;
    g_kbs.GetNextMonotonicCount = (VOID *)kbs_get_next_monotonic_count;
    g_kbs.LocateProtocol = kbs_locate_protocol;
    g_kbs.CopyMem = (VOID *)kbs_copy_mem;
    g_kbs.SetMem = (VOID *)kbs_set_mem;

    /* ConIn */
    g_kconin.Reset = kconin_reset;
    g_kconin.ReadKeyStroke = kconin_read_key;
    g_kconin.WaitForKey = NULL;

    /* GOP: копия того, что отдала прошивка, - в нашей памяти */
    g_kgop_info.Version = 0;
    g_kgop_info.HorizontalResolution = g_kfb_w;
    g_kgop_info.VerticalResolution = g_kfb_h;
    g_kgop_info.PixelFormat = g_kfb_fmt;
    g_kgop_info.PixelInformation[0] = 0;
    g_kgop_info.PixelInformation[1] = 0;
    g_kgop_info.PixelInformation[2] = 0;
    g_kgop_info.PixelInformation[3] = 0;
    g_kgop_info.PixelsPerScanLine = g_kfb_stride;

    g_kgop_mode.MaxMode = 1;
    g_kgop_mode.Mode = 0;
    g_kgop_mode.Info = &g_kgop_info;
    g_kgop_mode.SizeOfInfo = sizeof(g_kgop_info);
    g_kgop_mode.FrameBufferBase = (UINT64)(UINTN)g_kfb;
    g_kgop_mode.FrameBufferSize =
        (UINTN)g_kfb_stride * (UINTN)g_kfb_h * 4u;

    g_kgop.QueryMode = (VOID *)kbs_unsupported;
    g_kgop.SetMode = (VOID *)kbs_unsupported;
    g_kgop.Blt = (VOID *)kbs_unsupported;
    g_kgop.Mode = &g_kgop_mode;

    /* Мышь. Resolution = GUI_MOUSE_PIXELS_PER_MM, чтобы GUI
       переводил "счётчики" мыши в пиксели ровно 1:1 */
    g_kptr_mode.ResolutionX = GUI_MOUSE_PIXELS_PER_MM;
    g_kptr_mode.ResolutionY = GUI_MOUSE_PIXELS_PER_MM;
    g_kptr_mode.ResolutionZ = 1;
    g_kptr_mode.LeftButton = TRUE;
    g_kptr_mode.RightButton = TRUE;

    g_kptr.Reset = kptr_reset;
    g_kptr.GetState = kptr_get_state;
    g_kptr.WaitForInput = NULL;
    g_kptr.Mode = &g_kptr_mode;

    /* Сама системная таблица - по полям (не присваиванием
       структуры целиком: компилятор мог бы вставить вызов
       memcpy, которого у нас нет) */
    g_kst.Hdr.Signature = fw_st->Hdr.Signature;
    g_kst.Hdr.Revision = fw_st->Hdr.Revision;
    g_kst.Hdr.HeaderSize = fw_st->Hdr.HeaderSize;
    g_kst.Hdr.CRC32 = 0;
    g_kst.Hdr.Reserved = 0;
    g_kst.FirmwareVendor = fw_st->FirmwareVendor;
    g_kst.FirmwareRevision = fw_st->FirmwareRevision;
    g_kst.ConsoleInHandle = NULL;
    g_kst.ConIn = &g_kconin;
    g_kst.ConsoleOutHandle = NULL;
    g_kst.ConOut = &g_kcon_out;
    g_kst.StandardErrorHandle = NULL;
    g_kst.StdErr = &g_kcon_out;
    g_kst.RuntimeServices = fw_st->RuntimeServices;
    g_kst.BootServices = &g_kbs;
    g_kst.NumberOfTableEntries = fw_st->NumberOfTableEntries;
    g_kst.ConfigurationTable = fw_st->ConfigurationTable;
}


/* ================================================================
 * 8. Сам переход: команда "ebs"
 * ================================================================ */

static void kernel_ebs_and_enter(EFI_SYSTEM_TABLE *st)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out = st->ConOut;

    if (g_kernel_mode) {
        print(out, "Already running without Boot Services.\n");
        return;
    }

    print(out, "=== Leaving the firmware for good ===\n");
    print(out, "ExitBootServices will be called, then MyOS brings up its\n");
    print(out, "own console, interrupts, timer, memory manager and USB/PS2\n");
    print(out, "input, and returns to this same shell - without firmware.\n\n");

    /* --- 1. экран --- */
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    if (
        !st->BootServices->LocateProtocol ||
        st->BootServices->LocateProtocol(
            &gop_guid, NULL, (VOID **)&gop
        ) != EFI_SUCCESS ||
        !gop || !gop->Mode || !gop->Mode->Info ||
        gop->Mode->FrameBufferBase == 0
    ) {
        print(out, "No usable Graphics Output Protocol framebuffer - aborting\n");
        print(out, "(nothing to draw the console on after the exit).\n");
        return;
    }

    if (gop->Mode->Info->PixelFormat == PixelBltOnly) {
        print(out, "GOP is Blt-only (no direct framebuffer) - aborting.\n");
        return;
    }

    g_kfb = (volatile UINT32 *)(UINTN)gop->Mode->FrameBufferBase;
    g_kfb_w = gop->Mode->Info->HorizontalResolution;
    g_kfb_h = gop->Mode->Info->VerticalResolution;
    g_kfb_stride = gop->Mode->Info->PixelsPerScanLine;
    g_kfb_fmt = gop->Mode->Info->PixelFormat;

    print(out, "Framebuffer: ");
    print_uint(out, g_kfb_w);
    print(out, "x");
    print_uint(out, g_kfb_h);
    print(out, " at 0x");
    print_hex(out, (UINT64)(UINTN)g_kfb, 16);
    print(out, "\n");

    /* --- 2. xHCI: то, что требует Boot Services, - сейчас --- */
    UINT8 xb = 0, xd = 0, xf = 0;
    UINT64 xm = 0;

    g_kx.present = FALSE;

    if (pci_find_xhci(&xb, &xd, &xf, &xm) && xm != 0) {

        print(out, "xHCI found - asking firmware to release it...\n");

        pci_enable_device(xb, xd, xf);
        xhci_disconnect_firmware_driver(st, out, xb, xd, xf);
        xhci_read_cap_regs(xm, &g_kx.cap);
        xhci_bios_handoff(st, xm, g_kx.cap.ExtCapOff, out);

        if (g_kx.cap.CapLength != 0 || g_kx.cap.HciVersion != 0) {

            g_kx.present = TRUE;
            g_kx.bus = xb;
            g_kx.devn = xd;
            g_kx.func = xf;
            g_kx.mmio = xm;
        }

    } else {

        print(out, "No xHCI controller on the PCI bus.\n");
    }

    /* --- 3. TSC по Stall прошивки (пока он есть) - как
       контрольный замер для калибровки по PIT ниже --- */
    {
        UINT64 t0 = rdtsc();
        st->BootServices->Stall(50000);
        UINT64 t1 = rdtsc();

        g_tsc_hz_stall = (t1 - t0) * 20u;
    }

    print(out, "\nStarting in 2 seconds...\n");
    st->BootServices->Stall(2000000);

    /* --- 4. карта памяти + ExitBootServices --- */
    GUI_GET_MEMORY_MAP get_map =
        (GUI_GET_MEMORY_MAP)st->BootServices->GetMemoryMap;
    GUI_ALLOCATE_POOL alloc_pool =
        (GUI_ALLOCATE_POOL)st->BootServices->AllocatePool;
    GUI_EXIT_BOOT_SERVICES exit_bs =
        (GUI_EXIT_BOOT_SERVICES)st->BootServices->ExitBootServices;

    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_ver = 0;

    get_map(&map_size, NULL, &map_key, &desc_size, &desc_ver);

    map_size += desc_size * 16u;

    VOID *map_buf = NULL;

    if (alloc_pool(GUI_EFI_BOOT_SERVICES_DATA, map_size, &map_buf) != EFI_SUCCESS) {
        print(out, "Could not allocate the memory map buffer - aborting.\n");
        return;
    }

    BOOLEAN exited = FALSE;
    UINTN final_size = 0;

    for (UINTN attempt = 0; attempt < 8; attempt++) {

        UINTN this_size = map_size;

        if (get_map(&this_size, map_buf, &map_key, &desc_size, &desc_ver)
                != EFI_SUCCESS)
            break;

        if (exit_bs(g_image_handle, map_key) == EFI_SUCCESS) {
            exited = TRUE;
            final_size = this_size;
            break;
        }
    }

    if (!exited) {
        print(out, "ExitBootServices failed - still in firmware mode, the\n");
        print(out, "shell keeps working as before.\n");
        return;
    }

    /*
     * ======== Прошивки больше нет. ========
     * С этой строки: никаких st->ConOut/ConIn/BootServices.
     * Первым делом - сохранить карту памяти (буфер лежит в
     * памяти, которую прошивка больше не охраняет) и поднять
     * консоль, чтобы было куда писать.
     */
    kx_cli();

    pmm_save_map(map_buf, final_size, desc_size);

    kcon_init();

    out = &g_kcon_out;

    set_color(out, 0x0B);
    print(out, "MyOS kernel mode - ExitBootServices: OK\n");
    set_color(out, 0x07);
    print(out, "Everything below runs on MyOS's own code, no firmware.\n\n");

    /* --- 5. CPU --- */
    set_color(out, 0x0E);
    print(out, "[cpu]\n");
    set_color(out, 0x07);

    kx_load_gdt();
    print(out, "  GDT loaded (code 0x08, data 0x10)\n");

    kx_load_idt();
    print(out, "  IDT loaded: 256 vectors, exceptions -> panic screen\n");

    kx_pic_disable();
    print(out, "  8259 PIC remapped to 0x20-0x2F and masked\n");

    UINTN ioapic_n = kx_ioapic_mask_all();

    if (ioapic_n > 0) {
        print(out, "  I/O APIC: ");
        print_uint(out, ioapic_n);
        print(out, " redirection entries masked\n");
    } else {
        print(out, "  I/O APIC: not found at 0xFEC00000 (skipped)\n");
    }

    /* --- 6. время --- */
    set_color(out, 0x0E);
    print(out, "[time]\n");
    set_color(out, 0x07);

    g_tsc_hz_pit = kx_pit_measure_tsc_hz();

    print(out, "  TSC by PIT (8254):      ");
    if (g_tsc_hz_pit >= 1000000ull) {
        print_uint(out, g_tsc_hz_pit / 1000000u);
        print(out, " MHz\n");
    } else {
        print(out, "PIT does not respond\n");
    }

    print(out, "  TSC by firmware Stall:  ");
    print_uint(out, g_tsc_hz_stall / 1000000u);
    print(out, " MHz (measured before the exit)\n");

    /* PIT - основной, firmware-независимый эталон. Если он
       молчит или явно расходится с контрольным замером - берём
       контрольный. */
    BOOLEAN pit_ok = FALSE;

    if (g_tsc_hz_pit >= 100000000ull && g_tsc_hz_pit <= 20000000000ull) {

        UINT64 a = g_tsc_hz_pit;
        UINT64 bref = g_tsc_hz_stall;

        if (bref == 0 || (a * 4u > bref * 3u && a * 3u < bref * 4u))
            pit_ok = TRUE;
    }

    if (pit_ok) {
        g_tsc_hz = g_tsc_hz_pit;
        g_tsc_source = "PIT 8254";
    } else if (g_tsc_hz_stall >= 100000000ull) {
        g_tsc_hz = g_tsc_hz_stall;
        g_tsc_source = "firmware Stall (PIT unusable)";
    } else {
        g_tsc_hz = 2000000000ull;
        g_tsc_source = "GUESS 2 GHz (no reference worked!)";
    }

    g_kboot_tsc = rdtsc();

    print(out, "  Using TSC = ");
    print_uint(out, g_tsc_hz / 1000000u);
    print(out, " MHz, source: ");
    print(out, g_tsc_source);
    print(out, "\n");

    if (kx_lapic_timer_start(out)) {

        kx_sti();

        UINT64 t_before = g_kticks;

        tsc_delay_us(50000);

        UINT64 got = g_kticks - t_before;

        g_ktimer_ok = (got >= 20u);

        print(out, "  Interrupts ON. Timer ticks in 50 ms: ");
        print_uint(out, got);
        print(out, g_ktimer_ok ? " (1 kHz tick is alive)\n" :
                                 " - TIMER NOT FIRING, using TSC only\n");
    }

    /* --- 7. память --- */
    set_color(out, 0x0E);
    print(out, "[memory]\n");
    set_color(out, 0x07);

    print(out, "  Final memory map: ");
    print_uint(out, g_kmm_map_count);
    print(out, " regions");

    if (g_kmm_map_dropped) {
        print(out, " (");
        print_uint(out, g_kmm_map_dropped);
        print(out, " dropped - table full)");
    }

    print(out, "\n");

    if (pmm_init()) {

        print(out, "  Page allocator: ");
        print_uint(out, (g_kmm_free_pages * 4u) / 1024u);
        print(out, " MiB free of ");
        print_uint(out, (g_kmm_usable_pages * 4u) / 1024u);
        print(out, " MiB usable, bitmap ");
        print_uint(out, g_kmm_bitmap_pages);
        print(out, " page(s) at 0x");
        print_hex(out, g_kmm_bitmap_phys, 8);
        print(out, "\n");

    } else {

        set_color(out, 0x0C);
        print(out, "  Page allocator FAILED to initialize - no free memory?\n");
        set_color(out, 0x07);
    }

    /* --- 8. ввод --- */
    set_color(out, 0x0E);
    print(out, "[input]\n");
    set_color(out, 0x07);

    ps2_init(out);

    if (g_kmm_ready)
        kx_usb_start(out);
    else
        print(out, "  USB skipped (no memory allocator)\n");

    /* --- 9. подмена системной таблицы и возврат в шелл --- */
    kx_install_shims(st);

    g_st = &g_kst;
    g_kernel_mode = TRUE;

    /* прокрутка истории (PageUp) - на всю высоту нашей консоли */
    if (g_kcon_rows > 2)
        g_scrollback_visible_rows = g_kcon_rows - 1;

    set_color(out, 0x0A);
    print(out, "\nDone. Back to the shell - now on MyOS drivers only.\n");
    set_color(out, 0x07);
    print(out, "Try: kinfo, usb, mem, start (GUI with the USB mouse), int3.\n\n");
}


/* ================================================================
 * 9. Команды: kinfo, usb, mem, int3
 * ================================================================ */

static void kernel_cmd_kinfo(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_kernel_mode) {
        print(out, "Mode: firmware (UEFI Boot Services active).\n");
        print(out, "Run 'ebs' to switch to MyOS's own kernel mode.\n");
        return;
    }

    print(out, "Mode: kernel (no Boot Services)\n");

    print(out, "Uptime in kernel mode: ");
    {
        UINT64 us = kx_uptime_us();
        print_uint(out, us / 1000000u);
        print(out, ".");
        UINT64 ms = (us / 1000u) % 1000u;
        if (ms < 100) print(out, "0");
        if (ms < 10) print(out, "0");
        print_uint(out, ms);
        print(out, " s\n");
    }

    print(out, "TSC: ");
    print_uint(out, g_tsc_hz / 1000000u);
    print(out, " MHz (");
    print(out, g_tsc_source);
    print(out, ")\n");

    print(out, "LAPIC timer: ");
    print(out, g_ktimer_ok ? "running, 1000 Hz, " : "NOT running, ");
    print_uint(out, g_kticks);
    print(out, " ticks\n");

    print(out, "Interrupts: spurious=");
    print_uint(out, g_kspurious);
    print(out, " unexpected=");
    print_uint(out, g_kstray);

    if (g_kstray) {
        print(out, " (last vector 0x");
        print_hex(out, g_kstray_last, 2);
        print(out, ")");
    }

    print(out, " breakpoints=");
    print_uint(out, g_kbreakpoints);
    print(out, "\n");

    print(out, "Memory: ");
    print_uint(out, (g_kmm_free_pages * 4u) / 1024u);
    print(out, " MiB free, pool blocks in use: ");
    print_uint(out, g_kpool_allocs);
    print(out, "\n");

    print(out, "Keyboard: ");
    print_uint(out, g_kbd_keys_total);
    print(out, " keys, PS/2 ");
    print(out, g_ps2_present ? "present (" : "absent");
    if (g_ps2_present) {
        print_uint(out, g_ps2_bytes);
        print(out, " bytes)");
    }
    print(out, ", CapsLock ");
    print(out, g_kbd_caps ? "ON" : "off");
    print(out, "\n");

    print(out, "Mouse: ");
    print(out, g_kmouse_present ? "present, " : "none, ");
    print_uint(out, g_kmouse_reports);
    print(out, " reports\n");

    print(out, "Console: ");
    print_uint(out, g_kcon_cols);
    print(out, "x");
    print_uint(out, g_kcon_rows);
    print(out, " chars on ");
    print_uint(out, g_kfb_w);
    print(out, "x");
    print_uint(out, g_kfb_h);
    print(out, " framebuffer\n");
}


/* Состояние конечной точки ГЛАЗАМИ КОНТРОЛЛЕРА - поле EP State
   в выходном Device Context (контроллер сам его обновляет) */
static const char *kx_ep_hw_state(KX_HID *h)
{
    KX_DEV *d = &g_kx_devs[h->dev];

    if (d->dev_ctx == 0)
        return "?";

    volatile UINT32 *ep =
        (volatile UINT32 *)(UINTN)
            (d->dev_ctx + (UINT64)h->dci * g_kx.ctx_size);

    switch (ep[0] & 0x7u) {
    case 0:  return "Disabled";
    case 1:  return "Running";
    case 2:  return "HALTED";
    case 3:  return "Stopped";
    case 4:  return "ERROR";
    default: return "?";
    }
}

static const char *kx_sw_state(UINT8 st)
{
    switch (st) {
    case KX_EP_RUN:    return "run";
    case KX_EP_RESET:  return "recovering(reset)";
    case KX_EP_SETDEQ: return "recovering(set-deq)";
    case KX_EP_DEAD:   return "DEAD";
    default:           return "?";
    }
}

/* Строка про контроллер целиком: USBSTS - HCHalted (бит 0), Host
   System Error (бит 2), Host Controller Error (бит 12). HSE/HCE
   означают, что контроллер сам встал из-за внутренней ошибки -
   тогда не работает вообще ничего на USB. */
static void kx_print_hc_status(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT32 sts = mmio_read32(g_kx.op + 0x04);

    print(out, "xHCI USBSTS=0x");
    print_hex(out, sts, 8);

    if (sts & 0x1u)    print(out, " HALTED");
    if (sts & 0x4u)    print(out, " HOST-SYSTEM-ERROR");
    if (sts & 0x1000u) print(out, " HOST-CONTROLLER-ERROR");
    if (!(sts & 0x1005u)) print(out, " (ok)");

    print(out, "  events ");
    print_uint(out, g_kx.events);
    print(out, "  unmatched ");
    print_uint(out, g_kx.stray_events);
    print(out, "  port-changes ");
    print_uint(out, g_kx.port_events);
    print(out, "\n");
}

static void kx_print_hid_line(SIMPLE_TEXT_OUTPUT_INTERFACE *out, KX_HID *h)
{
    print(out, "    if ");
    print_uint(out, h->iface);
    print(out, ": ");
    print(out, kx_role_name(h->role));

    if (h->mode_note != NULL && h->mode_note[0] != '\0') {
        print(out, " - ");
        print(out, h->mode_note);
    }
    print(out, "\n      reports ");
    print_uint(out, h->reports);

    if (h->role != KX_ROLE_KBD_BOOT) {
        print(out, " (ignored ");
        print_uint(out, h->rejected);
        print(out, ")");
    }

    print(out, "  errors ");
    print_uint(out, h->errors);
    print(out, " (streak ");
    print_uint(out, h->err_streak);
    print(out, ", last cc=");
    print_uint(out, h->last_err);
    print(out, ")  recoveries ");
    print_uint(out, h->recoveries);
    print(out, " (last cmd cc=");
    print_uint(out, h->last_cmd_cc);
    print(out, ")\n      driver state: ");
    print(out, kx_sw_state(h->state));
    print(out, "   controller EP state: ");
    print(out, kx_ep_hw_state(h));
    print(out, "   EP 0x");
    print_hex(out, h->ep_addr, 2);
    print(out, " maxpkt ");
    print_uint(out, h->maxpkt);
    print(out, " bInterval ");
    print_uint(out, h->interval_raw);
    print(out, "\n      last report (");
    print_uint(out, h->last_len);
    print(out, " bytes):");

    UINTN n = h->last_len;

    if (n > 16)
        n = 16;

    for (UINTN k = 0; k < n; k++) {
        print(out, " ");
        print_hex(out, h->last_rep[k], 2);
    }

    print(out, "\n");

    if (h->role == KX_ROLE_MOUSE_RPT) {

        HID_MOUSE_REPORT_LAYOUT *L = &h->layout;

        print(out, "      layout: ");

        if (L->has_report_id) {
            print(out, "ID=");
            print_uint(out, L->report_id);
            print(out, " ");
        }

        print(out, "X@");
        print_uint(out, L->x_bit_offset);
        print(out, "/");
        print_uint(out, L->x_bit_size);
        print(out, L->x_is_relative ? "rel" : "ABS");
        print(out, " Y@");
        print_uint(out, L->y_bit_offset);
        print(out, "/");
        print_uint(out, L->y_bit_size);
        print(out, L->y_is_relative ? "rel" : "ABS");

        if (L->has_buttons) {
            print(out, " btn@");
            print_uint(out, L->button_bit_offset);
            print(out, "x");
            print_uint(out, L->button_count);
        }

        if (L->has_wheel) {
            print(out, " wheel@");
            print_uint(out, L->wheel_bit_offset);
        }

        print(out, "\n");
    }
}


/*
 * mousetest - живая диагностика мыши (и вообще USB-ввода):
 * экран обновляется 4 раза в секунду, двигай мышь и смотри, что
 * происходит. Выход - любая клавиша. Сделано специально для
 * отладки на реальном железе по одному скриншоту: видно, идут ли
 * отчёты, есть ли ошибки, в каком состоянии конечная точка у
 * драйвера и у самого контроллера, и что лежит в последнем отчёте
 * байт в байт.
 */
static void kernel_cmd_mousetest(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_kernel_mode) {
        print(out, "Only in kernel mode (after 'ebs').\n");
        return;
    }

    INT64 px = (INT64)g_kfb_w / 2;
    INT64 py = (INT64)g_kfb_h / 2;
    UINT64 next = 0;

    g_kmouse_dx = 0;
    g_kmouse_dy = 0;
    g_kmouse_dz = 0;

    /* выкинуть уже накопленные клавиши (в т.ч. Enter от самой
       команды) */
    {
        EFI_INPUT_KEY k;
        kernel_poll_input();
        while (kbd_dequeue(&k)) { }
    }

    /* Курсор поверх текста: где нарисован сейчас (-1 = нигде) */
    INTN drawn_x = -1;
    INTN drawn_y = -1;

    for (;;) {

        kernel_poll_input();

        px += g_kmouse_dx;
        py += g_kmouse_dy;
        g_kmouse_dx = 0;
        g_kmouse_dy = 0;

        if (px < 0) px = 0;
        if (py < 0) py = 0;
        if (px > (INT64)g_kfb_w - GUI_CURSOR_SIZE)
            px = (INT64)g_kfb_w - GUI_CURSOR_SIZE;
        if (py > (INT64)g_kfb_h - GUI_CURSOR_SIZE)
            py = (INT64)g_kfb_h - GUI_CURSOR_SIZE;

        EFI_INPUT_KEY k;

        if (kbd_dequeue(&k))
            break;

        UINT64 now = rdtsc();

        if (now < next) {

            /* Курсор двигаем сразу, не дожидаясь обновления
               текста: стираем старый (перерисовав клетки
               консоли под ним) и рисуем новый */
            if ((INTN)px != drawn_x || (INTN)py != drawn_y) {

                if (drawn_x >= 0) {

                    for (INTN yy = drawn_y; yy < drawn_y + GUI_CURSOR_SIZE; yy += 4) {
                        for (INTN xx = drawn_x; xx < drawn_x + GUI_CURSOR_SIZE; xx += 4) {

                            if (xx < (INTN)g_kcon_x0 || yy < (INTN)g_kcon_y0)
                                continue;

                            UINTN c = ((UINTN)xx - g_kcon_x0) / (8u * g_kcon_scale);
                            UINTN r = ((UINTN)yy - g_kcon_y0) / (16u * g_kcon_scale);

                            kcon_draw_cell(r, c, FALSE);
                        }
                    }

                    /* правый/нижний край курсора */
                    for (INTN yy = drawn_y; yy < drawn_y + GUI_CURSOR_SIZE; yy += 4) {
                        INTN xx = drawn_x + GUI_CURSOR_SIZE - 1;
                        if (xx >= (INTN)g_kcon_x0 && yy >= (INTN)g_kcon_y0)
                            kcon_draw_cell(((UINTN)yy - g_kcon_y0) / (16u * g_kcon_scale),
                                           ((UINTN)xx - g_kcon_x0) / (8u * g_kcon_scale), FALSE);
                    }
                    for (INTN xx = drawn_x; xx < drawn_x + GUI_CURSOR_SIZE; xx += 4) {
                        INTN yy = drawn_y + GUI_CURSOR_SIZE - 1;
                        if (xx >= (INTN)g_kcon_x0 && yy >= (INTN)g_kcon_y0)
                            kcon_draw_cell(((UINTN)yy - g_kcon_y0) / (16u * g_kcon_scale),
                                           ((UINTN)xx - g_kcon_x0) / (8u * g_kcon_scale), FALSE);
                    }
                }

                gui_draw_cursor_at(
                    g_kfb, g_kfb_stride, g_kfb_w, g_kfb_h, g_kfb_fmt,
                    (INTN)px, (INTN)py
                );

                drawn_x = (INTN)px;
                drawn_y = (INTN)py;
            }

            cpu_pause();
            continue;
        }

        next = now + (g_tsc_hz / 1000u) * 250u;

        kcon_clear_screen(out);

        print(out, "MOUSETEST - move the mouse, press any key to exit.\n");
        print(out, "Take a screenshot if the mouse stops.\n\n");

        print(out, "position x=");
        print_int(out, (INT32)px);
        print(out, " y=");
        print_int(out, (INT32)py);
        print(out, "  buttons=0x");
        print_hex(out, g_kmouse_buttons, 2);
        print(out, "  mouse reports total ");
        print_uint(out, g_kmouse_reports);
        print(out, "  uptime ");
        print_uint(out, kx_uptime_us() / 1000000u);
        print(out, " s\n\n");

        if (!g_kx.running) {
            print(out, "USB driver is not running.\n");
        } else {
            kx_print_hc_status(out);

            for (UINTN i = 0; i < KX_MAX_DEVS; i++) {

                KX_DEV *d = &g_kx_devs[i];

                if (!d->used)
                    continue;

                print(out, "Port ");
                print_uint(out, d->port);
                print(out, " ");
                print_hex(out, d->vid, 4);
                print(out, ":");
                print_hex(out, d->pid, 4);
                print(out, " - ");
                print(out, d->status ? d->status : "?");
                print(out, "\n");

                for (UINTN j = 0; j < KX_MAX_HID; j++) {

                    KX_HID *h = &g_kx_hid[j];

                    if (h->used && h->dev == i)
                        kx_print_hid_line(out, h);
                }
            }
        }

        kcon_flush();

        /* экран перерисован целиком - курсор тоже заново */
        gui_draw_cursor_at(
            g_kfb, g_kfb_stride, g_kfb_w, g_kfb_h, g_kfb_fmt,
            (INTN)px, (INTN)py
        );

        drawn_x = (INTN)px;
        drawn_y = (INTN)py;
    }

    /* убрать курсор с экрана консоли */
    kcon_clear_screen(out);

    print(out, "\n");
}


static void kernel_cmd_usb(SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    if (!g_kernel_mode) {
        print(out, "The MyOS USB driver starts with 'ebs' (it needs the\n");
        print(out, "controller for itself). Use 'xhci' for a read-only look.\n");
        return;
    }

    if (!g_kx.running) {
        print(out, "USB driver is not running (no xHCI or init failed).\n");
        return;
    }

    kx_print_hc_status(out);

    UINTN shown = 0;

    for (UINTN i = 0; i < KX_MAX_DEVS; i++) {

        KX_DEV *d = &g_kx_devs[i];

        if (!d->used)
            continue;

        shown++;

        print(out, "Port ");
        print_uint(out, d->port);
        print(out, "  slot ");
        print_uint(out, d->slot);
        print(out, "  ");
        print_hex(out, d->vid, 4);
        print(out, ":");
        print_hex(out, d->pid, 4);
        print(out, "  ");
        print(out, kx_speed_name(d->speed));
        print(out, "  - ");
        print(out, d->status ? d->status : "?");
        print(out, "\n");

        for (UINTN k = 0; k < KX_MAX_HID; k++) {

            KX_HID *h = &g_kx_hid[k];

            if (!h->used || h->dev != i)
                continue;

            kx_print_hid_line(out, h);
        }
    }

    if (shown == 0)
        print(out, "No USB devices were found on the root ports.\n");
}


static void kernel_cmd_mem(EFI_SYSTEM_TABLE *st, SIMPLE_TEXT_OUTPUT_INTERFACE *out)
{
    UINT64 by_type[16];

    for (UINTN i = 0; i < 16; i++)
        by_type[i] = 0;

    if (!g_kernel_mode) {

        /* В режиме прошивки - просто показать её карту памяти */
        GUI_GET_MEMORY_MAP get_map =
            (GUI_GET_MEMORY_MAP)st->BootServices->GetMemoryMap;
        GUI_ALLOCATE_POOL alloc_pool =
            (GUI_ALLOCATE_POOL)st->BootServices->AllocatePool;
        GUI_FREE_POOL free_pool =
            (GUI_FREE_POOL)st->BootServices->FreePool;

        UINTN size = 0, key = 0, dsize = 0;
        UINT32 dver = 0;

        get_map(&size, NULL, &key, &dsize, &dver);
        size += dsize * 8u;

        VOID *buf = NULL;

        if (alloc_pool(GUI_EFI_BOOT_SERVICES_DATA, size, &buf) != EFI_SUCCESS)
            return;

        if (get_map(&size, buf, &key, &dsize, &dver) == EFI_SUCCESS)
            pmm_save_map(buf, size, dsize);

        free_pool(buf);

        print(out, "Firmware memory map (Boot Services still active):\n");
    } else {
        print(out, "Final memory map (as handed over at ExitBootServices):\n");
    }

    for (UINTN i = 0; i < g_kmm_map_count; i++) {

        UINT32 t = g_kmm_map[i].type;

        if (t < 16)
            by_type[t] += g_kmm_map[i].pages;
    }

    for (UINT32 t = 0; t < 16; t++) {

        if (by_type[t] == 0)
            continue;

        print(out, "  ");
        print(out, kmm_type_name(t));
        print(out, ": ");

        UINT64 kib = by_type[t] * 4u;

        if (kib >= 10240u) {
            print_uint(out, kib / 1024u);
            print(out, " MiB\n");
        } else {
            print_uint(out, kib);
            print(out, " KiB\n");
        }
    }

    print(out, "  (");
    print_uint(out, g_kmm_map_count);
    print(out, " regions)\n");

    if (g_kernel_mode && g_kmm_ready) {

        print(out, "\nMyOS page allocator (bitmap, 4 KiB pages):\n  free ");
        print_uint(out, g_kmm_free_pages);
        print(out, " pages (");
        print_uint(out, (g_kmm_free_pages * 4u) / 1024u);
        print(out, " MiB), in use ");
        print_uint(out, g_kmm_usable_pages - g_kmm_free_pages);
        print(out, " pages, bitmap covers ");
        print_uint(out, (g_kmm_total_pages * 4u) / 1024u);
        print(out, " MiB\n  pool blocks in use: ");
        print_uint(out, g_kpool_allocs);
        print(out, "\n");

        /* живая проверка: выделить, записать, освободить */
        UINT64 a = pmm_alloc_pages(4, 0);

        if (a != 0) {

            volatile UINT64 *q = (volatile UINT64 *)(UINTN)a;
            q[0] = 0x1122334455667788ull;
            BOOLEAN ok = (q[0] == 0x1122334455667788ull);

            pmm_free_pages(a, 4);

            print(out, "  self-test: 4 pages at 0x");
            print_hex(out, a, 8);
            print(out, ok ? " - allocated, written, freed: OK\n" :
                            " - WRITE CHECK FAILED\n");
        } else {
            print(out, "  self-test: allocation FAILED\n");
        }
    }
}


/* ============================================================
 * Command dispatcher
 * ============================================================ */

static void run_command(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *line
)
{
    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        st->ConOut;


    push_history(line);


    /* --------------------------------------------------------
     * Команды, которым нужна прошивка (Boot Services) или
     * которые сбросили бы xHCI-контроллер из-под нашего же
     * работающего USB-драйвера - в kernel mode запрещены.
     * -------------------------------------------------------- */

    if (
        g_kernel_mode &&
        (streq(line, "xhci") || streq(line, "ebsdemo"))
    ) {

        print(
            out,
            "Not available in kernel mode: this command needs UEFI\n"
            "Boot Services, or would reset the USB controller that\n"
            "MyOS's own driver is using right now. See 'usb'.\n"
        );

        return;
    }


    /* --------------------------------------------------------
     * Empty
     * -------------------------------------------------------- */

    if (streq(line, "")) {

        return;


    /* --------------------------------------------------------
     * kinfo / usb / mem / int3 - состояние "ядра" (см. блок
     * KERNEL MODE выше)
     * -------------------------------------------------------- */

    } else if (streq(line, "kinfo")) {

        kernel_cmd_kinfo(out);

    } else if (streq(line, "usb")) {

        kernel_cmd_usb(out);

    } else if (streq(line, "mousetest")) {

        kernel_cmd_mousetest(out);

    } else if (streq(line, "mem")) {

        kernel_cmd_mem(st, out);

    } else if (streq(line, "crash")) {

        if (!g_kernel_mode) {

            print(
                out,
                "Only in kernel mode (after 'ebs') - our panic screen\n"
                "is not installed while the firmware is in charge.\n"
            );

        } else {

            print(out, "Executing an invalid instruction (ud2) on purpose...\n");

            /* пиксели консоли - на экран до того, как упадём */
            kcon_flush();

            /* ud2 - "гарантированно неверная инструкция",
               процессор бросает #UD (вектор 6) -> kx_panic */
            __asm__ __volatile__("ud2");
        }

    } else if (streq(line, "int3")) {

        if (!g_kernel_mode) {

            /* обработчик #BP сейчас - прошивочный: в OVMF он
               печатает дамп и вешает машину */
            print(
                out,
                "Only in kernel mode (after 'ebs') - right now the\n"
                "firmware's exception handler is active.\n"
            );

        } else {

            UINT64 before = g_kbreakpoints;

            __asm__ __volatile__("int3");

            if (g_kbreakpoints == before + 1) {

                print(out, "int3 -> our IDT vector 3 handler ran and returned.\n");
                print(out, "Return address (RIP) = 0x");
                print_hex(out, g_kbp_rip, 16);
                print(out, "\n");

            } else {

                print(out, "int3 did not reach our handler?!\n");
            }
        }


    /* --------------------------------------------------------
     * help
     * -------------------------------------------------------- */

    } else if (streq(line, "help")) {

        print(
            out,
            "Available commands:\n"
        );

        print(
            out,
            "  help          - show this list\n"
        );

        print(
            out,
            "  fetch         - show system info (like neofetch/fastfetch)\n"
        );

        print(
            out,
            "  about         - short info about MyOS\n"
        );

        print(
            out,
            "  ver           - print MyOS version\n"
        );

        print(
            out,
            "  banner        - reprint the startup banner\n"
        );

        print(
            out,
            "  time          - show current UEFI time\n"
        );

        print(
            out,
            "  date          - show current UEFI date\n"
        );

        print(
            out,
            "  uptime        - time elapsed since boot\n"
        );

        print(
            out,
            "  echo <text>   - print text back\n"
        );

        print(
            out,
            "  calc <a> <op> <b> - basic calculator (+ - * /)\n"
        );

        print(
            out,
            "  color <0-15>  - change text color (0=black..15=white)\n"
        );

        print(
            out,
            "  clear         - clear the screen\n"
        );

        print(
            out,
            "  ebs           - leave UEFI firmware for good and keep running\n"
            "                  on MyOS drivers (timer, memory, USB/PS2 input)\n"
        );

        print(
            out,
            "  ebsdemo       - old step-by-step xHCI mouse demo (reboots after)\n"
        );

        print(
            out,
            "  kinfo         - kernel status: timer, interrupts, input\n"
        );

        print(
            out,
            "  usb           - USB devices found by MyOS's own driver\n"
        );

        print(
            out,
            "  mousetest     - live USB mouse/keyboard diagnostics (kernel mode)\n"
        );

        print(
            out,
            "  mem           - memory map and page allocator status\n"
        );

        print(
            out,
            "  int3          - test our interrupt table (kernel mode)\n"
        );

        print(
            out,
            "  crash         - trigger a CPU exception -> panic screen\n"
            "                  (kernel mode; the machine halts after it)\n"
        );

        print(
            out,
            "  lspci         - scan PCI bus via raw ports, find USB controllers\n"
        );

        print(
            out,
            "  xhci          - xHCI: reset, Enable Slot, Address Device,\n"
            "                  GET_DESCRIPTOR (real USB VID/PID)\n"
        );

        print(
            out,
            "  history       - show recently run commands\n"
        );

        print(
            out,
            "  whoami        - print current user\n"
        );

        print(
            out,
            "  sleep <sec>   - pause for N seconds\n"
        );

        print(
            out,
            "  --- files (RAM disk, cleared on reboot) ---\n"
        );

        print(
            out,
            "  ls            - list files\n"
        );

        print(
            out,
            "  touch <name>  - create an empty file\n"
        );

        print(
            out,
            "  cat <name>    - print file contents\n"
        );

        print(
            out,
            "  write <n> <t> - overwrite file with text\n"
        );

        print(
            out,
            "  append <n> <t>- append text to file\n"
        );

        print(
            out,
            "  edit <name>   - multi-line editor, end with '.'\n"
        );

        print(
            out,
            "  rm <name>     - delete a file\n"
        );

        print(
            out,
            "  mv <a> <b>    - rename a file\n"
        );

        print(
            out,
            "  cp <a> <b>    - copy a file\n"
        );

        print(
            out,
            "  size <name>   - show file size in bytes\n"
        );

        print(
            out,
            "  start         - launch GUI desktop (icons, taskbar clock, arrows+Enter)\n"
        );

        print(
            out,
            "  reboot        - cold reboot\n"
        );

        print(
            out,
            "  shutdown      - power off the machine\n"
        );

        print(
            out,
            "  exit          - same as shutdown\n"
        );


    /* --------------------------------------------------------
     * whoami
     * -------------------------------------------------------- */

    } else if (streq(line, "whoami")) {

        print(
            out,
            "root@myos\n"
        );


    /* --------------------------------------------------------
     * history
     * -------------------------------------------------------- */

    } else if (streq(line, "history")) {

        cmd_history(st);


    /* --------------------------------------------------------
     * sleep
     * -------------------------------------------------------- */

    } else if (starts_with(line, "sleep ")) {

        UINTN secs =
            parse_uint(
                line + 6
            );


        for (UINTN i = 0;
             i < secs;
             i++) {

            st->BootServices->Stall(
                1000000
            );
        }


        print(
            out,
            "Woke up.\n"
        );


    /* --------------------------------------------------------
     * calc
     * -------------------------------------------------------- */

    } else if (starts_with(line, "calc ")) {

        CHAR16 tmp[LINE_MAX];


        char16_copy(
            tmp,
            line + 5,
            LINE_MAX
        );


        cmd_calc(
            st,
            tmp
        );


    /* --------------------------------------------------------
     * ls
     * -------------------------------------------------------- */

    } else if (
        streq(line, "ls") ||
        streq(line, "dir") ||
        streq(line, "files")
    ) {

        cmd_ls(st);


    /* --------------------------------------------------------
     * touch
     * -------------------------------------------------------- */

    } else if (starts_with(line, "touch ")) {

        CHAR16 name[FS_NAME_MAX];


        take_word(
            skip_ws16(line + 6),
            name,
            FS_NAME_MAX
        );


        cmd_touch(
            st,
            name
        );


    /* --------------------------------------------------------
     * cat
     * -------------------------------------------------------- */

    } else if (starts_with(line, "cat ")) {

        CHAR16 name[FS_NAME_MAX];


        take_word(
            skip_ws16(line + 4),
            name,
            FS_NAME_MAX
        );


        cmd_cat(
            st,
            name
        );


    /* --------------------------------------------------------
     * write
     * -------------------------------------------------------- */

    } else if (starts_with(line, "write ")) {

        CHAR16 name[FS_NAME_MAX];


        CHAR16 *rest =
            take_word(
                skip_ws16(line + 6),
                name,
                FS_NAME_MAX
            );


        rest =
            skip_ws16(rest);


        cmd_write(
            st,
            name,
            rest
        );


    /* --------------------------------------------------------
     * append
     * -------------------------------------------------------- */

    } else if (starts_with(line, "append ")) {

        CHAR16 name[FS_NAME_MAX];


        CHAR16 *rest =
            take_word(
                skip_ws16(line + 7),
                name,
                FS_NAME_MAX
            );


        rest =
            skip_ws16(rest);


        cmd_append(
            st,
            name,
            rest
        );


    /* --------------------------------------------------------
     * edit
     * -------------------------------------------------------- */

    } else if (starts_with(line, "edit ")) {

        CHAR16 name[FS_NAME_MAX];


        take_word(
            skip_ws16(line + 5),
            name,
            FS_NAME_MAX
        );


        cmd_edit(
            st,
            name
        );


    /* --------------------------------------------------------
     * rm
     * -------------------------------------------------------- */

    } else if (starts_with(line, "rm ")) {

        CHAR16 name[FS_NAME_MAX];


        take_word(
            skip_ws16(line + 3),
            name,
            FS_NAME_MAX
        );


        cmd_rm(
            st,
            name
        );


    /* --------------------------------------------------------
     * mv
     * -------------------------------------------------------- */

    } else if (starts_with(line, "mv ")) {

        CHAR16 a[FS_NAME_MAX];
        CHAR16 b[FS_NAME_MAX];


        CHAR16 *rest =
            take_word(
                skip_ws16(line + 3),
                a,
                FS_NAME_MAX
            );


        rest =
            skip_ws16(rest);


        take_word(
            rest,
            b,
            FS_NAME_MAX
        );


        cmd_mv(
            st,
            a,
            b
        );


    /* --------------------------------------------------------
     * cp
     * -------------------------------------------------------- */

    } else if (starts_with(line, "cp ")) {

        CHAR16 a[FS_NAME_MAX];
        CHAR16 b[FS_NAME_MAX];


        CHAR16 *rest =
            take_word(
                skip_ws16(line + 3),
                a,
                FS_NAME_MAX
            );


        rest =
            skip_ws16(rest);


        take_word(
            rest,
            b,
            FS_NAME_MAX
        );


        cmd_cp(
            st,
            a,
            b
        );


    /* --------------------------------------------------------
     * size
     * -------------------------------------------------------- */

    } else if (starts_with(line, "size ")) {

        CHAR16 name[FS_NAME_MAX];


        take_word(
            skip_ws16(line + 5),
            name,
            FS_NAME_MAX
        );


        int idx =
            fs_find(name);


        if (idx < 0) {

            print(
                out,
                "No such file.\n"
            );

        } else {

            print_uint(
                out,
                g_fs[idx].size
            );

            print(
                out,
                " bytes\n"
            );
        }


    /* --------------------------------------------------------
     * fetch
     * -------------------------------------------------------- */

    } else if (
        streq(line, "fetch") ||
        streq(line, "neofetch") ||
        streq(line, "fastfetch")
    ) {

        cmd_fetch(st);


    /* --------------------------------------------------------
     * about
     * -------------------------------------------------------- */

    } else if (streq(line, "about")) {

        print(
            out,
            "MyOS 0.1 - a minimal 64-bit UEFI OS built from scratch.\n"
        );

        print(
            out,
            "No Linux, no Windows, no GNU-EFI/EDK2 - just gcc + ld.\n"
        );

        print(
            out,
            "Type 'fetch' for a system summary or 'help' for commands.\n"
        );


    /* --------------------------------------------------------
     * version
     * -------------------------------------------------------- */

    } else if (
        streq(line, "ver") ||
        streq(line, "version")
    ) {

        print(
            out,
            "MyOS 0.1\n"
        );


    /* --------------------------------------------------------
     * banner
     * -------------------------------------------------------- */

    } else if (streq(line, "banner")) {

        print(
            out,
            "================================\n"
        );

        print(
            out,
            "   MyOS 0.1 - kernel base\n"
        );

        print(
            out,
            "   64-bit UEFI, no Linux/Windows\n"
        );

        print(
            out,
            "================================\n"
        );


    /* --------------------------------------------------------
     * time
     * -------------------------------------------------------- */

    } else if (streq(line, "time")) {

        EFI_TIME now;


        if (
            st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now,
                NULL
            ) == EFI_SUCCESS
        ) {

            print_uint2(
                out,
                now.Hour
            );

            print(
                out,
                ":"
            );

            print_uint2(
                out,
                now.Minute
            );

            print(
                out,
                ":"
            );

            print_uint2(
                out,
                now.Second
            );

            print(
                out,
                "\n"
            );

        } else {

            print(
                out,
                "Time service unavailable.\n"
            );
        }


    /* --------------------------------------------------------
     * date
     * -------------------------------------------------------- */

    } else if (streq(line, "date")) {

        EFI_TIME now;


        if (
            st->RuntimeServices->GetTime &&
            st->RuntimeServices->GetTime(
                &now,
                NULL
            ) == EFI_SUCCESS
        ) {

            print_uint2(
                out,
                now.Day
            );

            print(
                out,
                "-"
            );

            print_uint2(
                out,
                now.Month
            );

            print(
                out,
                "-"
            );

            print_uint(
                out,
                now.Year
            );

            print(
                out,
                "\n"
            );

        } else {

            print(
                out,
                "Date service unavailable.\n"
            );
        }


    /* --------------------------------------------------------
     * uptime
     * -------------------------------------------------------- */

    } else if (streq(line, "uptime")) {

        if (
            !g_have_boot_time ||
            !st->RuntimeServices->GetTime
        ) {

            print(
                out,
                "Uptime unavailable.\n"
            );

        } else {

            EFI_TIME now;


            if (
                st->RuntimeServices->GetTime(
                    &now,
                    NULL
                ) == EFI_SUCCESS
            ) {

                INT64 secs =
                    (INT64)now.Hour * 3600 +
                    (INT64)now.Minute * 60 +
                    now.Second
                    -
                    (
                        (INT64)g_boot_time.Hour * 3600 +
                        (INT64)g_boot_time.Minute * 60 +
                        g_boot_time.Second
                    );


                if (secs < 0)
                    secs += 86400;


                print(
                    out,
                    "up "
                );


                print_uint(
                    out,
                    (UINT64)secs / 3600
                );


                print(
                    out,
                    "h "
                );


                print_uint(
                    out,
                    ((UINT64)secs / 60) % 60
                );


                print(
                    out,
                    "m "
                );


                print_uint(
                    out,
                    (UINT64)secs % 60
                );


                print(
                    out,
                    "s\n"
                );
            }
        }


    /* --------------------------------------------------------
     * echo
     * -------------------------------------------------------- */

    } else if (starts_with(line, "echo ")) {

        /*
         * print16(), чтобы echo попадал
         * в scrollback.
         */
        print16(
            out,
            line + 5
        );

        print(
            out,
            "\n"
        );


    } else if (streq(line, "echo")) {

        print(
            out,
            "\n"
        );


    /* --------------------------------------------------------
     * color
     * -------------------------------------------------------- */

    } else if (starts_with(line, "color ")) {

        UINTN c =
            parse_uint(
                line + 6
            );


        if (c > 15) {

            print(
                out,
                "Usage: color <0-15>\n"
            );

        } else {

            g_color = c;


            set_color(
                out,
                g_color
            );


            print(
                out,
                "Color changed.\n"
            );
        }


    /* --------------------------------------------------------
     * clear
     * -------------------------------------------------------- */

    } else if (
        streq(line, "clear") ||
        streq(line, "cls")
    ) {

        out->ClearScreen(out);


    /* --------------------------------------------------------
     * lspci - Шаг 2: своими руками, через порты 0xCF8/0xCFC,
     * найти на шине PCI все устройства и, отдельно, USB-
     * контроллер(ы). Не использует ни одного UEFI-протокола -
     * тот же самый код будет годиться и после ExitBootServices
     * (см. команду "ebs", которая его туда и подключает).
     * -------------------------------------------------------- */

    } else if (streq(line, "lspci")) {

        UINTN lspci_found = 0;
        UINTN lspci_usb    = 0;

        for (UINTN bus = 0; bus < 256; bus++) {

            for (UINTN dev = 0; dev < 32; dev++) {

                UINT32 id0 =
                    pci_config_read32(
                        (UINT8)bus, (UINT8)dev, 0, 0x00
                    );

                if ((id0 & 0xFFFF) == 0xFFFF)
                    continue;

                UINT32 hdr_dword =
                    pci_config_read32(
                        (UINT8)bus, (UINT8)dev, 0, 0x0C
                    );

                BOOLEAN multi_func =
                    (((hdr_dword >> 16) & 0x80) != 0);

                UINTN max_func = multi_func ? 8 : 1;

                for (UINTN func = 0; func < max_func; func++) {

                    UINT32 id =
                        (func == 0)
                            ? id0
                            : pci_config_read32(
                                  (UINT8)bus, (UINT8)dev,
                                  (UINT8)func, 0x00
                              );

                    if ((id & 0xFFFF) == 0xFFFF)
                        continue;

                    lspci_found++;

                    UINT32 class_dword =
                        pci_config_read32(
                            (UINT8)bus, (UINT8)dev,
                            (UINT8)func, 0x08
                        );

                    UINT8 base_class =
                        PCI_CLASS_DWORD_BASE_CLASS(class_dword);
                    UINT8 sub_class =
                        PCI_CLASS_DWORD_SUB_CLASS(class_dword);
                    UINT8 prog_if =
                        PCI_CLASS_DWORD_PROG_IF(class_dword);

                    print_uint(out, bus);
                    print(out, ":");
                    print_uint(out, dev);
                    print(out, ".");
                    print_uint(out, func);
                    print(out, "  vendor=");
                    print_hex(out, id & 0xFFFF, 4);
                    print(out, " device=");
                    print_hex(out, (id >> 16) & 0xFFFF, 4);
                    print(out, "  class=");
                    print_hex(out, base_class, 2);
                    print(out, " sub=");
                    print_hex(out, sub_class, 2);
                    print(out, " progif=");
                    print_hex(out, prog_if, 2);

                    if (
                        base_class == PCI_CLASS_SERIAL_BUS &&
                        sub_class  == PCI_SUBCLASS_USB
                    ) {

                        lspci_usb++;

                        UINT64 bar =
                            pci_read_bar_address(
                                (UINT8)bus, (UINT8)dev,
                                (UINT8)func, 0x10
                            );

                        if (prog_if == PCI_PROGIF_XHCI) {
                            print(out, "  <-- XHCI (USB3), MMIO=0x");
                        } else if (prog_if == PCI_PROGIF_EHCI) {
                            print(out, "  <-- EHCI (USB2), MMIO=0x");
                        } else if (prog_if == PCI_PROGIF_OHCI) {
                            print(out, "  <-- OHCI (USB1.1), MMIO=0x");
                        } else if (prog_if == PCI_PROGIF_UHCI) {
                            print(out, "  <-- UHCI (USB1.1), MMIO=0x");
                        } else {
                            print(out, "  <-- USB controller, MMIO=0x");
                        }

                        print_hex(out, bar, 16);
                    }

                    print(out, "\n");
                }
            }
        }

        print(out, "\n");
        print_uint(out, lspci_found);
        print(out, " device(s) found, ");
        print_uint(out, lspci_usb);
        print(out, " USB controller(s).\n");

        if (lspci_usb == 0) {

            print(
                out,
                "No USB controller on the PCI bus at all - "
                "this VM/machine has none configured "
                "(in QEMU add e.g. -device qemu-xhci).\n"
            );
        }


    /* --------------------------------------------------------
     * xhci - Шаг 3: не просто найти контроллер на шине PCI
     * (это уже делает lspci), а заговорить с ним самим -
     * включить Memory Space + Bus Master в его PCI Command
     * регистре и прочитать его собственные Capability
     * Registers через MMIO. Отсюда, в частности, берётся
     * MaxPorts - число портов корневого хаба, то есть куда
     * физически может быть воткнута мышь.
     * -------------------------------------------------------- */

    } else if (streq(line, "porttest")) {

        /*
         * ВРЕМЕННАЯ диагностическая команда (отладка
         * зависания в "xhci"): повторяет ТОЧНО ТУ ЖЕ
         * последовательность печати, что и порт-сканер
         * в "xhci" (префикс + hex-значение + "empty"),
         * восемь раз подряд, но БЕЗ единого обращения
         * к PCI/MMIO - только вымышленные, зашитые в
         * код значения.
         *
         * Если это тоже зависнет на 2-3 итерации - значит
         * дело в самой печати (ConOut/scrollback), никак
         * не связано с xHCI. Если пройдёт всё 8 раз без
         * проблем - значит зависание вызывают именно
         * PCI/MMIO-операции xHCI-кода (скорее всего порча
         * памяти где-то в DCBAA/Command Ring/Event Ring
         * страницах).
         */

        print(out, "porttest: repeating print pattern "
                    "8 times, no PCI/MMIO involved\n\n");

        for (UINTN p = 1; p <= 8; p++) {

            print(out, "[DBG] reading port ");
            print_uint(out, p);
            print(out, "\n");

            UINT32 fake_portsc = 0x000202A0;

            print(out, "  Port ");
            print_uint(out, p);
            print(out, ": PORTSC=0x");
            print_hex(out, fake_portsc, 8);
            print(out, "  empty\n");
        }

        print(out, "\nporttest: done, all 8 iterations "
                    "completed.\n");

    } else if (streq(line, "xhci")) {

        /*
         * Эта команда теперь делает только "предполётную"
         * проверку - находит контроллер, включает его в PCI
         * Command register, отбирает его у собственного
         * драйвера прошивки (если он там есть) и показывает
         * Capability Registers. Все эти шаги ОБЯЗАНЫ выполняться
         * до ExitBootServices - для отключения родного драйвера
         * прошивки (xhci_disconnect_firmware_driver) нужны
         * живые Boot Services (LocateHandleBuffer/HandleProtocol/
         * DisconnectController), без них это в принципе
         * невозможно.
         *
         * Весь остальной путь - сброс контроллера, запуск,
         * поиск порта, Enable Slot, Address Device,
         * GET_DESCRIPTOR, SET_CONFIGURATION и опрос отчётов
         * мыши - теперь выполняется ЦЕЛИКОМ ПОСЛЕ
         * ExitBootServices, автоматически, как часть команды
         * "ebs" (см. ниже) - без единого обращения к прошивке.
         */

        UINT8  xbus = 0, xdev = 0, xfunc = 0;
        UINT64 xmmio = 0;

        if (!pci_find_xhci(&xbus, &xdev, &xfunc, &xmmio)) {

            print(
                out,
                "No xHCI controller found on the PCI bus "
                "(run 'lspci' to see what is there).\n"
            );

        } else if (xmmio == 0) {

            print(
                out,
                "xHCI found, but its BAR0 is an I/O-port BAR, "
                "not memory-mapped - this driver only handles "
                "the memory-mapped case.\n"
            );

        } else {

            print(out, "xHCI at ");
            print_uint(out, xbus);
            print(out, ":");
            print_uint(out, xdev);
            print(out, ".");
            print_uint(out, xfunc);
            print(out, ", MMIO base=0x");
            print_hex(out, xmmio, 16);
            print(out, "\n");

            pci_enable_device(xbus, xdev, xfunc);

            print(
                out,
                "Memory Space + Bus Master enabled in PCI "
                "Command register.\n\n"
            );

            print(
                out,
                "--- disconnecting firmware's own driver "
                "from this device (if any) ---\n"
            );

            xhci_disconnect_firmware_driver(
                st, out, xbus, xdev, xfunc
            );

            print(out, "\n");

            XHCI_CAP_INFO cap;
            xhci_read_cap_regs(xmmio, &cap);

            print(out, "CapLength   = ");
            print_uint(out, cap.CapLength);
            print(out, " bytes\n");

            print(out, "HCI Version = ");
            print_hex(out, (cap.HciVersion >> 8) & 0xFF, 2);
            print(out, ".");
            print_hex(out, cap.HciVersion & 0xFF, 2);
            print(out, "  (raw 0x");
            print_hex(out, cap.HciVersion, 4);
            print(out, ")\n");

            print(out, "MaxSlots    = ");
            print_uint(out, cap.MaxSlots);
            print(out, "  (device slots the controller can track)\n");

            print(out, "MaxIntrs    = ");
            print_uint(out, cap.MaxIntrs);
            print(out, "  (interrupter lines)\n");

            print(out, "MaxPorts    = ");
            print_uint(out, cap.MaxPorts);
            print(out, "  (root hub ports - where devices plug in)\n");

            print(out, "DbOff       = 0x");
            print_hex(out, cap.DbOff, 8);
            print(out, "  (Doorbell registers offset)\n");

            print(out, "RtsOff      = 0x");
            print_hex(out, cap.RtsOff, 8);
            print(out, "  (Runtime registers offset)\n");

            print(out, "\n--- USB Legacy Support handoff ---\n");
            xhci_bios_handoff(
                st, xmmio, cap.ExtCapOff, out
            );

            if (cap.CapLength == 0 && cap.HciVersion == 0) {

                print(
                    out,
                    "\nAll zeros - MMIO is not actually "
                    "responding (wrong address, device not "
                    "enabled, or firmware did not map this "
                    "region). This is the next thing to fix, "
                    "not a working controller yet.\n"
                );

            } else {

                print(
                    out,
                    "\nController looks alive. The full "
                    "reset + init + mouse-polling pipeline no "
                    "longer runs from here - it now runs "
                    "automatically after ExitBootServices, as "
                    "part of the 'ebs' command, with zero "
                    "dependency on firmware from that point on. "
                    "Run 'ebs' to see it.\n"
                );
            }
        }


    /* --------------------------------------------------------
     * start (GUI)
     * -------------------------------------------------------- */

    } else if (streq(line, "start")) {

        gui_start(st);


    /* --------------------------------------------------------
     * ebs - Шаг 1 эксперимента "жизнь без Boot Services".
     *
     * Кэширует framebuffer, по-настоящему вызывает
     * ExitBootServices, и доказывает, что ОС жива после этого,
     * рисуя прямо в видеопамять собственным пиксельным шрифтом -
     * без ConOut, без ConIn, без Stall, без LocateProtocol.
     * Назад в этот шелл дороги нет (клавиатура всё равно не
     * будет работать), поэтому демо завершается своей же
     * перезагрузкой через RuntimeServices.
     * -------------------------------------------------------- */

    } else if (streq(line, "ebs")) {

        /* Новый "ebs": выйти из прошивки НАСОВСЕМ и продолжить
           работать - шелл, GUI, мышь, клавиатура уже на своих
           драйверах (см. блок KERNEL MODE выше) */
        kernel_ebs_and_enter(st);


    /* --------------------------------------------------------
     * ebsdemo - старое демо (бывший "ebs"): выход из Boot
     * Services, пошаговый лог xHCI-драйвера одной мыши, 200
     * отчётов, перезагрузка. Оставлено как подробная
     * диагностика - удобно, если новый драйвер на каком-то
     * железе поведёт себя не так.
     * -------------------------------------------------------- */

    } else if (streq(line, "ebsdemo")) {

        print(out, "=== ExitBootServices demo ===\n");
        print(out, "This will permanently exit UEFI Boot Services.\n");
        print(out, "After that: no more keyboard, no more mouse\n");
        print(out, "protocol, no more Stall() - this shell will\n");
        print(out, "not come back. If a USB mouse controller is\n");
        print(out, "found, its ENTIRE driver (reset, init, Enable\n");
        print(out, "Slot, Address Device, GET_DESCRIPTOR, report\n");
        print(out, "polling) now runs AFTER ExitBootServices too -\n");
        print(out, "with zero dependency on firmware from that\n");
        print(out, "point on. Then the machine reboots itself.\n");
        print(out, "Starting in 3 seconds...\n");

        if (st->BootServices->Stall)
            st->BootServices->Stall(3000000);

        /* 1. Найти GOP и закэшировать всё, что понадобится, -
              ПОСЛЕ выхода LocateProtocol звать уже нельзя. */

        EFI_GUID ebs_gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
        EFI_GRAPHICS_OUTPUT_PROTOCOL *ebs_gop = NULL;

        if (
            !st->BootServices->LocateProtocol ||
            st->BootServices->LocateProtocol(
                &ebs_gop_guid, NULL, (VOID **)&ebs_gop
            ) != EFI_SUCCESS ||
            !ebs_gop || !ebs_gop->Mode || !ebs_gop->Mode->Info
        ) {

            print(
                out,
                "No Graphics Output Protocol - aborting, "
                "nothing to draw with after the exit.\n"
            );

            return;
        }

        volatile UINT32 *ebs_fb =
            (volatile UINT32 *)ebs_gop->Mode->FrameBufferBase;

        UINT32 ebs_fb_w = ebs_gop->Mode->Info->HorizontalResolution;
        UINT32 ebs_fb_h = ebs_gop->Mode->Info->VerticalResolution;
        UINT32 ebs_stride = ebs_gop->Mode->Info->PixelsPerScanLine;

        EFI_GRAPHICS_PIXEL_FORMAT ebs_fmt =
            ebs_gop->Mode->Info->PixelFormat;

        /*
         * 1.5. Подготовка xHCI, ПОКА Boot Services ещё живы.
         *
         * Две вещи здесь принципиально обязаны случиться ДО
         * ExitBootServices и никак иначе:
         *
         *   - xhci_disconnect_firmware_driver() пользуется
         *     самими Boot Services (LocateHandleBuffer/
         *     HandleProtocol/DisconnectController), чтобы
         *     попросить прошивку отпустить устройство - без
         *     живых Boot Services это в принципе невозможно
         *     сделать (и было бы поздно уже после выхода -
         *     родной драйвер прошивки к тому моменту либо
         *     мёртв, либо не важен, но ПОКА мы ещё не вышли,
         *     он может конфликтовать с нами за одни и те же
         *     регистры, отсюда и сам смысл этого шага);
         *
         *   - AllocatePages() - Boot Service, после выхода
         *     недоступен вообще. Поэтому все страницы, которые
         *     понадобятся ПОСЛЕ выхода (DCBAA, Command Ring,
         *     Event Ring, ERST, Input/Device Context, EP0 ring,
         *     буфер под дескрипторы, Transfer Ring под
         *     Interrupt IN endpoint - 9 страниц), выделяются
         *     здесь заранее и передаются дальше уже готовыми
         *     физическими адресами.
         *
         * Сам сброс контроллера, запуск, сканирование портов,
         * Enable Slot, Address Device и опрос отчётов мыши -
         * ничего из этого Boot Services не требует (чистый
         * MMIO/порты PCI), поэтому выполняется уже ПОСЛЕ
         * ExitBootServices, через xhci_run_post_ebs() ниже.
         */

        UINT8  ebs_xbus = 0, ebs_xdev = 0, ebs_xfunc = 0;
        UINT64 ebs_xmmio = 0;

        BOOLEAN ebs_xhci_ready = FALSE;
        XHCI_CAP_INFO ebs_cap;

        UINT64 ebs_dcbaa_phys = 0;
        UINT64 ebs_cmdring_phys = 0;
        UINT64 ebs_evring_phys = 0;
        UINT64 ebs_erst_phys = 0;
        UINT64 ebs_input_ctx_phys = 0;
        UINT64 ebs_dev_ctx_phys = 0;
        UINT64 ebs_ep0_ring_phys = 0;
        UINT64 ebs_desc_buf_phys = 0;
        UINT64 ebs_int_ring_phys = 0;

        BOOLEAN ebs_xhci_found =
            pci_find_xhci(
                &ebs_xbus, &ebs_xdev, &ebs_xfunc, &ebs_xmmio
            );

        if (ebs_xhci_found && ebs_xmmio != 0) {

            print(out, "\nxHCI at ");
            print_uint(out, ebs_xbus);
            print(out, ":");
            print_uint(out, ebs_xdev);
            print(out, ".");
            print_uint(out, ebs_xfunc);
            print(out, " - preparing driver before exit...\n");

            pci_enable_device(ebs_xbus, ebs_xdev, ebs_xfunc);

            xhci_disconnect_firmware_driver(
                st, out, ebs_xbus, ebs_xdev, ebs_xfunc
            );

            xhci_read_cap_regs(ebs_xmmio, &ebs_cap);
            xhci_bios_handoff(
                st, ebs_xmmio, ebs_cap.ExtCapOff, out
            );

            GUI_ALLOCATE_PAGES ebs_alloc_pages =
                (GUI_ALLOCATE_PAGES)
                    st->BootServices->AllocatePages;

            if (
                ebs_cap.CapLength == 0 &&
                ebs_cap.HciVersion == 0
            ) {

                print(
                    out,
                    "MMIO is not responding - skipping the "
                    "mouse driver.\n"
                );

            } else if (!ebs_alloc_pages) {

                print(
                    out,
                    "AllocatePages is not available - "
                    "skipping the mouse driver.\n"
                );

            } else if (
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_dcbaa_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_cmdring_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_evring_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_erst_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_input_ctx_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_dev_ctx_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_ep0_ring_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_desc_buf_phys
                ) != EFI_SUCCESS ||
                ebs_alloc_pages(
                    GUI_ALLOCATE_ANY_PAGES,
                    GUI_EFI_BOOT_SERVICES_DATA,
                    1, &ebs_int_ring_phys
                ) != EFI_SUCCESS
            ) {

                print(
                    out,
                    "Could not allocate the pages the driver "
                    "will need after the exit - skipping the "
                    "mouse driver.\n"
                );

            } else {

                print(
                    out,
                    "All pages pre-allocated. Driver will run "
                    "fully after ExitBootServices.\n"
                );

                ebs_xhci_ready = TRUE;
            }

        } else {

            print(
                out,
                "\nNo xHCI controller found - continuing "
                "without the mouse driver.\n"
            );
        }

        /* 2. Получить карту памяти и вызвать ExitBootServices.
              MapKey может устареть между двумя вызовами (если
              что-то ещё меняет карту памяти между ними), поэтому
              - стандартный из спецификации UEFI цикл повтора. */

        GUI_GET_MEMORY_MAP ebs_get_map =
            (GUI_GET_MEMORY_MAP)st->BootServices->GetMemoryMap;

        GUI_ALLOCATE_POOL ebs_alloc =
            (GUI_ALLOCATE_POOL)st->BootServices->AllocatePool;

        GUI_EXIT_BOOT_SERVICES ebs_exit =
            (GUI_EXIT_BOOT_SERVICES)st->BootServices->ExitBootServices;

        if (!ebs_get_map || !ebs_alloc || !ebs_exit) {

            print(
                out,
                "Firmware is missing required Boot Services "
                "functions - aborting.\n"
            );

            return;
        }

        UINTN ebs_map_size = 0;
        UINTN ebs_map_key  = 0;
        UINTN ebs_desc_size = 0;
        UINT32 ebs_desc_ver = 0;

        /* Первый вызов - только чтобы узнать нужный размер. */
        ebs_get_map(
            &ebs_map_size, NULL, &ebs_map_key,
            &ebs_desc_size, &ebs_desc_ver
        );

        /* Запас на случай, если сам AllocatePool ниже добавит
           в карту памяти новую запись под свою аллокацию. */
        ebs_map_size += ebs_desc_size * 8;

        VOID *ebs_map_buf = NULL;

        if (
            ebs_alloc(
                GUI_EFI_BOOT_SERVICES_DATA,
                ebs_map_size,
                &ebs_map_buf
            ) != EFI_SUCCESS
        ) {

            print(
                out,
                "Could not allocate memory map buffer - aborting.\n"
            );

            return;
        }

        BOOLEAN ebs_ok = FALSE;

        for (UINTN ebs_try = 0; ebs_try < 4; ebs_try++) {

            UINTN this_size = ebs_map_size;

            if (
                ebs_get_map(
                    &this_size, ebs_map_buf, &ebs_map_key,
                    &ebs_desc_size, &ebs_desc_ver
                ) != EFI_SUCCESS
            ) {
                break;
            }

            if (ebs_exit(g_image_handle, ebs_map_key) == EFI_SUCCESS) {
                ebs_ok = TRUE;
                break;
            }

            /* MapKey успел устареть - прошивка поменяла карту
               памяти между двумя вызовами выше. Берём карту
               заново и пробуем ещё раз. */
        }

        if (!ebs_ok) {

            print(
                out,
                "ExitBootServices failed after several attempts - "
                "aborting. Boot Services are still active, it is "
                "safe to keep using the shell.\n"
            );

            return;
        }

        /*
         * === Дальше Boot Services больше нет. ===
         *
         * Никаких LocateProtocol/AllocatePool/Stall/ConIn/ConOut -
         * они держались на коде прошивки, который теперь либо
         * освобождён, либо не гарантированно работает. Есть
         * только: ebs_fb/ebs_stride/... (закэшированы ДО выхода),
         * RuntimeServices (GetTime, ResetSystem и т.п. - по
         * спецификации обязаны работать и после ExitBootServices),
         * и собственный код (gui_fill_rect/gui_draw_text - чистая
         * работа с пикселями в буфере, без обращений к прошивке).
         */

        /*
         * Строка со статусом xHCI-контроллера - результат
         * pci_find_xhci(), выполненного ЕЩЁ ДО ExitBootServices
         * (см. выше, шаг 1.5). Повторно искать контроллер здесь
         * не нужно: сам факт того, что весь драйвер ниже
         * (xhci_run_post_ebs) успешно работает с уже найденным
         * ebs_xmmio ПОСЛЕ выхода из Boot Services, и есть живое
         * доказательство независимости от прошивки - искать
         * заново ничего не доказывает дополнительно, только
         * дублирует код.
         */
        char ebs_xhci_line[64];

        if (ebs_xhci_found) {

            UINTN p = 0;
            const char *seg;

            seg = "XHCI: BUS ";
            for (UINTN i = 0; seg[i] != '\0'; i++)
                ebs_xhci_line[p++] = seg[i];

            p += gui_uint_to_str(ebs_xbus, ebs_xhci_line + p);

            seg = " DEV ";
            for (UINTN i = 0; seg[i] != '\0'; i++)
                ebs_xhci_line[p++] = seg[i];

            p += gui_uint_to_str(ebs_xdev, ebs_xhci_line + p);

            seg = " FUNC ";
            for (UINTN i = 0; seg[i] != '\0'; i++)
                ebs_xhci_line[p++] = seg[i];

            p += gui_uint_to_str(ebs_xfunc, ebs_xhci_line + p);

            seg = " MMIO=0x";
            for (UINTN i = 0; seg[i] != '\0'; i++)
                ebs_xhci_line[p++] = seg[i];

            p += gui_hex_to_str(ebs_xmmio, 16, ebs_xhci_line + p);

            ebs_xhci_line[p] = '\0';

        } else {

            gui_str_copy8(
                ebs_xhci_line,
                "XHCI: NOT FOUND ON PCI BUS.",
                sizeof(ebs_xhci_line)
            );
        }

        UINT32 ebs_bg  = gui_pack(ebs_fmt, 10, 10, 30);
        UINT32 ebs_fg  = gui_pack(ebs_fmt, 80, 220, 120);
        UINT32 ebs_hl  = gui_pack(ebs_fmt, 255, 210, 90);
        UINT32 ebs_box = gui_pack(ebs_fmt, 220, 160, 40);

        gui_fill_rect(
            ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
            0, 0, ebs_fb_w, ebs_fb_h, ebs_bg
        );

        gui_draw_text(
            ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
            30, 30, 3, ebs_fg,
            "EXITBOOTSERVICES: OK"
        );

        gui_draw_text(
            ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
            30, 70, 2, ebs_fg,
            "NO CONIN / CONOUT / STALL FROM HERE ON."
        );

        gui_draw_text(
            ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
            30, 95, 2, ebs_fg,
            "THIS TEXT IS DRAWN WITH OUR OWN PIXEL FONT."
        );

        gui_draw_text(
            ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
            30, 148, 2, ebs_hl,
            ebs_xhci_line
        );

        /* Короткая бегающая коробочка - быстрое живое
           доказательство, что кадры рисуются сами, без Stall
           (свой busy-wait) и вообще без какой-либо прошивки,
           перед тем как перейти к самому интересному - запуску
           xHCI-драйвера. */

        INTN ebs_bx = 30;
        INTN ebs_by = 180;
        INTN ebs_dx = 3;
        INTN ebs_dy = 2;

        for (UINTN ebs_frame = 0; ebs_frame < EBS_STEPS / 4; ebs_frame++) {

            gui_fill_rect(
                ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
                ebs_bx, ebs_by, EBS_BOX_SIZE, EBS_BOX_SIZE, ebs_bg
            );

            ebs_bx += ebs_dx;
            ebs_by += ebs_dy;

            if (ebs_bx < 0 || ebs_bx > (INTN)ebs_fb_w - EBS_BOX_SIZE)
                ebs_dx = -ebs_dx;

            if (
                ebs_by < 180 ||
                ebs_by > (INTN)ebs_fb_h - EBS_BOX_SIZE
            )
                ebs_dy = -ebs_dy;

            gui_fill_rect(
                ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
                ebs_bx, ebs_by, EBS_BOX_SIZE, EBS_BOX_SIZE, ebs_box
            );

            /* Собственный busy-wait вместо BootServices->Stall -
               обычный volatile-счётчик, чтобы компилятор не
               выкинул пустой цикл. Грубо и без калибровки под
               частоту CPU - точный таймер (PIT/HPET/TSC) это уже
               отдельный, следующий шаг. */
            for (
                volatile UINT64 ebs_spin = 0;
                ebs_spin < EBS_SPIN_PER_STEP;
                ebs_spin++
            ) { }
        }

        /*
         * Дальше - сам xHCI-драйвер, целиком после
         * ExitBootServices, без единого обращения к прошивке.
         * Он печатает через "поддельный" ConOut
         * (ebs_console_start/g_ebs_pixel_out, см. выше) - тот же
         * самый print()/print_uint()/print_hex(), что и раньше,
         * только вместо st->ConOut->OutputString рисует символы
         * прямо в framebuffer нашим пиксельным шрифтом.
         */

        if (ebs_xhci_ready) {

            ebs_console_start(
                ebs_fb, ebs_stride, ebs_fb_w, ebs_fb_h,
                ebs_fg, ebs_bg
            );

            xhci_run_post_ebs(
                &g_ebs_pixel_out, ebs_xmmio, &ebs_cap,
                ebs_dcbaa_phys, ebs_cmdring_phys,
                ebs_evring_phys, ebs_erst_phys,
                ebs_input_ctx_phys, ebs_dev_ctx_phys,
                ebs_ep0_ring_phys, ebs_desc_buf_phys,
                ebs_int_ring_phys
            );

            print(&g_ebs_pixel_out, "\nRebooting shortly...\n");

            busy_wait_ms(3000);
        }

        /* Обычный return сюда не годится: клавиатуры для шелла
           всё равно больше нет. RuntimeServices, в отличие от
           BootServices, обязаны работать и после выхода - поэтому
           ResetSystem ниже безопасен и является штатным финалом
           демо, а не костылём. */
        st->RuntimeServices->ResetSystem(
            EfiResetCold, EFI_SUCCESS, 0, NULL
        );

        /* На случай, если прошивка почему-то не перезагрузила
           сразу же - зависаем тут, а не проваливаемся в код,
           который ждёт клавиатурный ввод, которого уже нет. */
        for (;;) { }


    /* --------------------------------------------------------
     * reboot
     * -------------------------------------------------------- */

    } else if (streq(line, "reboot")) {

        print(
            out,
            "Rebooting...\n"
        );


        st->RuntimeServices->ResetSystem(
            EfiResetCold,
            EFI_SUCCESS,
            0,
            NULL
        );


    /* --------------------------------------------------------
     * shutdown
     * -------------------------------------------------------- */

    } else if (
        streq(line, "shutdown") ||
        streq(line, "exit")
    ) {

        print(
            out,
            "Shutting down...\n"
        );


        st->RuntimeServices->ResetSystem(
            EfiResetShutdown,
            EFI_SUCCESS,
            0,
            NULL
        );


    /* --------------------------------------------------------
     * unknown command
     * -------------------------------------------------------- */

    } else {

        print(
            out,
            "Unknown command: "
        );


        /*
         * Используем print16(), чтобы этот вывод
         * тоже попал в scrollback.
         */
        print16(
            out,
            line
        );


        print(
            out,
            "\n(type 'help')\n"
        );
    }
}


/* ============================================================
 * UEFI entry point
 * ============================================================ */

EFI_STATUS EFIAPI efi_main(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
)
{
    g_image_handle = ImageHandle;

    /* Таблица, через которую шелл получает ввод/вывод. После
       команды "ebs" она подменяется на нашу собственную (g_kst),
       поэтому главный цикл ниже берёт её заново на каждой
       итерации, а не держит SystemTable/out из начала функции -
       настоящий ConOut прошивки после ExitBootServices равен
       NULL. */
    g_st = SystemTable;


    SIMPLE_TEXT_OUTPUT_INTERFACE *out =
        SystemTable->ConOut;


    out->Reset(
        out,
        FALSE
    );


    out->ClearScreen(out);


    set_color(
        out,
        g_color
    );


    /* --------------------------------------------------------
     * Save boot time
     * -------------------------------------------------------- */

    if (
        SystemTable->RuntimeServices->GetTime &&
        SystemTable->RuntimeServices->GetTime(
            &g_boot_time,
            NULL
        ) == EFI_SUCCESS
    ) {

        g_have_boot_time = TRUE;
    }


    /* --------------------------------------------------------
     * Startup banner
     * -------------------------------------------------------- */

    print(
        out,
        "================================\n"
    );

    print(
        out,
        "   MyOS 0.1 - kernel base\n"
    );

    print(
        out,
        "   64-bit UEFI, no Linux/Windows\n"
    );

    print(
        out,
        "================================\n\n"
    );


    print(
        out,
        "Boot services: OK\n"
    );

    print(
        out,
        "Text output:   OK\n\n"
    );


    print(
        out,
        "Type 'help' for the list of commands, or 'fetch' for a system summary.\n\n"
    );


    /* --------------------------------------------------------
     * Main shell loop
     * -------------------------------------------------------- */

    CHAR16 line[LINE_MAX];


    for (;;) {

        out = g_st->ConOut;

        set_color(
            out,
            g_color
        );


        print(
            out,
            "> "
        );


        read_line(
            g_st,
            line,
            LINE_MAX
        );


        run_command(
            g_st,
            line
        );
    }


    return EFI_SUCCESS;
}
