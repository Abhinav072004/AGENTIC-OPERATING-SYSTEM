# Agent milestone 4 — offline inference and model connector

Verified 2026-11-18. `./scripts/env.sh make test` passes strict cross-build and
ELF checks, all ASan/UBSan host suites, bridge HTTP fixtures and BIOS/UEFI native
offline inference. Each native workflow reviews an inferred plan, confirms no
file exists before approval, executes it, reads back exact contents and cleans
all frames/blocks. Unsupported requests fail without tool effects.

The actual C integer classifier is tested against 14 held-out synthetic labels:
11 accepted predictions are correct and three uncertain requests are rejected.
20,000 additional sanitized requests exercise bounds, extraction, scope and
failure behavior. Exact regeneration of the checked-in trained weights passes.
The model is intentionally small and domain-limited; it is not an LLM.

`./scripts/env.sh make test-agent-network` verifies real e1000/TCP networking on
BIOS and UEFI, through a host bridge to a real local HTTP provider fixture.
Acceptance covers a typed model plan held for review, exact approved execution,
unsafe paths, malformed JSON, provider failure, wrong protocol magic, oversized
length, incomplete replies, timeout and in-flight cancellation. All physical
frames, transient file blocks and eight socket slots are reclaimed. Logs are in
build/tests/agent-model-{bios,uefi}/serial.log; the guest emits MODEL BRIDGE PASS,
AGENT4 PASS and READY. Every failure asserts its exact error code; no agent
invalid-instruction faults are accepted. A separate native -123 program return
verifies signed startup/exit status handling.

No API credential is embedded in the guest. The bridge uses an explicitly
configured provider endpoint and host environment key; live third-party model
inference is unverified because no model credentials were provided. See
[AGENT_INFERENCE.md](../AGENT_INFERENCE.md) for operation and limits.
