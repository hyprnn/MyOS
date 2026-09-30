/*
 * myos.h - общий заголовок MyOS.
 *
 * Раньше вся ОС была одним файлом main.c (~19 000 строк), где всё
 * было static. Теперь она разложена по модулям (lib/, drivers/,
 * kernel/, gui/, shell/), а этот заголовок - то, что модули видят
 * друг у друга: константы (#define), типы, глобальные переменные
 * (extern) и объявления функций. Порядок разделов важен:
 * сначала типы и константы, потом переменные, потом функции,
 * и в самом конце маленькие static inline функции (они
 * пользуются всем, что объявлено выше).
 *
 * Файл сгенерирован скриптом split_main.py из прежнего main.c,
 * дальше правится руками как обычный заголовок.
 */
#ifndef MYOS_H
#define MYOS_H

#include "efi.h"
#include "bootinfo.h"
#include <stdarg.h>   /* va_list для kvsnprintf - заголовок компилятора, не libc */

/*
 * Все символы MyOS - "скрытые" (hidden): модули ссылаются друг на
 * друга напрямую, относительно RIP. Без этого компилятор с -fpic
 * обращался бы к чужим глобальным переменным через GOT (таблицу
 * адресов для динамической линковки), которой в UEFI-образе нет.
 */
#pragma GCC visibility push(hidden)

/* ================================================================
 * Константы и типы
 * ================================================================ */

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

#define LINE_MAX 128


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


/* ============================================================
 * Command history
 * ============================================================ */

#define HIST_MAX 16


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

#define SCROLLBACK_VISIBLE_ROWS  ((int)g_scrollback_visible_rows)


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
 * ВАЖНО - вероятная настоящая причина случайных зависаний,
 * не связанная с порядком регистров: пока мы НЕ вызвали
 * ExitBootServices (а команда "xhci" тестируется именно
 * до этого), прошивка (OVMF) сама всё ещё жива и может
 * (если у неё есть встроенный UsbMouseDxe - см. коммент
 * про EFI_SIMPLE_POINTER_PROTOCOL в первом GUI) в фоне,
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
 * EFI_BS_*, потому что те определены значительно ниже по
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


/* ============================================================
 * Курсор и мышь
 * (Первый GUI - gui/gui.c с шрифтом 5x7 и перерисовкой всего кадра -
 * удалён на этапе 10 (Д5); рабочий стол - gui/wm.c, этап 7.)
 * ============================================================ */

#define GUI_CURSOR_SIZE   12     /* курсор-квадрат `mousetest` (gui/fb.c) */

/* Чувствительность мыши: условных "пикселей на мм" движения.
   Подбиралось на глаз - если курсор летает слишком быстро/
   медленно на конкретном устройстве, поправить это число. */
#define GUI_MOUSE_PIXELS_PER_MM 8

/*
 * Сигнатуры функций Boot Services для загрузчика (loader/loader.c):
 * в efi.h AllocatePages/AllocatePool/FreePool/GetMemoryMap/
 * ExitBootServices объявлены голыми VOID* (см. заглушки в
 * EFI_BOOT_SERVICES) - приводим их к настоящим сигнатурам сами.
 * MemoryMap - VOID*: записи карты разбирает сам загрузчик.
 */
typedef EFI_STATUS (EFIAPI *EFI_BS_ALLOCATE_PAGES)(
    UINTN Type, UINTN MemoryType, UINTN Pages, UINT64 *Memory
);

typedef EFI_STATUS (EFIAPI *EFI_BS_ALLOCATE_POOL)(
    UINTN PoolType, UINTN Size, VOID **Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_BS_FREE_POOL)(
    VOID *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_BS_GET_MEMORY_MAP)(
    UINTN *MemoryMapSize,
    VOID *MemoryMap,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
);

typedef EFI_STATUS (EFIAPI *EFI_BS_EXIT_BOOT_SERVICES)(
    EFI_HANDLE ImageHandle,
    UINTN MapKey
);





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





/* ################################################################
 * ################################################################
 *
 *   ЯДРО: как MyOS устроена с этапа 1
 *
 * ################################################################
 * ################################################################
 *
 * Загрузка: прошивка -> loader/loader.c (BOOTX64.EFI, UEFI-
 * программа) -> kernel.elf. Загрузчик собирает "паспорт"
 * (bootinfo.h), выходит из прошивки и прыгает в kmain
 * (kernel/kmain.c). Ядро о прошивке не знает ничего.
 *
 * Части ядра:
 *
 *   1. Консоль (kcon_*) - шрифт 8x16 прямо в framebuffer, цвета,
 *      прокрутка, курсор. Для остального кода выглядит как
 *      обычный SIMPLE_TEXT_OUTPUT_INTERFACE.
 *
 *   2. Процессор (kx_*, cpu.c) - своя GDT с TSS, своя IDT на все
 *      256 векторов, отдельные стеки (IST) для Double Fault/NMI/
 *      Machine Check, экран паники с объяснением причины.
 *
 *   3. Время (time.c, power.c) - TSC калибруется по PIT, таймер
 *      Local APIC 1000 Гц; часы - микросхема CMOS.
 *
 *   4. Память: pmm.c - страницы (битовая карта по карте памяти от
 *      загрузчика, включая бывшую память прошивки); vmm.c - свои
 *      таблицы страниц (ядро наверху, вся RAM в прямом
 *      отображении, код только для чтения, NX, WC для экрана,
 *      стеки с защитными страницами); kmalloc.c - куча.
 *
 *   5. Ввод (kbd_*, kx_*, ps2_*) - неблокирующий xHCI-драйвер
 *      (HID-клавиатуры и мыши на корневых портах) и PS/2.
 *
 *   6. Таблица функций ядра (shim.c, g_kst) - собственная таблица
 *      в формате EFI_SYSTEM_TABLE: шелл и GUI писались как UEFI-
 *      программы и зовут st->ConOut, st->BootServices->Stall,
 *      LocateProtocol и т.п. За каждым полем - код ядра, ни одного
 *      вызова прошивки. На этапе 6 это место займут системные
 *      вызовы.
 */


/* Коды ошибок UEFI, которых нет в efi.h - нужны нашим
   собственным реализациям сервисов (верхний бит = ошибка) */
#define K_EFI_ERR(n)             (0x8000000000000000ull | (UINT64)(n))
#define K_EFI_INVALID_PARAMETER  K_EFI_ERR(2)
#define K_EFI_UNSUPPORTED        K_EFI_ERR(3)
#define K_EFI_NOT_READY          K_EFI_ERR(6)
#define K_EFI_OUT_OF_RESOURCES   K_EFI_ERR(9)
#define K_EFI_NOT_FOUND          K_EFI_ERR(14)

/*
 * Размер "экрана" консоли в символах ограничен сверху, чтобы
 * массивы ниже были статическими (своего malloc на момент
 * инициализации консоли ещё нет - она поднимается САМОЙ ПЕРВОЙ
 * после ExitBootServices, чтобы было куда печатать лог).
 */
#define KCON_MAX_COLS 240
#define KCON_MAX_ROWS 100

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

#define KX_VEC_TIMER     0x40u
#define KX_VEC_PS2_KBD   0x21u    /* IRQ 1 */
#define KX_VEC_PS2_AUX   0x2Cu    /* IRQ 12 */
#define KX_VEC_XHCI      0x50u    /* MSI от USB-контроллера */
#define KX_VEC_ACPI      0x29u    /* SCI - прерывание ACPI (обычно IRQ 9) */

typedef void (*KX_IRQ_HANDLER)(void);
#define KX_VEC_RESCHED   0xF0u    /* IPI: "на тебя есть работа" (SMP) */
#define KX_VEC_HALT      0xF1u    /* IPI: "остановись" (паника на другом ядре) */
#define KX_VEC_SPURIOUS  0xFFu

extern void kx_isr_stubs(void) __attribute__((visibility("hidden")));

/* Рамка стека в момент вызова kx_isr_dispatch - ровно в
   обратном порядке относительно push'ей выше */
typedef struct {
    UINT64 r15, r14, r13, r12, r11, r10, r9, r8;
    UINT64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    UINT64 vector, error;
    UINT64 rip, cs, rflags, rsp, ss;
} KX_ISR_FRAME;



/* ================================================================
 * 4. Физическая память: карта от GetMemoryMap + битовая карта
 * ================================================================
 *
 * Карту памяти (список регионов физической памяти с типом у
 * каждого) собирает загрузчик и передаёт в паспорте загрузки.
 * Какие типы ядро считает свободными - см. начало kernel/pmm.c.
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

/* Смещения полей в EFI_MEMORY_DESCRIPTOR (в efi.h его нет) -
   читаем по байтовым смещениям, т.к. реальный размер записи
   (DescriptorSize) прошивка может сделать больше структуры */
#define KMM_DESC_TYPE   0
#define KMM_DESC_PHYS   8
#define KMM_DESC_PAGES  24


/* ================================================================
 * Виртуальная память (kernel/vmm.c)
 * ================================================================ */

/* атрибуты для vmm_map_page / vmm_map_mmio */
#define VMM_W   0x1u    /* можно писать */
#define VMM_X   0x2u    /* можно исполнять (без него - NX) */
#define VMM_UC  0x4u    /* некэшируемая (регистры устройств) */
#define VMM_WC  0x8u    /* write-combining (видеопамять) */

/* Стек ядра: [bottom, top), под ним - защитная страница guard.
   Каждому потоку (kernel/sched.c) - свой стек, поэтому запас с
   избытком; стеки завершившихся потоков используются повторно. */
#define KSTACK_MAX 64

typedef struct {
    UINT64 guard;
    UINT64 bottom;
    UINT64 top;
    const char *name;
} KSTACK_INFO;


/* ================================================================
 * Потоки и планировщик (kernel/sched.c)
 * ================================================================ */

/* Сколько потоков может существовать одновременно. Таблица
   маленькая и обходится целиком - так проще и нагляднее списков. */
#define KT_MAX          64
#define KT_NAME_LEN     16
/* Квант времени: столько миллисекунд (тиков таймера) поток
   работает подряд, если другие тоже хотят процессор */
#define KT_QUANTUM_MS   10

typedef enum {
    KT_UNUSED = 0,   /* слот свободен */
    KT_READY,        /* готов работать, ждёт своей очереди */
    KT_RUNNING,      /* работает прямо сейчас */
    KT_SLEEPING,     /* спит до момента wake_tick */
    KT_BLOCKED,      /* ждёт события (wait_on), возможно с таймаутом */
    KT_DEAD          /* завершился; слот и стек можно отдать новому */
} KT_STATE;

