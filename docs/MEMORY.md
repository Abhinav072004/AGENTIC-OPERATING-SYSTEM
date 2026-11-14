# Week 3: understanding memory management

Phase 3 adds physical-frame allocation, kernel-owned paging and a small kernel
heap. The normal kernel runs data-integrity checks, emits PHASE3 PASS and halts.
These are foundations for later processes and isolated agents, not an AI runtime.

## Physical versus virtual memory

A physical address identifies RAM or a device. A virtual address is what an
instruction uses. The CPU translates it through page tables and checks the
mapping's permissions. AgenticOS uses four levels and 4 KiB pages:

```text
virtual address -> PML4 -> PDPT -> PD -> PT -> physical frame + byte offset
                     ^
                     CR3 contains the physical root address
```

Each table contains 512 entries. Four 9-bit indexes select an entry; the last
12 address bits select a byte in a page. Virtual addresses must be canonical
48-bit addresses. This implementation rejects huge-page walks and LA57 mode.

## Boot map and physical ownership

Limine supplies a memory map and the kernel physical/virtual bases. Only whole
frames marked USABLE enter the physical allocator. Firmware, kernel/module,
ACPI, framebuffer and bootloader regions remain reserved. Partial usable pages
are conservatively omitted. Overflowing/overlapping maps and maps exceeding
128 entries are rejected before creating allocator state.

Two bitmaps separate eligibility from live allocations. An eligible bit marks
a frame the allocator may hand out; a used bit marks one already handed out.
This prevents treating reserved pages as merely free. Bitmap storage is placed
inside a complete usable extent and then reserved, including page padding.
The allocator uses a next-fit scan, reports exhaustion without changing output
ownership, and accepts physical frame zero through a separate boolean result.

Free rejects misalignment, reserved/out-of-range frames and double frees. It
cannot authenticate caller identity: kernel code must free only its own frames.
Bootloader-reclaimable and ACPI memory is deliberately not reclaimed yet. The
bootloader GDT and metadata must remain valid until later initialization removes
those dependencies. Reinitializing a live allocator is a caller error.

## Kernel-owned page tables and layout

AgenticOS allocates fresh tables and switches CR3 rather than editing the
bootloader's tables. The HHDM (higher-half direct map) lets kernel code access
physical RAM, including page-table frames, at offset + physical address.

| Address/range | Role |
| --- | --- |
| `0xffff800000000000` through `0xffffbfffffffffff` | HHDM policy window, up to 64 TiB of physical addresses |
| `0xffffd00000000000` | Unmapped lower heap guard |
| `0xffffd00000001000` | 256 KiB initial heap |
| Immediately after the heap | Unmapped upper guard |
| `0xffffd10000000000` | Temporary self-check/probe mapping |
| `0xffffffff80000000` | Linked kernel image |

Only described RAM/framebuffer regions populate the direct map; the entire
64 TiB window is not allocated or mapped. The fixed HHDM offset and four-level
mode are checked. The kernel gets RX instructions, RO/NX constants and RW/NX
requests/data/stack. Direct aliases of instructions/constants are RO/NX too.
Framebuffer mappings use a conservative uncacheable policy. No mapping accepts
write plus execute; every level is supervisor-only.

The architecture helper enables CR0.WP so supervisor writes respect read-only
pages and EFER.NXE so NX is enforced. It flushes inherited global TLB entries
when changing CR3. Map/protect/unmap invalidate the affected VA when active.
A guest check maps the same VA to a different frame and reads its distinct data
to detect stale translations. A new table is always zeroed, even if its physical
frame previously contained application/test data.
Protect changes permissions only and rejects cache-policy changes. Callers
must keep physical aliases' cache types consistent; PAT/cache-mode transitions
require additional coordination beyond a TLB invalidation.

A failed map rolls back only newly allocated path nodes. Unmap frees empty
tables but never frees the caller's data frame. Destruction refuses active
spaces and releases only table frames in an owned, unshared tree. Backend
pointers must remain valid while frames are owned. The query API reports leaf
permissions; manager-created parents are always writable supervisor entries
with execution controlled at the leaf. There are no shared tables, user spaces,
SMP shootdowns or concurrent edits in this milestone.

## Heap and fragmentation

The heap maps 64 frames into one contiguous virtual arena. Those frames need
not be physically adjacent. A first-fit allocator stores 32-byte headers in the
arena and returns uninitialized payloads aligned to 16 bytes. Blocks split only
when enough space remains for another header and at least 16 payload bytes.
Free coalesces adjacent free blocks.

Total free bytes can exceed a request while every individual hole is too small:
that is external fragmentation. Freeing the intervening block allows coalescing
and a larger allocation. Host tests demonstrate this behavior and perform 2048
deterministic allocate/free/data-preservation stress steps.

Metadata validation checks magic, sizes, state and monotonic offsets before
traversal. Offsets must cover the arena without gaps/cycles. Invalid, interior,
foreign and double frees return false without dereferencing the supplied pointer.
Detected metadata corruption makes the kernel wrapper panic; it is not ordinary
exhaustion. A NULL free is a no-op. Zero-size, overflowing and non-fitting
allocations return NULL. The arena does not grow and freed payloads are not erased.

Boundary guards catch accesses to those unmapped pages, not all object overflows
or use-after-free. Freed physical frames remain accessible through the direct
map. These checks are not a production memory-safety or user-isolation boundary.

## Actual CPU page-fault probes

Vector 14 reports failed translations or permissions. Isolated test images
install a minimal #PF gate and validate real CR2, error code and RIP:

| Probe | Required CPU error |
| --- | --- |
| Supervisor write to a present read-only mapping | `0x3` |
| Fetch from a present NX mapping | `0x11` |
| Supervisor read from the unmapped heap guard | `0x0` |

The write probe also verifies original physical contents were not changed.
The NX probe requires RIP equal to the attempted execution address. Each handler
reports the expected fault and halts rather than resuming. This IDT fixture is
excluded from normal builds: general GDT/IDT ownership and exception recovery
remain Phase 4. The normal kernel still keeps interrupts disabled.

Host ASan/UBSan checks exercise allocator and table algorithms; guest checks
separately prove hardware translation, CR3/WP/NXE state, TLB behavior and CPU
faults. OOM tests chain allocated frames in their own storage, avoiding another
allocator while memory is exhausted, then free every frame and run normal
memory checks again.

## Compiler runtime and limits

Freestanding memcpy/memmove/memset/memcmp satisfy compiler-generated operations
without a host libc. Tests link renamed versions to preserve sanitizer/libc
interceptors and compare behavior with the host library.

This subsystem assumes one serialized bootstrap CPU. Demand paging, swap,
huge pages, dynamic heap growth, user processes and arbitrary corruption recovery
are not implemented. QEMU BIOS is verified at 32/128/512 MiB; the pinned UEFI
path is verified at 128/256/512 MiB. UEFI 32/64 MiB experiments failed before
kernel entry, including a Limine high-memory allocator panic at 64 MiB.
Physical hardware and production security remain unverified.
