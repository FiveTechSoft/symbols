#!/usr/bin/env python3
"""Fixed, minimal boot-only newc image. No QEMU or target-code execution."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import stat

HERE = Path(__file__).resolve().parent
BUSYBOX_SHA = 'dbac288c29ba568459550a2da9e7ae0ded6b1fc728ee9fad3044c44e62d6ac14'
BUSYBOX_SIZE = 2124608
EXPECTED = [('bin','dir',0o755,None), ('bin/busybox','file',0o755,'busybox'),
            ('dev','dir',0o755,None),('init','file',0o755,'init'),
            ('proc','dir',0o755,None),('run','dir',0o755,None),
            ('supervisor','file',0o644,'supervisor'),('sys','dir',0o755,None),
            ('tmp','dir',0o1777,None)]
SCHEMA = 'symbols.minimal-initramfs.v1'

class Refusal(ValueError): pass

def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def checked_sources(manifest: dict, busybox: Path) -> dict[str, bytes]:
    if (set(manifest) != {'schema','source_snapshot','busybox','files','archive_format','archive_uid','archive_gid','archive_mtime'} or
        manifest['schema'] != SCHEMA or manifest['source_snapshot'] != '20260927T000000Z' or
        manifest['archive_format'] != 'newc-070701' or
        (manifest['archive_uid'],manifest['archive_gid'],manifest['archive_mtime']) != (0,0,0)):
        raise Refusal('manifest_schema')
    b = manifest['busybox']
    if b != {'source_package':'busybox-static','deb_sha256':'944b2728f53ceb3916cec2c962873c9951e612408099601751db2a0a5d81e0ed',
             'source_path':'usr/bin/busybox','size':BUSYBOX_SIZE,'sha256':BUSYBOX_SHA}:
        raise Refusal('busybox_source')
    rows = manifest['files']
    if len(rows) != len(EXPECTED): raise Refusal('file_count')
    for row, (path, kind, mode, source) in zip(rows,EXPECTED):
        needed={'path','kind','mode'} | ({'source'} if source else set())
        if source in ('init','supervisor'): needed |= {'size','sha256'}
        if set(row)!=needed or (row['path'],row['kind'],row['mode'],row.get('source'))!=(path,kind,mode,source):
            raise Refusal('file_row')
    sources={}
    for name in ('init','supervisor'):
        p=HERE/name
        if p.is_symlink() or not p.is_file() or p.stat().st_size>4096: raise Refusal('script_input')
        raw=p.read_bytes()
        # Windows checkouts can CRLF-convert these fixed text files. Restore
        # canonical LF bytes only if the complete pinned digest still matches.
        if b'\r' in raw:
            raw=raw.replace(b'\r\n',b'\n')
            if b'\r' in raw: raise Refusal('script_line_endings')
        row=next(x for x in rows if x['path']==name)
        if len(raw)!=row['size'] or not raw.startswith(b'#!/bin/busybox sh\n') or sha(raw)!=row['sha256']:
            raise Refusal('script_digest')
        sources[name]=raw
    if busybox.is_symlink() or not busybox.is_file() or busybox.stat().st_size != BUSYBOX_SIZE:
        raise Refusal('busybox_input')
    raw=busybox.read_bytes()
    if len(raw)!=BUSYBOX_SIZE or sha(raw)!=BUSYBOX_SHA:
        raise Refusal('busybox_digest')
    if not raw.startswith(b'\x7fELF'):
        raise Refusal('busybox_elf')
    sources['busybox']=raw
    return sources

def entry(name: str, data: bytes, mode: int, inode: int) -> bytes:
    if not re.fullmatch(r'[A-Za-z0-9/!.]+',name) or name.startswith('/') or any(part in ('','.','..') for part in name.split('/')):
        raise Refusal('path')
    if inode < 1 or inode > 0xffffffff or len(data)>4_000_000: raise Refusal('entry_limit')
    name_raw=name.encode('ascii')+b'\0'
    # Linux newc: magic + thirteen 8-hex-digit fields, 4-byte aligned name/data.
    fields=(inode,mode,0,0,1,0,len(data),0,0,0,0,len(name_raw),0)
    header=b'070701'+b''.join(f'{n:08x}'.encode('ascii') for n in fields)+name_raw
    header+=b'\0'*((-len(header))%4)
    return header+data+b'\0'*((-len(data))%4)

def assemble(manifest: dict, sources: dict[str, bytes]) -> bytes:
    chunks=[]
    for inode,row in enumerate(manifest['files'],1):
        mode=(stat.S_IFDIR if row['kind']=='dir' else stat.S_IFREG)|row['mode']
        chunks.append(entry(row['path'],sources[row['source']] if row['kind']=='file' else b'',mode,inode))
    chunks.append(entry('TRAILER!!!',b'',0,len(chunks)+1))
    return b''.join(chunks)

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--busybox',type=Path,required=True,help='Already verified static BusyBox extracted from signed-index deb')
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    if a.out.exists(): raise Refusal('out_exists')
    manifest=json.loads((HERE/'manifest.json').read_bytes())
    data=assemble(manifest,checked_sources(manifest,a.busybox))
    a.out.write_bytes(data)
    print(f'newc {len(data)} {sha(data)}')

if __name__=='__main__':main()
