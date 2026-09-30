#!/usr/bin/env python3
"""No-boot, non-root namespace failure localization on an exact hosted image.

Output is a closed vocabulary. This never credits isolation or runtime coverage.
"""
from __future__ import annotations
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile

PHASES = ('user_only', 'user_mount', 'full', 'mount_private', 'tmpfs', 'proc', 'cleanup')
ERRORS = ('eperm', 'eacces', 'einval', 'enoent', 'enospc', 'eagain', 'enomem', 'unsupported', 'timeout', 'other')
BASE = ('/usr/bin/unshare', '--user', '--map-root-user')
MODES = (
    ('user_only', ()),
    ('user_mount', ('--mount',)),
    ('full', ('--mount', '--net', '--pid', '--ipc', '--uts', '--fork')),
)


def errno_class(stderr):
    """Map a bounded utility error to one token; never publish the message."""
    text = stderr[:512].lower()
    for phrase, kind in (('operation not permitted', 'eperm'),
                         ('permission denied', 'eacces'),
                         ('invalid argument', 'einval'),
                         ('no such file or directory', 'enoent'),
                         ('no space left on device', 'enospc'),
                         ('resource temporarily unavailable', 'eagain'),
                         ('out of memory', 'enomem'),
                         ('not supported', 'unsupported')):
        if phrase in text:
            return kind
    return 'other'


def run(argv):
    # A timed-out --fork unshare may leave a child; own and reap its process group.
    try:
        with subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True, errors='replace', start_new_session=True) as p:
            try:
                stdout, stderr = p.communicate(timeout=8)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(p.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                p.communicate(timeout=5)
                return -1, '', '', False
            return p.returncode, stdout[:2049], stderr[:512], len(stdout) > 2048
    except (OSError, ValueError, subprocess.TimeoutExpired):
        return -2, '', '', False


def failure(phase, kind):
    return {'phase': phase, 'status': 'failed', 'errno_class': kind}


def classify_process(phase, result):
    rc, stdout, stderr, oversized = result
    if rc == 0 and not oversized and not stdout:
        return {'phase': phase, 'status': 'ok'}
    return failure(phase, 'timeout' if rc == -1 else errno_class(stderr))


def child(root):
    """Called only after the full unshare; no host mount namespace operations."""
    if root.exists() or root.is_symlink() or not root.parent.is_dir() or root.parent.is_symlink():
        return failure('tmpfs', 'other')
    root.mkdir(mode=0o700)
    tmpfs = proc = False
    step = {'phase': 'mount_private', 'status': 'ok'}
    cleanup_failed = False
    try:
        for name, cmd in (
            ('mount_private', ('/usr/bin/mount', '--make-rprivate', '/')),
            ('tmpfs', ('/usr/bin/mount', '-t', 'tmpfs', '-o',
                       'size=1m,mode=0700,nosuid,nodev,noexec', 'tmpfs', str(root))),
            ('proc', ('/usr/bin/mount', '-t', 'proc', '-o',
                      'nosuid,nodev,noexec,hidepid=2', 'proc', str(root / 'proc'))),
        ):
            if name == 'proc':
                (root / 'proc').mkdir()
            step = classify_process(name, run(cmd))
            if step['status'] != 'ok':
                break
            if name == 'tmpfs':
                tmpfs = True
            if name == 'proc':
                proc = True
    finally:
        if proc and classify_process('cleanup', run(('/usr/bin/umount', str(root / 'proc'))))['status'] != 'ok':
            cleanup_failed = True
        if (root / 'proc').is_dir():
            try:
                (root / 'proc').rmdir()
            except OSError:
                cleanup_failed = True
        if tmpfs and classify_process('cleanup', run(('/usr/bin/umount', str(root))))['status'] != 'ok':
            cleanup_failed = True
        try:
            root.rmdir()
        except OSError:
            cleanup_failed = True
    return failure('cleanup', 'other') if cleanup_failed else step


def validate_child(result):
    rc, stdout, stderr, oversized = result
    if rc == -1:
        return failure('full', 'timeout')
    if oversized:
        return failure('full', 'other')
    try:
        obj = json.loads(stdout)
        if not isinstance(obj, dict) or set(obj) not in ({'phase', 'status'}, {'phase', 'status', 'errno_class'}):
            raise ValueError()
        if obj['phase'] not in PHASES or obj['status'] not in ('ok', 'failed'):
            raise ValueError()
        if obj['status'] == 'failed' and obj.get('errno_class') not in ERRORS:
            raise ValueError()
        if obj['status'] == 'ok' and ('errno_class' in obj or obj['phase'] != 'proc'):
            raise ValueError()
        if rc == 0 and obj['status'] == 'ok' or rc == 2 and obj['status'] == 'failed':
            return obj
    except (ValueError, KeyError, TypeError):
        pass
    return failure('full', errno_class(stderr) if rc not in (0, 2) else 'other')


def diagnose(target_sha, expected_image):
    report = {'schema': 'symbols.runner-b-namespace-diagnostic.v1',
              'classification': 'blocked', 'runtime_complete': False,
              'isolation_accredited': False, 'boot_attempted': False,
              'phases': [], 'reason': 'identity'}
    if not (sys.platform == 'linux' and os.getuid() != 0 and os.geteuid() != 0
            and re.fullmatch(r'[0-9a-f]{40}', target_sha or '')
            and target_sha == os.environ.get('GITHUB_SHA')
            and os.environ.get('GITHUB_REF') == 'refs/heads/master'
            and re.fullmatch(r'[0-9]{8}\.[0-9]+\.[0-9]+', expected_image or '')
            and expected_image == os.environ.get('ImageVersion')):
        return report
    for phase, extra in MODES:
        try:
            with tempfile.TemporaryDirectory(prefix='symbols-b-diag-', dir=os.environ['RUNNER_TEMP']) as tmp:
                if phase == 'full':
                    cmd = BASE + extra + ('--', '/usr/bin/python3', str(Path(__file__).resolve()),
                                          '--child', str(Path(tmp) / 'synthetic'))
                    row = validate_child(run(cmd))
                else:
                    row = classify_process(phase, run(BASE + extra + ('--', '/usr/bin/true')))
            report['phases'].append(row)
            if row['status'] != 'ok':
                report['reason'] = row['phase']
                break
        except (OSError, ValueError):
            report['reason'] = 'cleanup'
            report['phases'].append(failure('cleanup', 'other'))
            break
    else:
        report['classification'] = 'measured_only'
        report['reason'] = 'namespace_setup_phases_ok_not_isolation'
    return report


def main():
    if len(sys.argv) == 3 and sys.argv[1] == '--child':
        try:
            result = child(Path(sys.argv[2]))
        except (OSError, ValueError):
            result = failure('cleanup', 'other')
        print(json.dumps(result, sort_keys=True))
        return 0 if result['status'] == 'ok' else 2
    if len(sys.argv) != 3:
        return 2
    result = diagnose(sys.argv[1], sys.argv[2])
    print(json.dumps(result, sort_keys=True))
    return 0 if result['classification'] == 'measured_only' else 2


if __name__ == '__main__':
    raise SystemExit(main())
