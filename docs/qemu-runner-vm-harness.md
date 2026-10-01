# Confined trusted helper capability gate, no workload or boot

This gate tests a tiny trusted C helper only, not QEMU, a guest or candidate
code. No public endpoint, network connection, credential access, arbitrary
command, fork baseline or child tree is created. Socket/process syscalls are
attempted only after a filter installation succeeded, and must return EPERM.
Setup refusal exits before those attempts. No sudo, namespace, global policy,
cgroup mutation or server `.16` operation occurs.

## Child policy

The workflow compiles the exact reviewed C helper. The supervisor launches its
fixed runner-temp path with empty env, stdin/stderr `/dev/null`, stdout owned
pipe, close_fds and no shell. The helper is single-threaded. Non-root check,
close_range(3..UINT_MAX), hard AND soft limits, no_new_privs set/readback and
seccomp installation must succeed, or it refuses without a positive report.

Numerical test budgets: virtual AS 64 MiB, CPU 1 second, per-file FSIZE 1024
bytes, NOFILE 16, CORE zero, applied to helper only. All five hard/soft values
are read back after filtering; raising both soft and hard limits by one for each of AS, CPU, FSIZE, NOFILE
and CORE must fail EPERM, followed by unchanged hard/soft readback.
These are trusted-helper budgets, not chosen QEMU budgets. No memory/CPU/file
stress or aggregate resource bound is proved by this patch.

The classic BPF policy accepts only x86-64 native ABI, kills wrong architecture
or x32 bit, and defaults to EPERM. Allowlist is exit, exit_group, getpid,
getrlimit, setrlimit and write ONLY to FD1 with high argument bits zero. Every
other syscall is refused, including fork/vfork/clone/clone3/exec/open/ptrace
and io_uring setup. No thread or descendant is allowed. Hard rlimits cannot be
raised by the non-root helper; permitting setrlimit allows only equivalent or
more restrictive values within hard ceilings. No return to libc exit handlers
is needed: raw exit_group finishes after one fixed JSON write.

Checks: allowed getpid succeeds; nine denied syscall classes return EPERM;
all hard/soft limits read back; all five ceiling raises denied. This does not make nine
negative samples an exhaustive egress test. Inherited FDs are closed except
reviewed stdio; stdout remains a parent pipe and the child can still write to
it. No local baseline listener, successful socket, QEMU exec, file-output
violation, Landlock policy or management-plane isolation is tested.

## Parent and failure bounds

Parent is not filtered or resource-limited. It reads at most1024 child output
bytes under a3-second monotonic wall deadline, kills and reaps the direct
child on timeout/output overflow, with2-second reap bound. pidfd is used for
SIGKILL when available; otherwise an unreaped direct-child PID cannot be
recycled. Failure to establish/use a handle, reap, parse exact schema/types or
complete setup remains a closed refusal. No raw stderr/path/env/errno string
or failed values leave the observer.

The helper cannot create descendants after filter; before filter it is trusted
single-thread code with no process creation. This is not cleanup proof for an
untrusted QEMU process tree. Parent cancellation/crash and an uninterruptible
child are not proven clean. The outer20-second timeout plus5-second kill grace
exceeds normal inner3+2-second budget, but external cancellation can interrupt
supervision; do not call the workflow timeout a process-scope guarantee.

Fixed binary is removed in an always step; this benign deletion is not
adversarial workspace cleanup. Artifact is fixed JSON only, one-day retention.
A child success is `measured_only`; parent still reports `blocked`, all egress,
resources, process_cleanup, workspace_cleanup gates `not_proven`, all
boot/runtime/isolation flags false and exit2. Unknown output blocks. No boot
may be chained from this gate.

## Execution gates and next decisions

Separate workflow bootstrap and code patch byte-reviews, CI/readbacks and fresh
ImageVersion are required before one separately approved observation. Helper
compile checks and mocked parser/supervisor tests are offline, not capability
measurements. x86-64-only is deliberate; unsupported syscall/kernel/ABI refuses.
Any new red or drift stops without rerun.

If this passes, next gates must independently prove controlled local egress
baselines/denials, FD identity, hard-limit violations and exact QEMU thread/exec
compatibility. This policy deliberately denies exec and all clone operations;
it cannot launch QEMU unchanged and is not a proposed QEMU syscall policy.
Resources and cleanup aggregate gates stay not_proven until a scope or proven
equivalent no-multiplication policy exists. Prior credential/control-plane and
host-content gaps remain. Server `.16` remains paused.

Sources: https://www.man7.org/linux/man-pages/man2/seccomp.2.html ;
https://www.man7.org/linux/man-pages/man2/PR_SET_NO_NEW_PRIVS.2const.html ;
https://www.man7.org/linux/man-pages/man2/setrlimit.2.html ;
https://www.man7.org/linux/man-pages/man2/pidfd_open.2.html .


## Ceiling-raise tranche, not resource stress

The child v2 schema requires `all_five_ceiling_raises_denied:true` instead of
v1's single NOFILE `hard_raise_denied`. Parent v2 emits five closed
`ceiling_raise_denials` rows and `consumption_violations:not_tested`. An old
schema, duplicate/extra key or wrong type refuses. Only exact child exit0 and
schema validation can set these rows to measured_only. Aggregate gates and
boot/runtime/isolation flags remain unchanged and blocked.

Each attempted pair is the reviewed ceiling plus one: AS 67108865 bytes,
CPU 2 seconds, FSIZE 1025 bytes, NOFILE 17 descriptors and CORE 1 byte. The
original ceilings remain AS 67108864, CPU 1, FSIZE 1024, NOFILE 16, CORE 0.
The same five limits are read back after each EPERM. This tests inability to
raise hard ceilings, not memory allocation failure, CPU exhaustion/signals,
file-size overrun, FD exhaustion or core-dump output. No mapping, file open,
process multiplication, stress loop or relaxed syscall policy is introduced.

The existing native x86-64 seccomp block is byte-compared against the reviewed
egress child by an offline test. All limits, supervisor deadlines, output
bounds and launch mechanics are unchanged. This policy still denies exec and
all clones and is not QEMU-compatible. Real consumption tests need a separate
helper/policy review. A passing ceiling test never promotes resources or
cleanup to aggregate proof.
