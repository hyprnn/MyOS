/*
 * loader/loader.c - загрузчик MyOS (это и есть BOOTX64.EFI).
 *
 * Раньше BOOTX64.EFI был всей ОС сразу: UEFI-программой, которая
 * по команде "ebs" сама себя вытаскивала из прошивки и дальше жила
 * на её таблицах страниц, в её адресах. Настоящие ОС устроены
 * иначе, и теперь MyOS тоже:
 *
 *   1. прошивка запускает ЭТОТ маленький загрузчик;
 *   2. загрузчик, пока прошивка жива, собирает "паспорт загрузки"
 *      (bootinfo.h): видеорежим, карту памяти, адрес ACPI, время,
 *      частоту TSC; отнимает у прошивки USB-контроллер;
 *   3. читает с того же диска файл ядра kernel.elf и раскладывает
 *      его в памяти так, как просит ELF-заголовок;
 *   4. строит временные таблицы страниц: ядро будет жить по
 *      адресу 0xFFFFFFFF80000000 ("верхняя половина"), а вся
 *      физическая память - видна по адресу 0xFFFF800000000000 + X;
 *   5. вызывает ExitBootServices - прошивки больше нет;
 *   6. переключается на свои таблицы страниц и прыгает в ядро,
 *      передав ему адрес паспорта.
 *
 * Ядро (kernel/kmain.c) с первой инструкции о прошивке не знает
 * ничего, кроме паспорта.
 *
 * Здесь собирается с -DMYOS_LOADER: в этом режиме P2V() (перевод
 * физического адреса в указатель) - просто приведение типа, потому
 * что прошивка отображает память 1:1.
 */
#include "myos.h"

/* ================================================================
 * Протоколы прошивки, которые нужны только загрузчику
 * ================================================================ */

/* EFI_LOADED_IMAGE_PROTOCOL: нам нужно одно поле - DeviceHandle,
   "диск, с которого нас загрузили" (там же лежит kernel.elf) */
typedef struct {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    VOID *FilePath;
    VOID *Reserved;
    UINT32 LoadOptionsSize;
    VOID *LoadOptions;
    VOID *ImageBase;
    UINT64 ImageSize;
    UINT32 ImageCodeType;
    UINT32 ImageDataType;
    VOID *Unload;
} LDR_LOADED_IMAGE;

#define LDR_LOADED_IMAGE_GUID \
    { 0x5B1B31A1, 0x9562, 0x11d2, \
      { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } }

#define LDR_SIMPLE_FS_GUID \
    { 0x964e5b22, 0x6459, 0x11d2, \
      { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } }

#define LDR_ACPI20_GUID \
    { 0x8868e871, 0xe4f1, 0x11d3, \
      { 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81 } }

#define LDR_ACPI10_GUID \
    { 0xeb9d2d30, 0x2d88, 0x11d3, \
      { 0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } }

/* EFI_FILE_PROTOCOL - файл или папка на диске (FAT) */
typedef struct LDR_FILE LDR_FILE;

struct LDR_FILE {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *Open)(LDR_FILE *self, LDR_FILE **out,
                              CHAR16 *name, UINT64 mode, UINT64 attr);
    EFI_STATUS (EFIAPI *Close)(LDR_FILE *self);
    VOID *Delete;
    EFI_STATUS (EFIAPI *Read)(LDR_FILE *self, UINTN *size, VOID *buf);
    VOID *Write;
    EFI_STATUS (EFIAPI *GetPosition)(LDR_FILE *self, UINT64 *pos);
    EFI_STATUS (EFIAPI *SetPosition)(LDR_FILE *self, UINT64 pos);
    VOID *GetInfo;
    VOID *SetInfo;
    VOID *Flush;
};

typedef struct {
    UINT64 Revision;
    EFI_STATUS (EFIAPI *OpenVolume)(VOID *self, LDR_FILE **root);
} LDR_SIMPLE_FS;

typedef EFI_STATUS (EFIAPI *LDR_HANDLE_PROTOCOL)(
    EFI_HANDLE h, EFI_GUID *guid, VOID **iface);

typedef EFI_STATUS (EFIAPI *LDR_FREE_PAGES)(UINT64 mem, UINTN pages);

/* Одна запись таблицы конфигурации (ACPI, SMBIOS, ...) */
typedef struct {
    EFI_GUID guid;
    VOID *table;
} LDR_CONFIG_ENTRY;

/* ================================================================
 * Состояние загрузчика
 * ================================================================ */

