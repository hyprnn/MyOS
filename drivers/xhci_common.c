/*
 * drivers/xhci_common.c - общее для xHCI: регистры, сброс, отключение драйвера прошивки, BIOS handoff.
 * Часть MyOS; общие объявления - в myos.h.
 */
#include "myos.h"

/*
 * Свежая память от AllocatePages не гарантированно нулевая -
 * а контроллер xHCI обязан видеть нули в неинициализированных
 * полях DCBAA/колец (Cycle bit и т.п. иначе воспримутся как
 * мусор). Своего memset в freestanding-окружении нет, пишем
 * руками.
 */
void raw_zero_mem(volatile UINT8 *p, UINTN n)
{
    for (UINTN i = 0; i < n; i++)
        p[i] = 0;
}

UINT32 portsc_base_for_write(UINT32 cur)
{
    return cur & ~(PORTSC_RW1CS_MASK | PORTSC_BIT_PED);
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
BOOLEAN xhci_reset_controller(UINT64 op_base)
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

void xhci_disconnect_firmware_driver(
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

void xhci_read_cap_regs(UINT64 mmio_base, XHCI_CAP_INFO *info)
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
BOOLEAN xhci_bios_handoff(
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
