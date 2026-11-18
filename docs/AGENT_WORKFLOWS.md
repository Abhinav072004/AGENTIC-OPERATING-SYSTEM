# Native agent workflows

AgenticOS can plan and execute bounded multi-step filesystem/system requests
through its own native services. Offline planning works without a model service.
An optional model backend uses an explicitly configured host bridge. Plans are
held for owner review; no backend can approve its own actions.

## Desktop

Click **Ask Agent** in the top bar or Applications menu. Type an intent and press
Enter or Plan. The panel supports up to 511 printable ASCII characters; Ctrl+U
clears the input. It shows the last 168 input characters when the input is long.
Use explicit absolute /home paths. For example:

```text
mkdir /home/project; write /home/project/note 'Hello AgenticOS'; read /home/project/note
```

Use Prev/Next to inspect every action. Full path, destination and literal text
fields wrap across lines, with quotes/backslashes/LF/tab displayed explicitly.
The panel enables approval after all actions have been viewed. Writes replace
existing files. Deletion additionally requires **Allow deletion**. Approve starts
the reviewed job; the isolated worker requests one broker-controlled atomic
action at a time. The panel reports state and the exact completed/total count. Completion selects the last action and shows
its bounded result preview.
Cancel stops future actions; completed changes remain. Closing the Agent window
cancels pending work. Release frees a terminal job's slot; starting a new plan
also releases a previous terminal job. Audit advances through retained metadata;
Prev/Next returns to action review.

Select Offline or Model bridge before planning. The model option requires the
host bridge and QEMU networking described in [AGENT_INFERENCE.md](AGENT_INFERENCE.md).
Unavailable/malformed/timed-out inference produces a visible failure without
executing tools. Review/execution expires about 60 seconds after job creation.
The built-in Agent panel shares the desktop controller process; each worker has
its own isolated address space and zero direct tool capabilities.

## Shell and Terminal

The same commands work in the serial shell and desktop Terminal. Shell lines
are limited to 255 characters; quote the complete intent as one argument, using
the other quote type for file text:

```text
agent plan "mkdir /home/project; write /home/project/note 'Hello'; read /home/project/note"
agent jobs
agent review 12
agent approve 12
agent status 12
agent audit 12
agent release 12
```

Replace 12 with the returned job ID. `plan` prints the entire proposed plan and
returns without executing it. `approve` explicitly grants that plan and waits
for completion/failure. For a plan containing deletion, use
`agent approve ID delete`. `agent cancel ID` stops pending/future steps. Plans
are immutable after submission; editing a request requires a new job/approval.
`agent model "INTENT"` selects the optional connector. Released IDs cannot be
used for work again, but retained audit metadata can still be queried by ID.
At most two unreleased jobs fit alongside a controller. Use `agent jobs` and
release completed/cancelled/failed jobs before submitting more work.

## Current limits

This is an educational QEMU OS with seven typed tools, at most eight actions
per plan, 127-byte path/text fields and 127-byte read/list previews. All tool
paths stay under /home and actual VFS permissions/current owner capabilities
still apply. There is no arbitrary shell/code execution, general application
control, durable disk, background service across reboots or workflow rollback.
Files, settings, job state and the 64-event audit ring are RAM-only. No physical
installation has been performed. The small offline classifier is domain-limited;
a live third-party model needs user configuration and has not been validated.
