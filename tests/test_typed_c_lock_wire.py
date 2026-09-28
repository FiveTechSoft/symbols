"""SBL1 byte vectors and strict framing; no semantic approval or execution."""
import hashlib
from pathlib import Path
import sys
import unittest
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'typed_c_contract'))
from lock_wire import (LockWireRefusal, encode_lock_wire, decode_lock_wire,
                       MAX_FRAME, MAX_PAYLOAD)

PAYLOAD = b'{"architecture":"amd64","exclusions":[],"files":[],"packages":[],"review_by_utc":null,"schema":"symbols.slice-b-lock.v1","snapshot_id":"20260927T000000Z","state":"measured_payloads_only"}'
FRAME = bytes.fromhex('53424c31000000bb7b22617263686974656374757265223a22616d643634222c226578636c7573696f6e73223a5b5d2c2266696c6573223a5b5d2c227061636b61676573223a5b5d2c227265766965775f62795f757463223a6e756c6c2c22736368656d61223a2273796d626f6c732e736c6963652d622d6c6f636b2e7631222c22736e617073686f745f6964223a223230323630393237543030303030305a222c227374617465223a226d656173757265645f7061796c6f6164735f6f6e6c79227da63867ca788da68e2785f74e59477673b6ea86450dcba601f8b11a46885554a0')


def reframe(payload):
    body = b'SBL1' + len(payload).to_bytes(4, 'big') + payload
    return body + hashlib.sha256(body).digest()


class WireTests(unittest.TestCase):
    def refusal(self, frame, code):
        with self.assertRaises(LockWireRefusal) as err:
            decode_lock_wire(frame)
        self.assertEqual(err.exception.code, code)

    def test_exact_fixture(self):
        self.assertEqual(len(PAYLOAD), 187)
        self.assertEqual(len(FRAME), 227)
        self.assertEqual(FRAME[:8], bytes.fromhex('53424c31000000bb'))
        self.assertEqual(FRAME[8:-32], PAYLOAD)
        self.assertEqual(FRAME[-32:], hashlib.sha256(FRAME[:-32]).digest())
        self.assertEqual(hashlib.sha256(FRAME).hexdigest(),
                         '92350f01f2c84b3c1c500168de5da6f98869e02335c8060e7b7eccb15300bb59')
        decoded = decode_lock_wire(FRAME)
        self.assertEqual(decoded['state'], 'measured_payloads_only')
        self.assertEqual(encode_lock_wire(decoded), FRAME)
        self.assertEqual(reframe(PAYLOAD), FRAME)
        # This fixture lacks the complete semantic schema. Parse success never means launch.
        self.assertNotIn('image', decoded)
        self.assertNotIn('builder', decoded)

    def test_exact_negative_mutations(self):
        cases = [(b'\x00' + FRAME[1:], 'magic'),
                 (FRAME[:4] + bytes.fromhex('00400000') + FRAME[8:], 'length_limit'),
                 (FRAME[:-1], 'truncated'),
                 (FRAME + b'\x00', 'trailing'),
                 (FRAME[:-1] + b'\xa1', 'digest')]
        for frame, code in cases:
            with self.subTest(code=code):self.refusal(frame, code)
        duplicate = PAYLOAD[:-1] + b',"state":"measured_payloads_only"}'
        self.refusal(reframe(duplicate), 'duplicate_key')
        self.refusal(reframe(PAYLOAD[:1] + b' ' + PAYLOAD[1:]), 'noncanonical')

    def test_nested_duplicate_and_noncanonical(self):
        for payload, code in [(b'{"x":{"a":1,"a":2}}','duplicate_key'),
                              (b'{"z":1,"a":2}','noncanonical'),
                              (b'{"x":1.0}','float'),
                              (b'{"x":NaN}','constant'),
                              (b'{"x":01}','json'),
                              (b'{"x":"\\ud800"}','surrogate'),
                              (b'{"x":true} ','noncanonical'),
                              (b'\xef\xbb\xbf{}','encoding'),
                              (b'{}{}','json')]:
            with self.subTest(payload=payload):self.refusal(reframe(payload),code)

    def test_bounds_types_and_no_side_effects(self):
        self.assertEqual(MAX_FRAME, 4*1024*1024)
        self.assertEqual(MAX_PAYLOAD, MAX_FRAME-40)
        self.refusal('not bytes', 'type')
        self.refusal(b'SBL1', 'truncated')
        self.refusal(b'SBL1'+bytes.fromhex('00400000'), 'length_limit')
        with self.assertRaisesRegex(LockWireRefusal,'length_limit'):
            encode_lock_wire('a'*MAX_FRAME)
        with mock.patch('builtins.print', side_effect=AssertionError('log')), \
             mock.patch('subprocess.Popen', side_effect=AssertionError('launch')):
            self.assertEqual(decode_lock_wire(FRAME)['architecture'],'amd64')

if __name__ == '__main__': unittest.main()
