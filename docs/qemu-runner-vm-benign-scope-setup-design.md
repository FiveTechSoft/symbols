# Benign private scope setup design and runner fork

Base 7ce7918749640c5f06664e2bb1ecc9b8bf2bb02d. Documentation only, no apply,
implementation, observation or writes under this design assignment. No boot,
payload, privilege change, scope creation or server `.16` action. The nine-gate
contract remains blocked; isolation is not accredited.

## Evidence and authority boundary

Discovery2 at this base returned metadata_only_not_enforcement, 60 metadata
operations and 4094 input bytes. Current cgroup type was domain_candidate;
required controllers were present in cgroup.controllers but required_missing
in subtree_control. This means the required set was not all enabled there,
not which controllers were absent and not that no alternative delegation can
exist. cgroup.procs/threads/kill were regular_metadata_only; no contents or
writability measurement. Optional operator candidate was not_provided and
other_delegations not_excluded. No private parent is verified.

Evidence: https://github.com/FiveTechSoft/symbols/actions/runs/36856267262
JSON: https://github.com/FiveTechSoft/symbols/actions/runs/36856267262/artifacts/11157833393
ZIP SHA256 1bb78dc3abba113c26f3c6a1945e9a675eaa9b09015be8a324c2acc36f059861.
This does not authorize writing the runner's current cgroup.

A future test requires a separately authenticated operator grant binding an
exact private parent, intended runner/job identity, owner, exclusive use,
allowed mutations and cleanup responsibility. A path, environment flag,
current UID match or document claiming verified=true cannot satisfy it. The
controller must be outside an empty dedicated parent before enabling domain
controllers. No moving existing runner/controller/foreign processes out of
the current group to make it usable. No sudo or alternate privilege fallback.

Without that grant and verified dedicated parent: return
`operator_scope_unverified`, classification blocked, without mkdir/write/kill.
No read discovery is relabeled as a grant. The implementation is deferred
until these prerequisites can be supplied and independently checked.

## Proposed bounded empty-child test, not a payload scope test

The future native or carefully descriptor-anchored controller would use only
an operator-bound parent FD, no-follow resolution and stable device/inode/type
checks, not current membership as a mutable default. Freeze exact implementation,
workflow, SHA/image and grant before one separately approved observation.

Proposed outer bounds: one child directory, zero payload processes, 10 s
monotonic wall, at most 64 fixed metadata/control operations, 16 KiB total
reads, 4096+1 per file, 128 bytes per control write, 2 s empty-wait and 2 s
cleanup deadline within the whole budget. Reserve cleanup operations before
any mutation. If the reserve cannot fit, refuse before creation. No retry,
recursive crawl, foreign process listing or process migration.

Sequence and failure evidence:

1. Verify operator grant and parent identity/exclusivity. Read only fixed
   cgroup.type/controllers/subtree_control/events. Require empty domain parent,
   correct ownership and authorized controller configuration. A populated,
   changed or ambiguous parent refuses. No task-list contents.
2. If required controller set is not enabled, attempt one explicitly granted
   write `+cpu +memory +pids` to the dedicated parent's subtree_control. A
   failed write is a legitimate closed outcome, not repair authority. Require
   readback before proceeding. Enabling a controller is not an entitlement to
   alter its ancestor. No writes to current runner membership or ancestors.
3. Create exactly one exclusive child with a fixed per-observation identity
   known to the controller. Existing child means refusal, never adopt/remove it.
   Record identity from the newly opened child FD and verify parent-child
   relationship before every operation. No payload or process placement.
4. Write only child limits, proposed test values memory.max=16777216,
   memory.swap.max=0, pids.max=4, cpu.max='10000 100000', then read back all
   four exactly. These empty-scope test budgets are not the QEMU budgets.
   Configuration readback is not measured enforcement or consumption proof.
   Ancestor limits may be tighter; record effective-envelope uncertainty rather
   than call these local values the effective hierarchical limits. Do not
   inspect or modify arbitrary ancestor interfaces to resolve it in this test.
5. Write `1` to cgroup.kill only through the verified newly created child's
   control FD. Never write current group or parent kill. Zero payload means
   this tests availability of the empty-child interface, not kill effectiveness
   against descendants. Read cgroup.events populated=0 within the deadline.
6. Remove only the exact empty child with verified identity; verify absence
   through the parent FD. No recursive delete, adoption, task migration or kill
   of an unexpected scope. Identity drift or populated child blocks cleanup.
7. Restore only parent controller state changed by this test, if the grant
   explicitly covers restoration and the parent is still exclusively owned.
   Disable only controllers newly enabled here after child removal and read
   back the original set. Parent restoration failure is cleanup_refusal, not
   permission to retry or change ancestors. If restoration cannot be justified,
   refuse before step2 rather than leave a shared mutation.

On every failure, close owned descriptors and attempt only pre-authorized
owned-child removal and exact parent restoration, using the reserved budget.
No finally block is called cleanup proof. Cancellation, SIGKILL, runner loss
or identity drift may leave the child/configuration behind; missing proof
stays not_proven and prevents another test. A separate cancellation/controller
model is required before this setup can underpin workload cleanup.