typedef struct KTHREAD {
    /* ВАЖНО: rsp - первое поле, его адрес передаётся в kx_switch */
    UINT64      rsp;             /* сохранённый указатель стека, пока
                                    поток не работает */
    UINT32      tid;             /* номер потока (растёт, не повторяется) */
    UINT32      slot;            /* индекс в таблице g_kthreads */
    KT_STATE    state;
    char        name[KT_NAME_LEN];

    UINT64      stack_bottom;    /* [bottom, top) - стек потока */
    UINT64      stack_top;
    UINTN       stack_pages;

    void      (*entry)(void *);  /* что запустить в новом потоке */
    void       *arg;

    UINT64      wake_tick;       /* SLEEPING: когда разбудить;
                                    BLOCKED: таймаут (0 - без таймаута) */
    const void *wait_on;         /* BLOCKED: чего ждём (адрес объекта) */
    const char *wait_what;       /* ...и как это назвать в ps */
    BOOLEAN     timed_out;       /* ожидание закончилось таймаутом */

    /* место в очереди готовых: меньше - раньше встал в очередь */
    UINT64      ready_seq;
    /* только что проснулся (дождался сна/события) - пропустить
       вперёд тех, кто просто крутит процессор: так шелл и GUI
       отзываются сразу, даже если рядом кто-то считает без
       остановки */
    BOOLEAN     boost;
    /* сколько миллисекунд кванта осталось (если его вытеснили
       раньше срока - доработает остаток, а не начнёт заново) */
    UINT32      quantum_left;

    /* kx_lock (запрет прерываний) у каждого потока свой: поток,
       уснувший внутри kx_lock, не должен "передать" запрет
       следующему */
    UINTN       lock_depth;
    BOOLEAN     lock_if;

    /* учёт процессорного времени */
    UINT64      cpu_tsc;         /* всего тактов на процессоре */
    UINT64      cpu_tsc_prev;    /* ...на момент прошлого пересчёта */
    UINT32      load_permille;   /* доля процессора за последнюю
                                    секунду, 0..1000 */
    UINT64      switches;        /* сколько раз получал процессор */
    UINT64      started_ms;      /* когда создан (мс от старта таймера) */

    /* этап 6: поток программы (ring 3) */
    UINT64      cr3;             /* свои таблицы страниц (0 - ядра) */
    struct KPROC *proc;          /* чей это поток (NULL - ядра) */

    /* этап 10: несколько ядер */
    UINT32      bkl_depth;       /* сколько раз вошёл в "большой замок
                                    ядра" (0 - выполняет программу) */
    BOOLEAN     is_idle;         /* поток простоя какого-то ядра: его
                                    не берёт никто, кроме своего ядра */
    UINT32      cpu;             /* на каком ядре работал последним */
} KTHREAD;

/* Замок-"мьютекс": пока его держит один поток, другой, пришедший
   за ним, СПИТ (не крутится). Рекурсивный: владелец может взять
   его ещё раз (например, blk_register_disk -> blk_read). */
typedef struct {
    KTHREAD    *owner;
    UINTN       count;
    const char *name;
    UINT64      waits;           /* сколько раз кому-то пришлось ждать */
} KMUTEX;

/* Спин-замок: для очень коротких участков, в том числе в
   обработчиках прерываний. Запрещает прерывания на этом ядре и
   (когда появятся другие ядра, SMP) крутится, пока замок занят. */
typedef struct {
    volatile UINT32 locked;
} KSPINLOCK;

/* Снимок одного потока для ps (sched_snapshot) */
typedef struct {
    UINT32      tid;
    char        name[KT_NAME_LEN];
    KT_STATE    state;
    BOOLEAN     current;
    UINT32      load_permille;
    UINT64      cpu_ms;
    UINT64      switches;
    UINT32      stack_kib;
    UINT32      stack_used_kib;
    const char *wait_what;
    UINT64      wake_in_ms;
    void       *stack_ptr;       /* служебное: сам KTHREAD */
} KT_INFO;

#define KMUTEX_INIT(n)   { NULL, 0, (n), 0 }
#define KSPINLOCK_INIT   { 0 }


/* ================================================================
 * Диски (этап 5): блочные устройства, разделы, FAT, VFS
 * drivers/blk.c, drivers/ahci.c, drivers/nvme.c, fs/fat.c, fs/vfs.c
 * ================================================================ */

#define BLK_MAX            24      /* дисков и разделов вместе */
#define BLK_SECTOR         512     /* файловой системе нужны секторы
                                      по 512 байт (почти у всех так) */
#define BLK_CACHE_SECTORS  512     /* кэш: 512 секторов = 256 КиБ */

struct BLKDEV;

/* Чтение/запись целого диска: драйвер (USB, AHCI, NVMe) */
typedef BOOLEAN (*BLK_RW)(struct BLKDEV *d, UINT64 lba, UINT32 count,
                          VOID *buf, BOOLEAN write);

typedef struct BLKDEV {
    BOOLEAN     used;
    char        name[12];        /* "usb0", "sata0", "nvme0", "usb0p1" */
    char        model[41];       /* что говорит о себе устройство */
    const char *kind;            /* "USB", "SATA (AHCI)", "NVMe", "partition" */
    UINT32      sector_size;
    UINT64      sectors;
    BOOLEAN     writable;        /* можно ли писать */
    const char *ro_reason;       /* почему нельзя */

    /* целый диск */
    BLK_RW      rw;
    UINTN       drv_index;       /* номер у драйвера (порт, флешка) */
    UINT32      drv_serial;      /* "паспорт" подключения (флешку
                                    вынули и вставили другую) */
    UINT32      gen;             /* поколение: меняется при каждом
                                    появлении - кэш и тома сверяются */

    /* раздел: какой диск и с какого сектора */
    INTN        parent;          /* -1 для целого диска */
    UINT64      start;
    UINT32      part_no;
    char        ptype[24];       /* "FAT32", "EFI System", "Linux", ... */
    char        label[37];       /* имя раздела из GPT */

    UINT64      reads, writes, errors, cache_hits;
} BLKDEV;

/* --- VFS: файлы и папки --- */

#define VFS_MAX_MOUNTS   12
#define VFS_PATH_MAX     256
#define VFS_NAME_MAX     128
#define VFS_MAX_FD       64

/* Коды ошибок (отрицательные) */
#define VFS_OK            0
#define VFS_ENOENT       -2      /* нет такого файла или папки */
#define VFS_EEXIST       -3      /* уже есть */
#define VFS_ENOTDIR      -4      /* это не папка */
#define VFS_EISDIR       -5      /* это папка */
#define VFS_ENOTEMPTY    -6      /* папка не пустая */
#define VFS_ENOSPC       -7      /* нет места */
#define VFS_EROFS        -8      /* только чтение */
#define VFS_EIO          -9      /* ошибка диска */
#define VFS_EINVAL      -10      /* неверное имя/аргумент */
#define VFS_EBADF       -11      /* неверный номер файла */
#define VFS_EMFILE      -12      /* открыто слишком много файлов */
#define VFS_ENOSYS      -13      /* эта файловая система так не умеет */
#define VFS_EGONE       -14      /* диск вынули */
#define VFS_EXDEV       -15      /* между разными дисками так нельзя */

/* флаги vfs_open */
#define VFS_O_READ      0x01u
#define VFS_O_WRITE     0x02u
#define VFS_O_CREATE    0x04u
#define VFS_O_TRUNC     0x08u
#define VFS_O_APPEND    0x10u

/* Узел - файл или папка внутри одной файловой системы. Что в полях
   - знает только её драйвер (FAT: кластеры и место записи в папке;
   RAM-диск: номер файла). */
typedef struct {
    BOOLEAN is_dir;
    UINT64  size;
    UINT32  first_cluster;       /* FAT: первый кластер (0 - пусто;
                                    у корня FAT16 - 0) */
    UINT32  parent_cluster;      /* FAT: папка, где лежит запись */
    UINT32  entry_index;         /* FAT: номер 32-байтной записи */
    UINT32  lfn_count;           /* FAT: сколько записей длинного имени
                                    перед ней */
    BOOLEAN is_root;
    INTN    ram_index;           /* RAM-диск: индекс в g_fs */
    UINT8   attr;
    UINT16  wdate, wtime;        /* FAT: дата и время изменения */
    char    fpath[VFS_PATH_MAX]; /* exFAT (FatFs): путь внутри тома */
    /* FAT, открытый файл: где остановилось прошлое чтение (номер
       кластера в цепочке и сам кластер) - чтобы следующее чтение не
       шло по цепочке от начала файла (этап 10: иначе чтение большого
       файла кусками - квадратичное время). 0 - подсказки нет. */
    UINT32  hint_ci, hint_cl;
} VFS_NODE;

/* Запись каталога для ls */
typedef struct {
    char     name[VFS_NAME_MAX];
    VFS_NODE node;
} VFS_DIRENT;

struct VFS_MOUNT;

typedef struct {
    const char *name;            /* "FAT32", "ramfs" */
    INTN (*root)(struct VFS_MOUNT *m, VFS_NODE *out);
    /* *cookie = 0 в начале; возвращает 1 - есть запись, 0 - конец */
    INTN (*readdir)(struct VFS_MOUNT *m, VFS_NODE *dir, UINT64 *cookie, VFS_DIRENT *out);
    INTN (*lookup)(struct VFS_MOUNT *m, VFS_NODE *dir, const char *name, VFS_NODE *out);
    INTN (*read)(struct VFS_MOUNT *m, VFS_NODE *f, UINT64 off, VOID *buf, UINTN n);
    INTN (*write)(struct VFS_MOUNT *m, VFS_NODE *f, UINT64 off, const VOID *buf, UINTN n);
    INTN (*truncate)(struct VFS_MOUNT *m, VFS_NODE *f, UINT64 size);
    INTN (*create)(struct VFS_MOUNT *m, VFS_NODE *dir, const char *name, BOOLEAN is_dir, VFS_NODE *out);
    INTN (*remove)(struct VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node);
    INTN (*rename)(struct VFS_MOUNT *m, VFS_NODE *dir, VFS_NODE *node,
                   VFS_NODE *newdir, const char *newname);
    INTN (*statfs)(struct VFS_MOUNT *m, UINT64 *total_bytes, UINT64 *free_bytes);
    /* файл закрыт (может быть NULL): дописать отложенное на диск */
    INTN (*close)(struct VFS_MOUNT *m, VFS_NODE *f);
} VFS_OPS;

/* Том FAT (fs/fat.c) */
typedef struct {
    UINTN   dev;                 /* блочное устройство */
    UINT32  fat_bits;            /* 16 или 32 */
    UINT32  spc;                 /* секторов в кластере */
    UINT32  cluster_bytes;
    UINT32  reserved;            /* секторов до первой FAT */
    UINT32  nfats;
    UINT32  fat_size;            /* секторов в одной FAT */
    UINT32  root_entries;        /* FAT16: записей в корне */
    UINT32  root_sector;         /* FAT16: где корень */
    UINT32  root_sectors;
    UINT32  data_start;          /* первый сектор кластера 2 */
    UINT32  clusters;            /* сколько кластеров данных */
    UINT32  root_cluster;        /* FAT32: первый кластер корня */
    UINT32  fsinfo;              /* FAT32: сектор FSInfo */
    UINT32  next_free;           /* откуда искать свободный кластер */
    INT64   free_count;          /* -1 - ещё не считали */
    BOOLEAN fsinfo_invalidated;  /* уже сказали FSInfo "не знаю" */
    char    label[12];
} FAT_VOL;

typedef struct VFS_MOUNT {
    BOOLEAN        used;
    char           name[16];     /* каталог в корне: "ram", "usb0p1" */
    const VFS_OPS *ops;
    UINTN          dev;          /* блочное устройство (для дисков) */
    UINT32         dev_gen;      /* его поколение при монтировании */
    BOOLEAN        readonly;
    BOOLEAN        gone;         /* диск пропал - том мёртв */
    FAT_VOL        fat;          /* FAT; у exFAT - только метка и размеры */
    void          *xfs;          /* exFAT: том FatFs (fs/exfat.c) */
} VFS_MOUNT;


/* ================================================================
 * Программы и процессы (этап 6): kernel/proc.c, kernel/syscall.c
 * ================================================================ */

#include "sysnum.h"

#define PROC_MAX        24
#define PROC_FDS        32
#define PROC_IN_MAX     256
#define MAX_HEAP_BYTES  (512ull * 1024u * 1024u)  /* куча программы - не больше
                                                     (браузеру нужны десятки МБ) */

/* Куда программа пишет и откуда читает (fd 0, 1, 2) */
#define PROC_IO_CONSOLE 0      /* текстовая консоль шелла */
#define PROC_IO_GUI     1      /* программа с окном без терминала: вывод -
                                  строками в журнал ядра, ввода нет */
#define PROC_IO_TTY     2      /* окно-терминал рабочего стола (gui/tty.c) */

