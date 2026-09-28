"""Exact byte fixtures for SBM1/SBI1/SBO1. No VM or target execution."""
import hashlib
from pathlib import Path
import sys
import unittest
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'typed_c_contract'))
from framing import (FrameRefusal, encode_manifest, decode_manifest,
                     encode_source_frame, decode_source_frame, decode_result_frame)

SBO_SBI = {k: bytes.fromhex(v) for k,v in {'empty_ok': '53424f310000000000e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855', 'ok_no_lf': '53424f310000000002565339bc4d33d72817b583024112eb7f5cdf3e5eef0252d6ec1b9c9a94e12bb34f4b', 'ok_lf': '53424f310000000003a12b7cb43c9d9134b5bb1b35e9096b66775d9e92e7611d1cc92b02edd6782a874f4b0a', 'nul': '53424f31000000000376fe3925c7167317f2df68454339f5ec3650e4062178b4f2be219b105a507907410042', 'crlf': '53424f31000000000326ffd5886253906a36a7ea0f6e26056fc36472626cb4894bcb100a34dc69d1db410d0a', 'compile_error': '53424f310100000000e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855', 'source_min': '5342493100000019fb8b40ec02a97167bb21beb678bf459694a83050a770271e50879784c4992785696e74206d61696e28766f6964297b72657475726e20303b7d'}.items()}
SBM = {k: bytes.fromhex(v) for k,v in {'valid': '53424d31010a00066d61696e2e631f02545c3eaa72f84fec5078ae61fd70cf6dadf57658abeeae03c0d08caaa4a5894d529b9bde3ba099e59cd399bede03064e5a88abc183753f636db5955c6ded13b31ef95048e0f5531d65d29504ea873d544b47205c4c8d4cd9001ae59c4bf0b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000038b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000003a12b7cb43c9d9134b5bb1b35e9096b66775d9e92e7611d1cc92b02edd6782a8700001388000102030405060708090a0b0c0d0e0f553f44f653360f7c08facc055f383f8aa1b7549a31ee1ac8dae7f74e1301b5fd', 'changed_path': '53424d31010a00064d61696e2e631f02545c3eaa72f84fec5078ae61fd70cf6dadf57658abeeae03c0d08caaa4a5894d529b9bde3ba099e59cd399bede03064e5a88abc183753f636db5955c6ded13b31ef95048e0f5531d65d29504ea873d544b47205c4c8d4cd9001ae59c4bf0b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000038b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000003a12b7cb43c9d9134b5bb1b35e9096b66775d9e92e7611d1cc92b02edd6782a8700001388000102030405060708090a0b0c0d0e0f553f44f653360f7c08facc055f383f8aa1b7549a31ee1ac8dae7f74e1301b5fd', 'truncated': '53424d31010a00066d61696e2e631f02545c3eaa72f84fec5078ae61fd70cf6dadf57658abeeae03c0d08caaa4a5894d529b9bde3ba099e59cd399bede03064e5a88abc183753f636db5955c6ded13b31ef95048e0f5531d65d29504ea873d544b47205c4c8d4cd9001ae59c4bf0b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000038b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000003a12b7cb43c9d9134b5bb1b35e9096b66775d9e92e7611d1cc92b02edd6782a8700001388000102030405060708090a0b0c0d0e0f553f44f653360f7c08facc055f383f8aa1b7549a31ee1ac8dae7f74e1301b5', 'trailing': '53424d31010a00066d61696e2e631f02545c3eaa72f84fec5078ae61fd70cf6dadf57658abeeae03c0d08caaa4a5894d529b9bde3ba099e59cd399bede03064e5a88abc183753f636db5955c6ded13b31ef95048e0f5531d65d29504ea873d544b47205c4c8d4cd9001ae59c4bf0b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000038b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000003a12b7cb43c9d9134b5bb1b35e9096b66775d9e92e7611d1cc92b02edd6782a8700001388000102030405060708090a0b0c0d0e0f553f44f653360f7c08facc055f383f8aa1b7549a31ee1ac8dae7f74e1301b5fd00', 'too_long_declared': '53424d31030400066d61696e2e631f02545c3eaa72f84fec5078ae61fd70cf6dadf57658abeeae03c0d08caaa4a5894d529b9bde3ba099e59cd399bede03064e5a88abc183753f636db5955c6ded13b31ef95048e0f5531d65d29504ea873d544b47205c4c8d4cd9001ae59c4bf0b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000038b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000003a12b7cb43c9d9134b5bb1b35e9096b66775d9e92e7611d1cc92b02edd6782a8700001388000102030405060708090a0b0c0d0e0f553f44f653360f7c08facc055f383f8aa1b7549a31ee1ac8dae7f74e1301b5fd', 'bad_magic': '00424d31010a00066d61696e2e631f02545c3eaa72f84fec5078ae61fd70cf6dadf57658abeeae03c0d08caaa4a5894d529b9bde3ba099e59cd399bede03064e5a88abc183753f636db5955c6ded13b31ef95048e0f5531d65d29504ea873d544b47205c4c8d4cd9001ae59c4bf0b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000038b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000003a12b7cb43c9d9134b5bb1b35e9096b66775d9e92e7611d1cc92b02edd6782a8700001388000102030405060708090a0b0c0d0e0f553f44f653360f7c08facc055f383f8aa1b7549a31ee1ac8dae7f74e1301b5fd', 'zero_nonce': '53424d31010a00066d61696e2e631f02545c3eaa72f84fec5078ae61fd70cf6dadf57658abeeae03c0d08caaa4a5894d529b9bde3ba099e59cd399bede03064e5a88abc183753f636db5955c6ded13b31ef95048e0f5531d65d29504ea873d544b47205c4c8d4cd9001ae59c4bf0b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000038b7439fda44e3bb3bdbcd3eaa0089ff131bdaf04fca611108e842ba555babd5aa00000003a12b7cb43c9d9134b5bb1b35e9096b66775d9e92e7611d1cc92b02edd6782a870000138800000000000000000000000000000000553f44f653360f7c08facc055f383f8aa1b7549a31ee1ac8dae7f74e1301b5fd'}.items()}
H = lambda data: hashlib.sha256(data).digest()
SOURCE = b'#include <stdio.h>\nint main(void){puts("OK");return 0;}\n'
GOAL = b'OK\n'
CONTEXT = dict(path=b'main.c', contract_sha256=H(b'contract fixture'),
               workspace_sha256=H(b'workspace fixture'),
               before_sha256=H(b'before fixture'), after_sha256=H(SOURCE),
               source_len=len(SOURCE), source_sha256=H(SOURCE), goal_len=len(GOAL),
               goal_sha256=H(GOAL), timeout_ms=5000, nonce=bytes(range(16)))


