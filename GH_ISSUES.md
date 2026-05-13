# VertexOS GitHub Issue Backlog

These are candidate GitHub issues focused on improving VertexOS in practical, incremental ways.

Each issue includes:
- a suggested title
- why it matters
- a concrete implementation scope
- acceptance criteria

You can copy these directly into GitHub issues and add labels such as `kernel`, `drivers`, `filesystem`, `networking`, `graphics`, `testing`, `hardening`, `good first issue`, `medium`, or `large`.

---

## 1. Make the Kernel Build Warning-Free

**Suggested title:** `Make VertexOS build warning-free under strict compiler flags`

**Why:** Phase 8 still calls out static analysis and warning cleanup. A warning-free build makes later kernel work safer.

**Scope:**
- enable stricter warnings in the build
- fix existing warnings in kernel and userland code
- document any warnings that must remain intentionally suppressed

**Acceptance criteria:**
- `make` completes with no compiler warnings
- warnings are enabled consistently for kernel and user code
- README or developer docs mention the warning policy

---

## 2. Add Static Analysis to the Build Workflow

**Suggested title:** `Add static analysis target for kernel and userland code`

**Why:** Static analysis is still open in the roadmap and would catch classes of bugs normal builds miss.

**Scope:**
- add a `make analyze` target
- run tools such as `cppcheck` and/or `clang --analyze`
- document prerequisite packages and expected output

**Acceptance criteria:**
- `make analyze` runs locally on Linux
- findings are either fixed or tracked with clear suppressions
- docs explain how to use the analysis target

---

## 3. Define a Crash Dump Format for Kernel Failures

**Suggested title:** `Implement structured crash dump output for panics and faults`

**Why:** Postmortem debugging is still missing. Right now failures are readable, but not standardized enough for repeatable triage.

**Scope:**
- define a stable panic/crash dump format
- include register state, fault source, current task, and uptime
- mirror the dump to screen and serial

**Acceptance criteria:**
- panic output follows a documented field layout
- serial logs contain the same crash information as screen output
- developer docs include an example crash dump and field meanings

---

## 4. Add Page Fault Diagnostics with Address and Error Bits

**Suggested title:** `Improve page fault diagnostics with CR2 and decoded error flags`

**Why:** Memory bugs are some of the hardest OS bugs to chase. Better page fault decoding shortens debugging time immediately.

**Scope:**
- capture faulting linear address from `CR2`
- decode present/write/user/reserved/instruction bits
- print a human-readable explanation during faults

**Acceptance criteria:**
- a page fault shows the faulting address
- error-code bits are decoded in the panic output
- at least one test or deliberate fault path validates the format

---

## 5. Add Kernel Stack Overflow Detection

**Suggested title:** `Detect kernel stack overflows with guard pages or canary checks`

**Why:** Stack corruption usually fails far away from the cause. Catching it early makes the kernel more robust.

**Scope:**
- add stack canary or guard-page based detection for kernel tasks
- fail fast when corruption is detected
- include task identity in diagnostics

**Acceptance criteria:**
- stack corruption triggers a deterministic panic
- panic output identifies the affected task or stack
- implementation is documented for future task/thread work

---

## 6. Improve Syscall Argument Validation

**Suggested title:** `Harden syscall entry path with stronger argument validation`

**Why:** The syscall layer exists, but Phase 9 still calls out syscall validation as a security hardening task.

**Scope:**
- audit existing syscalls for invalid pointers and bad argument ranges
- reject malformed user requests safely
- standardize syscall error returns

**Acceptance criteria:**
- invalid syscall arguments do not crash the kernel
- syscall handlers return consistent error codes
- tests cover at least a few invalid syscall cases

---

## 7. Add Per-Task Accounting to the Scheduler

**Suggested title:** `Track per-task runtime statistics in the scheduler`

**Why:** Scheduling works, but there is little visibility into fairness or CPU usage.

**Scope:**
- store runtime tick counters per task
- expose task state, runtime, and total switches in the `tasks` command
- prepare the data model for future scheduler upgrades

