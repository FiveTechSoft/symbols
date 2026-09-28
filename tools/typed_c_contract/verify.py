#!/usr/bin/env python3
"""V2 contract gate. No production backend can compile or run target code."""
import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from schema_v2 import parse
from preview import snapshot, source_checks, file_identity, static_candidates, sha, fail

PROMISE = 'isolated candidate observation only; not a verified repair'
PROVENANCE = 'operator-supplied/identity-unverified'
REASONS = {'limit','utf8_bom','duplicate_key','fields','version','digest','stdout',
           'base64','exit','probe','scope','predicates','path','span','predicate',
           'contract_file','contract_link','contract_limit','contract_in_workspace',
           'workspace_root','file_identity','depth','audit_dir','git_dir','path_limit',
           'path_encoding','symlink','file_limit','binary_or_race','total_limit',
           'unsupported_entry','workspace_digest','workspace_changed_during_preview',
           'allow_path','deny_path','predicate_path','span_range','predicate_mismatch',
           'span_changed','generator_unavailable','generator_source_limit',
           'literal_guard_unrepresentable','scope_conflict','generator_failed',
           'candidate_limit','generator_output','candidate_bytes','candidate_no_change',
           'candidate_patch_limit'}
SOURCE = re.compile(
    rb'\A[ \t]*\#include <stdio\.h>[ \t\r]*\n[ \t\r\n]*int[ \t]+main[ \t]*\([ \t]*(?:void[ \t]*)?\)[ \t\r\n]*\{[ \t\r\n]*'
    rb'(?:(?P<decl>int\s+(?P<name>[A-Za-z_][A-Za-z_0-9]*)\s*=\s*(?P<value>-?(?:0|[1-9][0-9]*))\s*;\s*))?'
    rb'(?P<call>printf|puts)\s*\(\s*"(?P<literal>(?:[^"\\\r\n]|\\[nrt"\\])*)"\s*'
    rb'(?P<arg>,\s*(?P<ref>[A-Za-z_][A-Za-z_0-9]*)\s*)?\)\s*;\s*return\s+0\s*;\s*\}\s*\Z',
    re.ASCII)

def shape(data):
    if len(data)>65536 or b'??' in data or any(b>127 or b==0 for b in data): return None
    if any(b==13 for b in data): return None
    match=SOURCE.fullmatch(data)
    if match is None: return None
    name,value,call,ref=match.group('name','value','call','ref')
    if value is None and (ref is not None or (call not in (b'printf',b'puts'))): return None
    if value is not None and (call!=b'printf' or ref!=name or len(name)>64 or name==b'main'): return None
    if value is not None:
        try: n=int(value)
        except ValueError: return None
        if not -(2**31)<=n<2**31 or str(n).encode()!=value: return None
    raw=match.group('literal');decoded=bytearray();i=0
    escapes={ord('n'):10,ord('r'):13,ord('t'):9,ord('"'):34,ord('\\'):92}
    while i<len(raw):
        b=raw[i];i+=1
        if b==92:
            if i>=len(raw): return None
            b=escapes.get(raw[i]);i+=1
        if b is None or (b<32 and b not in (9,10,13)) or b>126: return None
        decoded.append(b)
    fmt=bytes(decoded);conversion=0;idx=0
    if call==b'puts':return match
    while idx<len(fmt):
        if fmt[idx]!=37: idx+=1;continue
        idx+=1
        if idx>=len(fmt): return None
        if fmt[idx] in b'diu' and value is not None:
            conversion+=1
            if conversion>1: return None
            if fmt[idx]==ord('u') and n<0: return None
        elif fmt[idx]!=37: return None
        idx+=1
    if (value is None and conversion) or (value is not None and conversion!=1): return None
    return match

