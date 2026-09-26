#!/usr/bin/env python3
"""Opt-in offline replay gate on fresh sealed tasks; never a solver input or edit authority."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

MAX_FILES = 64
MAX_FILE = 256 * 1024
MAX_TOTAL = 4 * 1024 * 1024
MAX_TASK = 8192
OP = re.compile(r'Operator ([a-z_]+): .*\(verified, kept\)')


class Unavailable(Exception):
    pass


def tree_digest(root):
    root = Path(root)
    files, total, digests = [], 0, {}
    if not root.is_dir() or root.is_symlink():
        raise Unavailable('invalid_tree')
    for path in sorted(root.rglob('*')):
        rel = path.relative_to(root).as_posix()
        if path.is_symlink() or not (path.is_file() or path.is_dir()):
            raise Unavailable('unsafe_entry')
        if any(part in ('.git', '.symbols', '..') for part in Path(rel).parts):
            raise Unavailable('forbidden_entry')
        if path.is_dir():
            continue
        if not path.is_file() or path.stat().st_nlink != 1:
            raise Unavailable('unsafe_file')
        data = path.read_bytes()
        if len(data) > MAX_FILE or b'\0' in data:
            raise Unavailable('oversize_or_binary')
        total += len(data)
        files.append(rel)
        digests[rel] = hashlib.sha256(data).hexdigest()
    if len(files) > MAX_FILES or total > MAX_TOTAL:
        raise Unavailable('snapshot_limit')
    canonical = json.dumps(digests, sort_keys=True, separators=(',', ':')).encode()
    return hashlib.sha256(canonical).hexdigest(), digests


def sealed_digest(root):
    # The sealed set includes the oracle and task text; only this digest is exposed.
    files = {}
    for p in sorted(Path(root).rglob('*')):
        if p.is_symlink() or not (p.is_file() or p.is_dir()):
            raise Unavailable('unsafe_sealed_entry')
        if p.is_file():
            files[p.relative_to(root).as_posix()] = hashlib.sha256(p.read_bytes()).hexdigest()
    return hashlib.sha256(json.dumps(files, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def call(argv, cwd, timeout=60, env=None):
    try:
        return subprocess.run(argv, cwd=cwd, env=env, capture_output=True, text=True,
                              encoding='utf-8', errors='replace', timeout=timeout)
    except (OSError, subprocess.TimeoutExpired):
        raise Unavailable('process_unavailable')


def private_copy(src, dst):
    shutil.copytree(src, dst)
    for p in dst.rglob('*'):
        if p.is_dir():
            p.chmod(0o700)
        else:
            p.chmod(0o600)


def collect(bank, index):
    lines = (bank / 'index.tsv').read_text(encoding='utf-8').splitlines()
    tasks = []
    for line in lines:
        if not line or line.startswith('#') or line.startswith('id\t'):
            continue
        parts = line.split('\t')
        if len(parts) != 3 or not re.fullmatch(r'[A-Za-z0-9_-]{1,48}', parts[0]):
            raise Unavailable('invalid_index')
        rel = Path(parts[2])
        if rel.is_absolute() or '..' in rel.parts or len(rel.parts) > 3:
            raise Unavailable('invalid_path')
        task = bank / rel
        if task.resolve().is_relative_to(bank.resolve()) is False:
            raise Unavailable('invalid_path')
        if not (task / 'before').is_dir() or not (task / 'task.md').is_file() or not (task / 'check.py').is_file():
            raise Unavailable('missing_task_parts')
        if any(p.is_symlink() for p in task.rglob('*')) or task.is_symlink():
            raise Unavailable('unsafe_task_entry')
        if (task / 'setup.py').exists():
            raise Unavailable('setup_not_supported')
        tree_digest(task / 'before')
        if (task / 'task.md').stat().st_size > MAX_TASK:
            raise Unavailable('task_too_large')
        tasks.append((parts[0], task))
    if not tasks or len({x[0] for x in tasks}) != len(tasks):
        raise Unavailable('missing_or_duplicate_tasks')
    return tasks


def source_changes(before, after):
    return sorted(k for k in before.keys() | after.keys() if before.get(k) != after.get(k))


def should_veto(prior, operator, changed):
    return bool(changed and operator and any(h['operator'] == operator and h['wrong_edit'] for h in prior))


def oracle(task, snapshot):
    # Never run hidden checker in the solver workspace. It cannot write to the snapshot.
    with tempfile.TemporaryDirectory(prefix='episode-oracle-') as temp:
        wd = Path(temp) / 'work'
        private_copy(snapshot, wd)
        shutil.copy2(task / 'check.py', wd / 'check.py')
        result = call([sys.executable, 'check.py'], wd)
        return result.returncode == 0


def run_agent(task, initial, branch, exe, report_bin, snapshots, seq, mode):
    private_copy(initial, branch)
    task_text = (task / 'task.md').read_text(encoding='utf-8').strip()
    env = dict(os.environ)
    env['SYMBOLS_ENGINEERING_EPISODES'] = '1'
    # No inherited decision-memory or trace switch.
    for key in ('SYMBOLS_TASK_OPS_MEMORY', 'SYMBOLS_TRACE'):
        env.pop(key, None)
    result = call([str(exe), '-w', str(branch), task_text], branch, env=env)
    episode = branch / '.symbols' / 'engineering_episodes.v1'
    if not episode.is_file():
        raise Unavailable('missing_episode')
    audit = call([str(report_bin), str(episode)], branch)
    match = re.search(r'records=([1-9][0-9]*)', audit.stdout)
    if audit.returncode or not match:
        raise Unavailable('invalid_episode')
    # v1 rows have no immutable per-attempt workspace bytes. Until a snapshot
    # boundary is implemented, only one-attempt tasks can be paired honestly.
    if int(match.group(1)) != 1:
        raise Unavailable('multi_attempt_snapshot_absent')
    # Strip the private audit store before hashing source. Capture it separately
    # for validation only, not policy: old v1 rows have no workspace bytes.
    shutil.rmtree(branch / '.symbols')
    digest, after = tree_digest(branch)
    op_matches = OP.findall(result.stdout)
    operator = op_matches[-1][0] if op_matches else None
    snap = snapshots / f'{seq:03d}-{mode}-post'
    private_copy(branch, snap)
    return {'agent_rc': result.returncode, 'operator': operator,
            'post_sha256': digest, 'after': after, 'snapshot': snap}


def evaluate(bank, tasks, exe, report_bin, output):
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    output.chmod(0o700)
    snapshots = output / 'snapshots'
    snapshots.mkdir(mode=0o700)
    snapshots.chmod(0o700)
    history = []
    aggregate = {key: {'resolved': 0, 'wrong_edits': 0, 'abstentions': 0,
                       'agent_errors': 0, 'unavailable': 0} for key in ('baseline', 'replay')}
    counts = []
    for seq, (tid, task) in enumerate(tasks):
        # Chronology fence is fixed before either target branch runs. Never use
        # target or future outcomes to choose this target's replay action.
        original = snapshots / f'{seq:03d}-initial'
        private_copy(task / 'before', original)
        before_digest, before = tree_digest(original)
        # A frozen checker must reject the original state; run in a copy.
        if oracle(task, original):
            raise Unavailable('oracle_accepts_original')
        # Leave out identical workspaces even if they happened earlier.
        prior = tuple(h for h in history if h['initial_sha256'] != before_digest)
        with tempfile.TemporaryDirectory(prefix='episode-target-') as temp:
            temp = Path(temp)
            branches = {}
            for mode in ('baseline', 'replay'):
                try:
                    run = run_agent(task, original, temp / mode, exe, report_bin, snapshots, seq, mode)
                    if tree_digest(original)[0] != before_digest:
                        raise Unavailable('initial_snapshot_changed')
                    changed = bool(source_changes(before, run['after']))
                    veto = mode == 'replay' and should_veto(prior, run['operator'], changed)
                    # Offline-only conservative historical gate: a prior wrong
                    # edit of the same operator triggers abstention. It is a
                    # deliberately testable candidate, not a live recommendation.
                    final_snap = original if veto else run['snapshot']
                    correct = oracle(task, final_snap)
                    final_changed = changed and not veto
                    status = 'resolved' if correct else 'wrong_edits' if final_changed else 'abstentions'
                    aggregate[mode][status] += 1
                    if run['agent_rc'] in (124, 127):
                        aggregate[mode]['agent_errors'] += 1
                    branches[mode] = {'status': status, 'operator': run['operator'],
                                      'wrong_edit': status == 'wrong_edits', 'veto': veto,
                                      'initial_sha256': before_digest,
                                      'final_sha256': before_digest if veto else run['post_sha256']}
                except (Unavailable, OSError, ValueError, UnicodeError):
                    aggregate[mode]['unavailable'] += 1
                    branches[mode] = {'status': 'unavailable'}
            # Only completed baseline observations from a previous task may
            # enter the next target's history; never target replay feedback.
            if branches['baseline']['status'] != 'unavailable':
                history.append({'operator': branches['baseline']['operator'],
                                'initial_sha256': before_digest,
                                'wrong_edit': branches['baseline']['wrong_edit']})
            counts.append({'sequence': seq, 'case': f'case-{seq:03d}', **branches})
    return {'schema': 1, 'mode': 'offline_read_only_replay',
            'history_rule': 'strict_prior_baseline_only_exclude_identical_workspace',
            'candidate_gate': 'abstain_if_prior_same_operator_wrong_edit',
            'task_count': len(tasks), 'aggregate': aggregate, 'paired': counts,
            'caveats': ['fresh_snapshots_only', 'v1_pre_snapshot_rows_unreplayable', 'multi_attempt_tasks_unavailable',
                        'mechanical_oracle_not_user_intent', 'offline_only_no_policy_change']}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sealed-bank', required=True)
    p.add_argument('--expected-digest', required=True)
    p.add_argument('--agent-bin', required=True)
    p.add_argument('--episode-report-bin', required=True)
    p.add_argument('--capture-root', required=True)
    a = p.parse_args()
    bank = Path(a.sealed_bank).resolve()
    digest = sealed_digest(bank)
    if not re.fullmatch(r'[0-9a-f]{64}', a.expected_digest) or digest != a.expected_digest:
        raise Unavailable('sealed_set_digest_mismatch')
    tasks = collect(bank, bank / 'index.tsv')
    exe, report = Path(a.agent_bin).resolve(), Path(a.episode_report_bin).resolve()
    if not exe.is_file() or not report.is_file():
        raise Unavailable('missing_binaries')
    output = Path(a.capture_root).resolve()
    if output.is_relative_to(bank) or bank.is_relative_to(output):
        raise Unavailable('capture_overlaps_sealed_set')
    result = evaluate(bank, tasks, exe, report, output)
    if sealed_digest(bank) != digest:
        raise Unavailable('sealed_set_changed_during_run')
    result['sealed_set_sha256'] = digest
    safe = json.dumps(result, sort_keys=True, indent=2) + '\n'
    (output / 'summary.json').write_text(safe, encoding='utf-8')
    (output / 'summary.json').chmod(0o600)
    print(safe, end='')


if __name__ == '__main__':
    try:
        main()
    except Unavailable as exc:
        print(f'REPLAY_UNAVAILABLE {exc}', file=sys.stderr)
        sys.exit(2)
