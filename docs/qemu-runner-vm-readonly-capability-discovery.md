# Read-only capability discovery plan, not an enforcement test

Base b438ffad014abd8cd8df5871ba9c90be343b8993. This is step 2 preparation for
workload-resource design B, documentation only. No implementation, observation,
boot, scope creation, controller write, storage write, quota syscall, privilege
change, mount, process launch or foreign-process enumeration occurs here.
Server `.16` stays paused; classification blocked and isolation unaccredited.

## What read-only discovery can and cannot answer

It can identify narrowly anchored candidate interfaces, reported access,
controller configuration, filesystem type and this observer's own identity.
It cannot prove exclusive delegation, effective enforcement, migration
resistance, launch placement, payload/controller separation, storage quota or
entry cap. An accessible interface is a candidate, not a passed gate. Even all
metadata checks passing leads to `candidate_requires_active_review`, never
`available_enforced`. A negative check concerns only the inspected anchors,
not every possible runner configuration or alternative scope.

No global search for a usable cgroup, filesystem, quota or alternate identity.
Use the current observer's own membership plus at most one candidate root
explicitly supplied by a separately verified operator configuration. Do not
accept a path or approval from environment/file claims alone. An operator's
claim of ownership/delegation is provenance to review, not a measurement.

## Fixed preflight and read bounds for a future implementation

Before reading: exact workflow and checkout SHA, canonical fresh image-set
membership, hosted Linux x86-64 identity and non-root, no secret inputs,
permissions empty, persisted checkout credentials disabled. No credential
values, arbitrary host files or full environment dumps. This is not a claim
that the management plane is secret-free.

A future reviewed observer would have 10 s whole wall deadline, no child,
no retries, at most 64 fixed metadata operations and 32 KiB total input.
Each text file read is capped at 4096 bytes (4097 detects overflow), no recursive
crawl or unbounded line parsing. Paths internally capped at 4096 bytes and
64 components; reject traversal, malformed/ambiguous membership, symlinks and
unsupported file types. Stop rather than expand limits. Closed enums/booleans
only in output, no paths, UID/GID numbers, PIDs, controller membership strings,
process names, mount source names, environment or raw errors. A resource whose
facts cannot fit the bounds is unknown, not silently truncated.

Use anchored directory FDs and no-follow metadata operations, preferably
openat2 resolution constraints where available. Metadata races still need an
explicit `unstable` outcome. No mutation is used to test access. access/faccessat
is a reported permission check, not proof that a later write would succeed.
Read-only opens must target only approved metadata interfaces, never control
sockets, device nodes, pipes or general writable report locations.

## A. Private cgroup v2 candidate

1. Read only `/proc/self/cgroup`, bounded. Require one canonical v2 membership
   line; resolve within `/sys/fs/cgroup` through no-follow anchors. Consult
   bounded `/proc/self/mountinfo` only to verify that this exact anchor is a
   cgroup2 mount and the observer's visible mount is not ambiguous. Do not
   output mount paths/source text. If mountinfo exceeds bounds, stop as unknown.
2. Inspect the current anchored directory and, if separately provided, one
   owned candidate directory. No sibling listing, ancestor probing for more
   writable scopes or namespace discovery. Component metadata is for safe
   traversal only, not a survey of their controller permissions.
3. Allowlist metadata/text reads: cgroup.type, cgroup.controllers,
   cgroup.subtree_control, cgroup.events, memory.max, memory.swap.max,
   memory.oom.group, pids.max, cpu.max. Parse only domain/threaded classification,
   required controller membership (memory/pids/cpu), populated boolean,
   finite/unlimited/unknown values and configured candidate limits. Do not
   read cgroup.procs or cgroup.threads contents: these enumerate tasks.
4. For candidate directory and cgroup.procs/cgroup.threads/
   cgroup.subtree_control/cgroup.kill, inspect only file type, relative owner
   match and reported read/write/search permission. cgroup.kill is write-only;
   do not open/read it to infer support. For memory/pids/cpu controls, record
   existence/readability and reported writability at the candidate, without
   asserting parent resource controls should be delegated writable.
5. Optionally read bounded `/sys/kernel/cgroup/delegate` only if this exact
   kernel metadata anchor is predeclared. It lists delegation-eligible files,
   not a grant to this user. No cgroup namespace creation or nsdelegate remount.

Closed findings: `v2_candidate`, `not_v2`, `domain_candidate`,
`threaded_incompatible`, `required_controller_missing`, `reported_access`,
`reported_no_access`, `operator_scope_unverified`, `unstable`, `unknown`.
Keep separate facts per interface. No single writable flag becomes delegated.
A populated candidate does not become ours; zero populated does not establish
ownership. Private/exclusive ownership and availability to create a child
remain unresolved until authority and active setup checks are reviewed.

A later separately approved benign test would need owned child creation,
limit readbacks, actual race-free placement, migration denial, scope kill,
populated=0 and removal. None follows from this read-only plan. No sudo,
userns, arbitrary systemd request, cgroup write or root fallback.

## B. Storage bytes and entry enforcement candidate

Inspect only the current `RUNNER_TEMP` directory and at most one separately
verified proposed private scratch root. Verify canonical no-follow directory
anchors and relation to the current observer's trusted configuration. Do not
list existing contents or inspect user/home/workspace files. No temp marker,
file creation, write attempt, deletion, recursive scan or mount operation.

