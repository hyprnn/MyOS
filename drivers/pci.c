/*
 * drivers/pci.c - конфигурационное пространство PCI через порты 0xCF8/0xCFC.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

/*
 * Читает 32-битное слово конфигурационного пространства PCI
 * по (bus, dev, func, offset). offset должен быть выровнен
 * по 4 байтам - младшие 2 бита адреса игнорируются самим
 * протоколом Mechanism #1, поэтому маскируем их явно.
 */
UINT32 pci_config_read32(
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

void pci_config_write32(
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