Output uses closed enums: phase (authority, parent_identity, parent_enable,
child_create, child_limit_write, child_readback, child_kill, empty_wait,
child_remove, parent_restore, absence_verify); outcome (operator_scope_unverified,
prerequisite_refusal, write_refused, readback_mismatch, setup_metadata_only,
cleanup_refusal, timeout, identity_drift, unknown). Error category and errno
must be allowlisted; no exception strings, paths, IDs, tasks or raw control
contents. Record which fixed interface/write phase failed, not host data.
Separate config readback, empty-child kill-call success, child absence and
parent restoration. Even success is measured_only setup, all aggregate gates
not_proven. No resource consumption, task containment or isolation claim.

## The honest fork

### A. GitHub-hosted, current no-privilege rules

No dedicated parent or operator grant is available in the current evidence.
Therefore the private-cgroup route is blocked **under the present no-privilege,
no-shared-parent-mutation rules**, until such a parent is verified. Do not spend
more observations writing current membership to discover the same gap.

It is not established that GitHub-hosted runners can never support setup or
that self-hosted hardware is the only technical route. GitHub documents
passwordless sudo on Linux/macOS hosted VMs. That capability is outside this
assignment and is not authorization to use it. A separately reviewed privileged
per-job setup could be a future fork only if the owner changes the present
rules, with payload/controller identity and secret exposure reviewed. This
document neither proposes running sudo now nor treats provider VM ownership
as permission to change policy. "Hosted without verified delegation is blocked"
is supported; "hosted delegation is impossible" is not.

### B. Self-hosted on Antonio's hardware

This is Antonio's decision and house rules, not a fallback dispatch. An operator
could provision a dedicated delegated parent and isolated identities/storage,
but the configuration, maintenance and risk become their responsibility.
Self-hosted does not imply a fresh destroyed VM per job. Persistent disks,
management credentials, other workloads, network and cleanup failure carry
extra risk. Server `.16` stays paused; no use or policy change follows here.
A separate approved setup/security contract and verified authority are required.

### C. What remains possible without a private cgroup

| Gate | Progress without private cgroup | What cannot be credited from current evidence |
| --- | --- | --- |
| runner_identity | Exact SHA/image/hosted consistency checks | Independent hypervisor attestation |
| payload_credentials | No explicit secrets, empty launch env, nonpersisted checkout | Complete runner/control-plane credential separation after escape |
| host_content | Public closure inventory and restricted filesystem design | Enforced complete reachability boundary from directory allowlists |
| host_fds | Known FD identity/closure and future after-exec inventory | All reopenable/same-UID channels or QEMU device identities |
| egress | Trusted seccomp transport denials, potential separately reviewed non-cgroup mechanisms | QEMU-compatible adversarial default-deny including local management channels |
| resources | Per-process ceilings/consumption fixtures; possible proven no-multiplication policy | Aggregate CPU/memory/PID limits for threaded QEMU or arbitrary descendants from rlimits alone |
| process_cleanup | Stable known-child reap, fixed no-descendant helper | Escaped sessions/all descendants after supervisor death or cancellation from killpg/pidfd alone |
| workspace_cleanup | Owned paths and descriptors, possible independent storage/FS restriction | Aggregate storage/entry enforcement, all permitted outputs and adversarial cleanup after loss |
| provider_lifecycle | Documented disposable standard VM plus externally read job conclusion | Locally measured destruction or local cleanup proof after runner termination |

Cgroup is a candidate mechanism, not the definition of proof: a genuinely
proven equivalent bound/no-multiplication and containment model could close a
scoped gate after separate design and tests. The tiny current filter cannot
provide it for QEMU because exec/clones are denied and QEMU compatibility is
unproved. QEMU/provider VM boundary may limit host escape consequences to the
job, but does not erase in-job credential/network/resource/cleanup gates.
Accepting provider lifecycle as a substitute for local cleanup would change
the contract and residual risk; only the owner can make that decision. Do not
mark the existing gates passed by narrowing them silently.

## Next decision, no execution implied

Keep the test design blocked on operator grant. Prepare an executable test only
when a dedicated parent exists and its authorized scope/identity can be checked.
Otherwise present the three route choices and their unresolved risks to the
owner through the parent. No hardware purchase, self-hosted provisioning,
privileged hosted setup, new observation or contract relaxation is part of this
patch. Classification remains blocked and isolation unaccredited.

Sources read:
- cgroup delegation, no-internal-process constraint, controllers and kill: https://docs.kernel.org/admin-guide/cgroup-v2.html
- Hosted VM lifecycle and documented passwordless sudo (not permission): https://docs.github.com/en/actions/reference/runners/github-hosted-runners
- Self-hosted management responsibilities: https://docs.github.com/en/actions/concepts/runners/self-hosted-runners

- QEMU security boundary limits: https://www.qemu.org/docs/master/system/security.html
- Per-process resource semantics: https://www.man7.org/linux/man-pages/man2/setrlimit.2.html
- Known process handles, not all-descendant containment: https://www.man7.org/linux/man-pages/man2/pidfd_open.2.html
