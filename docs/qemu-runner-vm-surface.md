# Disposable VM surface observations, metadata only, no boot

This is the first observation tranche after the revised runner VM contract.
It does not run QEMU, a guest, candidate C, namespace/mount operations, sudo,
network probes, firewall changes or server `.16` operations. It never reads
credential values, file contents, foreign process environ/cmdline, or FD
symlink targets. Only fixed vocabulary leaves the observer. It cannot prove
absence of secrets, complete host-content reachability, identity of inherited
FD contents, or provider destruction. All four B gates remain `not_proven`,
classification `blocked`, boot/runtime/isolation false and exit code 2.

## Measurements and blind spots

- Identity: exact reviewed master SHA and freshly measured ImageVersion,
  github-hosted/Linux/X64 environment assertions and non-root. Workflow checks
  checkout HEAD independently. These are consistency checks, not hypervisor
  attestation. No output includes an unchecked identity string.
- Credentials: key-presence only for a fixed env allowlist. Even empty values
  count as present; absent keys prove nothing about files or other processes.
  A boolean `${{ github.token != '' }}` reports whether the automatic Actions
  context token exists, without passing that token to Python. Presence does
  not measure scopes/validity or prove that escaped payload code can obtain it.
  This distinguishes Actions context from the observer environment. Runtime,
  OIDC, auth-socket and management endpoint keys are treated as potential
  credential/control-plane surfaces, not value-bearing output.
- Contents: lstat and read/search access metadata for fixed workspace, temp,
  home, proc/sys/dev and known auth-location classes. No directory crawl or
  content scan. Directory access is not evidence of any specific file inside;
  missing conventional paths do not mean credentials are absent elsewhere.
  Symlinks are left unresolved. Anchor symlink components block observations;
  leaf intermediate path symlinks and races remain unproved. No paths, names,
  sizes, ownership IDs or content samples appear in the report.
- FDs: bounded observer `/proc/self/fd` enumeration plus fstat class. No FD
  number, target, inode or contents leave the observer. Directory enumeration
  can leave a transient vanished FD, which is explicitly `unstable`. These are
  observer FDs, not a future QEMU after-exec inventory. Socket/dev/regular
  classes lack identity and cannot pass `host_fds`. Future enforcement needs
  its own reviewed final-launch identity measurements.

The observer is trusted code with no subprocesses. Environment count is capped
at 512, FDs at 64, path inputs at 4096 characters. Observations stop on invalid
identity, context, anchors or bounds. Unknown access and races remain explicit
in the bounded report; no positive gate status exists. Workflow timeout is
20 seconds with a 5-second kill grace. No helper children or staging are
created by the script. Missing/partial report, timeout or unexpected exit is
not an observation success. The workflow stays red even if metadata completes.
A fixed report artifact has one-day retention. Actions management/upload still
has its own egress and platform credentials; nothing here accredits separation
from a future escaped payload. There is no guest-controlled output.

## Review and execution

Workflow-only bootstrap and code/docs/tests are separate exact-byte patches.
Before any dispatch: bootstrap readback + CI, code readback + CI 5/5, fresh
no-boot ImageVersion measurement at the resulting SHA, then separate explicit
approval for one observation at that SHA/image. No stale image reuse, retries,
boot or other gate probes follow from applying these files. Egress, aggregate
resource/process limits, cleanup and provider lifecycle are intentionally not
measured here. The core reachability problem remains open: metadata presence
is useful evidence of surfaces, never a credential-free VM claim.

Sources: automatic context access is documented at
https://docs.github.com/en/actions/security-for-github-actions/security-guides/automatic-token-authentication ;
checkout persistence at https://github.com/actions/checkout ;
the provider boundary at https://docs.github.com/en/actions/reference/runners/github-hosted-runners .