**Acceptance criteria:**
- `tasks` prints runtime-related statistics per task
- scheduler updates counters without obvious regressions
- output is stable enough to compare runs while debugging

---

## 8. Add Sleep and Timed Wakeup Support

**Suggested title:** `Implement sleep syscall and timed wakeup queue`

**Why:** A usable multitasking environment needs time-based blocking, not only yield-based cooperation.

**Scope:**
- add a kernel sleep primitive based on PIT ticks
- expose it through a syscall and/or shell test command
- wake tasks efficiently when timeout expires

**Acceptance criteria:**
- a task can sleep without busy-waiting
- sleeping tasks do not consume scheduler time slices
- a demo or test shows multiple tasks waking at the expected times

---

## 9. Add File Append Support to the VFS and Shell

**Suggested title:** `Support append mode in VFS and shell write commands`

**Why:** Current file operations are useful, but append is a basic missing workflow for logs and incremental file editing.

**Scope:**
- add append semantics to VFS and SimpleFS where appropriate
- expose it via a shell command or `write` flag
- verify persistence across reboot

**Acceptance criteria:**
- appending to an existing file preserves previous contents
- appending creates the file if that behavior is chosen and documented
- tests cover append plus reboot persistence

---

## 10. Add Recursive Directory Operations

**Suggested title:** `Implement recursive directory copy and removal support`

**Why:** Filesystem commands are usable but still limited for realistic directory management.

**Scope:**
- support recursive `cp` and/or `rm`
- add safety checks for root and self-recursive paths
- keep error messages consistent with the command system style

**Acceptance criteria:**
- copying a directory tree preserves structure and file contents
- recursive remove handles nested files and directories cleanly
- dangerous cases are rejected with explicit error messages

---

## 11. Add Basic File Metadata Reporting

**Suggested title:** `Expose file metadata through stat-style API and shell command`

**Why:** Debugging filesystem issues and writing better tools gets easier once metadata is visible.

**Scope:**
- add file size, type, timestamps or creation order where feasible
- create a `stat` command or enhance `ls`
- document what metadata is guaranteed by SimpleFS

**Acceptance criteria:**
- users can inspect size and type for files and directories
- metadata output is stable and documented
- VFS API exposes the metadata cleanly for future commands

---

## 12. Reduce Graphics-Mode Full-Screen Redraw Flicker

**Suggested title:** `Reduce graphics-mode flicker by avoiding full-screen redraws for text updates`

**Why:** Repository notes already show that CLI output in graphics mode redraws the whole scene per character.

**Scope:**
- introduce dirty-cell or dirty-region tracking for terminal text updates
- avoid clearing and repainting the full framebuffer on every character
- preserve cursor and mouse rendering correctness

**Acceptance criteria:**
- typing in graphics mode no longer triggers full-screen redraw per character
- terminal responsiveness is visibly improved
- rendering behavior remains correct during scroll and prompt updates

---

## 13. Add a Back Buffer for Framebuffer Rendering

**Suggested title:** `Introduce framebuffer back buffering for smoother graphics updates`

**Why:** Current rendering writes directly to the visible framebuffer. A back buffer would reduce visible draw artifacts and simplify future UI work.

**Scope:**
- allocate a software back buffer for graphics mode
- render the scene into the back buffer
- copy or blit completed frames to the real framebuffer

**Acceptance criteria:**
- graphics mode renders through a back buffer
- visible tearing or flashing is reduced during interactive updates
- memory overhead is documented for each supported resolution

---

## 14. Refactor Display Code for Runtime-Independent Resolution Support

**Suggested title:** `Remove hardcoded resolution assumptions from display and UI layout`

**Why:** Current graphics code still has many compile-time width, height, and layout assumptions.

**Scope:**
- audit hardcoded dimensions in display, framebuffer, and GUI code
- centralize runtime resolution and pitch information
- make text grid and UI layout derive from current mode

**Acceptance criteria:**
- display code no longer assumes only one graphics resolution internally
- framebuffer pitch is used consistently instead of width-only math
- at least two graphics resolutions render correctly

---

## 15. Add TCP Client Utilities on Top of the Existing Network Stack

