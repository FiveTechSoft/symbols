# Workload resources and cleanup design B, no implementation or execution

Design base: d467a7d478a0a9b28845a1ea540402ee506d2aa7. This document selects a
candidate design, not an executable contract. Classification remains blocked;
isolation is not accredited. No boot, QEMU launch, stress helper, namespace,
cgroup write, mount, privilege change or new observation is authorized here.
The server `.16` stays paused. The nine gates in runner-vm-contract remain in
force. Closing resources and cleanup would not close credentials, contents,
FDs, egress or provider-lifecycle gaps.

## 1. Mode decision and immutable launch closure

Choose **TCG-only, x86-64, one vCPU, microvm** for the first future compatibility
tranche. This avoids requiring `/dev/kvm`, group changes or sudo and keeps one
explicit accelerator policy. It is a new proposed mode, not reuse of historical
KVM measurements. No KVM fallback, accelerator autodetection, or mode switch
on failure. The machine/CPU/device/firmware/loader choices must be frozen from
the pinned QEMU version's supported options before any compatibility run.
The existing contract's operational mode remains unselected until that review.

Freeze the QEMU executable, loader, libraries, accelerator modules, firmware,
read-only kernel/initramfs and complete argv/environment/FD manifest by digest.
The historical static .18 closure is a candidate, not a runtime pin for TCG.
Reconcile actual loaded/opened classes in a later trusted compatibility gate.
No arbitrary argv, working directory, PATH lookup, shell, plugin, device,
monitor endpoint, guest-controlled host path or writable guest disk. Proposed
minimal shape: explicit TCG accelerator, one vCPU, 128 MiB guest RAM, no default
NIC/devices, no graphics, no user monitor/QMP listener, fixed owned serial pipe,
read-only boot inputs and fixed guest shutdown marker. Exact argv is deferred
until it matches the chosen binary and machine. TCG threads and loader opens
must be supported by the reviewed launch policy, not silently skipped.

The existing tiny helper's filter denies exec and every clone; it cannot launch
QEMU. Keep it unchanged as historical evidence. QEMU needs a separately built
exec-time policy with architecture checks, no_new_privs, syscall argument
constraints and coverage of the actual fixed launch/runtime closure. A list of
observed syscalls alone is not an adversarial allowlist or accreditation.
QEMU's own optional sandbox is defense in depth, not a cgroup, filesystem,
credential or cancellation boundary. Unsupported policy setup blocks before
exec; no unfiltered retry. Permit only reviewed thread creation if required;
process creation/exec escape must remain denied or bounded by an independent
scope. A thread allowlist must not imply arbitrary clone flags are safe.

## 2. Required scope and controller separation

Use a **private cgroup v2 domain subtree** enclosing every payload thread and
process. The trusted supervisor/controller and artifact upload remain outside.
No payload-visible writable cgroup controls or migration handles. Prove that
all payload identities cannot migrate outside the scope or alter its limits.
Same-UID management access is an unresolved threat; empty env does not fix it.
If a separate restricted payload identity or filesystem policy is needed,
review its setup authority and feasibility first. Do not change users, global
policy or mount namespaces under this design-only approval.

Prerequisites before even a benign scope test:

- Resolve one narrowly owned delegated directory through no-follow anchors.
  Check domain type, controllers, subtree ownership and required writable
  delegation interfaces. Current membership or access() alone is not proof.
  Do not enumerate foreign tasks, read their environments or try arbitrary
  cgroup paths. Past six nonwritable current controls do not establish that
  every alternate delegated scope is absent.
- Prove bounded creation/configuration/removal of one owned child scope and
  real controller availability there. Read back effective memory, swap, PID
  and CPU controls before launch. Test refusal paths and cleanup with no
  payload. Absence of delegation is a blocker, not permission for sudo,
  system-wide firewall/AppArmor/sysctl changes or a killpg fallback.
- Use race-free placement before payload instructions execute. Preferred
  candidate: a reviewed native launcher using clone3 CLONE_INTO_CGROUP with
  the owned scope FD, then fixed exec. Unsupported kernel/access refuses.
  Any alternative stopped-child migration protocol needs its own race proof;
  writing the PID after an already running exec is not acceptable.