def change(data, offset, value, rehash=False):
    result = data[:offset] + value + data[offset + len(value):]
    return result[:-32] + H(result[:-32]) if rehash else result


class FramingTests(unittest.TestCase):
    def refusal(self, func, frame, code):
        with self.assertRaises(FrameRefusal) as err:
            func(frame)
        self.assertEqual(err.exception.code, code)

    def test_result_positive_exact_bytes(self):
        for name, status, payload in (('empty_ok',0,b''),('ok_no_lf',0,b'OK'),
                                      ('ok_lf',0,b'OK\n'),('nul',0,b'A\x00B'),
                                      ('crlf',0,b'A\r\n'),('compile_error',1,b'')):
            with self.subTest(name=name):
                frame=SBO_SBI[name]
                self.assertEqual(len(frame),41+len(payload))
                self.assertEqual(frame[9:41],H(payload))
                self.assertEqual(decode_result_frame(frame),
                                 dict(status=status,stdout=payload,complete=True,started=status==0))
        self.assertNotEqual(decode_result_frame(SBO_SBI['ok_lf'])['stdout'],b'OK')
        self.assertNotEqual(decode_result_frame(SBO_SBI['crlf'])['stdout'],b'A\n')

    def test_source_positive_exact_bytes(self):
        frame=SBO_SBI['source_min']
        self.assertEqual(decode_source_frame(frame),b'int main(void){return 0;}')
        self.assertEqual(encode_source_frame(b'int main(void){return 0;}'),frame)

    def test_commit_one_negative_mutations(self):
        good=SBO_SBI['ok_no_lf']; empty=SBO_SBI['empty_ok']; source=SBO_SBI['source_min']
        cases=[(decode_result_frame,change(good,0,b'\x00'),'magic'),
               (decode_result_frame,change(good,4,b'\x02'),'status'),
               (decode_result_frame,change(good,42,b'L'),'digest'),
               (decode_result_frame,good[:-1],'truncated'),
               (decode_result_frame,good+b'\x00','trailing'),
               (decode_result_frame,empty+empty,'trailing'),
               (decode_result_frame,change(empty,5,bytes.fromhex('00000080')),'length_limit'),
               (decode_result_frame,change(SBO_SBI['compile_error'],5,bytes.fromhex('00000002'))+b'OK','phase_length'),
               (decode_source_frame,source+b'\x00','trailing'),
               (decode_source_frame,change(source,64,b'~'),'digest')]
        for decoder,frame,code in cases:
            with self.subTest(code=code,frame=frame.hex()):self.refusal(decoder,frame,code)

    def test_manifest_positive_exact_bytes(self):
        frame=SBM['valid']
        self.assertEqual(len(frame),266)
        self.assertEqual(frame[:8],bytes.fromhex('53424d31010a0006'))
        self.assertEqual(H(frame[:-32]),frame[-32:])
        self.assertEqual(H(frame),bytes.fromhex('623380acdc4ba465703ada3c0b29e127a5d6de8f62da9ab19332615d0e88ff5c'))
        self.assertEqual(decode_manifest(frame,expected=CONTEXT),CONTEXT)
        encoded=encode_manifest(path=CONTEXT['path'],contract_sha256=CONTEXT['contract_sha256'],
            workspace_sha256=CONTEXT['workspace_sha256'],before_sha256=CONTEXT['before_sha256'],
            after_sha256=CONTEXT['after_sha256'],source=SOURCE,goal=GOAL,
            timeout_ms=5000,nonce=CONTEXT['nonce'])
        self.assertEqual(encoded,frame)

    def test_manifest_negative_table(self):
        for name,code in [('changed_path','digest'),('truncated','truncated'),
                          ('trailing','trailing'),('too_long_declared','length_limit'),
                          ('bad_magic','magic'),('zero_nonce','nonce')]:
            with self.subTest(name=name):self.refusal(decode_manifest,SBM[name],code)
        valid=SBM['valid']; p=6
        for offset,value,code in [(4,b'\x00\x00','trailing'),
                                  (6,b'\x00\x00','path_length'),
                                  (136+p,bytes.fromhex('00010001'),'source_limit'),
                                  (172+p,bytes.fromhex('00000080'),'goal_limit'),
                                  (208+p,bytes.fromhex('00000001'),'timeout'),
                                  (0,b'\x00','magic')]:
            with self.subTest(code=code,offset=offset):
                self.refusal(decode_manifest,change(valid,offset,value,rehash=True),code)
        self.refusal(decode_manifest,change(valid,212+p,bytes(16),rehash=True),'nonce')
        self.refusal(decode_manifest,change(valid,104+p,bytes(32),rehash=True),'source_digest')
        self.refusal(decode_manifest,change(valid,4,bytes.fromhex('010b'),rehash=True),'truncated')
        self.refusal(decode_manifest,valid+b'\x00','trailing')
        for key, alternate in [('path',b'Main.c'),('source_len',57),('goal_len',2),
                               ('nonce',bytes(range(1,17))),('contract_sha256',bytes(32)),
                               ('workspace_sha256',bytes(32)),('before_sha256',bytes(32)),
                               ('after_sha256',bytes(32)),('source_sha256',bytes(32)),
                               ('goal_sha256',bytes(32)),('timeout_ms',5001)]:
            with self.subTest(context_key=key):
                expected={**CONTEXT,key:alternate}
                with self.assertRaisesRegex(FrameRefusal,'context'):
                    decode_manifest(valid,expected=expected)

    def test_type_bounds_and_pure_parser(self):
        for decoder in (decode_manifest,decode_source_frame,decode_result_frame):
            self.refusal(decoder,'not bytes','type')
        for malformed,code in [(b'','magic'),(b'SBO1','truncated')]:
            self.refusal(decode_result_frame,malformed,code)
        for source,code in [(b'','source_limit'),(b'\x00','source_encoding'),
                            (b'\xff','source_encoding')]:
            with self.assertRaisesRegex(FrameRefusal,code):encode_source_frame(source)
        with mock.patch('builtins.print',side_effect=AssertionError('payload log')), \
                mock.patch('subprocess.Popen',side_effect=AssertionError('host launch')):
            decode_manifest(SBM['valid'],expected=CONTEXT)
            decode_result_frame(SBO_SBI['nul'])
            decode_source_frame(SBO_SBI['source_min'])

if __name__ == '__main__':unittest.main()
