# Runner B namespace diagnostic, no boot

> Historical namespace contract. The runner direction is superseded by
> [the disposable-VM contract](qemu-runner-vm-contract.md). This diagnostic
> remains no-boot and does not accredit that revised boundary. Server `.16`
> remains paused.

This manual, non-root, no-boot diagnostic localizes the `namespace_setup`
refusal from runner B #1. It runs only on `ubuntu-24.04` with exact reviewed
master SHA and ImageVersion; both are checked before checkout and again by the
script. No server `.16` access, QEMU package/boot, candidate C, credentials,
root, policy/sysctl change, or network request occurs in the diagnostic. The
report contains a closed phase and errno vocabulary, never raw stderr, host
paths, namespace IDs, or environment. Artifact retention is one day.

The sequence stops on its first failure: non-root `unshare --user
--map-root-user` with `true`, then user+mount with `true`, then
user/mount/net/pid/ipc/uts with fork. The final child, only after a full
namespace transition, attempts private mount propagation, bounded tmpfs and
proc mounts under a temporary directory and cleans them up. Each subprocess has an 8-second timeout; an expired process group is killed
and reaped before temp cleanup. The outer 180-second limit exceeds the
worst bounded sequential phase budget and should never cut across that cleanup. A failed or
timed-out subprocess reports its phase and a closed errno class. A cleanup
failure blocks regardless of earlier results. `measured_only` on all phases
would still not accredit confinement or runtime coverage; a failure cannot be
attributed to AppArmor alone from errno.

Runner B #1 failed at `namespace_setup` after passing identity and offline
checks: https://github.com/FiveTechSoft/symbols/actions/runs/36655850019/job/109699905273 .
The hosted Ubuntu 24.04 image used in that run was `20260920.314.1`, as
measured separately in https://github.com/FiveTechSoft/symbols/actions/runs/36655766697/job/109699663969 .
No rerun of B #1 is justified by this diagnostic patch. Dispatch this new
workflow only after a separate exact-byte review, master readback, full CI and
fresh ImageVersion measurement; stop on any drift or red.

A standard GitHub-hosted `ubuntu-24.04` job is a new VM, excluding the
single-CPU `ubuntu-slim` runner: https://docs.github.com/en/actions/reference/runners/github-hosted-runners .
This external job boundary does not synthesize restricted `/proc`, `/sys`,
`/etc`, `/dev` views for QEMU. A guest escape into QEMU executes in the host
job context; QEMU's own security model calls for least privilege and host
confinement: https://www.qemu.org/docs/master/system/security.html .
The VM boundary could be a separate accepted risk posture for a disposable,
secret-free job, but did not meet the original runner B namespace
criterion or the separate server `.16` goal. The runner-only revision now lives
in `qemu-runner-vm-contract.md`; it requires independent host-FD/content,
egress, process, resource and cleanup tests before any workload.
Do not disable global AppArmor/userns restrictions, use sudo for QEMU, or
call a successful diagnostic an isolation proof.