static EFI_SYSTEM_TABLE *ls;           /* таблица прошивки */
static SIMPLE_TEXT_OUTPUT_INTERFACE *lout;
static MYOS_BOOT_INFO *lbi;            /* паспорт (в памяти ядра) */

static GUI_ALLOCATE_PAGES l_alloc_pages;
static GUI_ALLOCATE_POOL l_alloc_pool;
static GUI_FREE_POOL l_free_pool;
static GUI_GET_MEMORY_MAP l_get_map;

/* Когда паспорт ещё не выделен, журнал копится здесь */
static char l_early_log[512];
static UINTN l_early_len = 0;

/* ================================================================
 * Печать. Имена - как у консоли шелла (print, print_hex,
 * print_uint): их зовут и общие драйверы, которые мы подключаем
 * к загрузчику (drivers/pci.c, drivers/xhci_common.c).
 * Всё напечатанное дублируется в журнал паспорта - в ядре его
 * покажет команда "boot".
 * ================================================================ */

static void l_log_char(char c)
{
    if (lbi != NULL) {
        if (lbi->log_len + 1 < sizeof(lbi->log)) {
            lbi->log[lbi->log_len++] = c;
            lbi->log[lbi->log_len] = '\0';
        }
    } else if (l_early_len + 1 < sizeof(l_early_log)) {
        l_early_log[l_early_len++] = c;
        l_early_log[l_early_len] = '\0';
    }
}

void print(SIMPLE_TEXT_OUTPUT_INTERFACE *out, const char *s)
{
    CHAR16 buf[130];
    UINTN n = 0;

    if (out == NULL)
        out = lout;

    for (; *s; s++) {

        l_log_char(*s);

        if (*s == '\n')
            buf[n++] = L'\r';

        buf[n++] = (CHAR16)(UINT8)*s;

        if (n >= 126) {
            buf[n] = 0;
            out->OutputString(out, buf);
            n = 0;
        }
    }

    if (n > 0) {
        buf[n] = 0;
        out->OutputString(out, buf);
    }
}

void print_uint(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINT64 v)
{
    char b[24];
    UINTN i = 23;

    b[i] = '\0';

    do {
        b[--i] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0 && i > 0);

    print(out, &b[i]);
}

void print_hex(SIMPLE_TEXT_OUTPUT_INTERFACE *out, UINT64 v, UINTN digits)
{
    char b[20];
    const char *hx = "0123456789ABCDEF";

    if (digits > 16)
        digits = 16;

    for (UINTN i = 0; i < digits; i++)
        b[i] = hx[(v >> ((digits - 1 - i) * 4)) & 0xFu];

    b[digits] = '\0';

    print(out, b);
}

/* Задержка для общих драйверов - через Stall прошивки */
void busy_wait_ms(UINTN ms)
{
    ls->BootServices->Stall(ms * 1000u);
}

static void l_color(UINTN attr)
{
    lout->SetAttribute(lout, attr);
}

/* Сообщить об ошибке и подождать клавишу: пользователь должен
   успеть прочитать, прежде чем прошивка перейдёт к следующему
   варианту загрузки */
static EFI_STATUS l_fail(const char *what)
{
    l_color(0x0C);
    print(lout, "\nMyOS loader: ");
    print(lout, what);
    print(lout, "\n");
    l_color(0x07);
    print(lout, "Press any key (or wait a minute) to return to the firmware.\n");

    EFI_INPUT_KEY k;

    /* ждём клавишу, но не дольше минуты - потом прошивка сама
       перейдёт к следующему варианту загрузки */
    for (UINTN i = 0; i < 6000; i++) {
        if (ls->ConIn->ReadKeyStroke(ls->ConIn, &k) == EFI_SUCCESS)
            break;
        ls->BootServices->Stall(10000);
    }

    return 1;   /* любой не-SUCCESS */
}

static void l_zero(VOID *p, UINTN n)
{
    volatile UINT8 *b = (volatile UINT8 *)p;

    for (UINTN i = 0; i < n; i++)
        b[i] = 0;
}

static void l_copy(VOID *d, const VOID *s, UINTN n)
{
    UINT8 *a = (UINT8 *)d;
    const UINT8 *b = (const UINT8 *)s;

    for (UINTN i = 0; i < n; i++)
        a[i] = b[i];
}

static BOOLEAN l_guid_eq(const EFI_GUID *a, const EFI_GUID *b)
{
    const UINT8 *x = (const UINT8 *)a;
    const UINT8 *y = (const UINT8 *)b;

    for (UINTN i = 0; i < sizeof(EFI_GUID); i++)
        if (x[i] != y[i])
            return FALSE;

    return TRUE;
}

