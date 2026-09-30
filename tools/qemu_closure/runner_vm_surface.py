#!/usr/bin/env python3
"""Bounded metadata-only no-boot observations. All B gates remain blocked."""
from __future__ import annotations
import json
import os
from pathlib import Path
import re
import stat
import sys

ENV_KEYS = ('GITHUB_TOKEN', 'GH_TOKEN', 'ACTIONS_RUNTIME_TOKEN',
            'ACTIONS_ID_TOKEN_REQUEST_TOKEN', 'ACTIONS_RESULTS_URL',
            'ACTIONS_RUNTIME_URL', 'SSH_AUTH_SOCK')
PATH_KEYS = ('workspace', 'runner_temp', 'home', 'proc', 'sys', 'dev',
             'git_config', 'git_credentials', 'ssh', 'cloud_aws', 'cloud_azure',
             'cloud_gcp', 'docker_config', 'runner_parent')
PATH_STATES = ('missing', 'regular_readable', 'regular_unreadable',
               'directory_searchable', 'directory_unsearchable', 'symlink_unresolved',
               'special', 'unknown')
FD_CLASSES = ('stdio', 'regular', 'directory', 'socket', 'pipe', 'device', 'other', 'unstable')
MAX_ENV = 512
MAX_FDS = 64


def base(reason):
    return {'schema': 'symbols.runner-vm-surface.v1', 'classification': 'blocked',
            'reason': reason, 'boot_attempted': False, 'runtime_complete': False,
            'isolation_accredited': False, 'identity': 'not_measured',
            'credentials': {'automatic_token_context': 'unknown', 'environment': []},
            'host_content': [], 'host_fds': [],
            'gates': [{'gate': name, 'status': 'not_proven'} for name in
                      ('runner_identity', 'payload_credentials', 'host_content', 'host_fds')]}


def identity(env, sha, image):
    return (sys.platform == 'linux' and os.getuid() != 0 and os.geteuid() != 0
            and re.fullmatch(r'[0-9a-f]{40}', sha or '') is not None
            and env.get('GITHUB_SHA') == sha and env.get('GITHUB_REF') == 'refs/heads/master'
            and env.get('RUNNER_ENVIRONMENT') == 'github-hosted'
            and env.get('RUNNER_OS') == 'Linux' and env.get('RUNNER_ARCH') == 'X64'
            and re.fullmatch(r'[0-9]{8}\.[0-9]+\.[0-9]+', image or '') is not None
            and env.get('ImageVersion') == image)


def path_state(path):
    # Never open contents or follow a symlink. No names or targets enter output.
    try:
        mode = os.lstat(path).st_mode
        if stat.S_ISLNK(mode):
            return 'symlink_unresolved'
        if stat.S_ISREG(mode):
            return 'regular_readable' if os.access(path, os.R_OK) else 'regular_unreadable'
        if stat.S_ISDIR(mode):
            return 'directory_searchable' if os.access(path, os.R_OK | os.X_OK) else 'directory_unsearchable'
        return 'special'
    except FileNotFoundError:
        return 'missing'
    except (OSError, ValueError):
        return 'unknown'


def anchors(env):
    result = {}
    for key, name in (('workspace', 'GITHUB_WORKSPACE'), ('runner_temp', 'RUNNER_TEMP'), ('home', 'HOME')):
        raw = env.get(name, '')
        if not raw or len(raw) > 4096 or '\x00' in raw or not Path(raw).is_absolute():
            return None
        path = Path(raw)
        # Reject symlinks in every anchor component. Missing/inaccessible is unknown.
        try:
            if any(not stat.S_ISDIR(os.lstat(p).st_mode) for p in (path, *path.parents)):
                return None
        except (OSError, ValueError):
            return None
        result[key] = path
    return result


def paths(root):
    home = root['home']
    return {'workspace': root['workspace'], 'runner_temp': root['runner_temp'], 'home': home,
            'proc': Path('/proc'), 'sys': Path('/sys'), 'dev': Path('/dev'),
            'git_config': root['workspace'] / '.git' / 'config',
            'git_credentials': home / '.git-credentials', 'ssh': home / '.ssh',
            'cloud_aws': home / '.aws', 'cloud_azure': home / '.azure',
            'cloud_gcp': home / '.config' / 'gcloud', 'docker_config': home / '.docker',
            'runner_parent': root['runner_temp'].parent}


def fd_inventory():
    # Inventory this observer only. No FD readlink or read of another process.
    result = []
    try:
        names = os.listdir('/proc/self/fd')
        if len(names) > MAX_FDS or any(not name.isdecimal() for name in names):
            return None
        for name in sorted(names, key=int):
            fd = int(name)
            try:
                mode = os.fstat(fd).st_mode
                if fd <= 2:
                    kind = 'stdio'
                elif stat.S_ISREG(mode):
                    kind = 'regular'
                elif stat.S_ISDIR(mode):
                    kind = 'directory'
                elif stat.S_ISSOCK(mode):
                    kind = 'socket'
                elif stat.S_ISFIFO(mode):
                    kind = 'pipe'
                elif stat.S_ISCHR(mode) or stat.S_ISBLK(mode):
                    kind = 'device'
                else:
                    kind = 'other'
            except OSError:
                # listdir's transient FD can disappear. Do not silently omit it.
                kind = 'unstable'
            result.append({'class': kind})
    except (OSError, ValueError):
        return None
    return result


def measure(sha, image):
    env = os.environ
    result = base('identity')
    if not identity(env, sha, image):
        return result
    result['identity'] = 'asserted_context_matches'
    if len(env) > MAX_ENV:
        result['reason'] = 'environment_bounds'
        return result
    # Presence only, not contents, validity, permissions or use of a token.
    result['credentials']['environment'] = [
        {'key': key.lower(), 'state': 'present' if key in env else 'absent'} for key in ENV_KEYS]
    context = env.get('SYMBOLS_AUTOMATIC_TOKEN_PRESENT')
    if context not in ('true', 'false'):
        result['reason'] = 'token_context'
        return result
    result['credentials']['automatic_token_context'] = 'present' if context == 'true' else 'absent'
    root = anchors(env)
    if root is None:
        result['reason'] = 'path_anchors'
        return result
    measured = paths(root)
    result['host_content'] = [{'surface': key, 'state': path_state(measured[key])} for key in PATH_KEYS]
    fds = fd_inventory()
    if fds is None:
        result['reason'] = 'fd_bounds'
        return result
    result['host_fds'] = fds
    result['reason'] = 'metadata_only_not_reachability_proof'
    return result


def main(argv):
    if len(argv) != 2:
        result = base('arguments')
    else:
        try:
            result = measure(*argv)
        except (OSError, ValueError, RuntimeError):
            result = base('observation_error')
    print(json.dumps(result, sort_keys=True, separators=(',', ':')))
    return 2


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
