#include "ata.h"
#include "arch/x86_64/io.h"
#include <stddef.h>
static uint32_t sectors;
static void delay(void) { for (unsigned int i = 0; i < 4; ++i) { (void)inb(0x3f6); } }
static bool wait_ready(bool data) {
    for (unsigned int i = 0; i < 1000000; ++i) {
        uint8_t status = inb(0x1f7);
        if (status == 0 || status == 0xff) { return false; }
        if ((status & 0x80u) != 0) { continue; }
        if ((status & 0x21u) != 0) { return false; }
        if (!data || (status & 8u) != 0) { return true; }
    }
    return false;
}
bool ata_init(void) {
    sectors = 0; outb(0x3f6, 2); /* Polling: disable device IRQs. */
    outb(0x1f6, 0xa0); delay();
    outb(0x1f2, 0); outb(0x1f3, 0); outb(0x1f4, 0); outb(0x1f5, 0); outb(0x1f7, 0xec);
    if (!wait_ready(true) || inb(0x1f4) != 0 || inb(0x1f5) != 0) { return false; }
    uint16_t identity[256]; for (unsigned int i = 0; i < 256; ++i) { identity[i] = inw(0x1f0); }
    if ((identity[49] & 0x200u) == 0) { return false; }
    sectors = (uint32_t)identity[60] | ((uint32_t)identity[61] << 16);
    return sectors != 0;
}
uint32_t ata_sectors(void) { return sectors; }
static bool select_sector(uint32_t lba, uint8_t command) {
    if (lba >= sectors || lba >= UINT32_C(0x10000000) || !wait_ready(false)) { return false; }
    outb(0x1f6, (uint8_t)(0xe0u | ((lba >> 24) & 15u))); delay();
    outb(0x1f2, 1); outb(0x1f3, (uint8_t)lba); outb(0x1f4, (uint8_t)(lba >> 8)); outb(0x1f5, (uint8_t)(lba >> 16));
    outb(0x1f7, command); return wait_ready(true);
}
bool ata_read(uint32_t lba, void *sector) {
    if (sector == NULL || !select_sector(lba, 0x20)) { return false; }
    unsigned char *bytes = sector;
    for (unsigned int i = 0; i < 256; ++i) { uint16_t value = inw(0x1f0); bytes[i * 2u] = (uint8_t)value; bytes[i * 2u + 1u] = (uint8_t)(value >> 8); }
    delay(); return wait_ready(false);
}
bool ata_write(uint32_t lba, const void *sector) {
    if (sector == NULL || !select_sector(lba, 0x30)) { return false; }
    const unsigned char *bytes = sector;
    for (unsigned int i = 0; i < 256; ++i) { outw(0x1f0, (uint16_t)((uint16_t)bytes[i * 2u] | ((uint16_t)bytes[i * 2u + 1u] << 8))); }
    delay(); if (!wait_ready(false)) { return false; } outb(0x1f7, 0xe7); delay(); return wait_ready(false);
}
