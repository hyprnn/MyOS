/*
 * firmware/firmware.S - прошивки устройств, вклеенные в ядро MyOS.
 *
 * rtw88/rtw8821c_fw.bin - программа для процессора внутри Wi-Fi-чипа
 * Realtek RTL8821CE (версия 24.11, из linux-firmware). Распространять
 * её можно только в двоичном виде и без изменений - условия Realtek в
 * firmware/LICENCE.rtlwifi_firmware.txt. Драйвер (drivers/rtw8821c.c)
 * загружает её в чип при каждом включении Wi-Fi.
 */
.section .rodata
.balign 16
.globl g_fw_rtw8821c
.globl g_fw_rtw8821c_end
g_fw_rtw8821c:
.incbin "firmware/rtw88/rtw8821c_fw.bin"
g_fw_rtw8821c_end:
.section .note.GNU-stack,"",@progbits