/* Терминал рабочего стола (gui/tty.c, этап 10) */
#define TTY_COLS   80
#define TTY_LINES  300          /* строк истории */
#define TTY_KEYS   64
typedef struct KTTY KTTY;

typedef struct KPROC {
    BOOLEAN          used;
    UINT32           pid;
    char             name[KT_NAME_LEN];
    char             path[VFS_PATH_MAX];
    char             cwd[VFS_PATH_MAX];   /* от какой папки считать пути */
    UINT64           pml4;                /* свои таблицы страниц */
    KTHREAD         *thread;
    UINT32           tid;
    UINT64           entry;
    UINT64           user_rsp;
    UINT64           brk_base, brk;       /* куча: [brk_base, brk) */
    UINT64           pages;               /* сколько страниц памяти занято */
    INTN             fds[PROC_FDS];       /* номера файлов VFS (-1 - нет) */
    volatile BOOLEAN exited;
    volatile BOOLEAN killed;              /* попросили завершиться */
    INT64            exit_code;
    char             why[160];            /* почему завершилась (ошибка) */
    UINT64           started_ms;
    UINT64           syscalls;

    UINT32           io;                  /* PROC_IO_* */
    /* PROC_IO_GUI: вывод копится строкой, строка - в журнал ядра */
    char             outline[128];
    UINTN            outlen;

    /* этап 10: программы запускает шелл-программа (/bin/sh) */
    struct KPROC    *parent;              /* кто запустил (ждёт через SYS_WAIT) */
    INTN             out_kfd;             /* вывод (fd 1, 2) - в этот файл VFS (-1 нет) */
    BOOLEAN          raw_keys;            /* читает клавиши по одной (SYS_READKEY):
                                             Ctrl+C ей - клавиша 3, а не "стоп" */
    BOOLEAN          autoreap;            /* родителя нет: убрать после выхода */
    BOOLEAN          was_fg;              /* её запустили на переднем плане */
    BOOLEAN          kcmd_probe;          /* SYS_KCMD: неизвестная команда - */
    BOOLEAN          kcmd_unknown;        /*   не печатать, а сказать шеллу */
    BOOLEAN          kcmd_kernel;         /* MYOS_KCMD_KERNEL: не отдавать команду
                                             программе из /bin (см. kcmd_is_program) */
    KTTY            *tty;                 /* PROC_IO_TTY: окно-терминал */
} KPROC;

/* TSS (64-битный), см. kernel/cpu.c */
typedef struct __attribute__((packed)) {
    UINT32 reserved0;
    UINT64 rsp0, rsp1, rsp2;
    UINT64 reserved1;
    UINT64 ist[7];
    UINT64 reserved2;
    UINT16 reserved3;
    UINT16 iomap_base;
} KX_TSS;

/* ACPI: всё, что ядро узнало из таблиц (kernel/acpi.c) */
#define ACPI_MAX_TABLES   48
#define ACPI_MAX_CPUS     64
#define ACPI_MAX_IOAPICS  8
#define ACPI_MAX_ISOS     24

typedef struct {
    char    sig[5];
    UINT64  phys;
    UINT32  len;
    UINT8   rev;
    BOOLEAN sum_ok;
    char    oem[7];
    char    oem_table[9];
} ACPI_TABLE_INFO;

typedef struct {
    UINT32  apic_id;
    UINT32  uid;
    BOOLEAN enabled;
    BOOLEAN online_capable;
    BOOLEAN x2;
} ACPI_CPU;

typedef struct {
    UINT8  id;
    UINT64 addr;
    UINT32 gsi_base;
    UINT32 count;       /* линий (узнаём из самого I/O APIC) */
} ACPI_IOAPIC;

typedef struct {       /* "IRQ N старого PC на самом деле - линия GSI" */
    UINT8  bus, irq;
    UINT32 gsi;
    UINT16 flags;       /* полярность [1:0], срабатывание [3:2] */
} ACPI_ISO;

typedef struct {
    BOOLEAN present;
    const char *why;
    UINT8   revision;
    UINT64  rsdp, root;
    BOOLEAN xsdt;
    char    oem[7];

    ACPI_TABLE_INFO tables[ACPI_MAX_TABLES];
    UINTN   ntables;

    /* MADT */
    BOOLEAN have_madt;
    UINT64  lapic_addr;
    BOOLEAN pcat_compat;
    ACPI_CPU cpus[ACPI_MAX_CPUS];
    UINTN   ncpus, ncpus_enabled;
    ACPI_IOAPIC ioapics[ACPI_MAX_IOAPICS];
    UINTN   nioapics;
    ACPI_ISO isos[ACPI_MAX_ISOS];
    UINTN   nisos;
    UINTN   n_lapic_nmi;

    /* FADT */
    BOOLEAN have_fadt;
    UINT8   fadt_rev;
    UINT16  sci_irq;
    UINT32  smi_cmd;
    UINT8   acpi_enable;
    UINT32  pm1a_evt, pm1a_cnt, pm1b_cnt;
    UINT32  pm_tmr;
    BOOLEAN pm_tmr_32;
    UINT8   century;
    UINT16  boot_arch;
    UINT32  fadt_flags;
    BOOLEAN hw_reduced;
    BOOLEAN reset_ok;
    UINT8   reset_space;
    UINT64  reset_addr;
    UINT8   reset_value;
    UINT64  dsdt;
    UINT32  dsdt_len;
    UINTN   n_ssdt;
    UINT32  aml_bytes;

    /* MCFG */
    UINTN   n_mcfg;
    UINT64  ecam_base;
    UINT16  ecam_seg;
    UINT8   ecam_bus_start, ecam_bus_end;

    /* HPET */
    BOOLEAN have_hpet, hpet_ok, hpet_64;
    UINT64  hpet_addr;
    UINT32  hpet_period_fs;
    UINT8   hpet_timers;
    UINT16  hpet_min_tick;
} ACPI_INFO;

/* Ядра процессора (kernel/smp.c, этап 10).
   У каждого ядра - своя структура KX_CPU; её адрес лежит в регистре
   базы GS этого ядра (MSR 0xC0000101), поэтому "мои данные" - это
   %gs:смещение, одна инструкция. Первые поля читает ассемблер
   (вход syscall) - их смещения не менять. */
#define KX_MAX_CPUS 32

typedef struct KX_CPU {
    struct KX_CPU *self;         /* 0:  %gs:0 - адрес этой структуры */
    UINT64   sc_kstack;          /* 8:  стек ядра текущего потока (syscall) */
    UINT64   sc_user_rsp;        /* 16: rsp программы при входе в syscall */
    struct KTHREAD *kcur;        /* 24: поток, который выполняет это ядро */
    struct KTHREAD *kidle;       /* поток простоя этого ядра */
    UINT32   index;              /* номер ядра: 0 - загрузочное */
    volatile UINT32 isr_depth;   /* >0 - внутри обработчика прерывания */
    volatile BOOLEAN need_resched;   /* сменить поток на выходе из прерывания */
    UINT32   quantum_left;       /* мс кванта текущего потока */
    UINT64   slice_start;        /* rdtsc начала работы kcur */
    volatile UINT64 idle_tsc;    /* сколько тактов ядро проспало в hlt */
    UINT64   tlb_gen;            /* какую версию отображений ядра видит TLB */
    UINT64   ticks;              /* тиков таймера этого ядра */
    KX_TSS  *tss_ptr;            /* TSS этого ядра (у cpu0 - g_ktss) */

    BOOLEAN  used;               /* есть в MADT (и включено прошивкой) */
    BOOLEAN  bsp;                /* загрузочное ядро (cpu0) */
    volatile BOOLEAN online;     /* проснулось и настроилось */
    volatile BOOLEAN in_sched;   /* работает в общем планировщике */
    UINT32   apic_id;            /* номер Local APIC (из MADT) */
    UINT32   apic_id_seen;       /* ...и какой оно назвало само */
    const char *why;             /* почему не запустилось */
    UINT64   started_tsc;
    UINT64   stack_top;          /* основной стек ядра */
    UINT64   ist_df, ist_nmi, ist_mc;   /* аварийные стеки */
    UINT64   gdt[8] __attribute__((aligned(16)));
    KX_TSS   tss __attribute__((aligned(16)));
} KX_CPU;

/* "моё" ядро и его поля - одной инструкцией (между чтением адреса
   структуры и чтением поля поток мог бы переехать на другое ядро) */
static inline KX_CPU *kx_cpu(void)
{
    KX_CPU *c;
    __asm__ __volatile__("movq %%gs:0, %0" : "=r"(c));
    return c;
}

static inline struct KTHREAD *kx_cur(void)
{
    struct KTHREAD *t;
    __asm__ __volatile__("movq %%gs:%c1, %0" : "=r"(t) : "i"(__builtin_offsetof(KX_CPU, kcur)));
    return t;
}

static inline struct KTHREAD *kx_idle_thread(void)
{
    struct KTHREAD *t;
    __asm__ __volatile__("movq %%gs:%c1, %0" : "=r"(t) : "i"(__builtin_offsetof(KX_CPU, kidle)));
    return t;
}

static inline UINT32 kx_isr_depth(void)
{
    UINT32 d;
    __asm__ __volatile__("movl %%gs:%c1, %0" : "=r"(d) : "i"(__builtin_offsetof(KX_CPU, isr_depth)));
    return d;
}

static inline UINT32 kx_cpu_index(void)
{
    UINT32 i;
    __asm__ __volatile__("movl %%gs:%c1, %0" : "=r"(i) : "i"(__builtin_offsetof(KX_CPU, index)));
    return i;
}

/* старые имена (до SMP это были глобальные переменные) - только
   для чтения */
#define g_kcur          kx_cur()
#define g_kidle         kx_idle_thread()
#define g_kx_isr_depth  kx_isr_depth()

/* ACPI-устройства через uACPI (kernel/acpi_dev.c, этап 9) */
#define ACPI_MAX_BATTERIES 2

typedef struct {
    BOOLEAN present;        /* батарея вставлена (_STA, бит 4) */
    BOOLEAN valid;          /* данные прочитаны без ошибок */
    char    name[8];        /* имя в AML: BAT0 */
    BOOLEAN mah;            /* единицы: mAh/mA (иначе mWh/mW) */
    UINT32  design;         /* ёмкость по паспорту */
    UINT32  full;           /* ёмкость последней полной зарядки */
    UINT32  design_mv;      /* напряжение по паспорту */
    UINT32  cycles;         /* циклов заряда (_BIX; 0 - неизвестно) */
    UINT32  state;          /* _BST[0]: 1 - разряжается, 2 - заряжается,
                               4 - критически мало */
    UINT32  rate;           /* ток/мощность сейчас (0xFFFFFFFF - неизвестно) */
    UINT32  remaining;      /* осталось */
    UINT32  mv;             /* напряжение сейчас */
    UINT32  percent;        /* 0..100 */
    INT32   minutes;        /* до разряда/до полной; -1 - неизвестно */
    char    model[20], type[8], oem[20];
} ACPI_BATTERY;

typedef struct {
    BOOLEAN ok;             /* uACPI работает (таблицы загружены) */
    const char *why;        /* ...а если нет - почему */
    UINT32  init_ms;        /* сколько заняла загрузка AML */

    UINT32  nbat;
    ACPI_BATTERY bat[ACPI_MAX_BATTERIES];
    UINT64  bat_updated_ms; /* когда читали батареи (мс от старта) */

    BOOLEAN have_ac, ac_online;     /* блок питания ACPI0003, _PSR */
    BOOLEAN have_lid, lid_open;     /* крышка PNP0C0D, _LID */
    UINT32  lid_changes;

    BOOLEAN have_ec;                /* контроллер EC (PNP0C09) */
    BOOLEAN ec_from_ecdt;
    UINT16  ec_data, ec_cmd;        /* его порты */
    INT32   ec_gpe;                 /* его GPE (-1 - нет) */
    BOOLEAN ec_glk;                 /* нужен глобальный замок */
    UINT64  ec_reads, ec_writes, ec_queries, ec_timeouts;

    BOOLEAN pwrbtn_fixed;           /* кнопка - "фиксированное событие" */
    UINT32  pwrbtn_devices;         /* ...или устройство PNP0C0C */
    UINT32  pwrbtn_presses;

    UINT32  n_warnings, n_errors;   /* сообщения uACPI */
    char    last_msg[96];
} ACPI_DEVS;

