/*
 * drivers/pci.c - конфигурационное пространство PCI.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Два способа до него добраться:
 *   * старый (Mechanism #1, с 1990-х): записать адрес в порт 0xCF8
 *     и прочитать данные из порта 0xCFC. Работает везде, но видит
 *     только первые 256 байт конфигурации устройства, и два порта
 *     на всю систему - узкое место;
 *   * современный (PCIe ECAM): конфигурация каждого устройства - это
 *     просто 4 КиБ памяти в большом окне, адрес окна сообщает
 *     таблица ACPI MCFG. Адрес = база + (шина << 20) + (устройство
 *     << 15) + (функция << 12) + смещение.
 * После разбора ACPI ядро включает ECAM (pci_use_ecam), если окно
 * отвечает то же, что и порты. Загрузчик всегда работает портами.
 */
#include "myos.h"

UINT64  g_pci_ecam_base = 0;      /* физический адрес окна (для шины 0) */
UINT8   g_pci_ecam_bus_start = 0;
UINT8   g_pci_ecam_bus_end = 0;
BOOLEAN g_pci_ecam_active = FALSE;

static BOOLEAN pci_ecam_covers(UINT8 bus)
{
    return g_pci_ecam_active &&
           bus >= g_pci_ecam_bus_start && bus <= g_pci_ecam_bus_end;
}

static UINT64 pci_ecam_addr(UINT8 bus, UINT8 dev, UINT8 func, UINT8 offset)
{
    return g_pci_ecam_base +
           ((UINT64)bus << 20) + ((UINT64)(dev & 31u) << 15) +
           ((UINT64)(func & 7u) << 12) + (UINT64)(offset & 0xFCu);
}

static UINT32 pci_port_read32(UINT8 bus, UINT8 dev, UINT8 func, UINT8 offset)
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

/*
 * Читает 32-битное слово конфигурационного пространства PCI
 * по (bus, dev, func, offset). offset должен быть выровнен
 * по 4 байтам - младшие 2 бита отбрасываем явно.
 */
UINT32 pci_config_read32(
    UINT8 bus, UINT8 dev, UINT8 func, UINT8 offset
)
{
    if (pci_ecam_covers(bus))
        return mmio_read32(pci_ecam_addr(bus, dev, func, offset));

    return pci_port_read32(bus, dev, func, offset);
}

#ifndef MYOS_LOADER
/*
 * Включить ECAM по данным MCFG. Проверка на честность: у всех
 * устройств на шине 0 идентификатор (Vendor/Device ID) через окно
 * должен совпасть с тем, что отвечают старые порты. Не совпал -
 * остаёмся на портах (TRUE/FALSE - включили или нет).
 */
BOOLEAN pci_use_ecam(UINT64 base, UINT8 bus_start, UINT8 bus_end)
{
    if (base == 0 || bus_end < bus_start)
        return FALSE;

    UINT64 first = base + ((UINT64)bus_start << 20);
    UINT64 size = ((UINT64)(bus_end - bus_start) + 1u) << 20;

    if (!vmm_ensure_mapped(first, size, VMM_UC))
        return FALSE;

    g_pci_ecam_base = base;
    g_pci_ecam_bus_start = bus_start;
    g_pci_ecam_bus_end = bus_end;

    if (bus_start != 0)
        return FALSE;       /* сверять нечего - не рискуем */

    UINTN seen = 0;

    for (UINT8 dev = 0; dev < 32; dev++) {

        UINT32 by_port = pci_port_read32(0, dev, 0, 0);
        UINT32 by_ecam = mmio_read32(pci_ecam_addr(0, dev, 0, 0));

        if (by_port != by_ecam)
            return FALSE;

        if ((by_port & 0xFFFFu) != 0xFFFFu)
            seen++;
    }

    if (seen == 0)
        return FALSE;

    g_pci_ecam_active = TRUE;

    return TRUE;
}
#endif

void pci_config_write32(
    UINT8 bus, UINT8 dev, UINT8 func, UINT8 offset, UINT32 value
)
{
    if (pci_ecam_covers(bus)) {
        mmio_write32(pci_ecam_addr(bus, dev, func, offset), value);
        return;
    }

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
void pci_enable_device(UINT8 bus, UINT8 dev, UINT8 func)
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
UINT64 pci_read_bar_address(
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
 * Ищет первый попавшийся xHCI-контроллер (класс 0x0C,
 * подкласс 0x03, prog-if 0x30) полным перебором bus/dev/func -
 * без рекурсивного обхода мостов, зато просто и надёжно:
 * непопулярные комбинации быстро отсеиваются по VendorID
 * == 0xFFFF. Возвращает TRUE и заполняет out-параметры, если
 * контроллер найден.
 */
BOOLEAN pci_find_xhci(
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

#ifndef MYOS_LOADER
/*
 * nth-е по счёту устройство PCI с данным классом/подклассом
 * (progif < 0 - любой prog-if). Полный перебор, как в pci_find_xhci.
 */
BOOLEAN pci_find_class(UINT8 base, UINT8 sub, INT16 progif, UINTN nth,
                       UINT8 *out_bus, UINT8 *out_dev, UINT8 *out_func)
{
    for (UINTN bus = 0; bus < 256; bus++) {

        for (UINTN dev = 0; dev < 32; dev++) {

            UINT32 id0 = pci_config_read32((UINT8)bus, (UINT8)dev, 0, 0x00);

            if ((id0 & 0xFFFF) == 0xFFFF)
                continue;

            UINT32 hdr = pci_config_read32((UINT8)bus, (UINT8)dev, 0, 0x0C);
            UINTN max_func = ((hdr >> 16) & 0x80) ? 8 : 1;

            for (UINTN func = 0; func < max_func; func++) {

                UINT32 id = (func == 0) ? id0 :
                    pci_config_read32((UINT8)bus, (UINT8)dev, (UINT8)func, 0x00);

                if ((id & 0xFFFF) == 0xFFFF)
                    continue;

                UINT32 cl = pci_config_read32((UINT8)bus, (UINT8)dev, (UINT8)func, 0x08);

                if (PCI_CLASS_DWORD_BASE_CLASS(cl) != base ||
                    PCI_CLASS_DWORD_SUB_CLASS(cl) != sub)
                    continue;

                if (progif >= 0 && PCI_CLASS_DWORD_PROG_IF(cl) != (UINT8)progif)
                    continue;

                if (nth > 0) {
                    nth--;
                    continue;
                }

                *out_bus = (UINT8)bus;
                *out_dev = (UINT8)dev;
                *out_func = (UINT8)func;
                return TRUE;
            }
        }
    }

    return FALSE;
}
#endif
