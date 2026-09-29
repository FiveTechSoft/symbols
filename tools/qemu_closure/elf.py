"""Bounded ELF64 little-endian x86_64 dependency reader; no code execution."""
from __future__ import annotations
import struct

class Refusal(ValueError): pass

def u(fmt: str, raw: bytes, offset: int):
    try: return struct.unpack_from('<'+fmt,raw,offset)
    except struct.error as exc: raise Refusal('elf_bounds') from exc

def dependencies(raw: bytes) -> tuple[str | None, tuple[str,...]]:
    if len(raw)>40_000_000 or len(raw)<64 or raw[:7]!=b'\x7fELF\x02\x01\x01':
        raise Refusal('elf_header')
    typ,machine,version=u('HHI',raw,16)
    if typ not in (2,3) or machine!=62 or version!=1: raise Refusal('elf_arch')
    phoff=u('Q',raw,32)[0]
    phsize,phnum=u('HH',raw,54)
    if phsize!=56 or phnum>128 or phoff+phnum*phsize>len(raw): raise Refusal('elf_program_headers')
    loads=[]; interp=None; dynamic=None
    for i in range(phnum):
        typ,flags,off,vaddr,_,filesz,memsz,_=u('IIQQQQQQ',raw,phoff+i*phsize)
        if off+filesz>len(raw) or filesz>memsz: raise Refusal('elf_segment')
        if typ==1: loads.append((vaddr,filesz,off))
        if typ==3:
            if interp is not None or not 1<filesz<=256: raise Refusal('elf_interpreter')
            data=raw[off:off+filesz]
            if data[-1:]!=b'\0' or b'\0' in data[:-1]: raise Refusal('elf_interpreter')
            try: interp=data[:-1].decode('ascii')
            except UnicodeDecodeError as exc: raise Refusal('elf_interpreter') from exc
            if not interp.startswith('/') or '..' in interp.split('/'): raise Refusal('elf_interpreter')
        if typ==2:
            if dynamic is not None or filesz>16384 or filesz%16: raise Refusal('elf_dynamic')
            dynamic=(off,filesz)
    if dynamic is None:
        if interp is not None: raise Refusal('elf_dynamic_missing')
        return interp,()
    entries=[]
    off,size=dynamic
    for i in range(size//16):
        tag,val=u('qQ',raw,off+i*16)
        if tag==0: break
        if tag in (15,29): raise Refusal('elf_search_path') # RPATH/RUNPATH needs a separately pinned policy
        entries.append((tag,val))
    else: raise Refusal('elf_dynamic_termination')
    # DT_STRTAB is a virtual address; DT_STRSZ bounds every name.
    vaddrs=[v for t,v in entries if t==5]; sizes=[v for t,v in entries if t==10]
    if len(vaddrs)!=1 or len(sizes)!=1 or not 0<sizes[0]<=2_000_000: raise Refusal('elf_strings')
    candidates=[(pos+(vaddrs[0]-va),sz-(vaddrs[0]-va)) for va,sz,pos in loads if va<=vaddrs[0]<va+sz]
    if len(candidates)!=1 or sizes[0]>candidates[0][1]: raise Refusal('elf_strings')
    strbase=candidates[0][0]; names=[]
    for tag,val in entries:
        if tag!=1: continue
        if val>=sizes[0]: raise Refusal('elf_needed')
        end=raw.find(b'\0',strbase+val,strbase+sizes[0])
        if end<0: raise Refusal('elf_needed')
        try: name=raw[strbase+val:end].decode('ascii')
        except UnicodeDecodeError as exc: raise Refusal('elf_needed') from exc
        if not name or '/' in name or '\\' in name or name in ('.','..') or len(name)>255: raise Refusal('elf_needed')
        if name in names: raise Refusal('elf_needed_duplicate')
        names.append(name)
    return interp,tuple(names)