/* Звук: Intel HD Audio (drivers/hda.c, этап 10) */
typedef struct {
    BOOLEAN ok;
    UINT32  pci_id;             /* vendor | device << 16 */
    UINT8   bus, dev, fn;
    UINT8   codec_addr;
    UINT32  codec_vendor;       /* vendor << 16 | device */
    UINT32  nout;
    UINT8   out_pin[4], out_dac[4], out_dev[4];
    BOOLEAN immediate;          /* команды кодеку - без колец CORB/RIRB */
    BOOLEAN headphones;         /* штекер наушников вставлен */
    UINT32  volume;             /* 0..100 */
    BOOLEAN muted;
    UINT32  owner_pid;          /* какая программа играет (0 - никто) */
    UINT32  underruns;          /* программа не успела подать звук */
    UINT32  cmd_timeouts;
    UINT64  bytes_played;
} HDA_INFO;

/* Яркость экрана (kernel/backlight.c, этап 9) */
typedef enum {
    BL_NONE = 0,     /* управлять нечем */
    BL_NATIVE,       /* регистры ШИМ видеокарты Intel */
    BL_ACPI          /* методы _BCM/_BQC устройства экрана в AML */
} BL_MODE;

typedef struct {
    BL_MODE mode;
    UINT16  gpu_id;          /* PCI Device ID видеокарты Intel (0 - нет) */
    BOOLEAN bxt;             /* регистры вида Apollo/Gemini Lake */
    BOOLEAN inverted;        /* ШИМ инвертирован (бит 29) */
    UINT32  pwm_max;         /* период ШИМ = 100% */
    UINT32  acpi_last;       /* последний уровень _BCM (если нет _BQC) */
    UINT32  hotkeys;         /* сколько раз нажали Fn+яркость */
    UINT64  bar;             /* BAR0 видеокарты */
    UINT16  pch;             /* семейство чипсета (LPC & 0xFF80) */
    UINT32  ctl1, r54, r58;  /* регистры ШИМ при загрузке (для debug) */
    const char *why;         /* почему не свой ШИМ */
    BOOLEAN blanked;         /* погашена (крышка закрыта, этап 10) */
    INT32   saved_pct;       /* яркость до гашения - вернуть при открытии */
} BACKLIGHT_INFO;

/* ACPI: то, что нужно для выключения (kernel/power.c) */
typedef struct {
    BOOLEAN ok;
    const char *why;
    UINT32 pm1a_cnt, pm1b_cnt;
    UINT32 smi_cmd;
    UINT8  acpi_enable;
    UINT8  slp_typa, slp_typb;
} ACPI_POWER;


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

#define KBD_REPEAT_DELAY_MS 500u
#define KBD_REPEAT_RATE_MS  33u



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
#define KX_MAX_DEVS     32
#define KX_MAX_HID      32    /* "трубы" прерываний: HID + хабы */
#define KX_MAX_HUBS     8
#define KX_MAX_MSD      4
#define KX_MAX_IF_PER_DEV 4
#define KX_MAX_DEV_PAGES  24   /* DMA-страниц на устройство */
#define KX_HUB_MAX_PORTS  15   /* больше в route string не влезает */
#define KX_DMA_LIMIT    0x100000000ull        /* ниже 4 ГиБ */

#define KX_ROLE_NONE        0
#define KX_ROLE_KBD_BOOT    1
#define KX_ROLE_MOUSE_RPT   2
#define KX_ROLE_MOUSE_BOOT  3
#define KX_ROLE_HUB         4   /* труба "изменились порты" хаба */
#define KX_ROLE_NET         5   /* bulk IN USB-модема (drivers/usbnet.c) */

#define KX_EP_RUN     0
#define KX_EP_RESET   1    /* ждём завершения Reset Endpoint */
#define KX_EP_SETDEQ  2    /* ждём завершения Set TR Dequeue */
#define KX_EP_DEAD    3

typedef struct {
    UINT64 phys;
    UINTN  seq;   /* сквозной номер следующего TRB (0,1,2,...) */
} KX_RING;

/*
 * Устройство USB. Устройства образуют дерево: у каждого, кроме
 * воткнутых прямо в контроллер, есть родитель - хаб.
 */
typedef struct {
    BOOLEAN used;
    UINT8   slot;
    UINT8   speed;         /* 1 FS, 2 LS, 3 HS, 4 SS, 5 SS+ */
    UINT8   root_port;     /* порт контроллера, через который
                              устройство подключено (1..) */
    INT8    parent;        /* индекс хаба-родителя в g_kx_devs,
                              -1 = прямо в порту контроллера */
    UINT8   parent_port;   /* порт на хабе-родителе */
    UINT8   depth;         /* 0 - в порту контроллера, 1 - за
                              одним хабом, ... */
    UINT32  route;         /* route string xHCI: по 4 бита на
                              каждый хаб по пути */
    UINT8   tt_slot;       /* LS/FS-устройство за HS-хабом: слот
                              этого хаба (Transaction Translator) */
    UINT8   tt_port;       /* ...и порт на нём */
    UINT16  vid;
    UINT16  pid;
    UINT8   dclass;
    UINT8   dprotocol;
    UINT16  mps0;
    UINT64  dev_ctx;
    UINT64  in_ctx;
    UINT64  buf;           /* страница под дескрипторы/запросы */
    KX_RING ep0;
    const char *status;
    INT8    hub;           /* индекс в g_kx_hubs или -1 */
    INT8    msd;           /* индекс в g_kx_msd или -1 */
    INT8    net;           /* USB-модем: индекс в drivers/usbnet.c или -1 */
    UINT64  pages[KX_MAX_DEV_PAGES];   /* всё, что вернуть при
                                          отключении */
    UINT8   npages;
} KX_DEV;

/* Труба прерываний (Interrupt IN): отчёты HID-клавиатуры/мыши
   или сообщения хаба "на таких-то портах что-то изменилось" */
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
    UINT8   net_slot;      /* KX_ROLE_NET: какой USB-модем */
} KX_HID;

/* USB-хаб */
typedef struct {
    BOOLEAN used;
    UINT8   dev;              /* индекс в g_kx_devs */
    UINT8   nports;
    BOOLEAN ss;               /* хаб USB 3 (SuperSpeed) */
    UINT8   think;            /* TT Think Time (для HS-хаба) */
    UINT16  pwr_ms;           /* сколько ждать после включения питания */
    UINT8   pipe;             /* индекс трубы в g_kx_hid */
    volatile UINT32 pending;  /* биты: порт N изменился (бит 0 - сам хаб) */
    INT8    child[KX_HUB_MAX_PORTS + 1];   /* устройство на порту */
    UINT64  events;
} KX_HUB;

/* USB-флешка / диск (Mass Storage, Bulk-Only + SCSI) */
typedef struct {
    BOOLEAN used;
    BOOLEAN ready;            /* ответил на READ CAPACITY */
    UINT8   dev;
    UINT8   iface;
    UINT8   in_addr, out_addr;
    UINT8   in_dci, out_dci;
    UINT16  in_mps, out_mps;
    KX_RING in_ring, out_ring;
    UINT64  cmd_buf;          /* CBW/CSW */
    UINT64  data_buf;         /* 4 КиБ данных */
    UINT32  tag;
    UINT64  blocks;           /* сколько секторов */
    UINT32  block_size;       /* байт в секторе */
    char    vendor[9];
    char    product[17];
    const char *note;
    UINT64  reads, writes, errors;
    UINT32  serial;           /* номер подключения: у каждой вставленной
                                 флешки свой (blk.c отличает новую от
                                 вынутой) */
} KX_MSD;

typedef struct {
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

    /* прерывания */
    const char *irq_mode; /* "MSI", "MSI-X" или "polling" */
    UINT64  irqs;

    /* кто сейчас синхронно ждёт событие (см. kx_wait_event) */
    volatile BOOLEAN wait_active;
    volatile BOOLEAN wait_done;
    UINT8   wait_type;
    UINT64  wait_ptr;
    UINT8   wait_slot, wait_ep;
    UINT32  wait_ev[4];

    /* корневые порты, на которых что-то изменилось (из обработчика
       прерывания; разбирается в kx_service): бит 0 - было событие,
       бит 1 - менялось подключение */
    volatile UINT8 root_change[256];
    volatile BOOLEAN any_change;

    UINT64  hot_added;
    UINT64  hot_removed;
} KX_STATE;


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

/* Конечная точка для команды Configure Endpoint */
typedef struct {
    UINT8  dci;
    UINT8  type;          /* 2 Bulk OUT, 3 Interrupt OUT, 6 Bulk IN,
                             7 Interrupt IN */
    UINT16 maxpkt;
    UINT8  burst;
    UINT8  interval;      /* поле Interval xHCI (для прерываний) */
    UINT64 ring;
    UINT16 avg;
    UINT32 esit;
} KX_EPCFG;

/* Интерфейс флешки, найденный в Configuration Descriptor */
typedef struct {
    BOOLEAN found;
    UINT8  iface;
    UINT8  in_addr, out_addr;
    UINT16 in_mps, out_mps;
    UINT8  in_burst, out_burst;
} KX_MSD_CAND;


/* ================================================================
 * Глобальные переменные (определены в модулях)
 * ================================================================ */

