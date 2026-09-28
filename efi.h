#ifndef EFI_H
#define EFI_H

typedef unsigned char      UINT8;
typedef signed char        INT8;
typedef unsigned short     UINT16;
typedef unsigned int       UINT32;
typedef unsigned long long UINT64;
typedef int                INT32;
typedef long long          INT64;

typedef UINT64             UINTN;
typedef INT64              INTN;
typedef UINT16             CHAR16;
typedef UINT8              BOOLEAN;
typedef void               VOID;

typedef UINTN              EFI_STATUS;
typedef VOID*              EFI_HANDLE;
typedef VOID*              EFI_EVENT;

#define TRUE  1
#define FALSE 0
#define NULL  ((void*)0)

#define EFIAPI __attribute__((ms_abi))
#define EFI_SUCCESS 0

/* ============================================================
 * GUID
 * ============================================================ */

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

/* ============================================================
 * Time
 * ============================================================ */

typedef struct {
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
    UINT8  Pad1;
    UINT32 Nanosecond;
    INT64  TimeZone;
    UINT8  Daylight;
    UINT8  Pad2;
} EFI_TIME;

/* ============================================================
 * Table header
 * ============================================================ */

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/* ============================================================
 * Simple Text Output Protocol
 * ============================================================ */

typedef struct SIMPLE_TEXT_OUTPUT_INTERFACE
    SIMPLE_TEXT_OUTPUT_INTERFACE;

typedef struct {
    INTN MaxMode;
    INTN Mode;
    INTN Attribute;
    INTN CursorColumn;
    INTN CursorRow;
    BOOLEAN CursorVisible;
} SIMPLE_TEXT_OUTPUT_MODE;

typedef EFI_STATUS (EFIAPI *EFI_TEXT_RESET)(
    SIMPLE_TEXT_OUTPUT_INTERFACE *This,
    BOOLEAN ExtendedVerification
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    SIMPLE_TEXT_OUTPUT_INTERFACE *This,
    CHAR16 *String
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_CLEAR_SCREEN)(
    SIMPLE_TEXT_OUTPUT_INTERFACE *This
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_ATTRIBUTE)(
    SIMPLE_TEXT_OUTPUT_INTERFACE *This,
    UINTN Attribute
);

struct SIMPLE_TEXT_OUTPUT_INTERFACE {
    EFI_TEXT_RESET           Reset;
    EFI_TEXT_STRING          OutputString;
    VOID                    *TestString;
    VOID                    *QueryMode;
    VOID                    *SetMode;
    EFI_TEXT_SET_ATTRIBUTE   SetAttribute;
    EFI_TEXT_CLEAR_SCREEN    ClearScreen;
    VOID                    *SetCursorPosition;
    VOID                    *EnableCursor;
    SIMPLE_TEXT_OUTPUT_MODE *Mode;
};

/* Alias, чтобы оба имени работали */
typedef SIMPLE_TEXT_OUTPUT_INTERFACE EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

/* ============================================================
 * Simple Text Input Protocol
 * ============================================================ */

typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef struct SIMPLE_INPUT_INTERFACE SIMPLE_INPUT_INTERFACE;

typedef EFI_STATUS (EFIAPI *EFI_INPUT_RESET)(
    SIMPLE_INPUT_INTERFACE *This,
    BOOLEAN ExtendedVerification
);

typedef EFI_STATUS (EFIAPI *EFI_INPUT_READ_KEY)(
    SIMPLE_INPUT_INTERFACE *This,
    EFI_INPUT_KEY *Key
);

struct SIMPLE_INPUT_INTERFACE {
    EFI_INPUT_RESET     Reset;
    EFI_INPUT_READ_KEY  ReadKeyStroke;
    EFI_EVENT           WaitForKey;
};

/* ============================================================
 * Graphics Output Protocol
 * ============================================================ */

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    UINT32 PixelInformation[4];
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    UINT64 FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL
    EFI_GRAPHICS_OUTPUT_PROTOCOL;

struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    VOID *QueryMode;
    VOID *SetMode;
    VOID *Blt;

    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    { 0x9042a9de, 0x23dc, 0x4a38, \
      { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } }

/* ============================================================
 * Simple Pointer Protocol (мышь)
 * ============================================================ */

typedef struct {
    INT32   RelativeMovementX;
    INT32   RelativeMovementY;
    INT32   RelativeMovementZ;
    BOOLEAN LeftButton;
    BOOLEAN RightButton;
} EFI_SIMPLE_POINTER_STATE;

typedef struct {
    UINT64  ResolutionX;
    UINT64  ResolutionY;
    UINT64  ResolutionZ;
    BOOLEAN LeftButton;
    BOOLEAN RightButton;
} EFI_SIMPLE_POINTER_MODE;

