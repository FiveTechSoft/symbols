# File resource queries before creation-owner tracing

Base: 8ef52f161c08f53b5a54b8318e5545f1d24e4df0. Implementation follows the independently reviewed pre-code design.

## What is known and what is not
Final CI454 MSVC+ASAN located one new persistent File after first CreatePipe,
with stable PSS observer controls. Names unavailable in PSS. File is also the
type of the two returned pipe handles. This does NOT identify a resource, an
allocation/creation stack, a source-level owner, a leak, or harmlessness.
A cached NPFS-directory handle is a hypothesis only, not expected output.

PSS has no File-specific union or creation-stack field. Its HandleCount,
PointerCount and GrantedAccess members are reserved, so do not use those to
infer ownership. ObjectType=0 is not an identification beyond TypeName=File.

## Recommended next step: bounded external read-only resource queries
New native-only executable/gold/doc. Existing snapshot fixture, original strict
fixture/gold and all previous diagnostics stay byte-identical. No src/ABI change.
Reuse the independently reproduced count-only, snapshot-only and snapshot+pipe
arms in NEW test code. Frozen deltas remain [0,3,1,3,1] / all-zero, with complete
captures, pre duplicates and original identity rules. No handle transport pipes.

Only AFTER the fifth checkpoint has reproduced the full pattern, with both
returned pairs closed and candidate identified, pause that private target and
inspect the candidate externally. Thus queries cannot create the observed
first/second-CreatePipe count pattern. Before/after every query, compare target
count and PSS table identities/type/name/flags, including candidate still open.
Do not compare reserved reference-count fields as stable object identity.

For each query, create a fresh suspended helper with a kill-on-close job and
shared mapping/event result protocol. DuplicateHandle from target directly INTO
helper, noninheritable, DUPLICATE_SAME_ACCESS only. Never DUPLICATE_CLOSE_SOURCE.
Pass numeric helper-local handle via mapping; it is not valid in the parent.
Resume helper, perform one query, copy exact result into mapping, close only
that helper-owned duplicate, exit. Parent verifies exit/result/cleanup and then
recaptures target. A timed-out helper is killed and reaped; only its reference
is released by process teardown. The target's candidate is never closed.
Use an explicit inherited-handle list for helper IPC; do not leak target/process
or unrelated harness handles into helper. Mapping/events, not redirected pipes.

This is NOT zero perturbation: duplication adds a reference to the SAME object;
queries may enter its driver and may block. No ReadFile, WriteFile, seeks, sets,
IOCTLs, connect/disconnect, impersonation, remote threads or target code hooks.
Stable before/after counts/tables mean no detected table perturbation, not proof
that object/driver state was untouched.

### Queries, each isolated in its own helper
1. GetFileType, SetLastError(0) first. Record raw enum and GetLastError;
   UNKNOWN+NO_ERROR differs from API failure. PIPE means socket/named/anonymous
   pipe category, not a unique resource or owner. Do not infer NPFS from it alone.
2. GetFinalPathNameByHandleW(FILE_NAME_OPENED | VOLUME_NAME_NT). Opened name avoids
   normalized-component lookups. Copy length-checked UTF-16 and UTF-8; report exact
   failure/error or full path, never synthesize missing device/volume components.
3. NtQueryInformationFile(FileNameInformation=9), documented native fallback,
   dynamically resolved from already-loaded ntdll in helper only. Record return
   NTSTATUS, IOSB status/information and byte-length FileName. No undocumented
   ObjectNameInformation layout. STATUS_PENDING is incomplete, NOT success; do
   not consume a pending buffer, end the helper and report blocked. Fixed 64KiB
   buffer; overflow/oversize is reported incomplete, never clipped/retried wider.
   Native names may be volume-relative or partial. Preserve that limitation.

All three queries run once even if a previous query failed. No automatic wider
access, privilege enable, reopening a guessed path, or undocumented fallback.
GetFileInformationByHandleEx was considered but excluded here: despite generic
FILE_NAME_INFO wording, the owning function explicitly says handle should NOT
be a pipe. Candidate may be pipe; do not run an inapplicable query to manufacture
an identification. NtQueryObject's documented classes only add basic/type data,
not name or creation owner, so repeating it is not a useful next step.

