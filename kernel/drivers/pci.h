#ifndef AGENTICOS_PCI_H
#define AGENTICOS_PCI_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
struct pci_device { uint8_t bus, slot, function, class_code, subclass; uint16_t vendor, device; };
uint32_t pci_read(const struct pci_device *device, uint8_t offset);
void pci_write(const struct pci_device *device, uint8_t offset, uint32_t value);
size_t pci_scan(void);
bool pci_find(uint16_t vendor, uint16_t device, struct pci_device *result);
#endif
