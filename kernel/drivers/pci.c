#include "pci.h"
#include "arch/x86_64/io.h"
#include "debug/log.h"
static struct pci_device devices[64];
static size_t count;
static uint32_t address(const struct pci_device *device, uint8_t offset) {
    return UINT32_C(0x80000000) | ((uint32_t)device->bus << 16) | ((uint32_t)device->slot << 11) |
        ((uint32_t)device->function << 8) | (offset & 0xfcu);
}
uint32_t pci_read(const struct pci_device *device, uint8_t offset) { outl(0xcf8, address(device, offset)); return inl(0xcfc); }
void pci_write(const struct pci_device *device, uint8_t offset, uint32_t value) { outl(0xcf8, address(device, offset)); outl(0xcfc, value); }
size_t pci_scan(void) {
    count = 0;
    for (unsigned int bus = 0; bus < 256; ++bus) {
        for (unsigned int slot = 0; slot < 32; ++slot) {
            for (unsigned int function = 0; function < 8; ++function) {
                struct pci_device device = {.bus = (uint8_t)bus, .slot = (uint8_t)slot, .function = (uint8_t)function};
                uint32_t id = pci_read(&device, 0);
                if ((id & 0xffffu) == 0xffffu) { if (function == 0) { break; } continue; }
                device.vendor = (uint16_t)id; device.device = (uint16_t)(id >> 16);
                uint32_t class_code = pci_read(&device, 8); device.class_code = (uint8_t)(class_code >> 24); device.subclass = (uint8_t)(class_code >> 16);
                if (count < 64) { devices[count++] = device; }
                klog(LOG_INFO, "pci", "bus=%u slot=%u function=%u vendor=0x%x device=0x%x class=0x%x subclass=0x%x",
                     bus, slot, function, (unsigned int)device.vendor, (unsigned int)device.device, (unsigned int)device.class_code, (unsigned int)device.subclass);
                if (function == 0 && (pci_read(&device, 0x0c) & UINT32_C(0x00800000)) == 0) { break; }
            }
        }
    }
    return count;
}
bool pci_find(uint16_t vendor, uint16_t device, struct pci_device *result) {
    for (size_t i = 0; i < count; ++i) { if (devices[i].vendor == vendor && devices[i].device == device) { *result = devices[i]; return true; } }
    return false;
}
