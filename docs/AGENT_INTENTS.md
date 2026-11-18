# Typed agent intent and native tools

The first agent milestone defines plans as data. It does not execute them.
A separate action broker and user-space runtime follow in the next milestones.
The OS remains fully usable without inference access.

A plan contains an ABI number and one through eight typed actions. Each action
contains a tool ID, zero reserved flags and fixed 128-byte path/target/text
fields. Fields must terminate, have zero tails and contain supported ASCII.
Unused actions must be zero. No script, executable command or function pointer
is represented by this interface.

Tools are mkdir, write, read, list, move, delete and system. Read/list require
filesystem-read authority; mutating tools require filesystem-write authority;
system requires inspection authority. This metadata describes what a future
broker must check; parsing does not grant those capabilities.

The workspace is `/home`. Paths normalize repeated separators and `.`; parent
traversal, another namespace and mutation of the workspace root are rejected.
Kernel-side validation requires already canonical paths, independently of the
user parser. File text is literal data, including quoted words resembling tools.

Examples:

```text
Please create folder /home/project and write /home/project/note 'Hello AgenticOS'
move /home/project/note to /home/project/README; read /home/project/README
list /home; show system status
```

The deterministic grammar accepts explicit absolute paths, single/double quotes,
escapes and `and`, `then` or semicolon separators. Inputs are bounded to 511 bytes.
Unknown verbs, unterminated quotes, excessive actions and invalid paths fail
without returning a partial actionable plan. This is an explicit grammar, not a
claim of general language understanding or a bundled large language model.

The same pure C contract is compiled for host sanitizers, the kernel validator
and a native CPL3 planner. The native acceptance process has zero capabilities,
CPU/syscall budgets and no filesystem access. It proves parsing and validation
work while a direct filesystem syscall is denied. Its process resources are
reclaimed and no filesystem blocks change during planning.