### Controls and deadlines
Before target arms, run the same helper mechanism on a harness-owned temporary
disk file (type DISK, exact known filename and full final path; native name must
end in that known filename) and a known anonymous pipe (type PIPE; name-query
success is NOT assumed). Delete only that owned temporary file. Also exercise
an invalid-handle query (failure recorded, never a fabricated name) and a forced
helper timeout (kill/reap verified). These controls run outside target lifetime.
No candidate result can count as complete if harness/control/cleanup fails.

New experiment budgets ONLY: helper launch+query+exit 3000ms per query, each
paused target checkpoint 20000ms, total target lifetime 60000ms checked plus
kill-on-close job, CTest outer180s for whole executable. These are explicit new
experiment limits, NOT revisions to production or any existing fixture/gold.
Synchronous PSS calls retain outer-timeout fallback; timeout is red. All helper
API errors/timeouts are reported, queries continue if target remains valid.

## Frozen result and closeout policy
Require unchanged PSS observer controls and exactly one persistent File candidate
before query. Before/after each helper target count and full copied identity table
must match. Candidate never close. Every attempted query gets raw result/status,
bounded strings and cleanup evidence. No predicted path, file type enum or name.

Result levels:
- full_path_observed: complete nonempty NT final path actually returned by API;
  report exact resource path and file-type result. This still does NOT prove owner.
- partial_name_only: native volume-relative/partial name, or only category; report
  limitation, resource identification blocked, nonzero diagnostic result.
- unavailable/timeout/observer_effect: raw evidence plus blocker, nonzero result.
A root/device path may identify a device/directory, not an individual pipe; name
it at the scope observed. Do not turn any of these into source-owner proof.
No original red is waived. Native final CI closeout remains original255 state,
new diagnostic result, exact publication readbacks, terminal attempt/rerun state.
Split code A then workflow-only B, parent-reviewed patch hashes first, auto PAT CI
only, raw MSVC/ASAN, no environmental rerun or gold redefinition.

## Passive routes not exhausted yet; ownership decision after these results
This design exhausts practical documented read-only name/type queries, not every
possible Windows diagnostic. It does NOT promise source-owner identification.
If a resource path is recovered, report it and propose creation attribution only
if still needed. If queries fail/block, explicitly report the passive-query limit.
Do NOT silently escalate to any of these:

| Option | What it can add | Perturbation/cost and gate |
| --- | --- | --- |
| External ETW object-handle create/close + creation stack | Potential PID/TID/handle/object/name plus stack correlated with candidate | Provider/stack availability and privilege need a separate capability check; session/driver tracing overhead, event loss and cross-process scope must be bounded. ObHandleEvent documentation alone does not guarantee usable stack or CI availability. New design/gold/review required. |
| Debugger on private child at native open/create return with stack capture | Actual creator call stack and handle correlation if breakpoints cover creation path | Stops target, changes timing and debug state; more invasive, symbol/version dependence. No production attach. New design/gold/review required. |
| Target memory/disassembly or undocumented object-name layouts | Leads about caches/native implementation | Version-fragile, may misattribute ownership. Not evidence of this handle's creator by itself; not selected. |

Owner means source-level creator/retainer/lifecycle obligation. A named kernel
resource or process handle-table membership alone does not establish that owner.
No close/fix/contract change follows automatically from any result.

## Source evidence read
- https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/ns-processsnapshot-pss_handle_entry
- https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle
- https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfiletype
- https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew
- https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex
- https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_name_info
- https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryobject
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntqueryinformationfile
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-_file_name_information
- https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe
- https://learn.microsoft.com/en-us/windows/win32/etw/obhandleevent

## Implementation notes
Query logs distinguish control_disk, control_pipe, control_invalid and candidate.
Raw API return values carry attempted/returned flags, so a timeout or unavailable
native entry point is never rendered as an invented NT success. Helper timeout
is 3000ms from harness setup through normal exit; forced kill/reap has a separate
bounded 3000ms cleanup wait, within the target's 20s checkpoint/60s lifetime.
Pending native IO leaves its IOSB/buffer alive until immediate helper-process
teardown. No incomplete buffer is read. Helper exit is verified before target
recapture. A query timeout, pending status, duplicate error, encoding error or
helper cleanup error forces nonzero even if a different query found a path.
Ordinary query errors are preserved; another complete query can still supply the
resource path. Full path and owner status are separate fields in every closeout.

Additional API contracts checked:
https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute
https://learn.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-io_status_block
