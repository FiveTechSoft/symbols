# Revised goal B: disposable hosted job VM, not inner QEMU confinement

This revision selects the GitHub-hosted `ubuntu-24.04` disposable VM as the
external containment boundary for a future QEMU collection job. It supersedes
the runner B namespace requirement, not the separate server `.16` contract.
The server remains paused. The original userns diagnostics remain historical
evidence; neither is a prerequisite to relax global host policy or use root.

## Accepted boundary and residual risk

GitHub documents a new VM for each standard hosted job, excluding single-CPU
`ubuntu-slim`. The VM contains the entire job, including management actions.
This is a provider assertion, not a locally measured destruction guarantee.
A guest-to-QEMU escape can reach the job's accessible files, processes, devices
and network. This contract does not promise synthetic `/proc`, `/sys`, `/etc`
or `/dev` views for QEMU, or defense against such an escape inside the job.
No collection run, TCG/KVM mode, closure or isolation is accredited here.

The planned job has `permissions: {}`, checkout `persist-credentials: false`,
no user/repository secrets passed to its payload, no private input, cache
restore/save or guest-controlled artifact upload. These are requirements,
not findings about an existing workflow. They do not prove the entire VM
has no secrets: Actions exposes automatic authentication to actions and has
a runner control plane. Credential exposure to escaped payload code is an
unresolved gate, not excused by an empty permissions map. No full environment,
credential value or arbitrary host contents may be logged to investigate it.

## Gates required before any workload

Every gate needs an exact target SHA, measured ImageVersion, reviewed workload
closure and explicit evidence references. Unknown, incomplete or conflicting
evidence blocks. Configuration flags are not enforcement measurements.

| Gate token | Required evidence and limitation |
| --- | --- |
| `runner_identity` | Exact master SHA and ImageVersion, label `ubuntu-24.04`, GitHub-hosted identity checked before checkout and launch. Label/environment checks do not independently attest the hypervisor. |
| `payload_credentials` | Reviewed workflow/action closure with no secret inputs, empty token permissions and no persisted checkout credentials; sanitized payload environment. Resolve runner/control-plane credential reachability before treating an escape as acceptable. Never scan or upload secret values. |
| `host_content` | Reviewed public/non-sensitive job inputs and writable locations. Measure the payload's accessible host-content classes, including checkout, temp, home, process/proc interfaces, sockets and devices. A directory allowlist is not a filesystem restriction. Unexpected or unclassified contents block. |
| `host_fds` | Final launch descriptor inventory using identities/types, not just FD numbers; close inherited FDs except reviewed stdio and mode-specific device FDs. Measure the child after exec. Other same-UID processes and reopenable sockets are separate host-content risks. |
| `egress` | Default-deny enforcement covering the payload and descendants, with controlled positive/negative tests for IPv4/IPv6, DNS and reachable local proxy/socket paths. QEMU `-nic none` alone does not restrict an escaped QEMU process. Management/artifact egress must not be usable by the payload. No public target probing. |
| `resources` | Explicit reviewed CPU/wall, aggregate memory, PID count, file/disk/output and FD budgets. Enforce and test limits across descendants. Per-process rlimits are not aggregate cgroup proof; UID-wide NPROC can include unrelated runner processes. OOM/timeout/unknown kill state blocks. |
| `process_cleanup` | Own the child process group and reap; independently check all payload descendants, including session escape, after success/failure/timeout. killpg alone is insufficient. A reviewed bounded supervisor and enforceable process scope are needed. |
| `workspace_cleanup` | Enumerated scratch locations, no mounts or host policy changes, no cache or private outputs. Check removal and absence of payload processes before report/upload. Cleanup on cancellation and supervisor failure needs separate evidence; a finally block is not proof. |
| `provider_lifecycle` | Review the provider's documented per-job VM lifecycle and read final job conclusion externally, including post steps. The in-job observer cannot measure destruction after its own death. Keep provider reliance distinct from local cleanup. |

No operational gate is implemented by this patch. Non-root egress enforcement,
aggregate resource/process scope and control-plane credential exposure may
remain unavailable on this runner. That would be a blocker for running the
workload, not permission to use sudo, change global firewall/AppArmor/sysctl
policy, or silently narrow the tests. Future privileged setup, if proposed,
requires a separately reviewed plan; QEMU itself must remain non-root.

## First tranche: offline contract lock

`tools/qemu_closure/runner_vm_contract.json` locks the boundary, limitations,
paused server and full gate list. `runner_vm_contract.py` rejects unknown keys,
values, types, duplicate keys, malformed or oversized inputs. Its output uses
only closed tokens. A valid contract produces `contract_valid:true` but still
`classification:blocked`, reason `evidence_not_collected`, all gates
`not_collected`, and `boot_attempted:false`, `runtime_complete:false`,
`isolation_accredited:false`. It always exits 2, so no workflow can use a
syntax check as authority to boot. Tests run offline and are not gate evidence.

No executable workflow or dispatch is added in this tranche. Next: review the
feasibility of the unresolved gates, choose a mode and exact numerical budgets,
then prepare a separate no-boot measurement patch. Workflows require separate
bootstrap review. Every change retains exact-byte review, full CI and remote
readback; no probe follows from approval of this contract alone. Existing
collection #20 is not relabeled complete by the boundary revision.

## Sources

- Hosted VM boundary: https://docs.github.com/en/actions/reference/runners/github-hosted-runners
- QEMU least privilege and isolation model: https://www.qemu.org/docs/master/system/security.html
- Automatic token access and permissions: https://docs.github.com/en/actions/security-for-github-actions/security-guides/automatic-token-authentication
- Checkout credential persistence: https://github.com/actions/checkout
- Per-process resource limits: https://www.man7.org/linux/man-pages/man2/setrlimit.2.html
- Aggregate process/resource control: https://www.man7.org/linux/man-pages/man7/cgroups.7.html
- Descriptor close semantics: https://www.man7.org/linux/man-pages/man2/close_range.2.html