extern UINTN g_color;
extern UINTN g_current_attr;
extern EFI_TIME g_boot_time;
extern BOOLEAN  g_have_boot_time;
extern BOOLEAN g_kernel_mode;
extern FS_FILE g_fs[FS_MAX_FILES];
extern CHAR16 g_history[HIST_MAX][LINE_MAX];
extern UINTN  g_history_count;
extern UINTN g_scrollback_visible_rows;
extern CHAR16 g_scrollback[
    SCROLLBACK_MAX_LINES
][SCROLLBACK_LINE_MAX];
extern UINT8 g_scrollback_attr[
    SCROLLBACK_MAX_LINES
][SCROLLBACK_LINE_MAX];
extern UINTN g_scrollback_count;
extern UINTN g_scrollback_line_len;
extern int g_scrollback_view;
extern BOOLEAN g_scrollback_replaying;
extern UINT64 g_tsc_hz;
extern EFI_SYSTEM_TABLE *g_st;
extern volatile UINT32 *g_kfb;
extern UINT32 g_kfb_w;
extern UINT32 g_kfb_h;
extern UINT32 g_kfb_stride;
extern EFI_GRAPHICS_PIXEL_FORMAT g_kfb_fmt;
extern const UINT8 g_kfont[95][16];
extern UINT8 g_kcon_ch[KCON_MAX_ROWS][KCON_MAX_COLS];
extern UINT8 g_kcon_at[KCON_MAX_ROWS][KCON_MAX_COLS];
extern UINT8 g_kcon_sch[KCON_MAX_ROWS][KCON_MAX_COLS];
extern UINT8 g_kcon_sat[KCON_MAX_ROWS][KCON_MAX_COLS];
extern BOOLEAN g_kcon_shadow_valid;
extern UINTN g_kcon_cols;
extern UINTN g_kcon_rows;
extern UINTN g_kcon_scale;
extern UINTN g_kcon_x0;
extern UINTN g_kcon_y0;
extern UINTN g_kcon_col;
extern UINTN g_kcon_row;
extern UINT8 g_kcon_attr;
extern BOOLEAN g_kcon_cursor_on;
extern UINTN g_kcon_cur_drawn_r;
extern UINTN g_kcon_cur_drawn_c;
extern UINT32 g_kcon_palette[16];
extern SIMPLE_TEXT_OUTPUT_MODE g_kcon_mode;
extern SIMPLE_TEXT_OUTPUT_INTERFACE g_kcon_out;
extern BOOLEAN g_kcon_dirty;
extern UINT64  g_kcon_last_flush;
extern UINT64 g_kgdt[8] __attribute__((aligned(16)));
extern KX_TSS g_ktss;
extern MYOS_BOOT_INFO g_boot;
extern UINT64  g_vmm_pml4_phys;
extern UINT64  g_vmm_table_pages;
extern UINT64  g_vmm_hhdm_top;
extern UINT64  g_vmm_pages_2m;
extern UINT64  g_vmm_pages_4k;
extern BOOLEAN g_vmm_nx;
extern BOOLEAN g_vmm_pat;
extern BOOLEAN g_vmm_ready;
extern KSTACK_INFO g_kstacks[KSTACK_MAX];
extern UINTN g_kstack_count;
extern UINT64 g_kheap_slab_pages;
extern UINT64 g_kheap_big_pages;
extern UINT64 g_kheap_live;
extern UINT64 g_kheap_live_bytes;
extern UINT64 g_kheap_bad_frees;
extern UINT64 g_kmm_reclaimed_pages;
extern ACPI_POWER g_acpi_power;
extern ACPI_INFO g_acpi;
extern ACPI_DEVS g_acpid;
extern UINT64  g_pci_ecam_base;
extern UINT8   g_pci_ecam_bus_start;
extern UINT8   g_pci_ecam_bus_end;
extern BOOLEAN g_pci_ecam_active;
extern UINT64  g_tsc_hz_hpet;
extern UINT64  g_tsc_hz_pmtmr;
extern EFI_RUNTIME_SERVICES g_krt;
extern KX_IDT_ENTRY g_kidt[256] __attribute__((aligned(16)));
extern volatile UINT64 g_kticks;
extern volatile UINT64 g_kspurious;
extern volatile UINT64 g_kstray;
extern volatile UINT64 g_kstray_last;
extern volatile UINT64 g_kbreakpoints;
extern volatile UINT64 g_kbp_rip;
extern UINT64 g_tsc_hz_stall;
extern UINT64 g_tsc_hz_pit;
extern const char *g_tsc_source;
extern BOOLEAN g_lapic_x2;
extern UINT64  g_lapic_base;
extern UINT64  g_lapic_hz;
extern BOOLEAN g_ktimer_ok;
extern UINT64  g_kboot_tsc;
extern KMM_REGION g_kmm_map[KMM_MAX_REGIONS];
extern UINTN  g_kmm_map_count;
extern UINTN  g_kmm_map_dropped;
extern UINT64 *g_kmm_bitmap;
extern UINT64 g_kmm_bitmap_phys;
extern UINT64 g_kmm_bitmap_pages;
extern UINT64 g_kmm_total_pages;
extern UINT64 g_kmm_usable_pages;
extern UINT64 g_kmm_free_pages;
extern BOOLEAN g_kmm_ready;
extern EFI_INPUT_KEY g_kbd_queue[KBD_QUEUE_SIZE];
extern UINTN g_kbd_q_head;
extern UINTN g_kbd_q_tail;
extern BOOLEAN g_kbd_caps;
extern UINT8 g_kbd_layout;
extern UINT8   g_kbd_usb_mods;
extern UINT8   g_kbd_ps2_mods;
extern UINT64  g_kbd_keys_total;
extern UINT8  g_kbd_rep_usage;
extern UINT64 g_kbd_rep_next_tsc;
extern const char g_kbd_sym[0x39 - 0x1E][2];
extern BOOLEAN g_ps2_present;
extern BOOLEAN g_ps2_e0;
extern UINTN   g_ps2_skip;
extern UINT64  g_ps2_bytes;
extern UINT8   g_ps2_config;
extern const UINT8 g_ps2_to_hid[0x59];
extern INT64  g_kmouse_dx;
extern INT64  g_kmouse_dy;
extern INT64  g_kmouse_dz;
extern UINT32 g_kmouse_buttons;
extern BOOLEAN g_kmouse_present;
extern UINT64 g_kmouse_reports;
extern KX_STATE g_kx;
extern KX_DEV g_kx_devs[KX_MAX_DEVS];
extern KX_HID g_kx_hid[KX_MAX_HID];
extern EFI_SYSTEM_TABLE      g_kst;
extern EFI_BOOT_SERVICES     g_kbs;
extern SIMPLE_INPUT_INTERFACE g_kconin;
extern EFI_GRAPHICS_OUTPUT_PROTOCOL         g_kgop;
extern EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE    g_kgop_mode;
extern EFI_GRAPHICS_OUTPUT_MODE_INFORMATION g_kgop_info;
extern EFI_SIMPLE_POINTER_PROTOCOL g_kptr;
extern EFI_SIMPLE_POINTER_MODE     g_kptr_mode;


/* ================================================================
 * Функции, по модулям
 * ================================================================ */

/* --- lib/libc.c --- */
__attribute__((used, visibility("hidden")))
void *memcpy(void *dst, const void *src, myos_size_t n);
__attribute__((used, visibility("hidden")))
void *memmove(void *dst, const void *src, myos_size_t n);
__attribute__((used, visibility("hidden")))
void *memset(void *dst, int value, myos_size_t n);
__attribute__((used, visibility("hidden")))
int memcmp(const void *a, const void *b, myos_size_t n);

/* --- shell/console.c --- */
void set_color(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINTN attr
);
void scrollback_newline(void);
void scrollback_char(CHAR16 c);
void redraw_input(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const CHAR16 *line
);
void scrollback_render(
    EFI_SYSTEM_TABLE *st,
    int view
);
void print(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const char *s
);
void print16(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const CHAR16 *s
);
void print_uint(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 value
);
void print_int(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    INT32 value
);
void print_uint2(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINTN value
);
void print_hex(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT64 value,
    UINTN digits
);

/* --- drivers/pci.c --- */
BOOLEAN pci_use_ecam(UINT64 base, UINT8 bus_start, UINT8 bus_end);
UINT32 pci_config_read32(
    UINT8 bus, UINT8 dev, UINT8 func, UINT8 offset
);
void pci_config_write32(
    UINT8 bus, UINT8 dev, UINT8 func, UINT8 offset, UINT32 value
);
void pci_enable_device(UINT8 bus, UINT8 dev, UINT8 func);
UINT64 pci_read_bar_address(
    UINT8 bus, UINT8 dev, UINT8 func, UINT8 bar_offset
);
BOOLEAN pci_find_xhci(
    UINT8 *out_bus,
    UINT8 *out_dev,
    UINT8 *out_func,
    UINT64 *out_mmio
);

/* --- drivers/xhci_common.c --- */
void raw_zero_mem(volatile UINT8 *p, UINTN n);
UINT32 portsc_base_for_write(UINT32 cur);

/* --- kernel/time.c --- */
void tsc_delay_us(UINT64 us);
void busy_wait_ms(UINTN ms);

/* --- drivers/xhci_common.c --- */
BOOLEAN xhci_reset_controller(UINT64 op_base);
void xhci_disconnect_firmware_driver(
    EFI_SYSTEM_TABLE *st,
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINT8 target_bus,
    UINT8 target_dev,
    UINT8 target_func
);
void xhci_read_cap_regs(UINT64 mmio_base, XHCI_CAP_INFO *info);
BOOLEAN xhci_bios_handoff(
    EFI_SYSTEM_TABLE *st,
    UINT64 mmio_base,
    UINT32 ext_cap_off,
    SIMPLE_TEXT_OUTPUT_INTERFACE *out
);

/* --- lib/string.c --- */
UINTN char16_len(const CHAR16 *s);
void char16_copy(
    CHAR16 *dst,
    const CHAR16 *src,
    UINTN max
);
int char16_eq(
    const CHAR16 *a,
    const CHAR16 *b
);
int streq(
    const CHAR16 *a,
    const char *b
);
int kstreq(const char *a, const char *b);
int starts_with(
    const CHAR16 *a,
    const char *prefix
);
UINTN parse_uint(
    const CHAR16 *s
);
CHAR16 *skip_ws16(
    CHAR16 *s
);
CHAR16 *take_word(
    CHAR16 *s,
    CHAR16 *out,
    UINTN max
);

/* --- shell/readline.c --- */
void erase_input_line(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    UINTN len
);
void read_line(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *line,
    UINTN max
);
int fs_find(
    const CHAR16 *name
);

/* --- shell/fs.c --- */
int fs_find_free(void);
BOOLEAN fs_shell_command(EFI_SYSTEM_TABLE *st, const CHAR16 *line);




/* --- shell/fetch.c --- */
void print_label(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    const char *label
);
void print_separator(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out
);
void print_bool(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    BOOLEAN value
);
void cmd_fetch(
    EFI_SYSTEM_TABLE *st
);
void push_history(
    CHAR16 *line
);

/* --- shell/history.c --- */
void cmd_history(
    EFI_SYSTEM_TABLE *st
);

/* --- gui/fb.c: рисование прямо в видеопамять (консоль, паника) --- */
UINT32 gui_pack(
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    UINT8 r, UINT8 g, UINT8 b
);
void gui_fill_rect(
    volatile UINT32 *fb,
    UINT32 stride,
    UINT32 fb_w,
    UINT32 fb_h,
    INTN x, INTN y,
    UINTN w, UINTN h,
    UINT32 color
);
void gui_draw_cursor_at(
    volatile UINT32 *fb,
    UINT32 stride, UINT32 fb_w, UINT32 fb_h,
    EFI_GRAPHICS_PIXEL_FORMAT fmt,
    INTN x, INTN y
);

/* ================================================================
 * Оконная система (этап 7): gui/gfx.c, gui/wm.c, gui/apps.c
 * ================================================================ */

#define FONT_W 8
#define FONT_H 16

/* Поверхность для рисования: пиксели 0x00RRGGBB, строка stride */
typedef struct {
    UINT32 *px;
    UINT32  w, h, stride;
    INT32   cx0, cy0, cx1, cy1;    /* отсечение (clip) */
} GFX;

void gfx_init(GFX *g, UINT32 *px, UINT32 w, UINT32 h, UINT32 stride);
void gfx_noclip(GFX *g);
void gfx_clip(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h);
void gfx_fill(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h, UINT32 col);
void gfx_pixel(GFX *g, INT32 x, INT32 y, UINT32 col);
void gfx_blit(GFX *g, INT32 x, INT32 y, const UINT32 *src, INT32 w, INT32 h, UINT32 src_stride);
void gfx_bevel(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h, BOOLEAN raised);
void gfx_button(GFX *g, INT32 x, INT32 y, INT32 w, INT32 h, BOOLEAN pressed);
void gfx_glyph(GFX *g, INT32 x, INT32 y, UINT32 cp, UINT32 col);
void gfx_glyph_up(GFX *g, INT32 x, INT32 y, UINT32 cp, UINT32 col);
INT32 gfx_text(GFX *g, INT32 x, INT32 y, const char *s, UINT32 col);
INT32 gfx_text_fit(GFX *g, INT32 x, INT32 y, const char *s, UINT32 col, UINTN max_chars);
INT32 gfx_text_bold(GFX *g, INT32 x, INT32 y, const char *s, UINT32 col);
INT32 gfx_text_width(const char *s);
void gfx_icon(GFX *g, INT32 x, INT32 y, const char *const *rows, UINT32 scale, BOOLEAN selected);
UINT32 utf8_next(const char **s);
UINTN utf8_put(UINT32 c, char *out);
UINTN utf8_len(const char *s);
INTN font_index(UINT32 cp);
UINT32 font_alpha(INTN idx, UINT32 x, UINT32 y);
const char *const *gui_icon(const char *name);