/* Выделить страницы (EfiLoaderData), обнулить и записать в список
   занятой памяти паспорта с пометкой kind (KEEP/TEMP). 0 - не вышло. */
static UINT64 l_pages(UINTN count, UINT32 kind)
{
    UINT64 addr = 0;

    if (l_alloc_pages(0 /* AllocateAnyPages */, 2 /* EfiLoaderData */,
                      count, &addr) != EFI_SUCCESS)
        return 0;

    l_zero((VOID *)(UINTN)addr, count * 4096u);

    /* сам паспорт выделяется первым, когда lbi ещё нет - его
       запишем в список сразу после */
    if (lbi != NULL && lbi->nreserved < MYOS_MAX_RESERVED) {
        lbi->reserved[lbi->nreserved].phys = addr;
        lbi->reserved[lbi->nreserved].pages = count;
        lbi->reserved[lbi->nreserved].kind = kind;
        lbi->nreserved++;
    }

    return addr;
}

/* ================================================================
 * Чтение kernel.elf с диска, с которого нас загрузили
 * ================================================================ */

/* Где искать ядро - по очереди. FAT не различает регистр. */
static CHAR16 *l_kernel_paths[] = {
    L"\\EFI\\BOOT\\KERNEL.ELF",
    L"\\KERNEL.ELF",
    L"\\EFI\\MYOS\\KERNEL.ELF",
    NULL
};

static VOID *l_read_kernel(EFI_HANDLE image, UINT64 *size_out)
{
    EFI_GUID li_guid = LDR_LOADED_IMAGE_GUID;
    EFI_GUID fs_guid = LDR_SIMPLE_FS_GUID;
    LDR_HANDLE_PROTOCOL handle_protocol =
        (LDR_HANDLE_PROTOCOL)ls->BootServices->HandleProtocol;

    LDR_LOADED_IMAGE *li = NULL;

    if (handle_protocol(image, &li_guid, (VOID **)&li) != EFI_SUCCESS ||
        li == NULL) {
        print(lout, "  cannot get LoadedImage protocol\n");
        return NULL;
    }

    LDR_SIMPLE_FS *fs = NULL;

    if (handle_protocol(li->DeviceHandle, &fs_guid, (VOID **)&fs)
            != EFI_SUCCESS || fs == NULL) {
        print(lout, "  boot device has no file system protocol\n");
        return NULL;
    }

    LDR_FILE *root = NULL;

    if (fs->OpenVolume(fs, &root) != EFI_SUCCESS || root == NULL) {
        print(lout, "  cannot open the boot volume\n");
        return NULL;
    }

    LDR_FILE *f = NULL;
    CHAR16 *found = NULL;

    for (UINTN i = 0; l_kernel_paths[i] != NULL; i++) {
        if (root->Open(root, &f, l_kernel_paths[i], 1 /* READ */, 0)
                == EFI_SUCCESS && f != NULL) {
            found = l_kernel_paths[i];
            break;
        }
        f = NULL;
    }

    if (f == NULL) {
        print(lout, "  kernel.elf not found (looked in \\EFI\\BOOT\\, \\, \\EFI\\MYOS\\)\n");
        root->Close(root);
        return NULL;
    }

    /* размер файла: встать в конец (позиция ~0 = "в конец") */
    UINT64 size = 0;

    f->SetPosition(f, 0xFFFFFFFFFFFFFFFFull);
    f->GetPosition(f, &size);
    f->SetPosition(f, 0);

    if (size < 64 || size > 64u * 1024u * 1024u) {
        print(lout, "  kernel.elf has a strange size\n");
        f->Close(f);
        root->Close(root);
        return NULL;
    }

    VOID *buf = NULL;

    if (l_alloc_pool(2 /* EfiLoaderData */, (UINTN)size, &buf) != EFI_SUCCESS) {
        f->Close(f);
        root->Close(root);
        return NULL;
    }

    UINTN got = (UINTN)size;

    if (f->Read(f, &got, buf) != EFI_SUCCESS || got != (UINTN)size) {
        print(lout, "  read error\n");
        f->Close(f);
        root->Close(root);
        return NULL;
    }

    f->Close(f);
    root->Close(root);

    /* путь - в паспорт (для команды boot) */
    UINTN k = 0;

    for (; found[k] && k + 1 < 64; k++)
        lbi->kernel_path[k] = found[k];

    lbi->kernel_path[k] = 0;
    lbi->kernel_file_size = size;

    *size_out = size;

    return buf;
}