def emitted(match):
    raw=match.group('literal');decoded=bytearray();i=0
    escapes={ord('n'):10,ord('r'):13,ord('t'):9,ord('"'):34,ord('\\'):92}
    while i<len(raw):
        b=raw[i];i+=1
        if b==92:b=escapes[raw[i]];i+=1
        decoded.append(b)
    fmt=bytes(decoded)
    if match.group('call')==b'puts':return fmt+b'\n'
    out=bytearray();i=0;value=match.group('value')
    while i<len(fmt):
        if fmt[i]!=37:out.append(fmt[i]);i+=1;continue
        code=fmt[i+1];i+=2
        if code==37:out.append(37);continue
        n=int(value);out.extend(str(n).encode())
    return bytes(out)

def apply_diff(original,patch,path):
    try:
        text=original.decode('ascii'); lines=text.splitlines(keepends=True)
        sections=patch.splitlines(keepends=True)
        if sections[0].rstrip('\n')!='--- before/'+path or sections[1].rstrip('\n')!='+++ after/'+path: return None
        result=[];pos=0;i=2
        while i<len(sections):
            m=re.fullmatch(r'@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@\n?',sections[i]);i+=1
            if not m:return None
            oldstart,oldcount,newstart,newcount=(int(m[1]),int(m[2] or 1),int(m[3]),int(m[4] or 1))
            if oldstart-1<pos or newstart-1!=len(result)+(oldstart-1-pos): return None
            result.extend(lines[pos:oldstart-1]);pos=oldstart-1
            oldseen=newseen=0
            while i<len(sections) and not sections[i].startswith('@@ '):
                item=sections[i];i+=1
                if item.startswith(' '):
                    if pos>=len(lines) or lines[pos]!=item[1:]:return None
                    result.append(lines[pos]);pos+=1;oldseen+=1;newseen+=1
                elif item.startswith('-'):
                    if pos>=len(lines) or lines[pos]!=item[1:]:return None
                    pos+=1;oldseen+=1
                elif item.startswith('+'):
                    result.append(item[1:]);newseen+=1
                elif item=='\\ No newline at end of file\n':return None
                else:return None
            if (oldseen,newseen)!=(oldcount,newcount):return None
        result.extend(lines[pos:]);return ''.join(result).encode('ascii')
    except (UnicodeError,ValueError,IndexError):return None

def eligible(original,after):
    a,b=shape(original),shape(after)
    if a is None or b is None or a.group('call')!=b.group('call'):return False
    if a.group('value') is None:
        if b.group('value') is not None:return False
        start,end=a.span('literal');bs,be=b.span('literal')
    else:
        if b.group('value') is None or a.group('name')!=b.group('name') or a.group('literal')!=b.group('literal'):return False
        start,end=a.span('value');bs,be=b.span('value')
    return original[:start]==after[:bs] and original[end:]==after[be:] and original!=after

def choose(files,contract,expected,producer=static_candidates):
    # A v2 probe never consumes a legacy or multi-file source. No generator
    # process is launched for rejected source envelopes.
    allowed=contract['edit_scope']['allow']
    if len(files)!=1 or len(allowed)!=1 or allowed[0] not in files or not allowed[0].endswith('.c'):
        return 'no_eligible_candidate',None,None
    path=allowed[0];before=files[path]
    if shape(before) is None:return 'no_eligible_candidate',None,None
    entries=producer(files,contract,expected)
    good=[]
    for record in entries:
        if record.get('path')!=path or record.get('rule') not in ('answer_value','answer_literal') or record.get('tier')!=4:
            continue
        after=apply_diff(before,record.get('unified_diff',''),path)
        if after is None or record.get('before_sha256')!=sha(before) or record.get('after_sha256')!=sha(after):continue
        if not eligible(before,after) or emitted(shape(after))!=expected:continue
        if (record['rule']=='answer_value') != (shape(before).group('value') is not None):continue
        try:source_checks({path:after},contract,files)
        except ValueError:continue
        good.append((record,after))
    if len(good)!=1:return ('no_eligible_candidate' if not good else 'ambiguous_candidate'),None,None
    record,after=good[0]
    return 'probe_unavailable',{k:record[k] for k in ('rule','path','before_sha256','after_sha256','tier')},after

class UnavailableBroker:
    def observe(self,request):
        return {'outcome':'unavailable'}