typedef struct EFI_SIMPLE_POINTER_PROTOCOL
    EFI_SIMPLE_POINTER_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_SIMPLE_POINTER_RESET)(
    EFI_SIMPLE_POINTER_PROTOCOL *This,
    BOOLEAN ExtendedVerification
);

typedef EFI_STATUS (EFIAPI *EFI_SIMPLE_POINTER_GET_STATE)(
    EFI_SIMPLE_POINTER_PROTOCOL *This,
    EFI_SIMPLE_POINTER_STATE *State
);

struct EFI_SIMPLE_POINTER_PROTOCOL {
    EFI_SIMPLE_POINTER_RESET     Reset;
    EFI_SIMPLE_POINTER_GET_STATE GetState;
    EFI_EVENT                    WaitForInput;
    EFI_SIMPLE_POINTER_MODE     *Mode;
};

#define EFI_SIMPLE_POINTER_PROTOCOL_GUID \
    { 0x31878c87, 0x0b75, 0x11d5, \
      { 0x9a, 0x4f, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } }

/* ============================================================
 * PCI I/O Protocol - нужен ТОЛЬКО для того, чтобы найти
 * EFI_HANDLE прошивки, соответствующий уже найденному нами
 * (через порты 0xCF8/0xCFC) PCI-устройству xHCI, и вызвать
 * на нём DisconnectController - см. комментарий у
 * xhci_disconnect_firmware_driver() в main.c про то, зачем
 * это вообще нужно (гонка с фоновым USB-драйвером прошивки).
 *
 * Члены до GetLocation объявлены голыми VOID* (кроме тех,
 * что на самом деле являются встроенными двухпольными
 * структурами Read/Write - у них тот же суммарный размер
 * в указателях, но раскладка по отдельным VOID* была бы
 * не в 1:1 соответствии с полями, поэтому здесь честно
 * объявлены как пары VOID*) - порядок и размер каждого поля
 * сверены со спекой (PciIo.h), потому что от этого зависит
 * смещение GetLocation внутри структуры; сами эти функции
 * (PollMem, Mem.Read и т.п.) мы никогда не вызываем.
 * ============================================================ */

typedef struct EFI_PCI_IO_PROTOCOL EFI_PCI_IO_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_PCI_IO_PROTOCOL_GET_LOCATION)(
    EFI_PCI_IO_PROTOCOL *This,
    UINTN *SegmentNumber,
    UINTN *BusNumber,
    UINTN *DeviceNumber,
    UINTN *FunctionNumber
);

struct EFI_PCI_IO_PROTOCOL {
    VOID *PollMem;              /* EFI_PCI_IO_PROTOCOL_POLL_IO_MEM */
    VOID *PollIo;                /* EFI_PCI_IO_PROTOCOL_POLL_IO_MEM */
    VOID *MemRead;               /* EFI_PCI_IO_PROTOCOL_ACCESS.Read */
    VOID *MemWrite;               /* EFI_PCI_IO_PROTOCOL_ACCESS.Write */
    VOID *IoRead;                 /* EFI_PCI_IO_PROTOCOL_ACCESS.Read */
    VOID *IoWrite;                /* EFI_PCI_IO_PROTOCOL_ACCESS.Write */
    VOID *PciRead;                /* EFI_PCI_IO_PROTOCOL_CONFIG_ACCESS.Read */
    VOID *PciWrite;               /* EFI_PCI_IO_PROTOCOL_CONFIG_ACCESS.Write */
    VOID *CopyMem;               /* EFI_PCI_IO_PROTOCOL_COPY_MEM */
    VOID *Map;                    /* EFI_PCI_IO_PROTOCOL_MAP */
    VOID *Unmap;                  /* EFI_PCI_IO_PROTOCOL_UNMAP */
    VOID *AllocateBuffer;         /* EFI_PCI_IO_PROTOCOL_ALLOCATE_BUFFER */
    VOID *FreeBuffer;             /* EFI_PCI_IO_PROTOCOL_FREE_BUFFER */
    VOID *Flush;                  /* EFI_PCI_IO_PROTOCOL_FLUSH */
    EFI_PCI_IO_PROTOCOL_GET_LOCATION GetLocation;
    /* Дальше в спеке идут ещё поля (Attributes,
       GetBarAttributes, SetBarAttributes, RomSize,
       RomImage) - не нужны, GetLocation - последнее,
       что мы вызываем через этот протокол. */
};

#define EFI_PCI_IO_PROTOCOL_GUID \
    { 0x4cf5b200, 0x68b8, 0x4ca5, \
      { 0x9e, 0xec, 0xb2, 0x3e, 0x3f, 0x50, 0x02, 0x9a } }

