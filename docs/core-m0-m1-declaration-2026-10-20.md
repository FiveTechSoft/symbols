# M0 and M1 declaration (2026-10-20)

Declared by Antonio over final SHA `3fdbf7e8881946115b7f8849ed7055106f3ad98c` (master, 2026-10-18),
after the evidence report of 2026-10-18 and the scope decisions of 2026-10-20. Nothing on this page
is declared by an agent; the declaration is Antonio's, approved by him on 2026-10-20.

## Declaration (Antonio's words, Spanish original)

> Declaro M0 (shell seguro) y M1 (sistema de ficheros estructurado) cumplidos sobre el SHA
> `3fdbf7e8881946115b7f8849ed7055106f3ad98c`, con estos límites declarados: una runner por
> plataforma; escritores que cooperan; ASan Linux y macOS sin estrés; fuzzing como muestreo con las
> semillas registradas; criterio 2 de M1 limitado a lotes create y replace; sin durabilidad ante
> corte de energía; sin recuperación del bug antiguo del padre-fichero. La evidencia es la del
> informe del 18-oct-2026 más estas decisiones de alcance del 20-oct-2026.

English rendering (a translation, not the signed text):

> I declare M0 (safe shell) and M1 (structured filesystem) met on SHA
> `3fdbf7e8881946115b7f8849ed7055106f3ad98c`, with these declared limits: one runner per platform;
> cooperating writers; no ASan-Linux or macOS stress; fuzzing as sampling with the registered
> seeds; M1 criterion 2 limited to create and replace batches; no power-loss durability; no
> recovery of the old parent-file bug. The evidence is the 2026-10-18 report plus the 2026-10-20
> scope decisions.

## Scope decisions behind the limits (2026-10-20)

Taken by the development agent under Antonio's "decide tú" of 2026-10-20, choosing the report's
recommended option for each, and ratified by him:

1. One runner per platform and cooperating writers suffice for M1; everything wider is later
   hardening, not an exit criterion. Recorded as a declared limit.
2. The current fuzzing suffices for M1 criterion 5 as bounded sampling with the registered seeds
   (`test_fs_fuzz_ops` seeds 1, 7, 12648430 at 400 iterations plus the default, `test_core_fuzz`,
   the six POSIX and six Windows journal byte-fuzzes with real kills, the `AgentPatch` fuzzes).
   Sampling, not proof; more seeds can be added later without reopening the milestone.
3. Refusing a foreign inode satisfies M1 criterion 2: the criterion requires a complete commit or a
   byte-exact restore, and not touching foreign bytes preserves exactly that (fail closed).
   Evidence: `test_fs_win_sweep_foreign`, `test_fs_win_sweep_foreign_mc13`.
4. M1 criterion 2 covers create and replace batches only; the 2026-10-02 scope decision stands.
   Mixed create/replace/move/remove batches are later work, not missing exit evidence.
5. Recovery of intents left by the old parent-file bug stays outside M1; current writes no longer
   produce that state, and recovery is new functionality, a known issue for later work.

## Evidence on the final SHA

| What | Result | Source |
|---|---|---|
| Full CI of the SHA (Linux, MSVC, MSVC+ASan, Windows AST) | 5 of 5 jobs success; job checkouts read as `3fdbf7e` | run 39124598727 |
| Linux Release ctest | 248 of 248 pass | build-test-linux job log of that run |
| Windows MSVC Release ctest | 278 of 278 pass, 16 skipped (external Python/CMake tools) | build-test-msvc job log |
| Windows MSVC ASan ctest | 278 of 278 pass, same 16 skipped | asan-msvc job log |
| Shell stress Linux, 100 iterations | 100/100, 0 failures, 97 assertions, no leftover process or file | run 39152320089 |
| Shell stress Windows (MSVC Release), 100 iterations | 100/100, 0 failures, 91 assertions, clean baseline | run 39152362662 |

Detail and per-criterion readings: [docs/core-m0-m1-exit-audit.md](core-m0-m1-exit-audit.md),
[docs/core-m0-evidence-reading-2026-10-17-gold.json](core-m0-evidence-reading-2026-10-17-gold.json),
[docs/core-m1-exit-reading.md](core-m1-exit-reading.md). The ROADMAP note that called the path-fuzz
and filesystem-API diff checks pending was stale against the 2026-10-17 reconciliation.

## What this declaration does not do

- It declares neither M2 nor any later phase; M2 is the next open item under "What is missing for
  core v1" and its declaration is also Antonio's.
- The declared limits stand as limits, not as new work items. Later hardening (more runners,
  non-cooperating writers, wider fuzzing, power-loss durability) does not reopen M0 or M1.
