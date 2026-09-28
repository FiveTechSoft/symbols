"""Semantic measured-only lock binding tests. No artifacts executed."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest
from unittest import mock
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'/'typed_c_contract'))
from lock_wire import decode_lock_wire, encode_lock_wire
from verify_lock import LockRefusal, EXCLUSIONS, verify_lock
FIXTURE = Path(__file__).with_name('typed_c_measured_lock.sbl1')
SHA256 = '52dd50f15f0c5638a02ef5b750ad756a07ed7925281dfb36606c2af474b978b8'
NOW = '2026-09-28T12:55:00Z'


def context(lock):
    expected = {k:copy.deepcopy(lock[k]) for k in ('packages','files','directories','exclusions','provenance','image','snapshot_id')}
    expected['raw_setuid_paths'] = sorted(path for _,path in EXCLUSIONS)
    return expected


class SemanticTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=FIXTURE.read_bytes()
        assert hashlib.sha256(raw).hexdigest()==SHA256
        cls.original=decode_lock_wire(raw)
        assert encode_lock_wire(cls.original)==raw
        cls.trusted=context(cls.original)

    def refuse(self, mutation, code, trusted=None, **kwargs):
        candidate=copy.deepcopy(self.original)
        mutation(candidate)
        with self.assertRaises(LockRefusal) as error:
            verify_lock(candidate,trusted=copy.deepcopy(self.trusted if trusted is None else trusted),now_utc=NOW,**kwargs)
        self.assertEqual(error.exception.code,code)

    def test_full_measured_fixture_evidence_only(self):
        lock=self.original
        self.assertEqual((len(lock),len(lock['packages']),len(lock['files']),len(lock['directories']),len(lock['exclusions'])),(12,128,5283,698,8))
        self.assertEqual(sum(x['size'] for x in lock['packages']),146235932)
        self.assertEqual(sum(x['size'] or 0 for x in lock['files']),383592580)
        self.assertEqual(lock['image']['kernel_sha256'],'cd5fcfd260b91782637b7b4e221a48e656358f6eef549602540e62380b6f2f2c')
        self.assertEqual(lock['image']['qemu_sha256'],'8a35ccba41582fc6c38b9df85fc9e35fa1d42f414d2d7d8090ee9b2f5e7c0854')
        self.assertEqual(lock['image']['busybox_sha256'],'dbac288c29ba568459550a2da9e7ae0ded6b1fc728ee9fad3044c44e62d6ac14')
        with mock.patch('subprocess.Popen',side_effect=AssertionError('process launch')):
            self.assertEqual(verify_lock(lock,trusted=self.trusted,now_utc=NOW),{'status':'validated_evidence_only','launch_eligible':False})

    def test_fields_type_order_caps(self):
        self.refuse(lambda x:x.__setitem__('unknown',1),'fields')
        self.refuse(lambda x:x['builder'].__setitem__('extra',1),'fields')
        self.refuse(lambda x:x['files'][0].__setitem__('extra',1),'fields')
        self.refuse(lambda x:x['directories'][0].pop('mode'),'fields')
        self.refuse(lambda x:x['provenance']['inrelease_sha256'].__setitem__('extra','0'*64),'fields')
        self.refuse(lambda x:x.__setitem__('architecture','arm64'),'architecture')
        self.refuse(lambda x:x.__setitem__('snapshot_id','20260932T000000Z'),'snapshot')
        self.refuse(lambda x:x['packages'][0].__setitem__('size',True),'type')
        self.refuse(lambda x:x['files'][0].__setitem__('mode',0o4755),'type')
        self.refuse(lambda x:x['files'].reverse(),'order')
        self.refuse(lambda x:x['packages'].append(copy.deepcopy(x['packages'][-1])),'order')
        self.refuse(lambda x:x['files'][0].__setitem__('size',2**30+1),'type')
        self.refuse(lambda x:x['files'][0].__setitem__('path','../escape'),'path')

    def test_context_exclusions_setuid_state(self):
        self.refuse(lambda x:next(row for row in x['files'] if row['type']=='file').__setitem__('sha256','0'*64),'context')
        self.refuse(lambda x:x['packages'][0].__setitem__('size',x['packages'][0]['size']+1),'context')
        self.refuse(lambda x:x['directories'][0].__setitem__('mode',0o700),'context')
        self.refuse(lambda x:x['exclusions'][0].__setitem__('reason','wrong'),'exclusions')
        t=context(self.original);t['raw_setuid_paths'].append('usr/bin/new-setuid')
        self.refuse(lambda x:None,'setuid',trusted=t)
        self.refuse(lambda x:x['builder'].__setitem__('state','pretend-pinned'),'state')
        self.refuse(lambda x:x.__setitem__('state','pinned_image'),'pin_incomplete')
        self.refuse(lambda x:x['files'][0].__setitem__('path','usr/bin/passwd'),'order')

    def test_precedence(self):
        # Field/schema check wins over context, then type/order, then context, then exclusions.
        self.refuse(lambda x:(x.__setitem__('unknown',1),next(row for row in x['files'] if row['type']=='file').__setitem__('sha256','0'*64)),'fields')
        self.refuse(lambda x:(x['packages'][0].__setitem__('size',True),next(row for row in x['files'] if row['type']=='file').__setitem__('sha256','0'*64)),'type')
        self.refuse(lambda x:(next(row for row in x['files'] if row['type']=='file').__setitem__('sha256','0'*64),x['exclusions'][0].__setitem__('reason','wrong')),'context')

if __name__ == '__main__':unittest.main()
