# Agent inference backends

Offline is the default. The native, zero-capability worker first tries the
explicit multi-step grammar. It then uses a tiny integer multinomial Bayes
classifier trained from 56 project-owned synthetic phrases in ai/intents.json.
The 76-word log-likelihood table is checked in; regenerate with
`python3 tools/train_intents.py`. `--check` verifies exact weights. This is a
narrow intent model, not a general LLM. Of 14 separate synthetic held-out
phrases, 11 are classified correctly and three are rejected for insufficient
score margin. These small synthetic results do not establish general accuracy.

The fallback extracts at most two explicit /home paths and, for writes, one
quoted literal. It requires all tool arguments, rejects unsupported/ambiguous
requests and compound requests it cannot parse completely, and revalidates the
whole typed plan. Examples:

- `please put 'hello world' into /home/note`
- `I would like a directory at /home/notes`
- `show me the files in /home`
- `how much free memory`

For multi-step requests, use the explicit grammar, for example:
`mkdir /home/project; write /home/project/note 'hello'; read /home/project/note`.
Inference never executes a tool or approves a plan.

## Optional model connector

A host bridge can call an OpenAI-compatible chat completion endpoint. Run it
on the same host as QEMU:

```sh
# Set AGENTICOS_MODEL_KEY in your host environment if the provider requires it.
python3 tools/model_bridge.py --url https://YOUR_PROVIDER/v1/chat/completions --model YOUR_MODEL
./scripts/env.sh make BUILD=build/desktop BOOT_TEST=desktop iso
./scripts/env.sh python3 tools/qemu.py --iso build/desktop/agenticos.iso --network user --display gtk
```

For a local model server, use its complete localhost URL and explicitly add
`--allow-local-http`. Credentials are read only by the host bridge, never put in
the ISO or sent to the guest. Provider redirects are rejected. The bridge binds
127.0.0.1:8088, limits concurrent handlers to four, bounds input/provider output
and strictly validates JSON keys, action counts, paths, types and string sizes.
Configure another port with `--port PORT` and `make AGENT_BRIDGE_PORT=PORT ...`.
Select Model bridge in Ask Agent, or use `agent model "INTENT"` in the shell.

The guest connects only to QEMU's host gateway 10.0.2.2 on that configured port,
using its native e1000/TCP implementation. It sends `AGI1`, a little-endian
32-bit length and ASCII intent; the bridge returns `AGP1`, signed status, length
and a fixed 3144-byte typed plan. There are no guest API keys, DNS or guest TLS.
The host performs HTTPS to the configured provider. This local link is intended
for a trusted QEMU host; it is not a remote authenticated/encrypted protocol.

Only an owner explicitly selecting the model backend with CAP_NETWORK can
initiate inference. The kernel proxies a job-bound socket; the worker retains
zero network authority. The native worker and kernel independently validate
model output. Model output is always held in REVIEW until owner approval.
Transport requests time out after about 20 seconds, within the 60-second job
deadline; the dedicated transport tests shorten that wait. Errors are reported
without silent model substitution or filesystem effects. Cancel/exit closes
in-flight sockets and reclaims the worker.

`make test-agent-network` boots real BIOS and UEFI guests, traverses actual
QEMU user networking, the binary bridge and an HTTP provider fixture. It checks
valid review/execution, unsafe paths, malformed JSON, provider failures, invalid
wire magic/length, partial replies, timeouts and in-flight cancellation. It also
verifies all eight socket slots are available afterward. A live third-party
model has not been tested: no provider/model/key has been supplied. The included
fixture proves the connector, not the quality or availability of a live model.
