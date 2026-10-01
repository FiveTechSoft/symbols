# Read-only discovery MVP implementation coverage

Base 07805dae49adb1bd172ac407d84eef29b95113e6. Prepared for exact review only;
no application or observation is claimed here. Workflow and code are separate
patches because workflow bootstrap and code apply use different guarded routes.

The MVP uses zero optional operator candidates (`not_provided`), not an
unverified environment path. This does not exclude other delegations. No global
search, sibling crawl, task enumeration or cgroup.procs/threads contents.
`/proc/<getpid()>` is constructed internally and opened through no-follow
`/proc`; `/proc/self` symlink is not followed and external PIDs are not inputs.

Coverage is explicitly `prioritized_not_complete`, never a complete discovery.
Within 64 total counted operations (including opens, close reservations,
getuid/euid/pid, stat/read/statvfs), 32 KiB total input and 10 s alarm/monotonic
bound, it reads own membership, bounded own mountinfo, selected own status,
then current cgroup type/controllers/subtree_control. It checks metadata only
for procs/threads/kill. Runner-temp directory metadata and statvfs are last.
A long anchor, oversized mountinfo or exhausted operation budget refuses the
whole result; partial results do not become a successful candidate report.
Closing owned read handles is reserved in the operation budget even on error.
Every text read is at most 4097 bytes and refuses if over4096; insufficient
remaining total budget refuses before reading. No retries.

Not implemented in MVP: cgroup.events or memory/swap/oom/pids/cpu control
values, reported access checks, operator candidate root, kernel delegate file,
filesystem type (statvfs does not identify it), quota mount predicates or
storage ownership/exclusivity. These omissions are not availability findings.
Separation observes self identity relation/effective-capability presence,
no_new_privs and seccomp mode only; payload remains uninstantiated. No raw
UIDs/PIDs/masks/paths/mount text/credentials leave the observer. Storage bytes,
entries, delegation and separation stay not_proven. All nine gates not_proven,
classification blocked and no boot/runtime/isolation accreditation.

The observer opens only directories and allowlisted regular metadata with
O_RDONLY/O_NOFOLLOW/O_CLOEXEC, using dirfd traversal; no symlink following.
Kernel metadata can still change during reads. This MVP does not claim stable
snapshot, exclusive ownership or enforced access from these reads. Errors
collapse to closed refusal, not permission or evidence of absent capabilities.
The workflow creates only fixed guard/report artifacts for management upload;
that bookkeeping is not payload/scope/storage mutation by the observer.
No compiled helper, shell child, native execution, quota ioctl, scope create,
mount, network probe, sudo or server action. Workflow sets PYTHONDONTWRITEBYTECODE=1 to prevent import-cache writes. Python
startup/import reads use the normal interpreter environment and are not a
payload isolation claim or counted metadata operations. The 64-op budget covers
explicit observer metadata operations plus owned closes, not interpreter
startup, signal timer setup or management artifact stdout/upload.

Offline mocks test parsers, output secrecy, task-file refusal, unsafe paths,
per-file/total/operation bounds and owned-descriptor closure reservations.
Future real metadata observation requires separate approval at exact SHA and
three fresh image measurements, frozen set <=3, one dispatch only. Unknown red,
identity/image refusal or drift stops with no retry. Missing artifact is not
proof. Report canonical readback/CI and actual coverage in the ledger closeout;
never promote these prerequisites to enforcement.
