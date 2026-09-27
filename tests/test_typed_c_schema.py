"""Freeze the typed C contract wire shape."""
import json
import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1] / 'tools' / 'typed_c_contract'))
from schema import SCHEMA, parse

class SchemaTests(unittest.TestCase):
    def setUp(self):
        self.v={'schema':SCHEMA,'workspace_digest':'0'*64,
                'stdout':{'bytes_b64':'T0sK','length':3,'termination':'exact'},
                'exit_code':0,'probe':{'kind':'single-c-main','timeout_ms':5000},
                'edit_scope':{'allow':['main.c'],'deny':[]},'source_predicates':[]}
    def check(self, v=None):
        return parse(json.dumps(self.v if v is None else v, separators=(',',':')).encode())
    def test_exact_bytes(self):
        self.assertEqual(self.check()[1],b'OK\n')
        for raw,expected in [('',b''),('T0s=',b'OK'),('T0sK',b'OK\n'),('QQBC',b'A\x00B'),('DQo=',b'\r\n')]:
            self.v['stdout']={'bytes_b64':raw,'length':len(expected),'termination':'exact'}
            self.assertEqual(self.check()[1],expected)
    def test_reject_shape(self):
        raw=b'{"schema":"a","schema":"b"}'
        with self.assertRaises(ValueError): parse(raw)
        for field,value in [('exit_code',True),('probe',{'kind':'single-c-main','timeout_ms':1}),
                            ('workspace_digest','F'*64),('source_predicates',[{'kind':'call_order'}])]:
            old=self.v[field];self.v[field]=value
            with self.assertRaises(ValueError): self.check()
            self.v[field]=old
        self.v['extra']=1
        with self.assertRaises(ValueError): self.check()
    def test_base64_and_scope(self):
        for raw in ['T0s','T0s=\n','T0s_','T0s===','TR==']:
            self.v['stdout']['bytes_b64']=raw
            with self.subTest(raw=raw), self.assertRaises(ValueError): self.check()
        self.v['stdout']['bytes_b64']='T0sK'
        self.v['edit_scope']['allow']=['../main.c']
        with self.assertRaises(ValueError): self.check()
        self.v['edit_scope']['allow']=['main.c']
        self.v['edit_scope']['deny']=['main.c']
        with self.assertRaises(ValueError): self.check()
    def test_duplicate_nested_keys_and_limits(self):
        base=json.dumps(self.v,separators=(',',':'))
        duplicate=base.replace('"length":3','"length":3,"length":3')
        with self.assertRaises(ValueError):parse(duplicate.encode())
        self.v['stdout']['bytes_b64']='QQ=='
        self.v['stdout']['length']=0
        with self.assertRaises(ValueError):self.check()
        self.v['stdout']['length']=1
        self.check()
        self.v['stdout']['bytes_b64']='QQ='  # missing padding
        with self.assertRaises(ValueError):self.check()
        self.v['stdout']['bytes_b64']='QQ=='
        with self.assertRaises(ValueError):parse(json.dumps(self.v).encode()+b' '*8200)
    def test_path_and_duplicate_predicates(self):
        for bad in ['/main.c','a/../main.c','a//main.c','a/*.c','a\\main.c','a/./main.c']:
            self.v['edit_scope']['allow']=[bad]
            with self.subTest(path=bad),self.assertRaises(ValueError):self.check()
        self.v['edit_scope']['allow']=['main.c']
        p={'kind':'file_bytes_equal','path':'main.c','sha256':'a'*64}
        self.v['source_predicates']=[p,p.copy()]
        with self.assertRaises(ValueError):self.check()
    def test_predicate_bounds(self):
        self.v['source_predicates']=[{'kind':'span_bytes_equal','path':'main.c','start':0,'length':8,'sha256':'a'*64}]
        self.check()
        self.v['source_predicates'][0]['start']=True
        with self.assertRaises(ValueError): self.check()

if __name__=='__main__': unittest.main()