**Suggested title:** `Build simple TCP client utilities using the existing networking base`

**Why:** Networking exists, but practical validation tools beyond ping and DNS would make the stack more useful.

**Scope:**
- implement a minimal outbound TCP client path
- add a small shell tool such as `tcpget`, `http`, or `nc`
- log connection state transitions for debugging

**Acceptance criteria:**
- the OS can open a TCP connection to a known host in QEMU networking
- the shell command can send and receive simple payloads
- failure cases print useful diagnostics instead of silent timeouts

---

## 16. Add Network Self-Test and Diagnostics Command

**Suggested title:** `Create a network self-test command for NIC, ARP, DHCP, DNS, and ICMP`

**Why:** Networking bugs often span several layers. A single diagnostic command would speed up troubleshooting.

**Scope:**
- add a command that runs staged checks and reports pass/fail per layer
- include NIC presence, MAC, IP config, gateway, DNS server, and optional ping
- make output concise enough for serial logs

**Acceptance criteria:**
- one command provides a quick health report for the network stack
- failures identify the failing stage clearly
- documentation explains expected output in normal QEMU setups

---

## 17. Expand Automated QEMU Integration Tests

**Suggested title:** `Expand QEMU integration tests to cover filesystem, exec, and networking flows`

**Why:** The repo already has kernel tests, but broader integration coverage would catch regressions earlier.

**Scope:**
- add scripted boot-and-assert coverage for core shell flows
- include persistence, ELF execution, and one networking happy path
- make failures produce logs suitable for CI

**Acceptance criteria:**
- test scripts exercise multiple subsystems end to end
- failures return non-zero exit codes reliably
- test docs explain local usage and CI expectations

---

## 18. Add a Release Checklist and Stability Gate

**Suggested title:** `Create a release checklist and stability gate for milestone builds`

**Why:** Phase 8 calls for a clear release checklist. That is low effort and high leverage.

**Scope:**
- define a milestone release checklist in docs
- include build, boot, filesystem, exec, and networking sanity checks
- specify what blocks a tagged release

**Acceptance criteria:**
- a documented checklist exists in the repo
- checklist covers both manual and automated verification steps
- milestone releases use a consistent readiness standard

---

## 19. Add a Minimal Init Program and Boot Startup Script Concept

**Suggested title:** `Introduce init-style startup flow for commands or user programs`

**Why:** The OS is now functional enough that a startup workflow would make it feel more like a system than a demo shell.

**Scope:**
- define a simple boot-time init flow
- allow running a startup script, built-in command list, or userland ELF
- fail safely if startup items are missing or invalid

**Acceptance criteria:**
- the OS can automatically run a configured startup sequence after boot
- startup failures are logged clearly without wedging the shell
- configuration format is documented

---

## 20. Prepare the Codebase for a 64-bit Transition

**Suggested title:** `Audit 32-bit assumptions and document blockers for long mode migration`

**Why:** The roadmap explicitly targets a future 64-bit path. A blocker audit is a good first issue before large architecture work.

**Scope:**
- identify 32-bit assumptions in boot, paging, tasking, syscalls, and pointer-sized code
- document blockers and likely migration order
- mark structures and APIs that would need redesign

**Acceptance criteria:**
- a concrete migration document exists in the repo
- major 32-bit assumptions are cataloged by subsystem
- the document proposes a staged path instead of a one-shot rewrite

---

## Suggested Labels

- `good first issue`: 1, 2, 11, 18
- `kernel`: 3, 4, 5, 6, 7, 8, 20
- `filesystem`: 9, 10, 11
- `graphics`: 12, 13, 14
- `networking`: 15, 16
- `testing`: 17, 18
- `hardening`: 1, 2, 3, 4, 5, 6
- `large`: 13, 14, 15, 20

## Suggested Prioritization

If you want the highest practical payoff first, start with:

1. Make the kernel build warning-free.
2. Add static analysis to the build workflow.
3. Implement structured crash dump output.
4. Improve page fault diagnostics.
5. Reduce graphics-mode full-screen redraw flicker.