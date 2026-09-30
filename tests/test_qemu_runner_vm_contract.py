"""Offline contract refusal tests. No runner, network, namespace or QEMU calls."""
import copy
import contextlib
import io
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'qemu_closure'))
from runner_vm_contract import CONTRACT, GATES, decode, main, report, verify


class RunnerVMContractTests(unittest.TestCase):
    def assert_refusal(self, result):
        self.assertEqual(result['classification'], 'blocked')
        for key in ('boot_attempted', 'runtime_complete', 'isolation_accredited'):
            self.assertIs(result[key], False)
        self.assertEqual(result['gates'], [{'gate': g, 'status': 'not_collected'} for g in GATES])

    def test_committed_contract_exact(self):
        raw = (Path(__file__).resolve().parents[1] / 'tools/qemu_closure/runner_vm_contract.json').read_bytes()
        result = decode(raw)
        self.assert_refusal(result)
        self.assertIs(result['contract_valid'], True)
        self.assertEqual(result['reason'], 'evidence_not_collected')

    def test_each_field_drift_blocks(self):
        for key in CONTRACT:
            value = copy.deepcopy(CONTRACT)
            value[key] = 'UNKNOWN'
            with self.subTest(key=key):
                self.assertIs(verify(value)['contract_valid'], False)
                self.assert_refusal(verify(value))

    def test_missing_and_extra_keys(self):
        for key in CONTRACT:
            value = copy.deepcopy(CONTRACT)
            del value[key]
            self.assertEqual(verify(value)['reason'], 'contract_schema')
        value = copy.deepcopy(CONTRACT)
        value['runtime_complete'] = True
        self.assertEqual(verify(value)['reason'], 'contract_schema')

    def test_boolean_is_not_integer(self):
        for key in ('persist_credentials', 'in_job_guest_escape_defense', 'synthetic_qemu_fs_views'):
            value = copy.deepcopy(CONTRACT)
            value[key] = 0
            self.assertEqual(verify(value)['reason'], 'contract_mismatch')

    def test_gates_exact_order_and_membership(self):
        for gates in (list(reversed(GATES)), list(GATES[:-1]), list(GATES) + ['other'], [False]):
            value = copy.deepcopy(CONTRACT)
            value['required_gates'] = gates
            self.assertEqual(verify(value)['reason'], 'contract_mismatch')

    def test_input_refusals_closed_no_echo(self):
        for raw in (b'{"secret":"private"', b'{"x":1,"x":2}', b'\xff', b'x' * 4097,
                    b'[]', b'null', b'[' * 2000 + b']' * 2000):
            result = decode(raw)
            self.assert_refusal(result)
            self.assertNotIn('private', json.dumps(result))
            self.assertIs(result['contract_valid'], False)
        self.assertEqual(report('PRIVATE')['reason'], 'contract_input')

    def test_cli_never_returns_success(self):
        for args in ([], ['/not-a-real-contract'], ['one', 'two']):
            with self.subTest(args=args), contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(main(args), 2)
                self.assert_refusal(json.loads(output.getvalue()))
        with patch.object(Path, 'open', side_effect=PermissionError('/private')), \
                contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(main([]), 2)
            self.assertNotIn('/private', output.getvalue())


if __name__ == '__main__':
    unittest.main()
