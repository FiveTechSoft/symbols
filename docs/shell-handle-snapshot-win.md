# External PSS snapshots around first private CreatePipe

Implementation base: 0aa7eb174dbb2dfdf200f406b5d6dbe840b7feb1.
Previous native traces located a first-CreatePipe delta+3 returning2handles;
all5owned handles closed, net+1 stable. Object identity/type/name remain unknown.

## Observation route
Use documented Process Snapshotting APIs from the parent test process to capture
the paused private child's handle table. The child never calls PSS. Use only
PSS_CAPTURE_HANDLES | PSS_CAPTURE_HANDLE_NAME_INFORMATION |
PSS_CAPTURE_HANDLE_BASIC_INFORMATION, no memory clone, threads, dump or tracing.
PSS_HANDLE_ENTRY supplies handle value, type name and optional object name.
Copy strings by documented BYTE lengths before walk-marker disposal; validate
UTF-16 lengths and fixed bounds, never assume NUL termination or silently clip.
Missing names are reported as unavailable, never fabricated. No handle is closed
because it appears as an extra candidate; close only the two returned pipe handles
and harness-owned events/mapping/process/job handles during normal cleanup.

Three separate fresh child processes share an event/shared-memory protocol. All
harness handles exist before the initial checkpoint. Child count and returned
pipe values travel through the mapping, never a pipe. Parent pauses/resumes via
precreated events, with per-checkpoint waits and total child30s bound.

A: count-only control. No snapshots. Record baseline, first real CreatePipe open,
close its2returned handles, then second real CreatePipe open/close. Match actual
production parameters (inheritable SECURITY_ATTRIBUTES,size0), but this is a
minimal direct-Win32 reproduction, not another AgentShellExec execution test.
B: snapshot-only control. No CreatePipe. Capture child twice while idle, then at
matching no-op checkpoints. Child's own count must remain constant. Snapshots
must enumerate stable handle identities, with count equality; resource disposal
must not change the target child's count. If this perturbs the target, report
observer effect and do not attribute an extra object from armC.
C: snapshot+pipe. Snapshot twice before the first call, then at first pipe open,
after closing those2pipe handles, at second pipe open and after close. Record
child counts immediately before signalling each checkpoint, before output.
Print every table delta including candidate handle value/type/name/status.
Compare snapshot counts to child's count. Do not confuse parent's PSS resource
handles with child's handles; target process identifiers are printed.

## Frozen evidence checks and causal limit
Count-only and snapshot+pipe each must show baseline->baseline+3->baseline+1,
then second pipe +2 and close-2, with successful APIs. That pattern is a bounded
reproduction expectation from previous boundary evidence, not proof of ownership.
Any other pattern is red and reported, not redefined. Snapshot-only must stay
flat and pre-pipe duplicate snapshots must agree, or report observer effect.
All expected checkpoints and complete capture/walk/free operations required.

With stable observer controls, require pipe-open minus pre contains3new handle
values including both returned pipe handles. After closing those2, require one
remaining candidate that was absent pre and is present after the second close.
Print that candidate's type name and name-presence flag/name (including empty or
unavailable) without requiring a guessed type/name. An inability to recover a
type or a complete table is a blocker, not a successful identification. Type and
name identify the object category/resource, not the responsible source-level
owner or a license to close it. No production change or contract revision.

Original strict fixture/gold/prior diagnostics remain byte-identical and red.
Native-only separate target and independent gold. Explicit raw invocation before
CTest on MSVC/ASAN. Publication splitcode->bootstrapworkflow with exact readback;
use automatic PAT-push finalSHA CI, no duplicate explicit run if present.
Closeout: observed type/name or clearly reported observer effect/blocker plus
original-red state, not5/5. No envrerun, Charles or efficacy bank.

## Microsoft sources read
https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/nf-processsnapshot-psscapturesnapshot
https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/ne-processsnapshot-pss_capture_flags
https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/ns-processsnapshot-pss_handle_entry
https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/ne-processsnapshot-pss_handle_flags
https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/nf-processsnapshot-psswalksnapshot
https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/nf-processsnapshot-psswalkmarkercreate

## Implementation and interpretation
The executable `test_shell_handle_snapshot_win` is native-only. The independent
JSON gold is frozen evidence for review, not runtime-generated expected output.
After every snapshot is freed, the parent reads the target process count again
and requires equality to the paused child's checkpoint count. Duplicate baseline
captures compare values, types, names and availability flags. The candidate and
all baseline identities must persist in every later table. Both returned handles
from each pipe must be distinct, absent from the prior table and absent after
close. No candidate handle is passed to CloseHandle.

A checkpoint wait is bounded at 5 seconds; child lifetime is checked against
30 seconds, with a kill-on-close job and an outer CTest timeout of 120 seconds
for all three arms. Synchronous Windows snapshot APIs cannot be individually
interrupted by this harness. An outer timeout is red, never a partial success.
UTF-16 lengths, storage limits, and UTF-8 conversion failures are red. Object
names may be explicitly unavailable or empty; missing type is a blocker. This
slice does not assert a particular object type or name.
