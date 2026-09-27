#!/usr/bin/env python3
"""Read-only typed contract validator and bounded C candidate preview."""
import argparse
import ctypes
import difflib
import hashlib
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
from schema import parse

MAX_FILES=64
MAX_FILE=256*1024
MAX_TOTAL=4*1024*1024


def file_identity(path, observed=None):
    """Return link count and stable metadata; Windows DirEntry.stat is incomplete."""
    if os.name != 'nt':
        st=observed if observed is not None else path.stat(follow_symlinks=False)
        return st.st_nlink,st
    from ctypes import wintypes
    class FILETIME(ctypes.Structure):
        _fields_=[('low',wintypes.DWORD),('high',wintypes.DWORD)]
    class BY_HANDLE_FILE_INFORMATION(ctypes.Structure):
        _fields_=[('attributes',wintypes.DWORD),('created',FILETIME),
                  ('accessed',FILETIME),('written',FILETIME),('volume',wintypes.DWORD),
                  ('high',wintypes.DWORD),('low',wintypes.DWORD),('links',wintypes.DWORD),
                  ('index_high',wintypes.DWORD),('index_low',wintypes.DWORD)]
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,
                            wintypes.LPVOID,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
    k.CreateFileW.restype=wintypes.HANDLE
    k.GetFileInformationByHandle.argtypes=[wintypes.HANDLE,ctypes.POINTER(BY_HANDLE_FILE_INFORMATION)]
    k.GetFileInformationByHandle.restype=wintypes.BOOL
    k.CloseHandle.argtypes=[wintypes.HANDLE];k.CloseHandle.restype=wintypes.BOOL
    handle=k.CreateFileW(str(path),0x80,0x1|0x2|0x4,None,3,0x200000,None)
    if handle in (None,wintypes.HANDLE(-1).value): fail('file_identity')
    try:
        info=BY_HANDLE_FILE_INFORMATION()
        if not k.GetFileInformationByHandle(handle,ctypes.byref(info)):
            fail('file_identity')
        if info.attributes & 0x400: fail('symlink')
        identity=(info.volume,info.index_high,info.index_low,
                  info.high,info.low,info.written.high,info.written.low)
        return info.links,identity
    finally:
        k.CloseHandle(handle)

def sha(data): return hashlib.sha256(data).hexdigest()
def fail(reason): raise ValueError(reason)

def snapshot(root):
    if root.is_symlink() or not root.is_dir(): fail('workspace_root')
    files={};total=0
    def walk(folder, prefix='', depth=0):
        nonlocal total
        if depth>8: fail('depth')
        with os.scandir(folder) as iterator:
            entries=list(iterator)
        for entry in entries:
            name=entry.name
            if any(ord(ch)<32 or ord(ch)>126 for ch in name): fail('path_encoding')
            if not prefix and name=='.symbols':
                if entry.is_symlink() or not entry.is_dir(follow_symlinks=False): fail('audit_dir')
                continue
            if name=='.git': fail('git_dir')
            rel=f'{prefix}/{name}' if prefix else name
            if len(rel.encode())>=512: fail('path_limit')
            if entry.is_symlink(): fail('symlink')
            st=entry.stat(follow_symlinks=False)
            if stat.S_ISDIR(st.st_mode): walk(Path(entry.path), rel, depth+1)
            elif stat.S_ISREG(st.st_mode):
                path=Path(entry.path)
                links,identity=file_identity(path,st)
                if links!=1 or st.st_size>MAX_FILE or len(files)>=MAX_FILES: fail('file_limit')
                data=path.read_bytes()
                after_links,after_identity=file_identity(path)
                if after_links!=1: fail('file_limit')
                if after_identity!=identity: fail('binary_or_race')
                if len(data)!=st.st_size or b'\x00' in data: fail('binary_or_race')
                total+=len(data)
                if total>MAX_TOTAL: fail('total_limit')
                files[rel]=data
            else: fail('unsupported_entry')
    walk(root)
    ordered=sorted(files.items(),key=lambda x:x[0].encode())
    encoded=b''.join(len(name.encode()).to_bytes(4,'big')+name.encode()+len(data).to_bytes(8,'big')+sha(data).encode('ascii') for name,data in ordered)
    return files,sha(encoded)

def source_checks(files, contract, original=None):
    scope=contract['edit_scope']
    if any(p not in files or not p.endswith('.c') for p in scope['allow']): fail('allow_path')
    if any(p not in files for p in scope['deny']): fail('deny_path')
    for pred in contract['source_predicates']:
        path=pred['path']
        if path not in files: fail('predicate_path')
        data=files[path]
        if pred['kind']=='file_bytes_equal': region=data
        else:
            a,b=pred['start'],pred['length']
            if a+b>len(data): fail('span_range')
            region=data[a:a+b]
        if sha(region)!=pred['sha256']: fail('predicate_mismatch')
        if original is not None and pred['kind']=='span_bytes_equal':
            a,b=pred['start'],pred['length']
            if original[path][a:a+b]!=region: fail('span_changed')
    return True

