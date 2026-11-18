# RAM filesystem and file ABI

The VFS owns a bounded, hierarchical RAM filesystem. Absolute paths contain at
most 127 bytes and component names at most 31. Repeated separators and `.` are
normalized; `..` at root is rejected. A path cannot traverse a regular file.
Directories cannot be renamed beneath themselves or removed while nonempty.
Rename rejects an existing destination, making failure leave both names intact.

There are 64 inodes, 128 zeroed 4 KiB data blocks and a 64 KiB per-file limit.
Sparse holes read as zero. Writes preflight block availability before mutation;
quota failure leaves size, offset and data unchanged. Data is volatile: reboot
restores the built-in `/bin`, `/etc`, `/home` and `/tmp` seed. No persistent disk
format is implemented in this phase.

Every process owns 16 descriptors numbered from 3. Descriptors 0–2 retain the
console ABI. File descriptors have independent offsets and access flags; append
chooses the inode's current end on each write. An unlinked open file survives
until its final descriptor closes. Reaping normal or faulted processes closes
all descriptors and releases unlinked storage. No syscall retains a user pointer.
All filesystem mutations execute with interrupts disabled on the single CPU.

The shared definitions are `kernel/fs/vfs.h` and `kernel/syscall/abi.h`:
`open`, `close`, absolute `seek`, `mkdir`, `unlink`, `rename`, `stat` and indexed
`list`, plus the original `read`/`write`. Transfers are bounded to 128 bytes per
call; callers loop for larger files. Paths are copied with a checked, bounded
NUL search. Output buffers are validated before changing offsets or filesystem
state. This is a RAM backend with one root; mountable persistent backends and
multi-user access policy are separate milestones.
