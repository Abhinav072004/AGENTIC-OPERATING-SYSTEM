# Agent milestone 1 — intent and tool contracts

Verified 2026-11-18. Plans are bounded data, not executable scripts. Seven typed
native tools have canonical `/home` scope, explicit permission requirements,
zero reserved/unused fields and fixed path/text bounds. Parsing rejects an entire
request on failure; quoted tool-like text remains file data.

`./scripts/env.sh make test` passed strict cross-build/ELF checks, all host
sanitizers, the new 20,000-input contract suite and BIOS/UEFI native acceptance.
Additional targeted ASan/UBSan checks pass for oversized intents and a ninth
action. The actual native planner has no capabilities, runs under CPU/syscall
budgets, confirms a filesystem call is denied, and leaves all physical frames
and transient filesystem blocks unchanged after reaping. Both normal firmware
logs reach AGENT1 PASS and READY (`build/tests/{bios,uefi}/serial.log`).

This milestone does not execute plans and contains no inference provider. The
following milestones implement the broker, bounded worker, inference/failure
handling and native review interface. See [AGENT_INTENTS.md](../AGENT_INTENTS.md).