/* Событие окну (в формате struct myos_event из sysnum.h) */
#define WIN_TITLE_MAX  64
#define WM_MAX_WINDOWS 16

struct WIN;

typedef struct {
    const char *name;                       /* для панели задач и заголовка */
    const char *const *icon;                /* значок 16x16 */
    void  (*paint)(struct WIN *w, GFX *g);  /* нарисовать содержимое окна */
    void  (*event)(struct WIN *w, struct myos_event *e);   /* нажатие/клик */
    void  (*tick)(struct WIN *w);           /* раз в ~0.1 с (часы, таймер) */
    void  (*close)(struct WIN *w);          /* окно закрывают */
} WIN_CLASS;

typedef struct WIN {
    BOOLEAN         used;
    UINT32          id;
    INT32           x, y;                   /* левый верх ОКНА (с рамкой) */
    INT32           cw, ch;                 /* размер содержимого */
    char            title[WIN_TITLE_MAX];
    UINT32          z;                      /* порядок: больше - выше */
    BOOLEAN         minimized;
    BOOLEAN         want_redraw;
    volatile BOOLEAN dead;                   /* хозяин-программа ушёл: закрыть */

    UINT32         *buf;                     /* пиксели содержимого cw x ch */
    UINTN           buf_pages;
    UINT64          buf_phys;                /* для окна программы (общее с ней) */

    const WIN_CLASS *cls;                    /* родное окно ядра (иначе NULL) */
    void           *state;                   /* данные родного окна */

    struct KPROC   *proc;                    /* окно программы (иначе NULL) */
    const char *const *icon;                 /* значок окна программы (по её имени) */
    /* очередь событий окна программы */
    struct myos_event evq[32];
    volatile UINTN  ev_head, ev_tail;
} WIN;

/* Рамка окна */
#define WIN_TITLE_H   20
#define WIN_BORDER    3
#define WIN_FRAME_W(cw) ((cw) + 2 * WIN_BORDER)
#define WIN_FRAME_H(ch) ((ch) + WIN_TITLE_H + 2 * WIN_BORDER)

extern WIN g_windows[WM_MAX_WINDOWS];
extern volatile BOOLEAN g_wm_running;
void wm_start(EFI_SYSTEM_TABLE *st);
WIN *wm_open(const WIN_CLASS *cls, INT32 cw, INT32 ch, const char *title, void *state);
void wm_close(WIN *w);
void wm_invalidate(WIN *w);            /* содержимое изменилось - перерисовать */
void wm_set_title(WIN *w, const char *title);
GFX wm_client_gfx(WIN *w);
void wm_focus(WIN *w);
WIN *wm_focused(void);
BOOLEAN wm_push_event(WIN *w, struct myos_event *e);
void wm_request_stop(void);
void wm_invalidate_desktop(void);

/* родные приложения (gui/apps.c) */
void app_open_terminal(void);
void app_open_notepad(const char *path);
void app_open_explorer(const char *path);
void app_open_minesweeper(void);
void app_open_about(void);
void app_open_program(const char *name);
void app_run_bare(const char *name);
void apps_desktop_launch(const char *what);   /* ярлык рабочего стола */
void apps_draw_shortcuts(GFX *g);
void apps_shortcut_click(INT32 x, INT32 y);

/* окна программ (kernel/winproc.c, из syscall.c) */
INT64 win_sys_create(struct KPROC *p, UINT64 w, UINT64 h, UINT64 utitle);
INT64 win_sys_update(struct KPROC *p, UINT64 id, UINT64 urect);
INT64 win_sys_event(struct KPROC *p, UINT64 id, UINT64 uevent, UINT64 wait_ms);
INT64 win_sys_close(struct KPROC *p, UINT64 id);
INT64 win_sys_title(struct KPROC *p, UINT64 id, UINT64 utitle);
void win_proc_cleanup(struct KPROC *p);

/* --- drivers/hid.c --- */
UINT32 hid_extract_bits(
    volatile UINT8 *report,
    UINTN report_len_bytes,
    UINT32 bit_offset,
    UINT8 bit_size
);
INT32 hid_sign_extend(UINT32 raw_value, UINT8 bit_size);
INT32 hid_item_signed_value(
    UINT32 raw,
    UINT8 item_size_bytes
);
void hid_parse_report_descriptor(
    SIMPLE_TEXT_OUTPUT_INTERFACE *out,
    volatile UINT8 *desc,
    UINT16 desc_len,
    HID_MOUSE_REPORT_LAYOUT *layout
);

/* --- kernel/kcon.c --- */
void kcon_draw_cell(UINTN r, UINTN c, BOOLEAN with_cursor);
void kcon_commit_cell(UINTN r, UINTN c);
void kcon_sync(void);
void kcon_update_cursor(void);
void kcon_flush(void);
void kcon_scroll(void);
void kcon_newline(void);
void kcon_put_char(CHAR16 c);
EFI_STATUS EFIAPI kcon_output_string(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    CHAR16 *str
);
EFI_STATUS EFIAPI kcon_set_attribute(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    UINTN attr
);
EFI_STATUS EFIAPI kcon_clear_screen(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out
);
EFI_STATUS EFIAPI kcon_reset(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    BOOLEAN extended
);
EFI_STATUS EFIAPI kcon_test_string(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    CHAR16 *str
);
EFI_STATUS EFIAPI kcon_query_mode(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    UINTN mode_number,
    UINTN *columns,
    UINTN *rows
);
EFI_STATUS EFIAPI kcon_set_mode(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    UINTN mode_number
);
EFI_STATUS EFIAPI kcon_set_cursor_position(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    UINTN column,
    UINTN row
);
EFI_STATUS EFIAPI kcon_enable_cursor(
    SIMPLE_TEXT_OUTPUT_INTERFACE *this_out,
    BOOLEAN visible
);
void kcon_init(void);
UINTN kx_hex_str(UINT64 v, UINTN digits, char *buf);
void kx_raw_text(
    UINTN x, UINTN y,
    const char *s,
    UINT32 fg, UINT32 bg
);

/* --- kernel/cpu.c --- */
void kx_load_gdt(void);
const char *kx_exception_name(UINT64 v);
void kx_panic(KX_ISR_FRAME *f);
__attribute__((used, visibility("hidden")))
void kx_isr_dispatch(KX_ISR_FRAME *f);
void kx_idt_set(UINTN vec, UINT64 handler);
void kx_load_idt(void);
void kx_pic_disable(void);
UINTN kx_ioapic_mask_all(UINT64 base);
void kx_load_tss(UINT64 ist_df, UINT64 ist_nmi, UINT64 ist_mc, UINT64 rsp0);

/* --- kernel/vmm.c --- */
BOOLEAN vmm_init(void);
BOOLEAN vmm_map_page(UINT64 virt, UINT64 phys, UINT32 attr);
void vmm_unmap_page(UINT64 virt);
BOOLEAN vmm_map_mmio(UINT64 phys, UINT64 size, UINT32 cache);
BOOLEAN vmm_ensure_mapped(UINT64 phys, UINT64 size, UINT32 cache);
BOOLEAN vmm_ensure_writable(UINT64 phys, UINT64 size, UINT32 cache);
UINT64 vmm_virt_to_phys(UINT64 virt);
UINT64 vmm_query(UINT64 virt);
UINT64 vmm_alloc_stack(UINTN pages, const char *name);
const char *vmm_describe(UINT64 a);

/* --- kernel/kmalloc.c --- */
VOID *kmalloc(UINTN size);
VOID *kzalloc(UINTN size);
BOOLEAN kfree(VOID *ptr);
BOOLEAN kmalloc_selftest(char *report, UINTN cap);

/* --- kernel/acpi.c --- */
BOOLEAN acpi_init(void);
const UINT8 *acpi_table(const char *sig, UINTN n, UINT32 *len_out);
UINT64 acpi_hpet_counter(void);
UINT64 acpi_hpet_measure_tsc_hz(void);
UINT64 acpi_pmtimer_measure_tsc_hz(void);
UINT32 acpi_current_apic_id(void);
void kernel_cmd_acpi(SIMPLE_TEXT_OUTPUT_INTERFACE *out);