/* ============================================================
 * Boot Services
 *
 * До LocateProtocol идут 16 указателей.
 * ============================================================ */

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_PROTOCOL)(
    EFI_GUID *Protocol,
    VOID *Registration,
    VOID **Interface
);

typedef struct {
    EFI_TABLE_HEADER Hdr;

    /* Task Priority Services */
    VOID *RaiseTPL;
    VOID *RestoreTPL;

    /* Memory Services */
    VOID *AllocatePages;
    VOID *FreePages;
    VOID *GetMemoryMap;
    VOID *AllocatePool;
    VOID *FreePool;

    /* Event & Timer Services */
    VOID *CreateEvent;
    VOID *SetTimer;
    VOID *WaitForEvent;
    VOID *SignalEvent;
    VOID *CloseEvent;
    VOID *CheckEvent;

    /* Protocol Handler Services */
    VOID *InstallProtocolInterface;
    VOID *ReinstallProtocolInterface;
    VOID *UninstallProtocolInterface;
    VOID *HandleProtocol;
    VOID *Reserved;
    VOID *RegisterProtocolNotify;
    VOID *LocateHandle;
    VOID *LocateDevicePath;
    VOID *InstallConfigurationTable;

    /* Image Services */
    VOID *LoadImage;
    VOID *StartImage;
    VOID *Exit;
    VOID *UnloadImage;
    VOID *ExitBootServices;

    /* Miscellaneous Services (часть 1) */
    VOID *GetNextMonotonicCount;

    UINT64 (EFIAPI *Stall)(UINTN Microseconds);

    VOID *SetWatchdogTimer;

    /* DriverSupport Services */
    VOID *ConnectController;
    VOID *DisconnectController;

    /* Open and Close Protocol Services */
    VOID *OpenProtocol;
    VOID *CloseProtocol;
    VOID *OpenProtocolInformation;

    /* Library Services */
    VOID *ProtocolsPerHandle;
    VOID *LocateHandleBuffer;

    EFI_LOCATE_PROTOCOL LocateProtocol;

    VOID *InstallMultipleProtocolInterfaces;
    VOID *UninstallMultipleProtocolInterfaces;

    /* 32-bit CRC Services */
    VOID *CalculateCrc32;

    /* Miscellaneous Services (часть 2) */
    VOID *CopyMem;
    VOID *SetMem;
    VOID *CreateEventEx;

} EFI_BOOT_SERVICES;

/* ============================================================
 * Runtime Services
 * ============================================================ */

typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown,
    EfiResetPlatformSpecific
} EFI_RESET_TYPE;

typedef EFI_STATUS (EFIAPI *EFI_GET_TIME)(
    EFI_TIME *Time,
    VOID *Capabilities
);

typedef EFI_STATUS (EFIAPI *EFI_GET_VARIABLE)(
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 *Attributes,
    UINTN *DataSize,
    VOID *Data
);

typedef struct {
    EFI_TABLE_HEADER Hdr;

    EFI_GET_TIME GetTime;

    VOID *SetTime;
    VOID *GetWakeupTime;
    VOID *SetWakeupTime;
    VOID *SetVirtualAddressMap;
    VOID *ConvertPointer;

    EFI_GET_VARIABLE GetVariable;

    VOID *GetNextVariableName;
    VOID *SetVariable;
    VOID *GetNextHighMonotonicCount;

    VOID (EFIAPI *ResetSystem)(
        EFI_RESET_TYPE ResetType,
        EFI_STATUS ResetStatus,
        UINTN DataSize,
        VOID *ResetData
    );

    VOID *UpdateCapsule;
    VOID *QueryCapsuleCapabilities;
    VOID *QueryVariableInfo;

} EFI_RUNTIME_SERVICES;

/* ============================================================
 * System Table
 * ============================================================ */

typedef struct {
    EFI_TABLE_HEADER Hdr;

    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;

    EFI_HANDLE ConsoleInHandle;
    SIMPLE_INPUT_INTERFACE *ConIn;

    EFI_HANDLE ConsoleOutHandle;
    SIMPLE_TEXT_OUTPUT_INTERFACE *ConOut;

    EFI_HANDLE StandardErrorHandle;
    SIMPLE_TEXT_OUTPUT_INTERFACE *StdErr;

    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;

    UINTN NumberOfTableEntries;
    VOID *ConfigurationTable;

} EFI_SYSTEM_TABLE;

/* ============================================================
 * Characters
 * ============================================================ */

#define CHAR_BACKSPACE       0x0008
#define CHAR_CARRIAGE_RETURN 0x000D

#endif
