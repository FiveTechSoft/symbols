#!/usr/bin/env python3
"""No-boot prerequisite observations. No network/process/cgroup policy mutation."""
from __future__ import annotations
import json
import os
from pathlib import Path, PurePosixPath
import resource
import stat
import sys
import tempfile
from runner_vm_surface import identity, anchors

LIMITS = ('AS', 'CPU', 'FSIZE', 'NOFILE', 'NPROC', 'CORE')
CG_FILES = ('cgroup.procs', 'cgroup.subtree_control', 'cgroup.kill',
            'memory.max', 'pids.max', 'cpu.max')
REASONS = ('arguments', 'identity', 'path_anchors', 'scratch_cleanup',
           'observation_error', 'prerequisites_only_not_enforcement')


def base(reason):
    return {'schema': 'symbols.runner-vm-controls.v1',
            'classification': 'blocked', 'reason': reason if reason in REASONS else 'observation_error',
            'boot_attempted': False, 'runtime_complete': False, 'isolation_accredited': False,
            'identity': 'not_measured', 'egress': 'not_tested', 'limits': [],
            'cgroup': {'version': 'unknown', 'surfaces': []},
            'scratch': 'not_tested', 'process_cleanup': 'not_tested',
            'gates': [{'gate': g, 'status': 'not_proven'} for g in
                      ('egress', 'resources', 'process_cleanup', 'workspace_cleanup', 'provider_lifecycle')]}


def limit_class(value):
    if value == resource.RLIM_INFINITY:
        return 'unlimited'
    if type(value) is int and value >= 0:
        return 'zero' if value == 0 else 'finite'
    return 'unknown'


def limits():
    rows = []
    for name in LIMITS:
        try:
            soft, hard = resource.getrlimit(getattr(resource, 'RLIMIT_' + name))
            states = (limit_class(soft), limit_class(hard))
        except (OSError, ValueError, AttributeError):
            states = ('unknown', 'unknown')
        rows.append({'limit': name.lower(), 'soft': states[0], 'hard': states[1]})
    return rows


def current_cgroup():
    # Only this process's non-secret membership, capped. Never output its path.
    try:
        with open('/proc/self/cgroup', 'rb') as source:
            raw = source.read(4097)
        if len(raw) > 4096:
            return None
        lines = raw.decode('ascii').splitlines()
        if len(lines) != 1 or not lines[0].startswith('0::/'):
            return None
        member = lines[0][3:]
        parts = PurePosixPath(member).parts
        if any(p in ('.', '..') for p in member.split('/')) or not parts or parts[0] != '/':
            return None
        path = Path('/sys/fs/cgroup').joinpath(*parts[1:])
        # No symlink components or resolution of arbitrary target paths.
        for p in (path, *path.parents):
            if not stat.S_ISDIR(os.lstat(p).st_mode):
                return None
        return path
    except (OSError, ValueError, UnicodeError):
        return None


def access_class(path):
    try:
        mode = os.lstat(path).st_mode
        if stat.S_ISLNK(mode):
            return 'symlink_unresolved'
        if not stat.S_ISREG(mode):
            return 'unexpected_type'
        return 'write_access_reported' if os.access(path, os.W_OK) else 'no_write_access_reported'
    except FileNotFoundError:
        return 'missing'
    except (OSError, ValueError):
        return 'unknown'


def cgroup():
    root = current_cgroup()
    if root is None:
        return {'version': 'unknown', 'surfaces': []}
    return {'version': 'v2_membership', 'surfaces': [
        {'surface': name.replace('.', '_'), 'state': access_class(root / name)} for name in CG_FILES]}


def scratch(root):
    # One owned empty directory + one regular marker, no recursive deletion,
    # no symlinks, subprocesses, mounts, cgroup writes or unrelated files.
    directory = None
    marker = None
    ok = False
    try:
        directory = Path(tempfile.mkdtemp(prefix='symbols-controls-', dir=root))
        marker = directory / 'marker'
        fd = os.open(marker, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        try:
            os.write(fd, b'probe\n')
        finally:
            os.close(fd)
        marker.unlink()
        marker = None
        directory.rmdir()
        ok = not directory.exists() and not directory.is_symlink()
    except (OSError, ValueError):
        ok = False
    finally:
        if marker is not None:
            try:
                marker.unlink()
            except OSError:
                ok = False
        if directory is not None and directory.exists():
            try:
                directory.rmdir()
            except OSError:
                ok = False
    return 'owned_marker_removed' if ok else 'failed_or_unknown'


def measure(sha, image):
    result = base('identity')
    if not identity(os.environ, sha, image):
        return result
    result['identity'] = 'asserted_context_matches'
    root = anchors(os.environ)
    if root is None:
        result['reason'] = 'path_anchors'
        return result
    result['limits'] = limits()
    result['cgroup'] = cgroup()
    result['scratch'] = scratch(root['runner_temp'])
    result['reason'] = ('prerequisites_only_not_enforcement'
                        if result['scratch'] == 'owned_marker_removed' else 'scratch_cleanup')
    return result


def main(argv):
    try:
        result = measure(*argv) if len(argv) == 2 else base('arguments')
    except (OSError, ValueError, RuntimeError):
        result = base('observation_error')
    print(json.dumps(result, sort_keys=True, separators=(',', ':')))
    return 2


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