def static_candidates(files, contract, expected):
    if not any(p.endswith('.c') for p in files): fail('no_c_source')
    generator=Path(__file__).resolve().parents[2]/'build'/'c_contract_static'
    if not generator.is_file() or generator.is_symlink(): fail('generator_unavailable')
    if generator.parent.is_symlink() or generator.resolve()!=generator: fail('generator_unavailable')
    # The legacy generator's answer-literal guard is C-string based. An
    # unrepresentable answer refuses; never disable this guard for coverage.
    if b'\x00' in expected or any(ch>127 for ch in expected):
        fail('literal_guard_unrepresentable')
    goal=expected.decode('ascii')
    entries=[]
    with tempfile.TemporaryDirectory(prefix='symbols-static-c-') as d:
        work=Path(d)
        for index,path in enumerate(contract['edit_scope']['allow']):
            if path in contract['edit_scope']['deny']: fail('scope_conflict')
            if path not in files or not path.endswith('.c'): fail('allow_path')
            text=files[path]
            if len(text)>65536: fail('generator_source_limit')
            source=work/f'source-{index}.c';source.write_bytes(text)
            output=work/f'output-{index}';output.mkdir()
            proc=subprocess.run([str(generator.resolve()),str(source),str(output),goal],
                                capture_output=True,text=True,timeout=5,check=False)
            if proc.returncode: fail('generator_failed')
            lines=proc.stdout.splitlines()
            if len(lines)>96: fail('candidate_limit')
            seen=set()
            for line in lines:
                parts=line.split('\t')
                if (len(parts)!=3 or parts[0] not in ('1','2','3','4') or
                   parts[2] not in {f'{i:03d}.c' for i in range(len(lines))}):
                    fail('generator_output')
                if parts[2] in seen: fail('generator_output')
                seen.add(parts[2])
                candidate_path=output/parts[2]
                if candidate_path.is_symlink() or not candidate_path.is_file(): fail('generator_output')
                candidate=candidate_path.read_bytes()
                if not candidate or len(candidate)>MAX_FILE or b'\x00' in candidate: fail('candidate_bytes')
                if candidate==text: fail('candidate_no_change')
                changed=dict(files);changed[path]=candidate
                try: source_checks(changed,contract,files)
                except ValueError as exc:
                    if str(exc) in ('predicate_mismatch','span_changed','span_range'):continue
                    raise
                old_lines=text.decode('utf-8').splitlines(keepends=True)
                new_lines=candidate.decode('utf-8').splitlines(keepends=True)
                patch=''.join(difflib.unified_diff(old_lines,new_lines,fromfile='before/'+path,
                                                   tofile='after/'+path,n=2))
                if len(patch.encode('utf-8'))>4096: fail('candidate_patch_limit')
                entries.append({'tier':int(parts[0]),'rule':parts[1],
                                'path':path,'after_sha256':sha(candidate),
                                'before_sha256':sha(text),'unified_diff':patch})
            if len(entries)>384: fail('candidate_limit')
    return entries

def validate(contract_file, root):
    contract_file=Path(contract_file);root=Path(root)
    if contract_file.is_symlink() or not contract_file.is_file(): fail('contract_file')
    if file_identity(contract_file,contract_file.stat())[0] != 1: fail('contract_link')
    if contract_file.stat().st_size>8192: fail('contract_limit')
    if root.resolve()==contract_file.resolve() or root.resolve() in contract_file.resolve().parents: fail('contract_in_workspace')
    raw=contract_file.read_bytes();obj,expected,identity=parse(raw)
    files,digest=snapshot(root)
    if digest!=obj['workspace_digest']: fail('workspace_digest')
    source_checks(files,obj)
    candidates=static_candidates(files,obj,expected)
    after,after_digest=snapshot(root)
    if after_digest!=digest or after!=files: fail('workspace_changed_during_preview')
    return {'status':'static_candidate_unverified' if candidates else 'no_static_candidate',
            'promise':'contract validated + static candidate preview; stdout NOT verified, nothing executed',
            'not_executed':True,'provenance':'operator-supplied/identity-unverified',
            'contract_sha256':identity,'workspace_digest':digest,
            'candidate_count':len(candidates),'candidates':candidates}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--preview-c-contract',required=True)
    ap.add_argument('-w','--workspace',required=True)
    args=ap.parse_args()
    try:
        import json
        print(json.dumps(validate(args.preview_c_contract,args.workspace),sort_keys=True))
    except (ValueError,OSError,UnicodeError,subprocess.TimeoutExpired) as e:
        reason=str(e)
        if reason not in {'limit','utf8_bom','duplicate_key','fields','version','digest','stdout',
            'base64','exit','probe','scope','predicates','path','span','predicate',
            'contract_file','contract_link','contract_limit','contract_in_workspace',
            'workspace_root','file_identity','depth','audit_dir','git_dir','path_limit','path_encoding',
            'symlink','file_limit','binary_or_race','total_limit','unsupported_entry',
            'allow_path','deny_path','predicate_path','span_range','predicate_mismatch',
            'span_changed','no_c_source','generator_unavailable','generator_source_limit',
            'literal_guard_unrepresentable','scope_conflict','generator_failed',
            'candidate_limit','generator_output','candidate_bytes','candidate_no_change',
            'candidate_patch_limit','workspace_digest','workspace_changed_during_preview'}:
            reason='invalid_or_unavailable'
        print('refused: '+reason,file=sys.stderr);return 2
    return 0
if __name__=='__main__':sys.exit(main())