- Keep a stable direct-child pidfd, observe exit and reap. A new process group
  is supplementary, not an all-descendant boundary. Subreaping helps wait for
  adopted children, but does not retain session-escaped tasks in a scope.
- Payload cannot write controller/report/supervisor paths, inherit controller
  handles, ptrace same-UID management or open its control sockets. This must
  be independently enforced and tested before untrusted workload use.

Provider VM freshness is a separate boundary assertion. It does not prove this
private scope exists or that escaped guest code cannot access the runner's
management credentials. If these prerequisites cannot be met without new
privilege/setup authority, return a blocker and a separate setup design.

## 3. Proposed budgets, not measured sufficiency

These are initial **review targets** for one trusted TCG fixture, not inferred
from ambient runner limits or approved workload capacity. Freeze concrete
budgets with mode/closure before each future run; insufficient budget means
refusal and a new review, never automatic enlargement.

| Resource | Proposed envelope | Enforcement and independent evidence |
| --- | --- | --- |
| Guest RAM | 128 MiB | Fixed QEMU argv; not host-memory enforcement |
| Payload memory | 512 MiB aggregate, zero swap | memory.max + memory.swap.max readback; touch committed pages, memory.events deltas; OOM group handling reviewed |
| Tasks | 32 total kernel tasks, including threads | pids.max readback, pids.current and pids.events deltas; trusted at-limit fork/thread tests within outer cap |
| CPU rate | 100000/100000 microseconds, at most one CPU worth per period | cpu.max readback; cpu.stat usage/throttle deltas; rate quota does not cap total CPU time |
| Total CPU | 10 CPU seconds per payload scope | External supervisor usage_usec sampling plus scope kill; overshoot bound proposed 250 ms plus scheduling lag, not assumed guaranteed |
| Wall | 20 s launch-to-exit; kill/reap deadline 5 s | Independent monotonic deadline outside scope; enforce emptiness or refuse, never declare success on timeout alone |
| Per-process AS | 768 MiB | Hard=soft RLIMIT_AS readback and trusted committed-allocation test; complements, does not replace aggregate memory.max |
| Per-process CPU | 12 s | Hard=soft RLIMIT_CPU; trusted separate signal test, not aggregate CPU proof |
| Per-file output | 1 MiB | Hard=soft RLIMIT_FSIZE plus regular-file overrun test; pipe output is separately bounded |
| FDs | 64 per process | Hard=soft RLIMIT_NOFILE plus trusted exhaustion/readback; descendants still need PID cap |
| Core | 0 bytes | Hard=soft RLIMIT_CORE; no dump claimed from absence alone; host core-routing remains a reviewed gap |
| Supervisor captures | 64 KiB stdout + 4 KiB closed status; stderr discarded | Reader bounds outside payload; overflow triggers scope kill, never upload arbitrary guest contents |
| Writable workspace | 8 MiB total, 32 entries, no links/devices/mounts | Need real scoped storage/entry enforcement plus bounded inventory; FSIZE alone cannot cap many files |

The workspace row is a **blocking prerequisite**: periodic scanning is not a
quota. A private constrained storage facility or proven equivalent no-file-
creation policy is needed. A tmpfs requires separately authorized mount/setup,
uses memory accounting and still needs entry/inode bounds; project quotas need
verified filesystem support and administration. Neither is assumed present.
Do not let the workload write arbitrary host locations. Guest disks are absent
in this candidate; future disk/device needs reopen the closure and budgets.

## 4. New trusted consumption-test helper and policies

Build a new fixed native helper family, not arbitrary candidate code and not
an extension that turns ceiling-denials into stress. Each mode has a compiled
constant action, bound, exact schema and closed failure enums. No shell, random
paths, resolver/public traffic, foreign process scan or credential value read.
Positive below-limit and negative above-limit samples use fresh owned scopes;
setup failure is never a negative-test success. Tests are sequential, one
reviewed observation per tranche, with predeclared outer caps and teardown.
No native execution is part of this document patch.

