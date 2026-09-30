# Hosted-runner B namespace capability preflight, no QEMU boot

This manual no-boot diagnostic uses the existing `ubuntu-24.04` hosted runner.
It does **not** touch Antonio's shared server `.16`, install QEMU, start a
microVM, compile candidate C, read credentials, or enable the v2 probe.
Require a reviewed 40-hex master SHA and an exact ImageVersion before launch.
The runner image must match; no implicit latest. The script measures a
non-root child with distinct user/mount/net/pid/ipc/uts namespaces and PID 1.
In its private mount namespace it stages a 1 MiB tmpfs, a proc mount, and an
empty `sys` directory, then unmounts and removes them. `sys` is an empty
staged directory, not a mounted host sysfs, and neither staged location is
remapped as `/proc` or `/sys` for any QEMU process. No `/etc` or `/dev` view
has been specified or enforced.

The only positive classification is `measured_only`, never
`runtime_complete:true` or isolation accreditation. An image drift, failed
unshare/mount, changed namespace, child result mismatch, cleanup failure or
unknown output blocks. The current GitHub-hosted image may deny user
namespaces; such a result is a finding, not a reason to change a global
AppArmor/sysctl setting or silently use root. Before adding a QEMU boot, a
separate gate must supply a reviewed binary/source closure for the exact
hosted image and prove actual confinement and host FD/content semantics in
the process's final view. An empty `sys` staging directory alone does not
meet the server `.16` containment goal. TCG and KVM are separate modes.

The server `.16` route remains paused after its non-root userns EPERM; the
server's original B0 script is not committed with this runner workflow.