Use statfs/statvfs on the opened anchor for filesystem class and metadata
availability. Bounded own mountinfo may provide fixed option predicates such
as read-only/read-write, tmpfs-like, or quota-option-present. Never output full
mount options or source identifiers. Report root ownership relation and
reported search/write access independently. Do not infer private ownership
from a path under RUNNER_TEMP or current user ownership alone.

What these facts mean:

- Free blocks/inodes are shared capacity, not a reservation or per-payload cap.
  Avoid emitting numeric ambient capacity as a proposed budget.
- tmpfs type or size option does not prove a private mount, memory accounting,
  immutable size or inode/entry quota available to an unprivileged payload.
- A project/user quota mount option does not prove a configured project ID,
  applicable byte/inode limits, immutable identity, inherited project policy
  or authority to administer it. Do not call quotactl/ioctl under this plan.
- A directory mode does not prevent same-UID payload access outside it or
  controller access to payload-created links. Hardlinks, mount boundaries and
  descriptor identities need their own future policy and race tests.
- RLIMIT_FSIZE limits one file, not total bytes or number of files. FD limits
  do not cap directory entries. A scanner is not enforcement.

Closed findings: `filesystem_candidate`, `quota_option_candidate`,
`tmpfs_candidate`, `ownership_unverified`, `no_enforcement_metadata`,
`unstable`, `unknown`. Byte quota and entry quota each remain
`enforcement_not_proven`; no `storage_available` conclusion.

A later setup plan must select a real private storage/entry facility, specify
who configures it and verify authority. If none is available without new
privileges, block and propose separate setup, not tmpfs mount or quota writes
silently. Fixed trusted below/above-byte and below/above-entry tests require a
separate helper/policy and one reviewed observation each.

## C. Payload/controller separation candidate

Only inspect the observer itself: own effective/real UID/GID relationship,
own supplementary-group classification, own capability masks, no_new_privs
and seccomp mode. `/proc/self/status` may be read bounded and parsed for those
specific fields only; discard unrelated fields and never emit raw masks,
numbers, namespace IDs or names. No passwd/NSS lookup, credential values,
foreign process status/environ/cmdline/FDs or ptrace attempt. Do not scan
sockets or enumerate runner/controller processes.

No payload exists in this read-only tranche. Therefore report
`payload_identity:not_instantiated`, `controller_identity:self_only`,
`separation:not_proven`. A non-root UID, absent capabilities, no_new_privs or
seccomp mode alone cannot prove separation from same-UID runner management.
A sanitized environment and closed launch FDs also do not prove prevention of
reopening files/sockets or reading another process's accessible credentials.

For a later operator-supplied dedicated identity or directory, record only
that a candidate configuration exists and whether this observer's anchor
metadata matches the reviewed configuration. Do not test setuid, impersonate
an identity, start a process as it or request service managers to do so.
Mount/LSM policy presence does not prove the future payload is attached to it.
Landlock candidate feasibility does not create a synthetic root or complete
credential boundary. Payload-visible controller handles and migration paths
must be denied by separately reviewed implementation and negative tests.

## Report and ledger requirements

A future observer schema must keep facts distinct from conclusions:

- Scope: own membership observed, optional reviewed candidate inspected,
  metadata prerequisites by interface, delegation/enforcement `not_proven`.
- Storage: filesystem/option predicates and ownership relation, aggregate
  bytes and entries enforcement `not_proven` separately.
- Separation: self-only identity observations; payload uninstantiated and
  controller separation `not_proven`.
- Global result: classification blocked; isolation_accredited false;
  boot_attempted/runtime_complete false; all nine gates still open except
  precisely referenced prior evidence, with no promotion by this observation.

Use exact schema/types, duplicate-key rejection, closed enums and explicit
unknown/unstable states. Report a lack of inspected candidates, not a global
absence of capability. Missing artifact means unavailable proof. Store exact
SHA/image/source scope, input bounds, digests, final external job outcome and
metadata-only caveats in the ledger. Never put a writable host path or raw
identity into uploaded diagnostics.

## Review and next decision

This patch only delivers the plan. Subsequent stages require new review:

1. Exact observer/workflow diff and offline fault/bounds tests, with no writes
   or native helper execution. Review every metadata path and output field.
2. One separately approved read-only observation on a fresh frozen image set,
   exact master, byte readback/CI/ledger; unknown red or drift stops, no retry.
3. If candidate prerequisites are insufficient, stop before creating anything
   and report the missing setup authority/capability. If they look sufficient,
   prepare a separate benign active setup design, not a permission to execute.
4. Only after real delegation/storage/separation evidence can consumption and
   cleanup fixtures be designed for execution. QEMU boot/workload still needs
   independent approval and all applicable gates.

The read-only result cannot close resources or cleanup. Its purpose is to
choose the smallest legitimate next setup test and identify blockers without
touching the runner's shared state.

## Sources read for semantics

- cgroup v2 delegation/interfaces: https://docs.kernel.org/admin-guide/cgroup-v2.html
- Filesystem statistics versus policy: https://www.man7.org/linux/man-pages/man2/statfs.2.html
- Filesystem capacity/inodes: https://www.man7.org/linux/man-pages/man2/statvfs.2.html
- Own process status: https://www.man7.org/linux/man-pages/man5/proc_pid_status.5.html
- Capability semantics: https://www.man7.org/linux/man-pages/man2/capget.2.html
- Anchored pathname resolution: https://www.man7.org/linux/man-pages/man2/openat2.2.html
- Landlock scope/limitations: https://docs.kernel.org/userspace-api/landlock.html

These sources define semantics, not current runner grants or availability.
