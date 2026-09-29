"""Offline test vectors for the signed metadata and deterministic candidate archive."""
import copy
import json
import io
import urllib.error
from pathlib import Path
import sys
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'builder_rootfs'))
from build import Refusal, bounded_deb_tar, bounded_xz, check_package_rows, digest, fetch, signed_indexes

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = json.loads((ROOT / 'tools' / 'builder_rootfs' / 'noble-20260927-source.json').read_bytes())


class SourceTests(unittest.TestCase):
    def test_manifest_closed_counts(self):
        self.assertEqual(MANIFEST['schema'], 'symbols.builder-rootfs-source.v1')
        self.assertEqual(len(MANIFEST['packages']), 142)
        self.assertEqual(MANIFEST['total_compressed_bytes'], 154352140)
        self.assertEqual(len(MANIFEST['setuid_exclusions']), 8)
        self.assertEqual(len(MANIFEST['indexes']), 3)
        self.assertEqual(MANIFEST['keyring_sha256'], 'bfdb0512804d0740816d836bb05b1ef8b27609771d7b9962c95bcc57bfaac230')

    def test_package_index_mismatch(self):
        row=MANIFEST['packages'][0]
        indexes={row['index']: ("\n".join(f"{k}: {row[k]}" for k in ('Package','Version','Architecture','Filename','Size','SHA256'))+'\n\n').encode()}
        with self.assertRaisesRegex(Refusal,'package_index_binding'):
            check_package_rows(MANIFEST,indexes)
        mutated=copy.deepcopy(MANIFEST)
        mutated['packages'][0]['SHA256']='0'*64
        with self.assertRaisesRegex(Refusal,'package_index_binding'):
            check_package_rows(mutated,indexes)

    def test_child_stdout_cap(self):
        self.assertEqual(bounded_deb_tar([sys.executable,'-c',"print('ok')"],1024,5),b'ok\n')
        with self.assertRaisesRegex(Refusal,'archive'):
            bounded_deb_tar([sys.executable,'-c',"print('x'*2048)"],1024,5)
        with self.assertRaisesRegex(Refusal,'archive'):
            bounded_deb_tar([sys.executable,'-c','raise SystemExit(1)'],1024,5)

    def test_manifest_package_path_and_arch(self):
        row=MANIFEST['packages'][0]
        indexes={row['index']: ("\n".join(f"{k}: {row[k]}" for k in ('Package','Version','Architecture','Filename','Size','SHA256'))+'\n\n').encode()}
        changed=copy.deepcopy(MANIFEST)
        changed['packages'][0]['Filename']='pool/../escape.deb'
        with self.assertRaisesRegex(Refusal,'package_index_binding'):
            check_package_rows(changed,indexes)
        changed=copy.deepcopy(MANIFEST)
        changed['packages'][0]['Architecture']='i386'
        with self.assertRaisesRegex(Refusal,'package_index_binding'):
            check_package_rows(changed,indexes)

    def test_duplicate_signed_index_row_refusal(self):
        row=MANIFEST['packages'][0]
        block="\n".join(f"{k}: {row[k]}" for k in ('Package','Version','Architecture','Filename','Size','SHA256'))+'\n\n'
        with self.assertRaisesRegex(Refusal,'duplicate_index_row'):
            check_package_rows(MANIFEST,{row['index']:(block+block).encode()})

    def test_bounded_xz(self):
        import lzma
        self.assertEqual(bounded_xz(lzma.compress(b'abc')),b'abc')
        self.assertEqual(bounded_xz(lzma.compress(b'abc')+lzma.compress(b'def')),b'abcdef')
        with self.assertRaisesRegex(Refusal,'index_expanded'):
            bounded_xz(lzma.compress(b'abc'),limit=2)
        with self.assertRaisesRegex(Refusal,'index_xz'):
            bounded_xz(b'not xz')

    def test_transient_http_retry_and_terminal_errors(self):
        url='https://snapshot.ubuntu.com/ubuntu/20260927T000000Z/test'
        class Response:
            status=200
            def __init__(self, data):self.url=url;self.data=data
            def __enter__(self):return self
            def __exit__(self,*args):pass
            def read(self,n):return self.data[:n]
        def error(code):return urllib.error.HTTPError(url,code,'failure',{},io.BytesIO())
        with mock.patch('urllib.request.build_opener') as open_builder, mock.patch('time.sleep') as delay:
            op=open_builder.return_value
            op.open.side_effect=[error(503),Response(b'abc')]
            self.assertEqual(fetch(url,3,digest(b'abc')),b'abc')
            self.assertEqual(op.open.call_count,2)
            delay.assert_called_once_with(5)
        with mock.patch('urllib.request.build_opener') as open_builder, mock.patch('time.sleep') as delay:
            op=open_builder.return_value
            op.open.side_effect=[error(503),error(503)]
            with self.assertRaisesRegex(Refusal,'fetch_http_503'):
                fetch(url,3,digest(b'abc'))
            self.assertEqual(op.open.call_count,2)
            delay.assert_called_once_with(5)
        with mock.patch('urllib.request.build_opener') as open_builder, mock.patch('time.sleep') as delay:
            op=open_builder.return_value
            op.open.side_effect=error(404)
            with self.assertRaisesRegex(Refusal,'fetch_http_404'):
                fetch(url,3,digest(b'abc'))
            op.open.assert_called_once()
            delay.assert_not_called()
        with mock.patch('urllib.request.build_opener') as open_builder, mock.patch('time.sleep') as delay:
            op=open_builder.return_value
            op.open.side_effect=Refusal('redirect')
            with self.assertRaisesRegex(Refusal,'redirect'):
                fetch(url,3,digest(b'abc'))
            op.open.assert_called_once()
            delay.assert_not_called()

    def test_fetch_refuses_non_snapshot_and_changed_bytes(self):
        for url in ('http://snapshot.ubuntu.com/ubuntu/20260927T000000Z/x', 'https://evil.example/x'):
            with self.assertRaisesRegex(Refusal,'fetch_input'):
                fetch(url, 3, digest(b'abc'))
        class FakeResponse:
            url='https://snapshot.ubuntu.com/ubuntu/20260927T000000Z/x'
            status=200
            def __enter__(self):return self
            def __exit__(self,*args):return None
            def read(self,n):return b'abd'
        with mock.patch('urllib.request.build_opener') as opener:
            opener.return_value.open.return_value=FakeResponse()
            with self.assertRaisesRegex(Refusal,'fetch_digest'):
                fetch(FakeResponse.url, 3, digest(b'abc'))


if __name__=='__main__':unittest.main()
