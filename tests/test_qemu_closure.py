"""Pure ELF and closure vectors. No QEMU boot, host execution or network."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'/'qemu_closure'))
import closure
from elf import dependencies,Refusal as ElfRefusal


def elf(needed=(), interpreter=None, search=False):
    # Independent minimal ELF64 fixture: two loadable segments plus PT_DYNAMIC.
    raw=bytearray(2048);raw[:16]=b'\x7fELF\x02\x01\x01'+b'\0'*9
    struct.pack_into('<HHI',raw,16,3,62,1)
    struct.pack_into('<Q',raw,32,64)
    struct.pack_into('<HH',raw,54,56,3 if interpreter else 2)
    start=64
    struct.pack_into('<IIQQQQQQ',raw,start,1,4,512,0x1000,0,1024,1024,4096)
    struct.pack_into('<IIQQQQQQ',raw,start+56,2,4,1024,0x1200,0,16*(len(needed)+4+(1 if search else 0)),16*(len(needed)+4+(1 if search else 0)),8)
    if interpreter:
        b=interpreter.encode()+b'\0';raw[1536:1536+len(b)]=b
        struct.pack_into('<IIQQQQQQ',raw,start+112,3,4,1536,0,0,len(b),len(b),1)
    strings=b'\0'; offsets=[]
    for name in needed:
        offsets.append(len(strings));strings+=name.encode()+b'\0'
    raw[600:600+len(strings)]=strings
    values=[(5,0x1000+88),(10,len(strings))]+[(1,v) for v in offsets]
    if search:values.append((29,0))
    values.append((0,0))
    for i,(tag,val) in enumerate(values):struct.pack_into('<qQ',raw,1024+16*i,tag,val)
    return bytes(raw)


class ClosureTests(unittest.TestCase):
    def test_elf_vectors(self):
        self.assertEqual(dependencies(elf(['liba.so.1','libb.so.2'],'/lib64/ld-linux-x86-64.so.2')),
                         ('/lib64/ld-linux-x86-64.so.2',('liba.so.1','libb.so.2')))
        for raw in (elf(['../escape']),elf(['liba.so'],search=True),elf(['liba.so'])[:100],b'bad'):
            with self.subTest(raw=raw[:12]),self.assertRaises(ElfRefusal):dependencies(raw)

    def test_static_closure_and_refusals(self):
        exe=elf(['liba.so'], '/lib64/ld-linux-x86-64.so.2')
        lib=elf(['libc.so'])
        libc=elf([])
        loader=elf([])
        paths={closure.BINARY:exe, closure.LOADER:loader,
               closure.LIB+'liba.so.1':lib,closure.LIB+'libc.so':libc}
        def row(name,data):return {'sha256':hashlib.sha256(data).hexdigest(),'size':len(data),
                                   'origin_package':name,'payload_sha256':hashlib.sha256(name.encode()).hexdigest()}
        source={p:row(p.replace('/','_'),b) for p,b in paths.items()}
        source[closure.LIB+'liba.so']={'link':'liba.so.1','origin_package':source[closure.LIB+'liba.so.1']['origin_package'],
                                      'payload_sha256':source[closure.LIB+'liba.so.1']['payload_sha256']}
        manifest={'source':source,'decisions':{},'included':[{'path':x} for x in paths],
                  'archive_sha256':'x','archive_size':1,'expanded_bytes':1,'bootstrap_paths':[]}
        packages={r['origin_package']:{'SHA256':r['payload_sha256'],'Architecture':'amd64'} for r in source.values()}
        def run(m,reader=paths.__getitem__,pkgs=packages):
            return closure.records(m,reader,pkgs,
                expected_binary_sha=source[closure.BINARY]['sha256'],
                expected_loader_sha=source[closure.LOADER]['sha256'],
                expected_archive_sha='x',expected_archive_size=1)
        result=run(manifest)
        self.assertFalse(result['runtime_complete'])
        self.assertEqual(set(result['files']),set(paths))
        changed=copy.deepcopy(manifest); changed['source'][closure.LIB+'liba.so']['link']='../evil'
        with self.assertRaisesRegex(closure.Refusal,'link_target'):run(changed)
        changed=copy.deepcopy(manifest); changed['source'][closure.BINARY]['sha256']='0'*64
        with self.assertRaisesRegex(closure.Refusal,'qemu_candidate'):run(changed)
        with self.assertRaisesRegex(closure.Refusal,'file_digest'):
            run(manifest,lambda p: paths[p]+b'X')
        changed=copy.deepcopy(manifest); changed['source'].pop(closure.LIB+'libc.so')
        with self.assertRaisesRegex(closure.Refusal,'dependency_resolution'):run(changed)

if __name__=='__main__':unittest.main()
