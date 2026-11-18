# Week 10 — native networking

Verified 2026-11-18. Ethernet, ARP, IPv4, ICMP echo, UDP and bounded TCP connect,
bind/listen/accept, stream transfer, FIN/EOF and failure handling run in the native
kernel through real e1000 DMA. Native CPL3 programs use copied socket syscalls;
all sockets check ownership and are released when their process is reaped.

Executed successfully after the final ARP rate-limit and shutdown changes:

- `./scripts/env.sh make test`: strict build, ELF checks, all existing sanitized
  suites and the new packet/socket suite. ASan/UBSan cover truncated packets,
  checksum/fragment rejection, zero-length UDP, owner/port checks, retry timeout
  and 30,000 malformed frames. Default BIOS/UEFI offline boots reach PHASE10.
- `./scripts/env.sh make test-network`: BIOS and UEFI native socket acceptance
  against an independent Python Ethernet peer through QEMU e1000. Checks include
  ARP, IPv4/transport checksums, ICMP, UDP, active TCP, passive TCP accept,
  dropped first SYN and first data segment, out-of-order rejection, data/FIN/EOF,
  refused port and a four-SYN timeout (initial plus three retries).
- `git diff --check`: clean.

The final peer records exactly one ARP resolution, two SYN attempts and two data
attempts for the loss test, four SYNs for timeout, and both completed FIN paths.
Guest status is zero with NETWORK SOCKET PASS and PHASE10 PASS. Evidence is
`build/tests/network-{bios,uefi}/{serial.log,results.json,command.json,
registers.txt,screen.png}`. Traffic remains on loopback; no external service,
credential or host data disk is used.

Default artifact SHA-256:

```text
7c562de0c13043895838c2ed86436de1eb898fce8c53cf78cf5d498c5aa5fc30  build/agenticos.iso
92ef2882a4edbed1036c9d8865765b2001d1aa61ba11d9b35c7405eceab5cec2  build/kernel.elf
```

Limits: static IPv4; eight sockets; 512-byte sends and 4096-byte receive windows;
one outstanding TCP segment; bounded retries; polling. No DHCP/DNS/TLS, IPv6,
fragment reassembly, full congestion control or general Internet compatibility.
See [NETWORK.md](../NETWORK.md). These limits remain explicit release criteria.