/* ================================================================
 * ELF: разложить сегменты ядра в памяти
 * ================================================================ */

typedef struct {
    UINT8  ident[16];
    UINT16 type, machine;
    UINT32 version;
    UINT64 entry, phoff, shoff;
    UINT32 flags;
    UINT16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} LDR_ELF_EHDR;

typedef struct {
    UINT32 type, flags;
    UINT64 offset, vaddr, paddr, filesz, memsz, align;
} LDR_ELF_PHDR;

static UINT64 l_kernel_entry = 0;

static BOOLEAN l_load_elf(UINT8 *file, UINT64 size)
{
    LDR_ELF_EHDR *eh = (LDR_ELF_EHDR *)file;

    if (eh->ident[0] != 0x7F || eh->ident[1] != 'E' ||
        eh->ident[2] != 'L' || eh->ident[3] != 'F' ||
        eh->ident[4] != 2 /* 64-bit */ || eh->machine != 0x3E /* x86-64 */) {
        print(lout, "  kernel.elf is not a 64-bit x86 ELF file\n");
        return FALSE;
    }

    if (eh->phoff + (UINT64)eh->phnum * eh->phentsize > size) {
        print(lout, "  kernel.elf: program headers are cut off\n");
        return FALSE;
    }

    /* Границы всех загружаемых (PT_LOAD) сегментов */
    UINT64 lo = ~0ull, hi = 0;

    for (UINTN i = 0; i < eh->phnum; i++) {

        LDR_ELF_PHDR *ph =
            (LDR_ELF_PHDR *)(file + eh->phoff + i * eh->phentsize);

        if (ph->type != 1 /* PT_LOAD */ || ph->memsz == 0)
            continue;

        if (ph->vaddr < lo) lo = ph->vaddr;
        if (ph->vaddr + ph->memsz > hi) hi = ph->vaddr + ph->memsz;

        if (ph->offset + ph->filesz > size) {
            print(lout, "  kernel.elf: a segment is cut off\n");
            return FALSE;
        }
    }

    if (lo != MYOS_KERNEL_VIRT || hi <= lo || hi - lo > 64u * 1024u * 1024u) {
        print(lout, "  kernel.elf is not linked at 0xFFFFFFFF80000000\n");
        return FALSE;
    }

    UINT64 span = (hi - lo + 4095u) & ~4095ull;
    UINT64 phys = l_pages((UINTN)(span / 4096u), MYOS_RES_KEEP);

    if (phys == 0) {
        print(lout, "  no memory for the kernel image\n");
        return FALSE;
    }

    /* копируем то, что есть в файле; хвост сегмента (.bss) уже
       нулевой - l_pages обнуляет память */
    for (UINTN i = 0; i < eh->phnum; i++) {

        LDR_ELF_PHDR *ph =
            (LDR_ELF_PHDR *)(file + eh->phoff + i * eh->phentsize);

        if (ph->type != 1 || ph->memsz == 0)
            continue;

        l_copy((VOID *)(UINTN)(phys + (ph->vaddr - lo)),
               file + ph->offset, (UINTN)ph->filesz);
    }

    lbi->kernel_phys = phys;
    lbi->kernel_virt = lo;
    lbi->kernel_size = span;
    l_kernel_entry = eh->entry;

    return TRUE;
}

/* ================================================================
 * Временные таблицы страниц
 *
 * Формат x86-64: 4 уровня (PML4 -> PDPT -> PD -> PT), в каждой
 * таблице 512 записей по 8 байт = ровно 4 КиБ. Запись PD может
 * сразу указывать на 2-мегабайтную страницу (бит PS) - так мы
 * отображаем всю физическую память малым числом таблиц.
 *
 * Что отображаем:
 *   * [0, top) один-к-одному (identity) - чтобы сам загрузчик,
 *     выполняющийся по физическим адресам, не упал в момент
 *     переключения CR3;
 *   * то же самое по адресу HHDM + X - этим пользуется ядро,
 *     пока не построит свои таблицы (обе половины указывают на
 *     одну и ту же таблицу PDPT - экономно);
 *   * образ ядра по адресу 0xFFFFFFFF80000000 4-КиБ страницами.
 *
 * Всё с правами "чтение+запись+исполнение": это временно, ядро
 * первым делом строит свои таблицы с нормальными правами.
 * ================================================================ */

