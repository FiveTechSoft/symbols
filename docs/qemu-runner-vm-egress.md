# Runner VM trusted local egress experiment v1, not enforcement accreditation

## Scope and anchor

Repository master was read live at f4772a853907b8fb62e7c52ae60f651b5695db54. Harness #2 measured the trusted child successfully; the parent still returned blocked/capabilities_only_not_enforcement. This design extends that experiment. It does not change the QEMU policy, boot anything, use the server, or promote any aggregate gate.

The only proposed successful network traffic is between owned processes on explicit numeric loopback addresses 127.0.0.1 and ::1. No wildcard bind, public address, external resolver, real hostname, interface discovery, proxy, credentials, external service, or resolver configuration read. No DNS library call. Workflow checkout/upload are management traffic, not payload traffic and not proved inaccessible to payloads.

Implementation is prepared for byte-review only. No native helper or local fixture has been executed; a remote observation requires separate approval.

## Architecture and fixed limits

1. Keep the measured helper's non-root checks, empty environment, native x86-64 filter, wrong-ABI kill, no_new_privs, five hard/soft limits, stdout-only write policy, close_range(3..UINT_MAX), and no-descendant policy unchanged.
2. Add a trusted local fixture owner in the supervisor, not a server subprocess. Use nonblocking sockets and a selector, not threads or shell. Bind four sockets: TCP/UDP for IPv4/IPv6; TCP backlog1, IPv6 IPV6_V6ONLY=1. Numeric loopback and OS-selected ephemeral ports only. All sockets non-inheritable by default. Failure of IPv6 or any baseline is a refusal, never a skipped success.
3. Run a fixed trusted positive-control client BEFORE the confined child, with the same compiler, numeric destination construction, protocol probes and inherited-descriptor setup. It is a distinct executable/mode with no seccomp, not a fallback for failed filtered setup. No arbitrary argv, paths or addresses. Validate its fixed output and exit0. This establishes that the local fixtures and individual operations actually work.
4. Run the confined child with the reviewed filter unchanged. Every negative probe requires -1/EPERM. Before any negative, filter installation/readback must succeed. A setup failure cannot turn into an unfiltered negative run.
5. Budget proposal: at most four listening sockets, one accepted connection at a time, and two prepared inherited sockets; sequential clients; at most64 payload bytes per exchange, at most512 received fixture bytes for the whole run; each child3s wall +2s reap, whole observer15s wall, outer workflow30s +5s kill grace. Child output1024B; fixed supervisor report8192B. Stop immediately on unexpected data, address, result, output overflow or timeout. These numbers require implementation tests before approval and are not measured QEMU budgets.

The fixture owner is unrestricted and trusted. Its traffic does not prove payload containment or control-plane isolation. Successful baselines are intentionally a new loopback-only scope compared with the original no-successful-network harness.

## Positive and negative matrix

For every socket class below, use fresh owned state. Tests report booleans/closed reasons, never addresses, ports, packets, process IDs or environment.

| Class | Positive control before confinement | Confined sample, unchanged policy |
|---|---|---|
| TCP IPv4 | socket(AF_INET, SOCK_STREAM), connect to owned listener, send fixed marker, receiver observes exact marker | socket creation and connect syscall each return EPERM |
| TCP IPv6 | Same on ::1, explicitly v6-only listener | AF_INET6 socket and connect return EPERM |
| UDP IPv4 | socket + sendto fixed marker; local receiver verifies exact marker | socket, sendto, sendmsg each EPERM |
| UDP IPv6 | Same on ::1 | socket, sendto, sendmsg each EPERM |
| Synthetic DNS/UDP v4 and v6 | Send one fixed non-sensitive DNS wire-format question to owned UDP fixture on ephemeral port. Fixture verifies exact bytes and emits one fixed response | Same attempted sendto/sendmsg blocked EPERM. Fixture observes no negative-phase bytes |
| Synthetic DNS/TCP v4 and v6 | Same fixed question with two-byte length framing on owned TCP listener | connect/sendmsg blocked EPERM. No negative-phase accept/data |
| Connected inherited TCP/UDP | Supervisor prepares two owned connected endpoints, deliberately passes ONLY their two exact descriptors to the positive control; control uses sendmsg and write and fixture receives fixed bytes | Supervisor passes same kind of endpoints ONLY to the trusted confined helper. Before filter, helper closes FD3+ and verifies both are EBADF with fcntl(F_GETFD). After filter, sendmsg/sendto/write on those descriptor numbers must be EPERM; fixture receives zero negative-phase bytes |
| Process creation | Separate trusted control forks ONE child, waits and reaps it; fixed exit0. No socket FDs available to this child | fork/vfork/clone/clone3/execve syscall samples EPERM. No process is created by negative probes |

The existing policy denies most calls regardless of descriptor validity. Therefore EPERM on a closed inherited descriptor does NOT prove closure: the separate pre-filter EBADF checks establish closure for the two known FDs. Conversely, zero packets alone does not prove syscall denial. Both observations are required.

The inherited-FD test never retains a socket across the installed filter to make the test easier. That would change the measured boundary. The matrix tests closure plus negative policy samples separately. It does not claim all possible ambient descriptor identities have been audited.