The policies below are **requirements to derive**, not syscall filters supplied
for installation. Allow only needed ABI/syscall arguments and owned handles;
keep no_new_privs, wrong-ABI refusal and default denial. Review initialization,
signal handling, libc/raw-syscall behavior, startup descriptors and resource
readbacks for each compiled helper. Different operations require different
policies; do not loosen the current helper to claim compatibility.

| Mode | Fixed positive/negative operation | Needed policy shape and attribution |
| --- | --- | --- |
| AS per-process | Map/touch 8 MiB below limit; request mapping larger than the frozen AS ceiling | Permit only fixed anonymous mmap/munmap and status write; require below-limit success and ENOMEM for above-limit, with hard/soft readback. No overcommit-only inference |
| Aggregate memory | Two bounded owned allocators together cross 512 MiB while each stays below its AS ceiling | Permit fixed anonymous mappings and scoped process creation only for this trusted test; maximum total demand 544 MiB, outer task/wall caps. Attribute memory.events oom/oom_kill deltas and known scope state; allocation ENOMEM alone is not cgroup OOM proof |
| CPU per-process | Bounded arithmetic loop, proposed test soft1s/hard2s, trusted SIGXCPU handling without changing limits | Review signal-policy syscalls and exact soft/hard pair; independent below-limit fixture returns before1s, negative fixture records soft signal then reaches hard SIGKILL with external CPU counters. Wait status distinguishes CPU signal from supervisor wall kill. No signal handler silently extends budget |
| Aggregate CPU rate/total | Bounded fixed worker threads consume CPU under cpu.max, with known throttle interval and external usage counters | Allow only constrained thread clone flags and necessary synchronization; prove rate throttle, cumulative stop and overshoot separately. Host contention can make a run inconclusive, not a passing rate test |
| PID/thread cap | Fixed controller creates up to frozen cap plus one attempt | Scope pids.events:max delta plus failed creation EAGAIN, below-cap success and independently empty scope afterward. Helper has a second constant creation cap so absent enforcement cannot fork indefinitely |
| FSIZE | Write constant bytes below 1 MiB to one preopened owned regular file, then one bounded overrun attempt | Permit writes only to that owned FD, fixed signal handling/status FD; establish SIGXFSZ or EFBIG and verified maximum length. No pipe-size claim and no multi-file quota inference |
| NOFILE | Duplicate a preopened harmless owned descriptor to fixed slots until cap, then one extra | Permit constrained dup/close/readback, no arbitrary opens/sockets; below-cap success and EMFILE above cap. Cleanup every owned FD; no count-only inference about ambient FD identities |
| Core | Separate trusted crash fixture with zero CORE limit | Review core-routing policy first; prohibit host dump sink or unrelated artifact retrieval. Signal death plus absent owned file alone cannot establish no host-wide dump |
| Workspace total/entries | Create fixed numbered constant files below quota, then one bounded exceed attempt | Only after actual storage/entry enforcement exists. Fixed aggregate requested bytes/entries bounded above envelope, ENOSPC/EDQUOT plus independent counters. This stays unavailable if only FSIZE exists |

Exact unexpected errno, exit, signal, output, counter or deadline state is a
closed refusal. Do not accept a child killed by the wrong boundary as proof of
its target resource. Repeat attempts are not authorized by a failed observation.

## 5. Cleanup proof and failure matrix

A controller outside the scope must own every launch and cleanup handle before
starting the child. Before success/failure report or upload: stop new launches,
request cgroup.kill for the exact owned domain subtree, reap known direct/
adopted children with stable identities, and wait boundedly for
cgroup.events populated=0. Recheck within the owned hierarchy, remove owned
empty child scopes and verify no scope remains. Kernel cgroup.kill covers
fork races and descendants in that scope; an escaped migration defeats the
premise, so migration denial must already be proved. An unkillable task or
unknown membership means cleanup not_proven, not benign timeout completion.

