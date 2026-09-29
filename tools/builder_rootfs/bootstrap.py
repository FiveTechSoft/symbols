"""Minimal bounded bootstrap of exact Noble dpkg-deb and its measured DSO closure."""
from __future__ import annotations
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile

from build import Refusal, fetch, BASE, digest, bounded_deb_tar


def bootstrap(manifest: dict, destination: Path, *, local_debs: Path | None = None) -> list[str]:
    wanted=json.loads(Path(__file__).with_name('bootstrap-files.json').read_bytes())
    packages={r['Package']:r for r in manifest['packages']}
    roots={r['resolved'] for r in wanted}
    if len(roots)!=len(wanted) or len(roots)!=8: raise Refusal('bootstrap_set')
    result=[]
    for package in sorted({r['package'] for r in wanted}):
        row=packages[package]; size=int(row['Size']); sha=row['SHA256']
        deb=local_debs/(sha+'.deb') if local_debs else None
        if deb and deb.stat().st_size != size: raise Refusal('bootstrap_size')
        if not deb: print(f'fetch bootstrap {package} {row["Filename"]}',flush=True)
        data=deb.read_bytes() if deb else fetch(BASE+row['Filename'],size,sha)
        if len(data)!=size or digest(data)!=sha: raise Refusal('bootstrap_deb')
        with tempfile.NamedTemporaryFile(suffix='.deb') as t:
            t.write(data);t.flush()
            # Host dpkg-deb is the narrowly bounded bootstrap parser only; its
            # output files become executable solely after exact byte checks.
            tar_bytes=bounded_deb_tar(['dpkg-deb','--fsys-tarfile',t.name],80_000_000,90)
        with tarfile.open(fileobj=io.BytesIO(tar_bytes),mode='r:*') as tar:
            observed={}
            for number, member in enumerate(tar,1):
                if number > 30000: raise Refusal('bootstrap_members')
                rel=member.name.removeprefix('./').rstrip('/')
                if rel in roots:
                    if not member.isfile() or rel in observed or member.mode&0o6000 or member.size > 5_000_000:
                        raise Refusal('bootstrap_type')
                    v=tar.extractfile(member).read(member.size+1)
                    if len(v)!=member.size:raise Refusal('bootstrap_length')
                    observed[rel]=v
            expected=[r for r in wanted if r['package']==package]
            if set(observed)!={r['resolved'] for r in expected}: raise Refusal('bootstrap_paths')
            for r in expected:
                raw=observed[r['resolved']]
                if len(raw)!=r['size'] or digest(raw)!=r['sha256']:raise Refusal('bootstrap_hash')
                final=destination/r['resolved'];final.parent.mkdir(parents=True,exist_ok=True)
                if final.exists() or final.is_symlink():raise Refusal('bootstrap_duplicate')
                final.write_bytes(raw);final.chmod(0o755 if r['resolved']=='usr/bin/dpkg-deb' or r['resolved'].endswith('ld-linux-x86-64.so.2') else 0o644)
                result.append(r['resolved'])
    # Aliases are all relative and exactly accounted for by the fixed manifest.
    for r in wanted:
        for name,link in r['chain']:
            if (name.startswith('/') or link.startswith('/') or '..' in name.split('/') or
                    '..' in link.split('/') or '/' in link or '\x00' in name+link):
                raise Refusal('bootstrap_link')
            alias=destination/name;alias.parent.mkdir(parents=True,exist_ok=True)
            if alias.exists() or alias.is_symlink():raise Refusal('bootstrap_duplicate')
            alias.symlink_to(link)
    loader=destination/'usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2'
    executable=destination/'usr/bin/dpkg-deb'
    lib=destination/'usr/lib/x86_64-linux-gnu'
    # Refuse new DT_NEEDED outside the eight measured ELF file paths.
    allowed={Path(r['alias']).name for r in wanted if r['alias']!='usr/bin/dpkg-deb'}
    for target in (loader,executable,*[destination/r['resolved'] for r in wanted if r['resolved'] not in ('usr/bin/dpkg-deb','usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2')]):
        dynamic=subprocess.run(['readelf','-d',str(target)],capture_output=True,text=True,timeout=10)
        if dynamic.returncode:raise Refusal('bootstrap_readelf')
        needed=set(re.findall(r'Shared library: \[([^\]]+)\]',dynamic.stdout))
        if not needed<=allowed:raise Refusal('bootstrap_needed')
    env=os.environ.copy();env['LC_ALL']='C';env.pop('LD_PRELOAD',None)
    p=subprocess.run([str(loader),'--library-path',str(lib),str(executable),'--version'],capture_output=True,text=True,timeout=15,env=env)
    if p.returncode or not p.stdout.startswith("Debian 'dpkg-deb' package archive backend version 1.22.6"):
        raise Refusal('bootstrap_execute')
    return sorted(result)
