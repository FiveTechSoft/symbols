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

## Historical pre-observation plan for ceiling-raise tranche

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

## Ceiling-raise closeout, October 1, 2026

Landed SHA d467a7d478a0a9b28845a1ea540402ee506d2aa7. Apply281 and four reusable CI jobs succeeded: https://github.com/FiveTechSoft/symbols/actions/runs/36837336780 . Pages and bank did not run at this SHA, with no older green imported.

Reviewed patch12441B SHA256395f83bd8113cef3d388964faee3e0714aff9329fbcf814a933f76a4fe468c4e. Canonical remote diff12441B SHA256fcd104f9af5b2df4d9be7e84fbdd7e03c2ba2387736807fdf4f1d05198065874. All five chunks byte-identical by path; only ledger chunk order differs. Exact-review decision accepted this serialization-only difference. Future canonical readback is the serialization produced by remote git diff; content/mode/path/hunk differences still stop. No corrective commit.

Three fresh no-boot measured-only jobs at landed SHA, before observation, freeze distinct set20260920.314.1,20260927.320.1:
- https://github.com/FiveTechSoft/symbols/actions/runs/36838569197/artifacts/11149659310 ZIP514B sha256:c9538524a06545c93a15674791371a864ae3843c787dcd70a5d35cbd8a30548b; image20260920.314.1.
- https://github.com/FiveTechSoft/symbols/actions/runs/36838847728/artifacts/11149949556 ZIP514B sha256:821bb8060501cbb7c475393acd79d2478a0a7fc73544514c062f4eedf8f6760f; image20260927.320.1.
- https://github.com/FiveTechSoft/symbols/actions/runs/36839114885/artifacts/11149734901 ZIP514B sha256:0ebe5ceb9d9d8e599b00779c0bc73dcda89a9bd28e85341d71fa6bedd08a1baa; image20260927.320.1.

One harness observation https://github.com/FiveTechSoft/symbols/actions/runs/36839443839 . Guard passed. Child exit0/exactv2 schema supported all five ceiling raises denied EPERM and unchanged soft/hard readbacks. Parentv2 exactschema confirmed, ceiling_raise_denials measured_only forAS/CPU/FSIZE/NOFILE/CORE. Consumption violations not_tested. All four aggregate gates not_proven, classificationblocked/capabilities_only_not_enforcement; boot/runtime/isolation false. No retry or second observation.

- https://github.com/FiveTechSoft/symbols/actions/runs/36839443839/artifacts/11150437496 ZIP448B sha256:72ee5c71ebd1909d4a11b2e279d353e4a86a27ccad5376a4d6625728a481ad72, downloaded bytes digest-verified.
- https://github.com/FiveTechSoft/symbols/actions/runs/36839443839/artifacts/11150264620 ZIP193B sha256:0558cb8f76b04546def3577df993f790e62b7380f0717a0f89e73a97fdabcd0e, downloaded bytes digest-verified.

Removal step success proves only reported command success, not independently attested absence or all-descendant/workspace cleanup. No separate cleanup artifact in this unchanged harness workflow. No new cleanup accreditation. QEMU incompatible, workload mode unselected, nine-gate contract still in force. This section records the verified closeout evidence for inclusion in the separately reviewed design-only revision. It adds no new observation or accreditation.
