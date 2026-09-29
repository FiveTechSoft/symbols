"""Exact newc vectors and refusal paths; never boot or run target code."""
import copy
import hashlib
import json
from pathlib import Path
import stat
import sys
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'minimal_initramfs'))
import pack
from pack import HERE, BUSYBOX_SHA, BUSYBOX_SIZE, Refusal, assemble, checked_sources, entry, sha

MANIFEST=json.loads((HERE/'manifest.json').read_bytes())


def parse_newc(raw):
    """Test-side independent decoder: validate header/align/closed entries."""
    pos=0; result=[]
    while True:
        assert raw[pos:pos+6]==b'070701'
        fields=[int(raw[pos+6+i*8:pos+14+i*8],16) for i in range(13)]
        pos+=110
        size,namesize=fields[6],fields[11]
        name=raw[pos:pos+namesize]
        assert name.endswith(b'\0') and len(name)==namesize
        pos+=namesize
        pos+=(-pos)%4
        data=raw[pos:pos+size]
        assert len(data)==size
        pos+=size
        pos+=(-pos)%4
        if name==b'TRAILER!!!\0':
            assert size==0 and pos==len(raw)
            return result
        result.append((name[:-1].decode('ascii'),fields,data))


class NewcTests(unittest.TestCase):
    def test_minimal_image_exact_bytes(self):
        source={'busybox':b'\x7fELF'+b'B'*(BUSYBOX_SIZE-4),
                'init':(HERE/'init').read_bytes(),'supervisor':(HERE/'supervisor').read_bytes()}
        a=assemble(MANIFEST,source)
        self.assertEqual(a,assemble(MANIFEST,source))
        rows=parse_newc(a)
        self.assertEqual([r[0] for r in rows],[r['path'] for r in MANIFEST['files']])
        self.assertEqual(len(rows),9)
        for path,fields,data in rows:
            m=next(r for r in MANIFEST['files'] if r['path']==path)
            self.assertEqual(fields[1],(stat.S_IFDIR if m['kind']=='dir' else stat.S_IFREG)|m['mode'])
            self.assertEqual((fields[2],fields[3],fields[5],fields[6],fields[7],fields[8],fields[9],fields[10],fields[12]),
                             (0,0,0,len(data),0,0,0,0,0))
            self.assertEqual(data,source[m['source']] if m['kind']=='file' else b'')
        self.assertTrue(set(x[0] for x in rows).isdisjoint({'dev/console','etc','usr','root'}))

    def test_windows_crlf_checkout_canonicalized_only_under_digest(self):
        with tempfile.TemporaryDirectory() as directory:
            here=Path(directory)
            for name in ('init','supervisor'):
                (here/name).write_bytes((HERE/name).read_bytes().replace(b'\n',b'\r\n'))
            busybox=here/'busybox'
            busybox.write_bytes(b'\x7fELF'+b'B'*(BUSYBOX_SIZE-4))
            with mock.patch.object(pack,'HERE',here):
                with self.assertRaisesRegex(Refusal,'busybox_digest'):
                    checked_sources(MANIFEST,busybox)
                (here/'init').write_bytes((HERE/'init').read_bytes().replace(b'\n',b'\r\n')+b'\r')
                with self.assertRaisesRegex(Refusal,'script_line_endings'):
                    checked_sources(MANIFEST,busybox)
                (here/'init').write_bytes((HERE/'init').read_bytes().replace(b'\n',b'\r\n')+b'X')
                with self.assertRaisesRegex(Refusal,'script_digest'):
                    checked_sources(MANIFEST,busybox)

    def test_input_and_manifest_refusals(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'busybox'
            path.write_bytes(b'\x7fELF'+b'B'*(BUSYBOX_SIZE-4))
            with self.assertRaisesRegex(Refusal,'busybox_digest'):
                checked_sources(MANIFEST,path)
            path.write_bytes(b'A')
            with self.assertRaisesRegex(Refusal,'busybox_input'):
                checked_sources(MANIFEST,path)
            changed=copy.deepcopy(MANIFEST)
            changed['files'].append(changed['files'][-1])
            with self.assertRaisesRegex(Refusal,'file_count'):
                checked_sources(changed,path)
            changed=copy.deepcopy(MANIFEST)
            changed['files'][0]['path']='../outside'
            with self.assertRaisesRegex(Refusal,'file_row'):
                checked_sources(changed,path)
            changed=copy.deepcopy(MANIFEST)
            changed['files'][3]['size']+=1
            with self.assertRaisesRegex(Refusal,'script_digest'):
                checked_sources(changed,path)
            changed=copy.deepcopy(MANIFEST)
            changed['busybox']['sha256']='0'*64
            with self.assertRaisesRegex(Refusal,'busybox_source'):
                checked_sources(changed,path)
        for name in ('../escape','/absolute','a//b'):
            with self.assertRaises(Refusal):entry(name,b'',stat.S_IFDIR|0o755,1)

if __name__=='__main__':unittest.main()
