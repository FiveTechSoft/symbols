"""Offline trace candidate tests; no subprocess or guest boot."""
import hashlib
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'qemu_closure'))
from trace import Refusal,events,reconcile

class TraceTests(unittest.TestCase):
    def test_parse_exact_and_refuse_incomplete(self):
        raw=[
            'execve("/stage/usr/bin/qemu-system-x86_64", ["qemu"], 0x0) = 0',
            '[pid 123] openat(AT_FDCWD, "/stage/usr/lib/x86_64-linux-gnu/liba.so.1", O_RDONLY|O_CLOEXEC) = 3',
            'openat(AT_FDCWD, "/stage/usr/lib/x86_64-linux-gnu/libmissing.so", O_RDONLY) = -1 ENOENT (No such file or directory)',
            '+++ exited with 0 +++']
        self.assertEqual(events(raw),{'opened':['/stage/usr/bin/qemu-system-x86_64','/stage/usr/lib/x86_64-linux-gnu/liba.so.1'],
                                      'unsuccessful_probes':['/stage/usr/lib/x86_64-linux-gnu/libmissing.so']})
        for bad in (raw+['openat(AT_FDCWD, "relative", O_RDONLY) = 4'],
                    raw+['openat(3, "/stage/etc/passwd", O_RDONLY) = 4'],
                    raw+['[pid 123] openat(AT_FDCWD, "/stage/x", <unfinished ...>'],
                    raw+['newfstatat(AT_FDCWD, "/stage/x", 0, 0) = 0'],
                    raw+['openat(AT_FDCWD, "/stage/../etc/passwd", O_RDONLY) = 4']):
            with self.subTest(bad=bad[-1]),self.assertRaises(Refusal):events(bad)
        with self.assertRaisesRegex(Refusal,'trace_no_exec'):events(raw[1:])
        with self.assertRaisesRegex(Refusal,'trace_no_exit'):events(raw[:-1])
        with self.assertRaisesRegex(Refusal,'trace_exit'):events(raw[:-1]+['+++ exited with 1 +++'])

    def test_reconcile_drift_and_unseen(self):
        exe=b'ELF1';lib=b'ELF2';prefix='/stage'
        inventory={'source':{'usr/bin/qemu-system-x86_64':{'size':4,'sha256':hashlib.sha256(exe).hexdigest()},
                             'usr/lib/x86_64-linux-gnu/liba.so.1':{'size':4,'sha256':hashlib.sha256(lib).hexdigest()}}}
        observed={'opened':['/stage/usr/bin/qemu-system-x86_64','/stage/usr/lib/x86_64-linux-gnu/liba.so.1'],
                  'unsuccessful_probes':[]}
        static={'schema':'symbols.qemu-static-closure-candidate.v1','runtime_complete':False,
                'files':{'usr/bin/qemu-system-x86_64':{},'usr/lib/x86_64-linux-gnu/liba.so.1':{}}}
        content={observed['opened'][0]:exe,observed['opened'][1]:lib}
        result=reconcile(observed,prefix=prefix,static=static,inventory=inventory,host_read=content.__getitem__)
        self.assertFalse(result['runtime_complete'])
        with self.assertRaisesRegex(Refusal,'host_drift'):
            reconcile(observed,prefix=prefix,static=static,inventory=inventory,host_read=lambda p: content[p]+b'X')
        with self.assertRaisesRegex(Refusal,'unseen_static_dependency'):
            reconcile({'opened':observed['opened'][:1],'unsuccessful_probes':[]},prefix=prefix,static=static,
                      inventory=inventory,host_read=content.__getitem__)
        with self.assertRaisesRegex(Refusal,'host_external_file'):
            reconcile({'opened':sorted(observed['opened']+['/etc/passwd']),'unsuccessful_probes':[]},prefix=prefix,
                      static=static,inventory=inventory,host_read=content.__getitem__)

if __name__=='__main__':unittest.main()
