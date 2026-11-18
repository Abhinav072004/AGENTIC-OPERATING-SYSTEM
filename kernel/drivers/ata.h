#ifndef AGENTICOS_ATA_H
#define AGENTICOS_ATA_H
#include <stdbool.h>
#include <stdint.h>
bool ata_init(void);
uint32_t ata_sectors(void);
bool ata_read(uint32_t lba, void *sector);
bool ata_write(uint32_t lba, const void *sector);
#endif
