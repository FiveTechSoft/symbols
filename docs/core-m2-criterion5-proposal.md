# M2 criterion 5: proposal for apply-patch.yml (NOT applied)

Status: a proposal for Antonio. Nothing in `.github/` is changed by this file. `apply-patch.yml` changes only by his hand.
Read against `master` at `1698c52`. Criterion 5 stays NOT MET until he decides and a measured run exists.

Criterion 5 (ROADMAP, M2): the apply-patch workflow uses the Git contracts instead of open-coded assumptions.

## What the workflow does today, step by step

| Step in `apply-patch.yml` | Today | Gate command that exists |
|---|---|---|
| Build the gate from the current master | `cmake` builds `symbols_git_gate` to `$RUNNER_TEMP`, before the patch is applied | n/a |
| Validate request and patch: expected head | open-coded: `test "$EXPECTED_HEAD" = "$(git rev-parse HEAD)"` | `preflight --expected-head` (already called two steps later) |
| Validate request and patch: patch source, size, path allowlist, `--numstat` | open-coded policy | none, and not a Git-state question |
| Git preflight | gate: `preflight --expected-head ... --branch master` | yes (used) |
| Remote check | open-coded: `git ls-remote --exit-code origin refs/heads/master`, tip must equal HEAD | `preflight --remote-sync` (exists, not used) |
| Already-applied check | gate: `patch-state .git/change.patch`, exit 3 means nothing to commit | yes (used) |
| Apply | open-coded: `git apply`, `git diff --check` | none (the gate does not act) |
| Stage | open-coded: `git add --` with a fixed path list | none |
| Staged tree equals the patch | gate: `verify-staged` | yes (used) |
| Commit | `git commit` | none |
| Commit equals the patch | gate: `verify-head` | yes (used) |
| Push | open-coded: `git push origin HEAD:master` (no force) | none |

So four gate commands are already in the workflow. The open-coded Git-state assumptions left are the expected-head `rev-parse`, the `ls-remote` equality test, and the three actions (apply, add, push) that no gate command covers.

## Proposed change (one step, small)

Replace the open-coded remote check by the gate's own, and drop the duplicate expected-head test:

```diff
-          test "$EXPECTED_HEAD" = "$(git rev-parse HEAD)" || { echo 'master moved; refusing stale patch'; exit 1; }
...
-          "$GATE" preflight --expected-head "$EXPECTED_HEAD" --branch master
-          # Explicit remote check: master on origin must be exactly this HEAD.
-          remote="$(git ls-remote --exit-code origin refs/heads/master | cut -f1)"
-          test "$remote" = "$(git rev-parse HEAD)" || { echo "origin/master is $remote, not $(git rev-parse HEAD)"; exit 1; }
+          "$GATE" preflight --expected-head "$EXPECTED_HEAD" --branch master --remote-sync
```

Differences, stated so nobody has to find them:

- `--remote-sync` accepts a remote tip that is HEAD or an ancestor of HEAD (local ahead). The old test accepts only equality. In this workflow HEAD is the fresh checkout of master, so local ahead should not happen, but that is reasoning, not a measurement.
- `--remote-sync` needs the branch to have an upstream (`git rev-parse @{upstream}`). Whether `actions/checkout` with `ref: master` leaves master tracking `origin/master` was NOT checked. If it does not, the step fails closed with `no_upstream` (exit 18) and the workflow stops; it does not apply anything wrong.
- Dropping the early expected-head test moves that failure from the validate step to the preflight step. The refusal is the same; the order of messages changes. If the early failure matters (before the URL download), keep that line.
- Exit codes change: `preflight` exits 10 plus a status (stale head 13, remote advanced 19, no upstream 18). The workflow only reads "non-zero", so nothing downstream depends on them.

Measured elsewhere (not in this workflow): `--remote-sync` ready, remote advanced (exit 19), unreachable remote and no upstream are cells of `test_git_gate_e2e` and `test_agent_git_remote`, on Linux and Windows CI.

## What would still be open-coded after that, and why

- `git apply`, `git add -- <paths>`, `git commit`, `git push origin HEAD:master`: the gate has no command that performs them. It checks the result (`verify-staged`, `verify-head`) and the state before (`preflight`, `patch-state`). The push is a plain fast-forward (no force); a master that moved is refused by git itself.
- Patch source and path allowlist: policy, not Git state.
- Closing the "no gate command for push" gap would need a new gate command and its own contract and tests. Not proposed here.

So after this change criterion 5 would read PARTIAL, not MET: the Git-state checks go through the gate, the Git actions do not. Whether that satisfies "uses the Git contracts instead of open-coded assumptions" is Antonio's reading.

## How it could be measured without touching master

The workflow has a `dry_run` input: every check, build, test and a local commit, then `git push --dry-run`. A dry run of the changed workflow on the current head would show: whether the upstream exists, and `preflight --remote-sync` ready. It would NOT show the remote-advanced refusal in the real workflow (that needs master to move during the run). The change would land through the bootstrap `apply-github-patch.yml` (anything under `.github/workflows/` goes that way); the guard inside `apply-patch.yml` stays as is.

## Not claimed

- No run of the changed workflow exists. Everything above about it is reading, not measurement.
- Race between the remote check and the push is not closed by either version; only git's fast-forward rule protects the push.
- This does not change the parked decision on the open-coded steps.
