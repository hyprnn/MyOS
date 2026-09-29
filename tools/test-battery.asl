/*
 * tools/test-battery.asl - поддельная батарея и блок питания для
 * проверки MyOS в QEMU (у QEMU своих нет). autotest.py подкладывает
 * её виртуальной машине как лишнюю таблицу SSDT:
 *     qemu ... -acpitable file=tools/test-battery.aml
 * Собрать заново (нужен iasl из пакета acpica-tools / acpica):
 *     iasl -p tools/test-battery tools/test-battery.asl
 *
 * Батарея: 32000 из 40000 mWh (80%), разряжается 10000 mW - то есть
 * 3 часа 12 минут до разряда; зарядка не подключена; крышка открыта.
 * Экран с яркостью через ACPI (_BCL/_BCM/_BQC): уровни 10..100 шагом 10,
 * сейчас 100 - у QEMU нет видеокарты Intel, так проверяется запасной путь.
 */
DefinitionBlock ("", "SSDT", 2, "MYOS", "BATTEST", 1)
{
    Scope (\_SB)
    {
        Device (BAT0)
        {
            Name (_HID, EisaId ("PNP0C0A"))
            Name (_UID, 1)
            Method (_STA, 0) { Return (0x1F) }

            /* паспорт: mWh; 42000 по паспорту, 40000 последняя полная;
               11.1 В; модель, серийный, тип, производитель */
            Method (_BIF, 0)
            {
                Return (Package (13) {
                    0, 42000, 40000, 1, 11100, 4000, 2000, 100, 100,
                    "TEST42", "0001", "LION", "MyOS Labs"
                })
            }

            /* состояние: разряжается, 10000 mW, осталось 32000 mWh, 11.5 В */
            Method (_BST, 0)
            {
                Return (Package (4) { 1, 10000, 32000, 11500 })
            }
        }

        Device (ADP1)
        {
            Name (_HID, "ACPI0003")
            Method (_PSR, 0) { Return (0) }
        }

        Device (LCD0)
        {
            Name (_ADR, 0x0400)
            Name (BRTL, 100)
            Method (_BCL, 0)
            {
                Return (Package (12) { 100, 60, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 })
            }
            Method (_BCM, 1) { BRTL = Arg0 }
            Method (_BQC, 0) { Return (BRTL) }
        }

        Device (LID0)
        {
            Name (_HID, EisaId ("PNP0C0D"))
            Method (_LID, 0) { Return (1) }
        }
    }
}