def interpret(response,goal):
    """Internal, test-injectable broker evidence validator; no production backend."""
    outcomes={'compile_error','compile_timeout','run_timeout','run_crash','exit_mismatch',
              'stdout_mismatch','output_limit','infrastructure_error','observed_match'}
    if type(response) is not dict or response.get('outcome') not in outcomes:
        return 'probe_unavailable',False,None
    outcome=response['outcome'];started=response.get('started')
    if type(started) is not bool:return 'probe_unavailable',False,None
    if outcome.startswith('compile_') and started:return 'probe_unavailable',False,None
    if outcome in ('run_timeout','run_crash','exit_mismatch','stdout_mismatch','output_limit','observed_match') and not started:
        return 'probe_unavailable',False,None
    observed=response.get('stdout')
    if outcome=='observed_match':
        if (type(observed) is not bytes or observed!=goal or response.get('exit_code')!=0 or
            response.get('complete') is not True or not all(type(response.get(k)) is str and response[k]
            for k in ('image_id','compiler_id','platform_id')) or
            type(response.get('limits')) is not dict or
            set(response['limits'])!={'wall_ms','memory_bytes','output_bytes'} or
            any(type(v) is not int or v<=0 for v in response['limits'].values())):
            return 'infrastructure_error',started,{'phase':'broker','outcome':'infrastructure_error'}
    else:
        if outcome in ('stdout_mismatch','exit_mismatch') and type(observed) is not bytes:
            return 'infrastructure_error',started,{'phase':'broker','outcome':'infrastructure_error'}
    phase='compile' if outcome.startswith('compile_') else 'broker' if outcome=='infrastructure_error' else 'run'
    obs={'phase':phase,'outcome':outcome}
    if type(observed) is bytes and response.get('complete') is True:
        obs.update(stdout_sha256=sha(observed),stdout_length=len(observed))
    if type(response.get('exit_code')) is int:obs['exit_code']=response['exit_code']
    if outcome=='observed_match':
        obs.update({k:response[k] for k in ('image_id','compiler_id','platform_id','limits')})
    return outcome,started,obs

def verify(contract_file,root,broker=None,producer=static_candidates):
    contract_file=Path(contract_file);root=Path(root)
    if contract_file.is_symlink() or not contract_file.is_file():fail('contract_file')
    if file_identity(contract_file,contract_file.stat())[0]!=1:fail('contract_link')
    if contract_file.stat().st_size>8192:fail('contract_limit')
    if root.resolve()==contract_file.resolve() or root.resolve() in contract_file.resolve().parents:fail('contract_in_workspace')
    obj,expected,identity=parse(contract_file.read_bytes())
    files,digest=snapshot(root)
    if digest!=obj['workspace_digest']:fail('workspace_digest')
    source_checks(files,obj)
    status,candidate,after=choose(files,obj,expected,producer)
    latest,now=snapshot(root)
    if now!=digest or latest!=files:fail('workspace_changed_during_preview')
    executed=False;observation=None
    if status=='probe_unavailable':
        response=(broker or UnavailableBroker()).observe({'candidate':candidate,'after':after,
                  'goal':expected,'timeout_ms':5000,'contract_sha256':identity,'workspace_digest':digest})
        if type(response) is not dict or response.get('outcome')!='unavailable':
            status,executed,observation=interpret(response,expected)
    return {'schema':'symbols.c-repair-contract.v2','status':status,'promise':PROMISE,
            'executed':executed,'provenance':PROVENANCE,'contract_sha256':identity,
            'workspace_digest':digest,'candidate':candidate,'observation':observation}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--verify-c-contract',required=True)
    parser.add_argument('-w','--workspace',required=True)
    args=parser.parse_args()
    try:print(json.dumps(verify(args.verify_c_contract,args.workspace),sort_keys=True,separators=(',',':')))
    except (ValueError,OSError,UnicodeError,subprocess.TimeoutExpired) as exc:
        reason=str(exc)
        if reason not in REASONS:reason='invalid_or_unavailable'
        print('refused: '+reason,file=sys.stderr);return 2
    return 0
if __name__=='__main__':sys.exit(main())