#define LPT_P   0x001ull
#define LPT_W   0x002ull
#define LPT_PS  0x080ull

static BOOLEAN l_build_page_tables(UINT64 top)
{
    UINT64 gib = top >> 30;           /* сколько гигабайт */
    UINT64 kpages = lbi->kernel_size / 4096u;
    UINT64 kpts = (kpages + 511u) / 512u;

    if (kpts == 0 || kpts > 32)
        return FALSE;

    /* PML4 + PDPT(низ) + gib*PD + PDPT(ядро) + PD(ядро) + kpts*PT */
    UINTN total = (UINTN)(1u + 1u + gib + 1u + 1u + kpts);
    UINT64 base = l_pages(total, MYOS_RES_TEMP);

    if (base == 0)
        return FALSE;

    UINT64 *pml4 = (UINT64 *)(UINTN)base;
    UINT64 pdpt_lo_phys = base + 4096u;
    UINT64 *pdpt_lo = (UINT64 *)(UINTN)pdpt_lo_phys;
    UINT64 pd_phys = base + 2u * 4096u;
    UINT64 pdpt_k_phys = pd_phys + gib * 4096u;
    UINT64 pd_k_phys = pdpt_k_phys + 4096u;
    UINT64 pt_k_phys = pd_k_phys + 4096u;

    /* прямое отображение [0, top) 2-МиБ страницами */
    for (UINT64 g = 0; g < gib; g++) {

        UINT64 *pd = (UINT64 *)(UINTN)(pd_phys + g * 4096u);

        for (UINT64 e = 0; e < 512; e++)
            pd[e] = ((g << 30) + (e << 21)) | LPT_P | LPT_W | LPT_PS;

        pdpt_lo[g] = (pd_phys + g * 4096u) | LPT_P | LPT_W;
    }

    pml4[0] = pdpt_lo_phys | LPT_P | LPT_W;               /* 1:1 */
    pml4[(MYOS_HHDM_BASE >> 39) & 511u] =
        pdpt_lo_phys | LPT_P | LPT_W;                      /* HHDM */

    /* ядро: 0xFFFFFFFF80000000 = PML4[511], PDPT[510], PD[0..] */
    UINT64 *pdpt_k = (UINT64 *)(UINTN)pdpt_k_phys;
    UINT64 *pd_k = (UINT64 *)(UINTN)pd_k_phys;

    pml4[(MYOS_KERNEL_VIRT >> 39) & 511u] = pdpt_k_phys | LPT_P | LPT_W;
    pdpt_k[(MYOS_KERNEL_VIRT >> 30) & 511u] = pd_k_phys | LPT_P | LPT_W;

    for (UINT64 t = 0; t < kpts; t++)
        pd_k[t] = (pt_k_phys + t * 4096u) | LPT_P | LPT_W;

    for (UINT64 p = 0; p < kpages; p++) {
        UINT64 *pt = (UINT64 *)(UINTN)(pt_k_phys + (p / 512u) * 4096u);
        pt[p % 512u] = (lbi->kernel_phys + p * 4096u) | LPT_P | LPT_W;
    }

    lbi->loader_cr3 = base;

    return TRUE;
}

/* Верхняя граница физических адресов, которые нужно отобразить:
   конец самой высокой RAM (или видеопамяти),
   округлённый вверх до гигабайта; не меньше 4 ГиБ (там всегда
   живут устройства: APIC, видеопамять, PCI) и не больше 512 ГиБ
   (одна запись PML4). */
static UINT64 l_phys_top(void)
{
    UINTN size = 0, key = 0, dsize = 0;
    UINT32 dver = 0;
    UINT64 top = 4ull << 30;

    l_get_map(&size, NULL, &key, &dsize, &dver);

    size += 16u * dsize;

    VOID *buf = NULL;

    if (l_alloc_pool(2, size, &buf) == EFI_SUCCESS &&
        l_get_map(&size, buf, &key, &dsize, &dver) == EFI_SUCCESS) {

        for (UINTN off = 0; off + dsize <= size; off += dsize) {

            UINT8 *d = (UINT8 *)buf + off;
            UINT32 type = *(UINT32 *)d;
            UINT64 phys = *(UINT64 *)(d + 8);
            UINT64 pages = *(UINT64 *)(d + 24);
            UINT64 end = phys + pages * 4096u;

            /* только RAM: далёкие "Reserved"/MMIO-записи (у QEMU -
               под самым 1 ТиБ) не нужны и стоили бы мегабайты таблиц */
            BOOLEAN ram = type == 1 || type == 2 || type == 3 ||
                          type == 4 || type == 5 || type == 6 ||
                          type == 7 || type == 9 || type == 10 ||
                          type == 14 || type >= 0x80000000u;

            if (ram && end > top)
                top = end;
        }
    }

    if (buf != NULL)
        l_free_pool(buf);

    UINT64 fb_end = lbi->fb_phys + lbi->fb_size;

    if (fb_end > top)
        top = fb_end;

    top = (top + (1ull << 30) - 1u) & ~((1ull << 30) - 1u);

    if (top > (512ull << 30))
        top = 512ull << 30;

    return top;
}

