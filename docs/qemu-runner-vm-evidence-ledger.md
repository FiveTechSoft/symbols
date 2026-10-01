# Runner VM evidence ledger

## Latest remote evidence before ceiling-raise tranche

Reviewed repository base: d938f50b61a1777203f7a0a6eef68621099a4cdd.
Apply280 and its four reusable CI jobs succeeded. Patch readback was byte-exact,
SHA256 08173979676f3d34bc46ad98373ad72e96a04541c843802f8a0b69a345b64a5f.
Pages and bank did not run at this SHA. They are not imported from earlier SHAs.

Egress4 ran at this SHA with frozen image set
20260920.314.1,20260927.320.1. Its image-set provenance is the three measured-only
no-boot jobs at 8e5d1ef85b636d0d1d2928a8fa4d8b3164c354ce, not at d938f50.
The unchanged guard and explicit tranche review permitted this reuse.

- Apply: https://github.com/FiveTechSoft/symbols/actions/runs/36808631438
- Observation: https://github.com/FiveTechSoft/symbols/actions/runs/36809515762
- Guard: https://github.com/FiveTechSoft/symbols/actions/runs/36809515762/artifacts/11139665109
  ZIP SHA256 627917692cc7873efaa32fe2da4d0ae67d77b14d5ea5cfe7b3779f2f58cbbd15.
- Observation JSON: https://github.com/FiveTechSoft/symbols/actions/runs/36809515762/artifacts/11139381346
  ZIP SHA256 fea020d9bfdd3eef87490cf6b62dc6dca43809528863c92902fb2a1372f06d61.
- Cleanup JSON: https://github.com/FiveTechSoft/symbols/actions/runs/36809515762/artifacts/11138469941
  ZIP SHA256 d0af625dc72dd2d4f500b5f87c8d8419488be109d16ea826b04ecaf3342bf95b.

Guard passed. Exact trusted-control and confined-child schemas/exit0 supported
11 local positive/negative classes. Classification remained blocked with
reason capabilities_only_not_enforcement. Direct children, fixture descriptors
and owned paths were confirmed_owned_direct; aggregate process and workspace
cleanup remained not_proven. No boot, runtime completion or isolation
accreditation followed.

## Ceiling-raise tranche prepared, not yet remotely observed

Evidence target: ceiling_raise_denials for AS, CPU, FSIZE, NOFILE and CORE in
one fixed trusted child with the unchanged seccomp policy. Local validation is
mock/parser/static/compile-only; no native helper has been executed locally.
No remote finding is claimed by this patch. Old single-NOFILE evidence is not
relabeled as all-five evidence. After exact-byte review, application/readback,
CI and image review, one separately scoped observation can add new evidence.
The actual landed SHA, CI and artifact digests belong in the closeout report;
this pre-application ledger cannot invent them.

| Item | Current evidence | Remaining gap |
| --- | --- | --- |
| Egress | Trusted numeric loopback samples and known-FD closure | Adversarial payload, local proxies, management channels and descendants |
| Resources | Five readback ceilings; historical NOFILE raise denial | Pending all-five raises; consumption violations not tested; aggregate scope unproved |
| Process cleanup | Known direct children reaped | Escaped sessions, all descendants, cancellation and supervisor death |
| Workspace cleanup | Fixed owned paths removed | All payload locations, adversarial races and cancellation |
| Payload credentials / host content / host FDs | Prior surface inventory; empty child env and known descriptors | Complete launch boundary and runner control-plane reachability |
| Provider lifecycle | Provider-asserted disposable VM, external final job conclusion | No local proof of destruction |
| QEMU compatibility | None from this helper | Exec/clones denied, mode and workload budgets unselected |

All four aggregate gate statuses remain not_proven. The nine-gate VM contract
remains in force. No helper observation substitutes for workload isolation.
