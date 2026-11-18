# Week 8 — device drivers

Verified 2026-11-18 with `./scripts/env.sh make test` and
`./scripts/env.sh make test-drivers`; all commands passed. The default target
also builds with strict freestanding warnings and passes kernel ELF validation.

Host ASan/UBSan checks exercise two simultaneous shift keys, extended releases,
mouse resynchronization/sign/overflow and bounded queue drops. Existing host
memory, task, user and filesystem tests pass. Default BIOS/UEFI boots reach
PHASE8 with optional network/data storage absent.

The dedicated BIOS and UEFI boots discover six PCI functions, initialize real
PS/2 keyboard/mouse and an e1000, identify the 2048-sector synthetic ATA device,
write/flush/read sector 1 and reject capacity violations. Independent host checks
confirm exact sector contents and unchanged data elsewhere. QMP injects an `a`
key plus relative mouse movement with a pressed button. Twenty-four sequential
Ethernet requests and replies cross the QEMU device in each firmware run, proving
both 16-entry rings wrap successfully. No host disk or external network is used.

Evidence: `build/tests/drivers-{bios,uefi}/{serial.log,command.json,results.json,
registers.txt,screen.png}`. Guest results include STORAGE, INPUT, NETWORK PASS
and PHASE8 PASS; exact sequence lists are independently captured by the runner.

Default artifact SHA-256:

```text
a5bdf17aa3567b5342ca2333a44a541bee09cbd593c926115e729afd5a349bb3  build/agenticos.iso
3ab32e0073344b706438a1d423b4066fc9b91ae64cffc9c11a479ad3d942f7a2  build/kernel.elf
```

Limits and supported hardware are documented in [DRIVERS.md](../DRIVERS.md).
These drivers poll; network protocols and persistent filesystem formats remain
separate work. Earlier reports and screenshots are preserved unchanged.