/* ================================================================
 * Точка входа загрузчика
 * ================================================================ */

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st)
{
    ls = st;
    lout = st->ConOut;

    l_alloc_pages = (GUI_ALLOCATE_PAGES)st->BootServices->AllocatePages;
    l_alloc_pool = (GUI_ALLOCATE_POOL)st->BootServices->AllocatePool;
    l_free_pool = (GUI_FREE_POOL)st->BootServices->FreePool;
    l_get_map = (GUI_GET_MEMORY_MAP)st->BootServices->GetMemoryMap;

    lout->Reset(lout, FALSE);
    lout->ClearScreen(lout);

    /* сторожевой таймер прошивки (5 минут по умолчанию) - выключить,
       пока мы тут что-то читаем с медленной флешки */
    ((EFI_STATUS (EFIAPI *)(UINTN, UINT64, UINTN, CHAR16 *))
        st->BootServices->SetWatchdogTimer)(0, 0, 0, NULL);

    l_color(0x0B);
    print(lout, "MyOS loader\n");
    l_color(0x07);

    /* 5-уровневые таблицы страниц (LA57) - наши 4-уровневые не
       подойдут, а выключить LA57, не выходя из 64-битного режима,
       нельзя. На ноутбуках такого почти не бывает. */
    {
        UINT64 cr4;
        __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
        if (cr4 & (1ull << 12))
            return l_fail("firmware uses 5-level paging (LA57) - not supported yet");
    }

    /* --- паспорт: 2 страницы памяти ядра --- */
    UINTN bi_pages = (sizeof(MYOS_BOOT_INFO) + 4095u) / 4096u;
    UINT64 bi_phys = l_pages(bi_pages, MYOS_RES_KEEP);

    if (bi_phys == 0)
        return l_fail("out of memory (boot info)");

    lbi = (MYOS_BOOT_INFO *)(UINTN)bi_phys;
    lbi->magic = MYOS_BOOT_MAGIC;
    lbi->version = MYOS_BOOT_VERSION;

    /* паспорт - первая запись списка занятой памяти */
    lbi->reserved[0].phys = bi_phys;
    lbi->reserved[0].pages = bi_pages;
    lbi->reserved[0].kind = MYOS_RES_KEEP;
    lbi->nreserved = 1;

    /* то, что успели напечатать до паспорта */
    for (UINTN i = 0; i < l_early_len; i++)
        l_log_char(l_early_log[i]);

    /* --- 1. экран --- */
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    if (st->BootServices->LocateProtocol(&gop_guid, NULL, (VOID **)&gop)
            != EFI_SUCCESS || gop == NULL || gop->Mode == NULL ||
        gop->Mode->Info == NULL || gop->Mode->FrameBufferBase == 0 ||
        gop->Mode->Info->PixelFormat == PixelBltOnly)
        return l_fail("no usable graphics framebuffer (GOP)");

    lbi->fb_phys = gop->Mode->FrameBufferBase;
    lbi->fb_width = gop->Mode->Info->HorizontalResolution;
    lbi->fb_height = gop->Mode->Info->VerticalResolution;
    lbi->fb_stride = gop->Mode->Info->PixelsPerScanLine;
    lbi->fb_format = (UINT32)gop->Mode->Info->PixelFormat;
    lbi->fb_size = (UINT64)lbi->fb_stride * lbi->fb_height * 4u;

    if (gop->Mode->FrameBufferSize > lbi->fb_size)
        lbi->fb_size = gop->Mode->FrameBufferSize;

    print(lout, "  screen: ");
    print_uint(lout, lbi->fb_width);
    print(lout, "x");
    print_uint(lout, lbi->fb_height);
    print(lout, " at 0x");
    print_hex(lout, lbi->fb_phys, 12);
    print(lout, "\n");

    /* --- 2. время и кто нас загрузил --- */
    if (st->RuntimeServices->GetTime != NULL &&
        st->RuntimeServices->GetTime(&lbi->boot_time, NULL) == EFI_SUCCESS)
        lbi->have_boot_time = 1;

    if (st->FirmwareVendor != NULL) {
        UINTN k = 0;
        for (; st->FirmwareVendor[k] && k + 1 < 48; k++)
            lbi->fw_vendor[k] = st->FirmwareVendor[k];
        lbi->fw_vendor[k] = 0;
    }

    lbi->fw_revision = st->FirmwareRevision;
    lbi->uefi_revision = st->Hdr.Revision;

    /* --- 3. ACPI: адрес RSDP из таблицы конфигурации --- */
    {
        EFI_GUID g20 = LDR_ACPI20_GUID;
        EFI_GUID g10 = LDR_ACPI10_GUID;
        LDR_CONFIG_ENTRY *ct = (LDR_CONFIG_ENTRY *)st->ConfigurationTable;

        for (UINTN i = 0; ct != NULL && i < st->NumberOfTableEntries; i++) {
            if (l_guid_eq(&ct[i].guid, &g20)) {
                lbi->rsdp_phys = (UINT64)(UINTN)ct[i].table;
                lbi->acpi_version = 2;
                break;
            }
            if (l_guid_eq(&ct[i].guid, &g10) && lbi->rsdp_phys == 0) {
                lbi->rsdp_phys = (UINT64)(UINTN)ct[i].table;
                lbi->acpi_version = 1;
            }
        }

        print(lout, "  ACPI RSDP: ");
        if (lbi->rsdp_phys) {
            print(lout, "0x");
            print_hex(lout, lbi->rsdp_phys, 8);
            print(lout, lbi->acpi_version == 2 ? " (ACPI 2.0+)\n" : " (ACPI 1.0)\n");
        } else {
            print(lout, "not found\n");
        }
    }

    /* --- 4. частота TSC по Stall прошивки (контрольный замер
       для ядра: основной эталон там - таймер PIT) --- */
    {
        UINT64 t0 = rdtsc();
        st->BootServices->Stall(50000);
        UINT64 t1 = rdtsc();
        lbi->tsc_hz_stall = (t1 - t0) * 20u;
    }

    /* --- 5. ядро. Читаем ДО того, как отнять у прошивки USB: если
       файла нет, пользователь должен суметь нажать клавишу, а
       USB-клавиатуру прошивка обслуживает только пока её драйвер
       подключён. --- */
    print(lout, "  loading kernel.elf... ");

    UINT64 ksize = 0;
    VOID *kfile = l_read_kernel(image, &ksize);

    if (kfile == NULL)
        return l_fail("could not read kernel.elf - it must lie next to BOOTX64.EFI");

    if (!l_load_elf((UINT8 *)kfile, ksize))
        return l_fail("kernel.elf is damaged or built wrongly");

    print_uint(lout, ksize / 1024u);
    print(lout, " KiB file, image ");
    print_uint(lout, lbi->kernel_size / 1024u);
    print(lout, " KiB at phys 0x");
    print_hex(lout, lbi->kernel_phys, 8);
    print(lout, "\n");

    l_free_pool(kfile);

    /* --- 6. USB-контроллер: отобрать у прошивки, пока она жива.
       Драйвер прошивки отключаем (DisconnectController), а у BIOS
       (SMM) просим владение через USB Legacy Support. Сам
       контроллер ядро потом сбрасывает и настраивает заново. --- */
    {
        UINT8 xb = 0, xd = 0, xf = 0;
        UINT64 xm = 0;

        if (pci_find_xhci(&xb, &xd, &xf, &xm) && xm != 0) {

            XHCI_CAP_INFO cap;

            lbi->xhci_found = 1;
            print(lout, "  xHCI at ");
            print_uint(lout, xb); print(lout, ":");
            print_uint(lout, xd); print(lout, ".");
            print_uint(lout, xf);
            print(lout, " - releasing it from the firmware\n");

            pci_enable_device(xb, xd, xf);
            xhci_disconnect_firmware_driver(st, lout, xb, xd, xf);
            lbi->xhci_disconnected = 1;

            xhci_read_cap_regs(xm, &cap);
            lbi->xhci_handoff_ok =
                xhci_bios_handoff(st, xm, cap.ExtCapOff, lout) ? 1 : 0;
        }
    }

    /* --- 7. стартовый стек ядра: 64 КиБ --- */
    lbi->stack_size = 64u * 1024u;
    lbi->stack_phys = l_pages((UINTN)(lbi->stack_size / 4096u), MYOS_RES_KEEP);

    if (lbi->stack_phys == 0)
        return l_fail("out of memory (kernel stack)");

    /* --- 8. таблицы страниц --- */
    UINT64 top = l_phys_top();

    if (!l_build_page_tables(top))
        return l_fail("could not build page tables");

    print(lout, "  page tables: RAM mapped up to ");
    print_uint(lout, top >> 30);
    print(lout, " GiB, kernel at 0xFFFFFFFF80000000\n");

    /* --- 9. буфер под итоговую карту памяти (в памяти ядра). С
       запасом: само это выделение и вызов ExitBootServices могут
       добавить в карту несколько записей. --- */
    UINTN map_size = 0, map_key = 0, desc_size = 0;
    UINT32 desc_ver = 0;

    l_get_map(&map_size, NULL, &map_key, &desc_size, &desc_ver);

    UINTN map_cap = map_size + 64u * desc_size;
    UINT64 map_phys = l_pages((map_cap + 4095u) / 4096u, MYOS_RES_KEEP);

    if (map_phys == 0)
        return l_fail("out of memory (memory map)");

    map_cap = ((map_cap + 4095u) / 4096u) * 4096u;

    print(lout, "  leaving the firmware, jumping to the kernel...\n");

    /* --- 10. ExitBootServices. Между GetMemoryMap и Exit ничего
       нельзя выделять и печатать (печать тоже может выделить
       память) - иначе ключ карты устареет и Exit откажет. --- */
    GUI_EXIT_BOOT_SERVICES exit_bs =
        (GUI_EXIT_BOOT_SERVICES)st->BootServices->ExitBootServices;

    BOOLEAN exited = FALSE;

    for (UINTN attempt = 0; attempt < 8; attempt++) {

        map_size = map_cap;

        if (l_get_map(&map_size, (VOID *)(UINTN)map_phys, &map_key,
                      &desc_size, &desc_ver) != EFI_SUCCESS)
            break;

        if (exit_bs(image, map_key) == EFI_SUCCESS) {
            exited = TRUE;
            break;
        }
    }

    if (!exited)
        return l_fail("ExitBootServices failed");

    /* ======== прошивки больше нет ======== */

    /* Знак "загрузчик вышел из прошивки": серая полоса 6 пикселей
       по верху экрана (серый одинаков в RGB и BGR). Печатать уже
       нечем, а по фото так видно, дошли ли мы досюда. Дальше ядро
       нарисует внизу строку "MyOS kernel: step N/7". */
    {
        volatile UINT32 *fb = (volatile UINT32 *)(UINTN)lbi->fb_phys;
        UINT32 w = lbi->fb_width / 2u;

        for (UINT32 y = 0; y < 6u && y < lbi->fb_height; y++)
            for (UINT32 x = 0; x < w; x++)
                fb[(UINTN)y * lbi->fb_stride + x] = 0x00909090u;
    }

    lbi->mmap_phys = map_phys;
    lbi->mmap_size = map_size;
    lbi->mmap_desc_size = desc_size;
    lbi->mmap_desc_version = desc_ver;

    UINT64 stack_top =
        MYOS_HHDM_BASE + lbi->stack_phys + lbi->stack_size;
    UINT64 bi_virt = MYOS_HHDM_BASE + bi_phys;
    UINT64 cr3 = lbi->loader_cr3;
    UINT64 entry = l_kernel_entry;

    lbi->tsc_at_jump = rdtsc();

    /*
     * Прыжок: запретить прерывания, включить наши таблицы страниц
     * (этот код продолжает выполняться, потому что низ памяти
     * отображён 1:1), взять стек ядра, положить адрес паспорта в
     * RDI (первый аргумент kmain) и перейти по адресу входа.
     * "push $0" - фиктивный адрес возврата: kmain - обычная
     * C-функция и ждёт стек в том виде, как после инструкции call.
     */
    __asm__ __volatile__(
        "cli\n\t"
        "mov %0, %%cr3\n\t"
        "mov %1, %%rsp\n\t"
        "xor %%rbp, %%rbp\n\t"
        "pushq $0\n\t"
        "jmp *%2\n\t"
        :
        : "r"(cr3), "r"(stack_top), "r"(entry), "D"(bi_virt)
        : "memory"
    );

    for (;;)
        __asm__ __volatile__("hlt");

    return EFI_SUCCESS;
}
