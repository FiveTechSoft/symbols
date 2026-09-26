# Offline snapshot replay gate (slice 5, narrow boundary)

`tools/episode_replay.py` is an opt-in offline evaluator. It never writes a
production solver decision store or changes candidate generation. It runs the
same existing `symbols-agent` separately on two immutable copies of each
fresh task's initial state. The baseline keeps the agent's result. The replay
branch can only abstain when an earlier baseline task on a different initial
workspace with the same observed operator made an edit rejected by its frozen
oracle. It cannot generate a repair, turn an abstention into an edit, or bypass
any live safety gate. This is a deliberately conservative candidate to test,
not a production policy recommendation.

The capture stores private, size-bounded initial and final workspace trees and
SHA-256 manifests, plus the existing strict v1 episode file for audit validation
before it strips `.symbols` from the source comparison. The source tree may
have at most 64 regular text/code files, 256 KiB per file, 4 MiB total. It
refuses symlinks, hardlinks, `.git`, `.symbols`, binaries, setup scripts and
unsafe paths. The sealed set's digest covers task text, frozen checks and
source bytes. No snapshot contents, task wording, oracle body, or raw agent
log appear in `summary.json`; the private capture root is mode 0700 and
summary is mode 0600. A capture directory must not already exist. Do not
upload or publish it. Delete it after authorized local evaluation when its
retention is not needed.

**Important boundary:** v1 episodes do not store replayable workspace bytes.
Any episode written before this opt-in capture remains unreplayable. Because
this harness captures only task-entry and task-exit trees, it accepts only a
single recorded attempt per task. Multi-attempt tasks are counted as
`unavailable`, not collapsed into a fictitious parent-child replay tree.
Parent links in v1 are observations, not proof of saved intermediate state.
The strict chronology uses only completed prior baseline tasks, excludes
identical initial workspaces, and freezes the prior list before either target
branch runs. It never reads a target's or future task's checker or result for
policy input. The hidden `check.py` runs in a separate copy only after each
branch has finished; this mechanical oracle is not proof of user intent.

Task format and first-run sealing are in the independent author contract.
This harness refuses `setup.py` for now, even if the author provides one;
those tasks are unsupported and must be counted unavailable in the sealed-set
preflight, not silently converted. It requires `index.tsv` rows of
`id<TAB>category<TAB>relative-case-path`, each with `before/`, `task.md` and
`check.py`. The checker is executed offline with Python and must return 0 only
on an acceptable final state. Run only after an independent author has frozen
and self-tested fresh tasks, published the sealed digest, and the harness code
is frozen:

```
python3 tools/episode_replay.py --sealed-bank /private/sealed-bank \
  --expected-digest SHA256 --agent-bin /absolute/build/symbols-agent \
  --episode-report-bin /absolute/build/engineering_episode_report \
  --capture-root /private/new-capture-directory
```

`summary.json` reports paired resolved, wrong-edit, abstention, agent-error
and unavailable counts, including a comparison against the baseline. If all
history gates abstain, a no-change result is still a negative experiment.
Never infer replay gain from these development smoke tests. The true result
requires an independent, first-run sealed set and frozen oracles. The parser
and existing v1 audit store are not a new source of live solver decisions.
