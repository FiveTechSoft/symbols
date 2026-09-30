# Runner VM control gates: prerequisites first, no boot

This tranche measures prerequisites for the egress, resources and cleanup gates.
It does not implement or accredit enforcement. It adds no QEMU/guest/candidate
execution, network connection, listener, subprocess, mount, sudo, firewall,
seccomp filter, cgroup write or server `.16` action. A single temporary marker
is created and removed only in an owned scratch directory. Credential keys,
values and foreign process metadata are not read. The prior surface report's
Actions-context token presence and readable/searchable host surfaces remain
unresolved; this tranche does not erase those findings.

All reports have classification `blocked`, boot/runtime/isolation false, five
fixed gates `not_proven`, and exit 2. No caller can use successful prerequisite
observation as boot authority. Workflow stays red by design, with a fixed
one-day report artifact. Unknown results remain explicit, never promoted.

## Implemented observations

- Identity uses the prior exact SHA/ImageVersion and non-root hosted-context
  checker, with checkout HEAD checked separately by the workflow. A fresh image
  measurement is required before any run; the morning's value is not reused.
- `getrlimit` for AS, CPU, FSIZE, NOFILE, NPROC and CORE emits only
  `finite`, `zero`, `unlimited` or `unknown` for soft/hard values. Exact numbers
  are deliberately absent, so `finite` is not a satisfactory budget. No limit
  changes or limit-stress programs run. NPROC is real-UID-wide, not a private
  job process budget; memory/CPU rlimits are not aggregate descendant limits.
- A bounded 4096-byte read of this observer's `/proc/self/cgroup` identifies
  only a single v2 membership. No membership path is output. Traversal or
  symlink components refuse classification. lstat/access for six fixed control
  files reports possible write access, missing, unknown or unsupported type.
  No file contents are read or changed. These permission checks do not prove
  delegation, hierarchy availability, permitted writes or enforceability.
- Cleanup creates one empty owned temp directory and an exclusive no-follow
  marker with constant non-sensitive bytes, closes, unlinks and rmdirs it.
  Failures attempt only the known marker/directory cleanup, never recursive
  removal. Existing temp files are not enumerated or touched. Success means
  that marker was removed in this benign run, not that a payload could not
  race cleanup or create other files/processes. No descendants exist here.

Egress and process cleanup are `not_tested`. Provider lifecycle is still a
provider assertion, not observable VM destruction. Missing report, timeout,
identity/anchor error, unexpected exit or scratch failure blocks. The observer
has no children; its 20-second timeout plus 5-second grace cannot strand an
observer-launched child. It can still be interrupted while the marker exists;
that cancellation path is not certified. Symlink/path races are not a tested
adversarial boundary. Do not run this beside an untrusted payload.

## Separate enforcement design, not implemented or authorized here

| Gate | Candidate design and proof required |
| --- | --- |
| Egress | A separately reviewed child-launch mechanism must default-deny external IPv4/IPv6, DNS and local socket/proxy/management channels and apply to every descendant. Start with a trusted local harness: controlled endpoints and nonce-tagged success before restriction, denial after restriction, IPv4/IPv6 and TCP/UDP plus inherited/reopened socket cases. No public targets or credential endpoint requests. `-nic none`, absence of a route or failed one-off connect is not proof. Any proposed seccomp/other policy must cover inherited FDs and descriptor transfer, privileged helper execution and bypasses, not just new socket calls. Unsupported enforcement blocks; no root/global firewall fallback. |
| Resources | A private delegated scope needs aggregate memory/PID/CPU bounds and removal capability; validate delegation before writes. Choose explicit numerical budgets in a later patch tied to the exact mode/workload closure, not from observed ambient rlimits. Test benign bounded children below each budget and controlled violations above it, with independent counters/termination evidence and bounded output. Wall-time supervision, file/output/FD limits and storage quotas need their own measurements. A timeout or OOM without known descendant state blocks. |
| Process cleanup | A private enforceable scope must retain escaped-session descendants; a process group alone fails that test. Verify success, failed exec, timeout, signal/cancellation and session-escape cases; bounded supervisor must reap, terminate scope and independently establish emptiness. Identity/PID-reuse ambiguity or unkillable descendants blocks. Do not scan/kill unrelated runner processes. |
| Workspace cleanup | Enumerate owned scratch boundaries before workload; no mounts/global policy changes, caches or guest-controlled reports. Check only owned paths after process emptiness, test write failure and cleanup failure/cancellation. Prove no residual payload artifacts under permitted output locations. Benign marker removal is a prerequisite only. |
| Provider lifecycle | Read job final conclusion externally after post steps, and retain the documented provider boundary as an assertion. No in-job program proves destruction after its own termination. Platform credentials/actions remain management-plane risk. |

No active egress/resource stress or child-cleanup proof belongs to this patch.
The next decision is whether a private process/resource scope and non-root
network-denial mechanism can be built without the userns capability already
found absent. If not, B remains blocked; do not shrink the criterion to get a
passing report. No numerical budget, KVM/TCG mode, payload run or isolation
claim is selected by these observations.

## Application and run gates

Workflow-only bootstrap and code/docs/tests patches require separate byte
review, remote readbacks and full CI. Code reuses `runner_vm_surface.identity`
and `anchors`, so it must be applied only where that reviewed module exists.
After green CI and fresh ImageVersion measurement, request a separate review
for one prerequisite observation. Applying files authorizes no diagnostic run,
retry, boot or enforcement test. Server `.16` stays paused throughout.

Sources read for design semantics:
- cgroup v2 and delegation: https://www.man7.org/linux/man-pages/man7/cgroups.7.html
- rlimit scopes: https://www.man7.org/linux/man-pages/man2/setrlimit.2.html
- seccomp filter model: https://www.man7.org/linux/man-pages/man2/seccomp.2.html
- Landlock policy scope and limitations: https://docs.kernel.org/userspace-api/landlock.html
