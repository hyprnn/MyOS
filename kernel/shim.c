/*
 * kernel/shim.c - системная таблица ядра для шелла и GUI.
 * Часть MyOS; общие объявления - в myos.h.
 *
 * Шелл и GUI писались, когда MyOS была UEFI-программой, и
 * общаются с миром через таблицу в формате UEFI: ConOut->
 * OutputString, BootServices->Stall, LocateProtocol(GOP) и т.п.
 * Прошивки в ядре нет ВООБЩЕ, поэтому здесь - наша собственная
 * таблица того же формата, где за каждым полем стоит код ядра:
 * консоль - kcon.c, клавиатура и мышь - драйверы USB/PS2, память -
 * kmalloc/pmm, время - LAPIC/TSC и микросхема CMOS, перезагрузка
 * и выключение - power.c. Это внутренний интерфейс ядра; на этапе
 * 6 (программы в ring 3) его место займут системные вызовы.
 */
#include "myos.h"



/* ================================================================
 * 7. "Прокладка": наши ConIn / BootServices / GOP / SimplePointer
 * ================================================================ */

EFI_SYSTEM_TABLE      g_kst;
EFI_BOOT_SERVICES     g_kbs;
SIMPLE_INPUT_INTERFACE g_kconin;

EFI_GRAPHICS_OUTPUT_PROTOCOL         g_kgop;
EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE    g_kgop_mode;
EFI_GRAPHICS_OUTPUT_MODE_INFORMATION g_kgop_info;

EFI_SIMPLE_POINTER_PROTOCOL g_kptr;
EFI_SIMPLE_POINTER_MODE     g_kptr_mode;

EFI_RUNTIME_SERVICES g_krt;


/* Всё, чего у нас нет, честно отвечает "не поддерживается".
   Вызывается с любым числом аргументов - в соглашении ms_abi
   лишние аргументы убирает вызывающий, это безопасно. */
EFI_STATUS EFIAPI kbs_unsupported(void)
{
    return K_EFI_UNSUPPORTED;
}

UINT64 EFIAPI kbs_stall(UINTN us)
{
    /* кто-то ждёт - самое время показать накопленный вывод */
    kcon_flush();

    kx_sleep_us(us);

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI kbs_allocate_pool(
    UINTN pool_type, UINTN size, VOID **buffer
)
{
    (void)pool_type;

    if (buffer == NULL)
        return K_EFI_INVALID_PARAMETER;

    *buffer = kmalloc(size);

    return (*buffer != NULL) ? EFI_SUCCESS : K_EFI_OUT_OF_RESOURCES;
}

EFI_STATUS EFIAPI kbs_free_pool(VOID *buffer)
{
    return kfree(buffer) ? EFI_SUCCESS : K_EFI_INVALID_PARAMETER;
}

/* Type: 0 = AllocateAnyPages, 1 = AllocateMaxAddress,
   2 = AllocateAddress (последнего у нас нет) */
EFI_STATUS EFIAPI kbs_allocate_pages(
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

EFI_STATUS EFIAPI kbs_free_pages(UINT64 memory, UINTN pages)
{
    pmm_free_pages(memory, pages);

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI kbs_set_watchdog(
    UINTN timeout, UINT64 code, UINTN size, CHAR16 *data
)
{
    (void)timeout;
    (void)code;
    (void)size;
    (void)data;

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI kbs_get_next_monotonic_count(UINT64 *count)
{
    if (count == NULL)
        return K_EFI_INVALID_PARAMETER;

    *count = g_kticks;

    return EFI_SUCCESS;
}

VOID EFIAPI kbs_copy_mem(VOID *dst, VOID *src, UINTN len)
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

VOID EFIAPI kbs_set_mem(VOID *buf, UINTN len, UINT8 value)
{
    UINT8 *d = (UINT8 *)buf;

    for (UINTN i = 0; i < len; i++)
        d[i] = value;
}


BOOLEAN kx_guid_eq(EFI_GUID *a, EFI_GUID *b)
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
EFI_STATUS EFIAPI kbs_locate_protocol(
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

EFI_STATUS EFIAPI kconin_reset(
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

EFI_STATUS EFIAPI kconin_read_key(
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

EFI_STATUS EFIAPI kptr_reset(
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

EFI_STATUS EFIAPI kptr_get_state(
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


/* --- Runtime Services: часы и питание --- */

EFI_STATUS EFIAPI krt_get_time(EFI_TIME *t, VOID *caps)
{
    (void)caps;

    if (t == NULL)
        return K_EFI_INVALID_PARAMETER;

    return rtc_read(t) ? EFI_SUCCESS : K_EFI_UNSUPPORTED;
}

VOID EFIAPI krt_reset_system(
    EFI_RESET_TYPE type, EFI_STATUS status, UINTN size, VOID *data
)
{
    (void)status;
    (void)size;
    (void)data;

    if (type == EfiResetShutdown)
        kx_shutdown();   /* если выключиться не вышло - перезагрузка */

    kx_reboot();
}

void kx_install_shims(void)
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
    /* виртуальный адрес (прямое отображение, WC) - GUI просто
       приводит его к указателю */
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
    /* Runtime Services: всё "не поддерживается", кроме часов и
       перезагрузки/выключения */
    VOID **rslots =
        (VOID **)((UINT8 *)&g_krt + sizeof(EFI_TABLE_HEADER));
    UINTN nrslots =
        (sizeof(EFI_RUNTIME_SERVICES) - sizeof(EFI_TABLE_HEADER)) /
        sizeof(VOID *);

    for (UINTN i = 0; i < nrslots; i++)
        rslots[i] = (VOID *)kbs_unsupported;

    g_krt.Hdr.Signature = 0x56524553544e5552ull;  /* "RUNTSERV" */
    g_krt.Hdr.HeaderSize = (UINT32)sizeof(EFI_RUNTIME_SERVICES);
    g_krt.GetTime = krt_get_time;
    g_krt.ResetSystem = krt_reset_system;

    g_kst.Hdr.Signature = 0x5453595320494249ull;  /* "IBI SYST" */
    g_kst.Hdr.Revision = g_boot.uefi_revision;
    g_kst.Hdr.HeaderSize = (UINT32)sizeof(EFI_SYSTEM_TABLE);
    g_kst.Hdr.CRC32 = 0;
    g_kst.Hdr.Reserved = 0;
    g_kst.FirmwareVendor = g_boot.fw_vendor;
    g_kst.FirmwareRevision = g_boot.fw_revision;
    g_kst.ConsoleInHandle = NULL;
    g_kst.ConIn = &g_kconin;
    g_kst.ConsoleOutHandle = NULL;
    g_kst.ConOut = &g_kcon_out;
    g_kst.StandardErrorHandle = NULL;
    g_kst.StdErr = &g_kcon_out;
    g_kst.RuntimeServices = &g_krt;
    g_kst.BootServices = &g_kbs;
    g_kst.NumberOfTableEntries = 0;
    g_kst.ConfigurationTable = NULL;
}
