#include "elf.h"
#include "memory/vmm.h"
#include "runtime/memory.h"
struct elf_header {
    unsigned char ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct elf_program_header { uint32_t type, flags; uint64_t offset, address, physical, file_bytes, memory_bytes, align; };
_Static_assert(sizeof(struct elf_header) == 64, "ELF64 header");
_Static_assert(sizeof(struct elf_program_header) == 56, "ELF64 program header");
bool user_elf_plan(const void *image, size_t length, struct elf_plan *plan) {
    if (image == NULL || plan == NULL || length < sizeof(struct elf_header)) { return false; }
    struct elf_header header;
    memcpy(&header, image, sizeof(header));
    static const unsigned char magic[7] = {0x7f, 'E', 'L', 'F', 2, 1, 1};
    if (memcmp(header.ident, magic, sizeof(magic)) != 0 || header.type != 2 || header.machine != 62 ||
        header.version != 1 || header.ehsize != sizeof(header) || header.phentsize != sizeof(struct elf_program_header) ||
        header.phnum == 0 || header.phnum > ELF_SEGMENT_CAPACITY || header.phoff > length ||
        header.phnum * sizeof(struct elf_program_header) > length - header.phoff) { return false; }
    struct elf_plan result = {.entry = header.entry};
    bool executable_entry = false;
    for (unsigned int i = 0; i < header.phnum; ++i) {
        struct elf_program_header ph;
        memcpy(&ph, (const unsigned char *)image + header.phoff + i * sizeof(ph), sizeof(ph));
        if (ph.type == 2 || ph.type == 3 || ph.type == 7 ||
            (ph.type == UINT32_C(0x6474e551) && (ph.flags & 1u) != 0)) { return false; } /* No dynamic/TLS/exec-stack ABI. */
        if (ph.type != 1) { continue; }
        if (ph.memory_bytes == 0) { if (ph.file_bytes != 0) { return false; } continue; }
        if (ph.file_bytes > ph.memory_bytes || ph.offset > length || ph.file_bytes > length - ph.offset ||
            ph.address < USER_IMAGE_BASE || ph.address >= USER_IMAGE_END || ph.memory_bytes > USER_IMAGE_END - ph.address ||
            ph.address % 4096 != 0 || ph.offset % 4096 != 0 || ph.align != 4096 ||
            (ph.flags & ~7u) != 0 || (ph.flags & 4u) == 0 || (ph.flags & 3u) == 3u) { return false; }
        uint64_t end = (ph.address + ph.memory_bytes + 4095) & ~UINT64_C(4095);
        for (unsigned int j = 0; j < result.count; ++j) {
            const struct elf_segment *other = &result.segments[j];
            uint64_t other_end = (other->address + other->memory_bytes + 4095) & ~UINT64_C(4095);
            if (ph.address < other_end && other->address < end) { return false; }
        }
        unsigned int pages = (unsigned int)((end - ph.address) / 4096);
        if (pages > USER_PAGE_CAPACITY - 6u - result.pages) { return false; }
        result.pages += pages;
        uint64_t flags = VM_USER | ((ph.flags & 2u) != 0 ? VM_WRITE : 0) | ((ph.flags & 1u) == 0 ? VM_NX : 0);
        result.segments[result.count++] = (struct elf_segment){ph.address, ph.offset, ph.file_bytes, ph.memory_bytes, flags};
        if ((ph.flags & 1u) != 0 && header.entry >= ph.address && header.entry - ph.address < ph.file_bytes) { executable_entry = true; }
    }
    if (!executable_entry || result.count == 0) { return false; }
    *plan = result;
    return true;
}