Synthetic DNS is a transport sample, not a test of libc getaddrinfo, NSS, mDNS, DoH, DoT or an actual resolver. No claim of complete DNS coverage. Unix-domain sockets, local proxies, shared memory, inherited stdout as a channel, ptrace/IPC escape paths and management credentials remain unresolved. Defaults deny unsampled syscalls, but these experiments do not prove a complete adversarial egress boundary.

## Artifact-based cleanup evidence, without logs

Use three distinct fixed outputs, all uploaded with if: always(), one-day retention and if-no-files-found:error. No raw logs, packet capture, process listing or host paths. Every producer uses umask077; every report has an exact schema, closed enum values, exact booleans and no extra keys.

1. Guard artifact: retain the existing one-line guard token mechanism unchanged.
2. Observation artifact: parent classification blocked, child/fixture observations measured_only only when their exact checks pass; four aggregate gates remain not_proven; boot_attempted/runtime_complete/isolation_accredited false. Closed refusal on all unknowns.
3. Cleanup artifact: a separate finalizer step after the observation, also if: always(). It records outcomes for each specifically owned resource, not a blanket cleaned=true. Missing evidence is not_proven, never converted to success.

Owned-resource sequence:

A. Supervisor starts with a fixed per-run scratch directory under RUNNER_TEMP, creates no other output locations, and records only a closed phase marker. Cleanup evidence starts as incomplete before launching any child.
B. Close all prepared inherited sockets in the parent immediately after child spawn. In finally, kill if needed and reap each known direct child with a stable unreaped PID or pidfd. Report reaped/not_proven, never an arbitrary PID. No scan or kill of unrelated processes.
C. Stop fixtures, close listener/accepted/UDP descriptors and confirm each local socket object is closed. No bind-retry/reconnect after cleanup and no port probes of unrelated services. This establishes descriptor release, not absence of every host process.
D. Only after reaping the known children, remove the fixed binaries and owned scratch files by an explicit allowlist. Do not recursively follow links. Check lstat absence of each exact owned path and the empty owned directory, then remove the directory. A symlink/type mismatch, unexpected filename or deletion failure is cleanup_refusal.
E. Finalizer verifies the expected supervisor cleanup result exists and is schema-valid, removes any still-existing fixed binaries/allowed scratch remnants, checks absence and writes its own fixed cleanup JSON. It must not infer reaping from missing files. If supervisor evidence is absent, record process cleanup not_proven even when deletion succeeds.
F. Upload observation and cleanup only after these checks. Outputs for upload are fixed reports outside the removed scratch boundary, explicitly excluded from the deletion list. Read actual artifacts/digests plus external final run conclusion; reports are not evidence of provider VM destruction.

Mocks cover timeout/output/spawn/schema/cleanup failures. Real cancellation remains untested and requires separate run review.

A workflow cancellation, SIGKILL of the supervisor, runner death or upload interruption may prevent finally/always from completing. In those cases an absent/stale artifact is not_proven, never claimed clean. Artifact always improves observability; it is not a reliable all-cancellation cleanup boundary. Escaped sessions and arbitrary descendants remain untested and forbidden by the confined policy. The trusted positive fork is single-child only, not a descendant-scope test.

## Implementation review notes

The implementation adds new egress files/workflow and does not change the previously measured harness. The seccomp byte block and numerical limit-setting lines are compared by an offline test. The positive control sends fixed LOCL and empty-name DNS-format question bytes; fixture acknowledgment is one fixed R byte, not a DNS server implementation. DNS results measure transport receipt and denial, not DNS resolver semantics.

Inherited endpoints are two explicitly passed FDs below16, checked as sockets before close_range, then checked EBADF before filtering. The pre-filter check establishes identity only as a socket for these owned endpoints; it is not an audit of ambient host descriptor identities. The supervisor creates connected TCP/UDP endpoints without sending application data. For the negative phase it accepts the owned TCP connection itself before setting the fixture selector to observe probe data; no unrelated listener or network target is involved.

The negative phase has one bounded100ms final selector observation to catch queued local data. Absence in that window is a sample, not a claim of permanent/exhaustive egress absence. No repeated account/site checking is involved.

The finalizer is embedded directly in the new workflow so a pre-checkout refusal can still remove only the fixed known resources and emit cleanup evidence. Missing supervisor evidence leaves direct_children and fixture_descriptors not_proven. The observation upload may error when a guard refusal produced no observation JSON; that is an expected artifact absence, never a helper success. The guard token and cleanup artifact still upload with always.

Trusted supervisor handles SIGTERM/SIGINT with a closed refusal and finally cleanup, but that branch is not remote-tested by this tranche. SIGKILL/cancellation/runner death remain not_proven; the workflow's always steps cannot guarantee completion. The positive control has one trusted fork that immediately exits and is waited; a killed control cannot be used as proof of no orphaned descendants. No aggregate cleanup claim results.

Before running: bootstrap workflow and code patches independently, read back exact diffs, require CI/Pages/bank per review, measure a fresh ImageVersion, freeze live master and authorize one observation. Compile-only and mocks do not prove that the new experiment runs successfully.