/* --- kernel/smp.c (ядра процессора, этап 10) --- */
extern KX_CPU g_cpus[KX_MAX_CPUS];
extern UINT32 g_ncpus, g_ncpus_found;
void smp_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void smp_describe(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kernel_cmd_smptest(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg);
void kx_lapic_send_ipi(UINT32 apic_id, UINT32 low);
void smp_early_init(void);
void smp_halt_others(void);
void kx_bkl_enter(void);
void kx_bkl_exit(void);
UINT32 kx_bkl_release_all(void);
void kx_bkl_reacquire(UINT32 depth);
void kx_bkl_relax(void);
extern volatile UINT64 g_tlb_gen;
extern volatile BOOLEAN g_smp_go;
void sched_ap_enter(void) __attribute__((noreturn));
void sched_kick_idle(void);

/* --- kernel/acpi_os.c, kernel/acpi_dev.c (uACPI, этап 9) --- */
extern UINT64 g_acpi_sci_count, g_acpi_work_done, g_acpi_work_lost;
void acpi_os_start(void);
void acpi_dev_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void acpi_dev_note_log(BOOLEAN error, const char *msg);
void acpi_dev_poweroff(void);
BOOLEAN acpi_battery_brief(char *buf, UINTN cap, BOOLEAN *charging);
void kernel_cmd_battery(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kernel_cmd_lid(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg);

/* --- kernel/backlight.c (яркость, этап 9) --- */
extern BACKLIGHT_INFO g_backlight;
extern HDA_INFO g_hda;
void hda_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
INT32 hda_volume(INT32 v);
void hda_mute_toggle(void);
UINT32 hda_queued(void);
UINT32 hda_write(const INT16 *s, UINT32 bytes);
void hda_wait_room(UINT32 ms);
void hda_test_tone(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINT32 ms);
void kernel_cmd_sound(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg);
void hda_proc_gone(UINT32 pid);
struct KPROC;
INT64 sys_audio(struct KPROC *p, UINT64 op, UINT64 a1, UINT64 a2);
void backlight_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
INT32 backlight_get(void);
INT32 backlight_set(INT32 pct);
INT32 backlight_step(INT32 dir);
BOOLEAN backlight_blank(BOOLEAN off);
void kernel_cmd_brightness(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg);

/* --- kernel/tz.c --- */
#define TZ_MOSCOW     0
#define TZ_JERUSALEM  1
extern UINTN g_tz;
INT32 tz_offset_minutes(const EFI_TIME *utc);
const char *tz_abbrev(const EFI_TIME *utc);
const char *tz_city(void);
void tz_to_local(EFI_TIME *t);
void tz_toggle(void);
void tz_describe(char *buf, UINTN cap);
void kernel_cmd_tz(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg);

/* --- kernel/power.c --- */
BOOLEAN rtc_read(EFI_TIME *t);
void kx_reboot(void) __attribute__((noreturn));
void kx_shutdown(void);
const UINT8 *acpi_find_table(const char *sig);
void acpi_power_init(void);

/* --- kernel/irq.c --- */
extern UINT64 g_irq_count[256];
extern volatile UINT32 g_cpu_load_permille;
extern volatile UINT32 g_cpu_load_valid;
void kx_irq_register(UINT8 vector, KX_IRQ_HANDLER fn);
BOOLEAN kx_irq_dispatch(UINT8 vector);
UINT32 kx_irq_to_gsi(UINT8 irq, BOOLEAN *level, BOOLEAN *active_low);
BOOLEAN kx_ioapic_route(UINT32 gsi, UINT8 vector, BOOLEAN level, BOOLEAN active_low);
UINT8 pci_find_cap(UINT8 bus, UINT8 dev, UINT8 fn, UINT8 id);
const char *kx_pci_enable_msi(UINT8 bus, UINT8 dev, UINT8 fn, UINT8 vector);
void kx_ioapic_mask(UINT32 gsi);
void kx_idle_hlt(void);
void kx_load_tick(void);
void kernel_cmd_cpu(SIMPLE_TEXT_OUTPUT_INTERFACE *out);

/* --- kernel/sched.c --- */
extern KTHREAD g_kthreads[KT_MAX];
extern volatile BOOLEAN g_sched_on;
extern UINT64 g_sched_switches;
extern UINT64 g_sched_preempts;
void kx_lock(void);
void kx_unlock(void);
BOOLEAN kx_lock_relaxable(void);
UINT64 kx_irq_save(void);
void kx_irq_restore(UINT64 fl);
void sched_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
KTHREAD *kthread_create(const char *name, void (*fn)(void *), void *arg, UINTN stack_pages);
void kthread_exit(void) __attribute__((noreturn));
void kthread_rename(KTHREAD *t, const char *name);
BOOLEAN kthread_alive(KTHREAD *t, UINT32 tid);
void sched_yield(void);
void schedule(void);
void sched_tick(void);
void sched_isr_exit(void);
BOOLEAN sched_can_block(void);
void sched_sleep_ms(UINT64 ms);
BOOLEAN sched_block(const void *obj, const char *what, UINT64 timeout_ms);
UINTN sched_wake_all(const void *obj);
BOOLEAN sched_wake_one(const void *obj);
void sched_account_load(void);
void kmutex_lock(KMUTEX *m);
void kmutex_unlock(KMUTEX *m);
UINT64 kspin_lock(KSPINLOCK *l);
void kspin_unlock(KSPINLOCK *l, UINT64 fl);
const char *kthread_state_name(KT_STATE s);
UINTN kthread_stack_used(KTHREAD *t);
UINTN sched_snapshot(KT_INFO *out, UINTN cap);
void kernel_cmd_ps(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kernel_cmd_threadtest(SIMPLE_TEXT_OUTPUT_INTERFACE *out);

/* --- kernel/proc.c, kernel/syscall.c --- */
extern KPROC g_procs[PROC_MAX];
extern KPROC *volatile g_fg_proc;
void proc_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void proc_switch_hook(KTHREAD *next);
void kx_syscall_cpu_init(void);
KPROC *proc_spawn(const char *path, const char *args, UINT32 io, INTN *err);
KPROC *proc_spawn_ex(const char *path, const char *args, UINT32 io, const char *cwd,
                     KPROC *parent, INTN out_kfd, KTTY *tty, INTN *err);

/* --- gui/tty.c (терминал рабочего стола) --- */
void tty_open_window(const char *cmd);
void tty_ref(KTTY *t);
void tty_unref(KTTY *t);
void tty_putc(KTTY *t, UINT32 c);
void tty_write(KTTY *t, const char *s, UINTN n);
INT32 tty_getkey(KTTY *t, KPROC *p, INT64 timeout_ms);
INTN tty_read_line(KTTY *t, KPROC *p, char *dst, UINTN n);
SIMPLE_TEXT_OUTPUT_INTERFACE *tty_output(KTTY *t);
void tty_set_fg(KTTY *t, KPROC *p);
KPROC *tty_get_fg(KTTY *t);
void kernel_shell_main(void) __attribute__((noreturn));
INT64 proc_wait(KPROC *p);
void proc_reap(KPROC *p);
void proc_fault(const char *what, UINT64 rip);
void proc_check_kill(void);
void proc_exit_current(INT64 code) __attribute__((noreturn));
void proc_kill(KPROC *p);
void proc_ctrl_c(void);
void proc_forget_volume(const char *name);
BOOLEAN proc_find_program(const char *name, char *path, UINTN cap);
INT64 kx_syscall_dispatch(UINT64 *frame);
void kx_user_fault(KX_ISR_FRAME *f);
void kernel_cmd_run(EFI_SYSTEM_TABLE *st, const char *path, const char *args, BOOLEAN quiet);
BOOLEAN proc_shell_try(EFI_SYSTEM_TABLE *st, const CHAR16 *line);
BOOLEAN uptr_ok(KPROC *p, UINT64 addr, UINT64 len, BOOLEAN write);
BOOLEAN proc_map_heap_page(KPROC *p, UINT64 va);
extern const char *g_proc_last_error;

/* --- fs/binfs.c --- */
extern const VFS_OPS g_bin_ops;
void binfs_mount(void);

/* --- drivers/blk.c --- */
extern BLKDEV g_blk[BLK_MAX];
extern KMUTEX g_vfs_mutex;
INTN blk_register_disk(const char *prefix, const char *kind, const char *model,
                       UINT64 sectors, UINT32 sector_size, BOOLEAN writable,
                       const char *ro_reason, BLK_RW rw, UINTN drv_index,
                       UINT32 drv_serial);
void blk_remove(UINTN idx);
BOOLEAN blk_read(UINTN dev, UINT64 lba, UINT32 count, VOID *buf);
extern INTN g_blk_write_window;
BOOLEAN blk_write(UINTN dev, UINT64 lba, UINT32 count, const VOID *buf);
void blk_sync_usb(void);
void blk_scan_partitions(UINTN idx);
void storage_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kernel_cmd_disk(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg);

/* --- drivers/usbmsd.c (для blk.c) --- */
BOOLEAN usb_msd_rw(UINTN mi, UINT32 serial, UINT64 lba, UINT32 count, VOID *buf, BOOLEAN write);
BOOLEAN usb_msd_alive(UINTN mi, UINT32 serial);
BOOLEAN usb_msd_info(UINTN mi, UINT32 *serial, UINT64 *blocks, UINT32 *bsize,
                     char *model, UINTN cap);

/* --- drivers/ahci.c, drivers/nvme.c --- */
void ahci_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void nvme_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
BOOLEAN pci_find_class(UINT8 base, UINT8 sub, INT16 progif, UINTN nth,
                       UINT8 *bus, UINT8 *dev, UINT8 *func);
BOOLEAN blk_wait(volatile UINT32 *reg, UINT32 mask, UINT32 want, UINT64 timeout_ms);

/* --- fs/fat.c --- */
extern const VFS_OPS g_fat_ops;
BOOLEAN fat_probe(UINTN dev, FAT_VOL *v, const char **why);

/* --- fs/exfat.c (exFAT через библиотеку FatFs) --- */
extern const VFS_OPS g_exfat_ops;
BOOLEAN exfat_detect(UINTN dev);
BOOLEAN exfat_mount(UINTN dev, VFS_MOUNT *m);
void exfat_release(VFS_MOUNT *m);

/* --- fs/vfs.c --- */
BOOLEAN vfs_is_disk(const VFS_MOUNT *m);
void vfs_fs_name(const VFS_MOUNT *m, char *buf, UINTN cap);
extern VFS_MOUNT g_mounts[VFS_MAX_MOUNTS];
extern char g_cwd[VFS_PATH_MAX];
void vfs_init(void);
INTN vfs_mount_dev(UINTN dev, const char *name);
void vfs_forget_dev(UINTN dev);
const char *vfs_strerror(INTN e);
INTN vfs_normalize(const char *path, char *out, UINTN cap);
INTN vfs_stat(const char *path, VFS_DIRENT *out);
INTN vfs_list(const char *path, INTN (*cb)(void *ctx, const VFS_DIRENT *e), void *ctx);
INTN vfs_open(const char *path, UINT32 flags);
INTN vfs_read(INTN fd, VOID *buf, UINTN n);
INTN vfs_write(INTN fd, const VOID *buf, UINTN n);
INTN vfs_close(INTN fd);
INTN vfs_size(INTN fd, UINT64 *size);
INTN vfs_seek(INTN fd, INT64 off, UINT32 whence, UINT64 *newpos);
INTN vfs_mkdir(const char *path);
INTN vfs_remove(const char *path);
INTN vfs_rename(const char *from, const char *to);
INTN vfs_chdir(const char *path);
INTN vfs_mount_info(const char *path, VFS_MOUNT **m);
INTN vfs_read_file(const char *path, VOID *buf, UINTN cap, UINTN *got);
INTN vfs_write_file(const char *path, const VOID *buf, UINTN n, BOOLEAN append);

/* --- kernel/kmain.c --- */
void kmain(MYOS_BOOT_INFO *bi) __attribute__((noreturn));

/* --- kernel/time.c --- */
UINT64 kx_pit_measure_tsc_hz(void);
void kx_lapic_write(UINT32 reg, UINT32 v);
UINT32 kx_lapic_read(UINT32 reg);
void kx_lapic_eoi(void);
BOOLEAN kx_lapic_timer_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
UINT64 kx_uptime_us(void);
void kx_sleep_us(UINT64 us);

/* --- kernel/pmm.c --- */
void pmm_save_map(
    VOID *map_buf,
    UINTN map_size,
    UINTN desc_size
);
BOOLEAN pmm_init(void);
UINT64 pmm_alloc_pages(UINT64 count, UINT64 limit);
void pmm_free_pages(UINT64 phys, UINT64 count);
UINT64 pmm_alloc_zeroed(UINT64 count, UINT64 limit);
UINT64 pmm_alloc_low_page(void);
const char *kmm_type_name(UINT32 t);
BOOLEAN pmm_free_type(UINT32 t);
UINT64 pmm_release_loader_temp(void);

/* --- drivers/keyboard.c --- */
void kbd_enqueue(UINT16 scan, CHAR16 uc);
BOOLEAN kbd_dequeue(EFI_INPUT_KEY *key);
BOOLEAN kbd_shift_down(void);
void kbd_press_usage(UINT8 u);
BOOLEAN kbd_usage_repeats(UINT8 u);
void kbd_usb_boot_report(
    volatile UINT8 *rep,
    UINTN len,
    UINT8 prev[6]
);
void kbd_repeat_tick(void);
void kbd_raw(UINT8 usage, BOOLEAN down);
BOOLEAN kbd_raw_dequeue(UINT8 *usage, BOOLEAN *down);
UINT8 kbd_led_bits(void);
extern BOOLEAN g_kbd_num;
extern BOOLEAN g_kbd_scroll;

/* --- drivers/ps2.c --- */
UINT8 ps2_e0_to_hid(UINT8 code);
void ps2_handle_byte(UINT8 b);
BOOLEAN ps2_wait_input_empty(void);
BOOLEAN ps2_wait_output_full(void);
void ps2_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void ps2_poll(void);
void ps2_service(void);
void ps2_set_leds(UINT8 usb_bits);
extern BOOLEAN g_ps2_aux_present;
extern BOOLEAN g_ps2_aux_wheel;
extern BOOLEAN g_ps2_irq;
extern UINT64  g_ps2_aux_packets;

/* --- drivers/usb.c --- */
#define KX_LOG_LINES 12
#define KX_LOG_LEN   96
void kx_msleep(UINTN ms);
extern KMUTEX g_usb_mutex;
extern volatile BOOLEAN g_kx_hub_pending;
extern UINT64 g_usb_thread_wakeups;
void kx_usb_start_thread(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
BOOLEAN kx_usb_threaded(void);
void kx_out(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void kx_event_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kx_print_event_log(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
UINT32 kx_ring_pcs(UINTN seq);
UINT64 kx_ring_slot_addr(KX_RING *r, UINTN seq);
void kx_ring_init(KX_RING *r, UINT64 phys);
UINT64 kx_ring_push(KX_RING *r, UINT32 d0, UINT32 d1, UINT32 d2, UINT32 d3);
void kx_erdp_update(void);
BOOLEAN kx_ev_fetch(UINT32 ev[4]);
void kx_doorbell(UINT8 slot, UINT32 target);
UINT64 kx_dma_page(void);
UINT64 kx_dev_page(KX_DEV *d);
void kx_pump(void);
void kx_handle_async_event(UINT32 ev[4]);
BOOLEAN kx_wait_event(UINT8 type, UINT64 match_ptr, UINT8 slot, UINT8 epid,
                      UINT32 out_ev[4], UINTN timeout_ms);
UINT8 kx_command(UINT32 d0, UINT32 d1, UINT32 d2, UINT32 d3, UINT8 *out_slot);
void kx_recover_sync(UINT8 slot, UINT8 dci, KX_RING *r);
UINT8 kx_control(KX_DEV *d, UINT8 bm_request_type, UINT8 b_request,
                 UINT16 w_value, UINT16 w_index, UINT16 w_length, UINT64 data_phys);
UINT8 kx_bulk(KX_DEV *d, UINT8 dci, UINT8 ep_addr, KX_RING *r,
              UINT64 buf, UINT32 len, UINT32 *actual, UINTN timeout_ms);
void kx_usb_irq(void);
UINT8 kx_interval_field(UINT8 speed, UINT8 b_interval);
UINT8 kx_configure_eps(KX_DEV *d, KX_EPCFG *eps, UINTN n, UINT8 hub_ports, UINT8 tt_think);
const char *kx_speed_name(UINT8 s);
void kx_dev_path(KX_DEV *d, char *buf, UINTN cap);
INTN kx_enum_device(SIMPLE_TEXT_OUTPUT_INTERFACE *out, INTN parent, UINT8 parent_port,
                    UINT8 root_port, UINT8 speed);
void kx_remove_device(UINTN di);
void kx_root_port_connect(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN p);
void kx_service(void);
void kx_usb_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kernel_poll_input(void);

/* --- drivers/usbhid.c --- */
extern volatile BOOLEAN g_kbd_leds_dirty;
const char *kx_role_name(UINT8 r);
void kx_pipe_queue(KX_HID *h);
void kx_pipe_report(KX_HID *h, UINTN len);
INTN kx_pipe_alloc(void);
UINTN kx_hid_prepare(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN di,
                     KX_HID_CAND *cand, UINTN ncand,
                     KX_EPCFG *eps, UINTN *neps, UINTN *pipes);
void kx_hid_start(UINTN *pipes, UINTN n);
void kx_hid_update_presence(void);
void kx_pipes_watchdog(void);
void kx_hid_service_leds(void);

/* --- drivers/usbhub.c --- */
extern KX_HUB g_kx_hubs[KX_MAX_HUBS];
void kx_hub_setup(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN di);
void kx_hub_report(KX_HID *h, volatile UINT8 *rep, UINTN len);
void kx_hub_service(void);

/* --- drivers/usbmsd.c --- */
extern KX_MSD g_kx_msd[KX_MAX_MSD];
INTN kx_msd_prepare(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN di,
                    KX_MSD_CAND *c, KX_EPCFG *eps, UINTN *neps);
void kx_msd_start(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN mi);

/* --- drivers/usbnet.c (USB-модемы: RNDIS, CDC-ECM, CDC-NCM) --- */
BOOLEAN kx_net_probe(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINTN di, UINT8 ncfg);
void kx_net_removed(INTN mi);
void kx_net_report(KX_HID *h, volatile UINT8 *buf, UINTN len);
void kx_net_print(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
extern BOOLEAN g_usbnet_prefer_rndis;

/* --- kernel/shim.c --- */
EFI_STATUS EFIAPI kbs_unsupported(void);
UINT64 EFIAPI kbs_stall(UINTN us);
EFI_STATUS EFIAPI kbs_allocate_pool(
    UINTN pool_type, UINTN size, VOID **buffer
);
EFI_STATUS EFIAPI kbs_free_pool(VOID *buffer);
EFI_STATUS EFIAPI kbs_allocate_pages(
    UINTN type, UINTN mem_type, UINTN pages, UINT64 *memory
);
EFI_STATUS EFIAPI kbs_free_pages(UINT64 memory, UINTN pages);
EFI_STATUS EFIAPI kbs_set_watchdog(
    UINTN timeout, UINT64 code, UINTN size, CHAR16 *data
);
EFI_STATUS EFIAPI kbs_get_next_monotonic_count(UINT64 *count);
VOID EFIAPI kbs_copy_mem(VOID *dst, VOID *src, UINTN len);
VOID EFIAPI kbs_set_mem(VOID *buf, UINTN len, UINT8 value);
BOOLEAN kx_guid_eq(EFI_GUID *a, EFI_GUID *b);
EFI_STATUS EFIAPI kbs_locate_protocol(
    EFI_GUID *protocol,
    VOID *registration,
    VOID **iface
);
EFI_STATUS EFIAPI kconin_reset(
    SIMPLE_INPUT_INTERFACE *this_in,
    BOOLEAN extended
);
EFI_STATUS EFIAPI kconin_read_key(
    SIMPLE_INPUT_INTERFACE *this_in,
    EFI_INPUT_KEY *key
);
EFI_STATUS EFIAPI kptr_reset(
    EFI_SIMPLE_POINTER_PROTOCOL *this_ptr,
    BOOLEAN extended
);
EFI_STATUS EFIAPI kptr_get_state(
    EFI_SIMPLE_POINTER_PROTOCOL *this_ptr,
    EFI_SIMPLE_POINTER_STATE *state
);
void kx_install_shims(void);
EFI_STATUS EFIAPI krt_get_time(EFI_TIME *t, VOID *caps);


/* --- kernel/kcmds.c --- */
void kernel_cmd_kinfo(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
const char *kx_ep_hw_state(KX_HID *h);
const char *kx_sw_state(UINT8 st);
void kx_print_hc_status(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kx_print_hid_line(SIMPLE_TEXT_OUTPUT_INTERFACE *out, KX_HID *h);
void kernel_cmd_mousetest(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kernel_cmd_usb(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kernel_cmd_mem(EFI_SYSTEM_TABLE *st, SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kernel_cmd_boot(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kernel_cmd_vm(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
void kernel_cmd_crash(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *what);

/* --- shell/commands.c --- */
void run_command(
    EFI_SYSTEM_TABLE *st,
    CHAR16 *line
);


/* --- kernel/random.c --- */
void krandom_stir(UINT64 v);
void krandom_fill(void *buf, UINTN n);
BOOLEAN krandom_hw(void);

/* --- net/ (этап 8: сеть) - подробности в net/net.h --- */
void net_init(SIMPLE_TEXT_OUTPUT_INTERFACE *out);
BOOLEAN net_running(void);
INTN sock_create(UINT32 type, UINT32 pid);
INTN sock_bind(INTN s, UINT32 ip, UINT16 port);
INTN sock_connect(INTN s, UINT32 ip, UINT16 port);
INTN sock_listen(INTN s, UINT32 backlog);
INTN sock_accept(INTN s, UINT32 pid, UINT32 *ip, UINT16 *port);
INTN sock_sendto(INTN s, const void *buf, UINTN n, UINT32 ip, UINT16 port);
INTN sock_send(INTN s, const void *buf, UINTN n);
INTN sock_recvfrom(INTN s, void *buf, UINTN n, UINT32 *ip, UINT16 *port, UINT8 *ttl);
INTN sock_recv(INTN s, void *buf, UINTN n);
INTN sock_setopt(INTN s, UINT32 opt, UINT64 val);
INTN sock_pending(INTN s);
UINT32 sock_poll(INTN s);
BOOLEAN sock_poll_wait(BOOLEAN (*check)(void *ctx), void *ctx, UINT64 slice_ms);
INTN sock_close(INTN s);
void sock_close_pid(UINT32 pid);
const char *net_strerror(INTN e);
INTN dns_resolve(const char *name, UINT32 *ip, UINT64 timeout_ms);
INTN net_sys_info(UINTN idx, struct myos_netif *out);
INTN net_sys_ctl(const struct myos_netctl *c);
void kernel_cmd_net(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg);
void kernel_cmd_wifi(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *arg);
void wifi_boot_autoconnect(SIMPLE_TEXT_OUTPUT_INTERFACE *out);

/* --- kernel/settings.c (настройки в EFI/MyOS тома загрузки, этап 9) --- */
BOOLEAN settings_boot_volume(char *vol, UINTN cap);
BOOLEAN settings_path(const char *name, char *out, UINTN cap);
BOOLEAN settings_readonly(void);
INTN settings_write(const char *name, const void *data, UINTN n, char *where, UINTN cap);
INTN settings_remove(const char *name, char *where, UINTN cap);
BOOLEAN net_status_line(char *buf, UINTN cap);
void net_gui_indicator(char *buf, UINTN cap);

/* Сокеты в таблице fd программы: номер сокета с этим флагом
   (файлы VFS - маленькие неотрицательные числа) */
#define PROC_FD_SOCK   0x40000000

/* --- lib/serial.c --- */
extern BOOLEAN g_serial_ok;
BOOLEAN serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_puts16(const CHAR16 *s);

/* --- lib/kprintf.c --- */
UINTN kvsnprintf(char *buf, UINTN cap, const char *fmt, va_list ap);
UINTN ksnprintf(char *buf, UINTN cap, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
void kprintf(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void klog(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));



/* ================================================================
 * Маленькие static inline функции (порты, MMIO, TSC, биты)
 * ================================================================ */

/* 16-битные порты: регистры ACPI PM1 (выключение), порты uACPI */
static inline void io_out16(UINT16 port, UINT16 value)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline UINT16 io_in16(UINT16 port)
{
    UINT16 value;
    __asm__ __volatile__("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

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


/* ============================================================
 * Физические адреса и MMIO.
 *
 * Ядро живёт на СВОИХ таблицах страниц (kernel/vmm.c): нижняя
 * половина адресного пространства пуста, а вся физическая память
 * и регистры устройств видны по адресу MYOS_HHDM_BASE + X
 * ("прямое отображение", см. bootinfo.h). Поэтому физический
 * адрес (из карты памяти, из PCI BAR, из pmm_alloc_pages) НЕЛЬЗЯ
 * просто привести к указателю - его надо перевести через P2V().
 *
 * Загрузчик собирается с -DMYOS_LOADER: там ещё таблицы
 * прошивки, память отображена 1:1 и P2V - просто приведение.
 *
 * Регистры устройства (MMIO) читаем/пишем через mmio_read32/
 * mmio_write32 по ФИЗИЧЕСКОМУ адресу - перевод делают они сами.
 * Участок MMIO должен быть отображён как некэшируемый - см.
 * vmm_map_mmio (всё ниже 4 ГиБ, что не RAM, уже отображено так).
 * ============================================================ */

#ifdef MYOS_LOADER
#define MYOS_PHYS_OFFSET 0ull
#else
#define MYOS_PHYS_OFFSET MYOS_HHDM_BASE
#endif

/* физический адрес -> указатель, по которому его видит ядро */
#define P2V(p)  ((void *)(UINTN)((UINT64)(p) + MYOS_PHYS_OFFSET))

/* указатель из прямого отображения (P2V, kmalloc, pmm) ->
   физический адрес. Для адресов образа ядра и стеков не годится -
   для них vmm_virt_to_phys. */
#define V2P(v)  ((UINT64)(UINTN)(v) - MYOS_PHYS_OFFSET)

static inline UINT32 mmio_read32(UINT64 addr)
{
    return *(volatile UINT32 *)P2V(addr);
}

static inline void mmio_write32(UINT64 addr, UINT32 value)
{
    *(volatile UINT32 *)P2V(addr) = value;
}

static inline UINT64 mmio_read64(UINT64 addr)
{
    return *(volatile UINT64 *)P2V(addr);
}

static inline void mmio_write64(UINT64 addr, UINT64 value)
{
    *(volatile UINT64 *)P2V(addr) = value;
}

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


#pragma GCC visibility pop

#endif /* MYOS_H */
