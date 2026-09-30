#!/usr/bin/env python3
"""Offline contract gate only. Never runs or accredits a workload."""
from __future__ import annotations
import json
from pathlib import Path
import sys

GATES = ('runner_identity', 'payload_credentials', 'host_content', 'host_fds',
         'egress', 'resources', 'process_cleanup', 'workspace_cleanup',
         'provider_lifecycle')
CONTRACT = {
    'schema': 'symbols.runner-vm-contract.v1',
    'boundary': 'github_hosted_disposable_vm',
    'runner_label': 'ubuntu-24.04',
    'payload_secrets': 'none',
    'workflow_permissions': 'none',
    'persist_credentials': False,
    'qemu_privilege': 'nonroot',
    'mode': 'unselected',
    'server_16': 'paused',
    'in_job_guest_escape_defense': False,
    'synthetic_qemu_fs_views': False,
    'provider_lifecycle': 'provider_assertion_not_local_measurement',
    'required_gates': list(GATES),
}
REASONS = ('evidence_not_collected', 'contract_schema', 'contract_mismatch',
           'contract_input')


def report(reason):
    if reason not in REASONS:
        reason = 'contract_input'
    return {'schema': 'symbols.runner-vm-contract-report.v1',
            'classification': 'blocked', 'reason': reason,
            'contract_valid': reason == 'evidence_not_collected',
            'boot_attempted': False, 'runtime_complete': False,
            'isolation_accredited': False,
            'gates': [{'gate': gate, 'status': 'not_collected'} for gate in GATES]}


def verify(value):
    # Exact types matter: False and 0 compare equal in Python, but differ here.
    if type(value) is not dict or set(value) != set(CONTRACT):
        return report('contract_schema')
    for key, expected in CONTRACT.items():
        actual = value[key]
        if type(actual) is not type(expected):
            return report('contract_mismatch')
        if isinstance(expected, list) and any(type(item) is not str for item in actual):
            return report('contract_mismatch')
        if actual != expected:
            return report('contract_mismatch')
    return report('evidence_not_collected')


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError('duplicate')
        value[key] = item
    return value


def decode(raw):
    if type(raw) is not bytes or len(raw) > 4096:
        return report('contract_input')
    try:
        value = json.loads(raw.decode('utf-8'), object_pairs_hook=unique_object)
    except (UnicodeError, ValueError, RecursionError):
        return report('contract_input')
    return verify(value)


def main(argv):
    # A passing contract syntax check is not a passing B gate. Always exit 2.
    if len(argv) > 1:
        result = report('contract_input')
    else:
        path = Path(argv[0]) if argv else Path(__file__).with_name('runner_vm_contract.json')
        try:
            with path.open('rb') as source:
                result = decode(source.read(4097))
        except (OSError, ValueError):
            result = report('contract_input')
    print(json.dumps(result, sort_keys=True, separators=(',', ':')))
    return 2


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