Only after process emptiness, close known supervisor/fixture descriptors and
remove an enumerated scratch boundary through dirfd-relative no-follow
operations. Reject unexpected type, symlink, hardlink ambiguity, mount point,
path escape, entry count or deletion error. No recursive traversal of foreign
locations. Fixed controller reports live outside the writable boundary and
are not guest-controlled evidence. Verify absence of the owned directory and
scope independently before uploads. Descriptor-rooted operations reduce
pathname races; they are not a replacement for forbidding payload writes
outside the boundary or for proving process emptiness.

| Scenario | Required future evidence |
| --- | --- |
| Normal exit / nonzero exit / failed exec | Scope placement before exec, wait/reap status, empty scope, exact owned-path absence |
| Wall/output/resource violation | Closed attributable trigger, kill whole owned scope, bounded reap/emptiness, scratch removal |
| setsid/double-fork fixture | Trusted fixed descendant remains inside cgroup despite session/group changes; scope kill empties it |
| Supervisor SIGTERM | Out-of-scope controller performs same teardown, independent evidence |
| Supervisor SIGKILL/crash | A separately owned watchdog/controller survives and tears down; in-process finally is insufficient |
| Workflow cancellation / runner failure | A durable external check reads final provider job outcome; missing cleanup artifacts stay not_proven. Provider destruction is an assertion, not local emptiness evidence |
| Upload interruption | Local cleanup proof must precede upload; lost proof remains unavailable, never reconstructed from missing files |
| Cleanup failure / uninterruptible task | Refuse, no new workload or repair dispatch; report only closed diagnostics |

An in-job watchdog can also die with the runner; it cannot guarantee every
cancellation path. Do not promise complete cancellation cleanup before a
separately reviewed provider/external-control model and tests exist. If local
cleanup is required independently of provider destruction, a lost-controller
case is an unresolved gate, not a reason to shrink that requirement.

## 6. Evidence order and stop rules

1. Design review only (this patch): mode proposal, budgets and open prerequisites.
2. Separate capability/read-only discovery plan for a private delegated scope,
   storage enforcement and payload/controller separation. No privileged fallback.
3. Separate exact code/workflow review for benign scope creation/readback/removal,
   before any consumption test. One bounded observation, artifacts and ledger.
4. One new helper/policy tranche per resource with both baseline and violation,
   exact SHA/image/mode closure, independent counters, teardown and closed report.
5. Dedicated failure/timeout/session-escape/supervisor-death cleanup tranches.
6. Only after gates permit it, a separately approved trusted QEMU TCG compatibility
   boot with frozen closure. No adversarial workload or boot authority is implied.

At every application: canonical remote git-diff serialization, path/mode/hunk
byte equivalence to the reviewed patch, reusable CI evidence and ledger. State
whether Pages/bank ran at that SHA; do not import old greens. Freeze image
provenance with no intervening commit between measurement and observation.
Unknown red, drift, absent proof or unexpected cleanup stops without retry or
repair. Report the actual evidence scope; never promote helper findings to
aggregate/workload proof. Isolation accreditation and classification remain
unchanged until all nine gates have explicit sufficient evidence and owner
approval for the intended workload.

## Sources read for design semantics

- QEMU security model: https://www.qemu.org/docs/master/system/security.html
- QEMU mode/options reference (not proof of candidate binary support): https://www.qemu.org/docs/master/system/invocation.html
- cgroup v2 delegation, cpu.max, memory/pids events, cgroup.kill and populated: https://docs.kernel.org/admin-guide/cgroup-v2.html
- Soft/hard limits, signals and scopes: https://www.man7.org/linux/man-pages/man2/setrlimit.2.html
- Race-free cgroup placement candidate CLONE_INTO_CGROUP: https://www.man7.org/linux/man-pages/man2/clone.2.html
- Stable direct-child handles: https://www.man7.org/linux/man-pages/man2/pidfd_open.2.html
- Descriptor-relative resolution constraints: https://www.man7.org/linux/man-pages/man2/openat2.2.html
- Landlock limitations and filesystem policy candidate: https://docs.kernel.org/userspace-api/landlock.html

Official semantics do not prove runner delegation, candidate binary support,
storage quotas, control-plane separation or workload capacity. Those remain
measurements to design and approve later.
