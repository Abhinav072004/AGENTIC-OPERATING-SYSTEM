#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory/pmm.h"
#include "user/user.h"
#include "user/copy.h"
#include "syscall/abi.h"
#define RAM_BASE UINT64_C(0x100000)
#define FRAME_COUNT 256u
struct fixture { struct pmm pmm; unsigned char bitmap[256]; unsigned char *ram; int budget; };
static bool allocate(void *context, uint64_t *physical) {
    struct fixture *f = context;
    if (f->budget == 0) { return false; }
    if (f->budget > 0) { --f->budget; }
    return pmm_alloc_frame(&f->pmm, physical);
}
static void release(void *context, uint64_t physical) { assert(pmm_free_frame(&((struct fixture *)context)->pmm, physical)); }
static void *pointer(void *context, uint64_t physical) {
    struct fixture *f = context;
    if (physical < RAM_BASE || physical >= RAM_BASE + FRAME_COUNT * PAGE_SIZE || !pmm_is_allocated(&f->pmm, physical)) { return NULL; }
    return f->ram + physical - RAM_BASE;
}
static void invalidate(void *context, uint64_t address) { (void)context; assert(address % 4096 == 0); }
static void put64(unsigned char *p, uint64_t value) { memcpy(p, &value, 8); }
static void put16(unsigned char *p, uint16_t value) { memcpy(p, &value, 2); }
static void elf_tests(const unsigned char *image, size_t length) {
    struct elf_plan plan;
    assert(user_elf_plan(image, length, &plan) && plan.entry >= USER_IMAGE_BASE && plan.entry < USER_IMAGE_END);
    assert(plan.pages > 0 && plan.count >= 2);
    assert(!user_elf_plan(NULL, length, &plan) && !user_elf_plan(image, 63, &plan));
    unsigned char *bad = malloc(length);
    assert(bad);
    uint64_t phoff;
    memcpy(&phoff, image + 32, 8);
    for (unsigned int test = 0; test < 14; ++test) {
        memcpy(bad, image, length);
        if (test == 0) { bad[0] = 0; }
        if (test == 1) { put16(bad + 16, 3); }
        if (test == 2) { put16(bad + 56, 9); }
        if (test == 3) { put64(bad + 32, UINT64_MAX); }
        if (test == 4) { put64(bad + phoff + 8, UINT64_MAX); }
        if (test == 5) { put64(bad + phoff + 32, UINT64_MAX); }
        if (test == 6) { bad[phoff + 4] = 7; }
        if (test == 7) { put64(bad + phoff + 16, UINT64_C(0xffffffff80000000)); }
        if (test == 8) { put64(bad + phoff + 48, 1); }
        if (test == 9) { put64(bad + 24, USER_IMAGE_END); }
        if (test == 10) { memcpy(bad + phoff + 56, bad + phoff, 56); }
        if (test == 11) { bad[phoff] = 2; }
        if (test == 12) { bad[phoff] = 7; }
        if (test == 13) {
            uint16_t count;
            memcpy(&count, bad + 56, 2);
            unsigned char *stack_header = bad + phoff + (count - 1u) * 56u;
            stack_header[4] |= 1u;
        }
        struct elf_plan original;
        memset(&original, 0xa5, sizeof(original));
        memcpy(&plan, &original, sizeof(plan));
        assert(!user_elf_plan(bad, length, &plan) && memcmp(&plan, &original, sizeof(plan)) == 0);
    }
    free(bad);
    puts("PASS: actual native ELF plan, bounds/overflow, W^X, overlap, dynamic/TLS/exec-stack and fourteen malformed-image rejections");
}
static void frame_tests(void) {
    struct interrupt_frame frame = {.cs = X86_USER_CODE, .ss = X86_USER_DATA, .rip = USER_IMAGE_BASE,
        .rsp = USER_STACK_TOP - 16, .rflags = UINT64_MAX};
    assert(user_frame_prepare(&frame));
    assert((frame.rflags & UINT64_C(0x3000)) == 0 && (frame.rflags & UINT64_C(0x202)) == UINT64_C(0x202));
    assert((frame.rflags & ((UINT64_C(1) << 14) | (UINT64_C(1) << 17) | (UINT64_C(1) << 18))) == 0);
    frame.rsp = USER_LIMIT;
    assert(!user_frame_prepare(&frame));
    frame.rsp = USER_STACK_TOP - 16; frame.rip = 0;
    assert(!user_frame_prepare(&frame));
    frame.rip = USER_IMAGE_BASE; frame.cs = X86_KERNEL_CODE;
    assert(!user_frame_prepare(&frame) && !user_frame_prepare(NULL));
    puts("PASS: safe user IRET selectors/addresses and RFLAGS privilege/NT/VM/AC sanitization");
}
static void copy_tests(struct fixture *f, const struct vmm *kernel, const struct vmm_ops *ops) {
    struct vmm child = {0};
    uint64_t before = f->pmm.free_frames, first, second, physical, flags;
    assert(vmm_create_user(&child, kernel) && child.shared_upper && child.root != kernel->root);
    assert(!vmm_create_user((struct vmm *)kernel, kernel));
    assert(pmm_alloc_frame(&f->pmm, &first) && pmm_alloc_frame(&f->pmm, &second));
    assert(vmm_map(&child, USER_IMAGE_BASE, first, VM_USER | VM_WRITE | VM_NX));
    assert(vmm_map(&child, USER_IMAGE_BASE + 4096, second, VM_USER | VM_WRITE | VM_NX));
    assert(!vmm_map(&child, UINT64_C(0xffffffff80001000), first, VM_USER | VM_NX));
    assert(!vmm_unmap(&child, UINT64_C(0xffffffff80000000), NULL));
    assert(!vmm_protect(&child, UINT64_C(0xffffffff80000000), VM_NX));
    assert(vmm_query(&child, UINT64_C(0xffffffff80000000), &physical, &flags) && (flags & VM_USER) == 0);
    assert(!user_buffer_valid(&child, UINT64_C(0xffffffff80000000), 1, false));
    unsigned char input[64], output[64];
    for (unsigned int i = 0; i < 64; ++i) { input[i] = (unsigned char)(i + 1); }
    assert(copy_to_user(&child, USER_IMAGE_BASE + 4080, input, 64));
    assert(copy_from_user(&child, output, USER_IMAGE_BASE + 4080, 64) && memcmp(input, output, 64) == 0);
    unsigned char *last = pointer(f, second);
    last[4094] = 0x71; last[4095] = 0x72;
    assert(!copy_to_user(&child, USER_IMAGE_BASE + 8190, input, 4) && last[4094] == 0x71 && last[4095] == 0x72);
    memset(output, 0x5a, sizeof(output));
    assert(!copy_from_user(&child, output, USER_IMAGE_BASE + 8190, 4) && output[0] == 0x5a && output[3] == 0x5a);
    assert(!user_buffer_valid(&child, 0, 1, false) && !user_buffer_valid(&child, UINT64_MAX - 1, 8, false));
    assert(!user_buffer_valid(&child, USER_LIMIT - 2, 4, false));
    assert(user_buffer_valid(&child, UINT64_MAX, 0, true));
    assert(vmm_protect(&child, USER_IMAGE_BASE, VM_USER | VM_NX));
    assert(!copy_to_user(&child, USER_IMAGE_BASE, input, 1) && copy_from_user(&child, output, USER_IMAGE_BASE, 1));
    uint64_t *root = ops->pointer(ops->context, child.root);
    uint64_t link = root[0];
    root[0] &= ~VM_USER;
    assert(vmm_query(&child, USER_IMAGE_BASE, NULL, &flags) && (flags & VM_USER) == 0);
    assert(!user_buffer_valid(&child, USER_IMAGE_BASE, 1, false));
    root[0] = link & ~VM_WRITE;
    assert(!user_buffer_valid(&child, USER_IMAGE_BASE + 4096, 1, true));
    root[0] = link;
    assert(vmm_destroy(&child) && pmm_is_allocated(&f->pmm, first) && pmm_is_allocated(&f->pmm, second));
    assert(pmm_free_frame(&f->pmm, first) && pmm_free_frame(&f->pmm, second) && f->pmm.free_frames == before);
    puts("PASS: borrowed supervisor mappings, effective parent U/W, cross-page copies, zero length and atomic bad-buffer rejection");
}
static void space_tests(struct fixture *f, struct vmm *kernel, const unsigned char *image, size_t length) {
    uint64_t before = f->pmm.free_frames;
    unsigned int rejected = 0, accepted = 0;
    for (int budget = 0; budget <= 32; ++budget) {
        struct user_space space = {0};
        f->budget = budget;
        if (!user_space_load(&space, kernel, image, length, 42, true)) { ++rejected; assert(!space.vm.ready); }
        else {
            ++accepted;
            uint64_t flags;
            assert(vmm_query(&space.vm, USER_IMAGE_BASE, NULL, &flags) && (flags & (VM_USER | VM_WRITE | VM_NX)) == VM_USER);
            assert(vmm_query(&space.vm, USER_STACK_BOTTOM, NULL, &flags) && flags == (VM_USER | VM_WRITE | VM_NX));
            assert(!vmm_query(&space.vm, USER_STACK_BOTTOM - 4096, NULL, NULL) && !vmm_query(&space.vm, USER_STACK_TOP, NULL, NULL));
            const struct user_info *info = pointer(f, space.info_physical);
            assert(info && info->abi == AGENTICOS_USER_ABI && info->pid == 42 && info->ticks == 0);
            struct elf_plan plan;
            assert(user_elf_plan(image, length, &plan));
            for (unsigned int i = 0; i < plan.count; ++i) {
                const struct elf_segment *segment = &plan.segments[i];
                for (uint64_t j = segment->file_bytes; j < segment->memory_bytes; ++j) {
                    unsigned char byte = 0xff;
                    assert(copy_from_user(&space.vm, &byte, segment->address + j, 1) && byte == 0);
                }
            }
            assert(user_space_destroy(&space));
        }
        assert(f->pmm.free_frames == before && vmm_query(kernel, UINT64_C(0xffffffff80000000), NULL, NULL));
    }
    assert(rejected > 10 && accepted > 0);
    f->budget = -1;
    struct user_space a = {0}, b = {0};
    assert(user_space_load(&a, kernel, image, length, 1, true) && user_space_load(&b, kernel, image, length, 2, false));
    uint64_t ap, bp;
    assert(a.vm.root != b.vm.root && a.writable_address == b.writable_address && a.writable_address != 0);
    assert(vmm_query(&a.vm, a.writable_address, &ap, NULL) && vmm_query(&b.vm, b.writable_address, &bp, NULL) && ap != bp);
    assert(vmm_query(&a.vm, USER_PRIVATE_ADDRESS, NULL, NULL) && !vmm_query(&b.vm, USER_PRIVATE_ADDRESS, NULL, NULL));
    assert(user_space_destroy(&a) && user_space_destroy(&b) && f->pmm.free_frames == before);
    puts("PASS: native image/BSS, stack guards/info page, independent roots/data and every loader allocation budget 0..32");
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long file_size = ftell(file); assert(file_size > 0 && fseek(file, 0, SEEK_SET) == 0);
    size_t length = (size_t)file_size;
    unsigned char *image = malloc(length); assert(image && fread(image, 1, length, file) == length); fclose(file);
    elf_tests(image, length); frame_tests();
    struct fixture f = {.budget = -1};
    f.ram = aligned_alloc(4096, FRAME_COUNT * 4096); assert(f.ram); memset(f.ram, 0xa5, FRAME_COUNT * 4096);
    const struct memory_region region = {RAM_BASE, FRAME_COUNT * PAGE_SIZE, true};
    assert(pmm_init(&f.pmm, &region, 1, f.bitmap, sizeof(f.bitmap)));
    struct vmm_ops ops = {&f, allocate, release, pointer, invalidate};
    struct vmm kernel = {0}; uint64_t kernel_data;
    assert(vmm_create(&kernel, &ops) && pmm_alloc_frame(&f.pmm, &kernel_data));
    assert(vmm_map(&kernel, UINT64_C(0xffffffff80000000), kernel_data, VM_NX));
    copy_tests(&f, &kernel, &ops); space_tests(&f, &kernel, image, length);
    assert(vmm_destroy(&kernel) && pmm_free_frame(&f.pmm, kernel_data) && f.pmm.free_frames == FRAME_COUNT);
    free(f.ram); free(image); return 0;
}
