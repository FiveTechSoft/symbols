"""Static QEMU ELF closure candidate from a signed-snapshot rootfs inventory.

This is a preflight manifest, not an executable runtime pin: dlopen, data,
firmware and observed host files need a later trace/reconciliation gate.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
from elf import dependencies, Refusal as ElfRefusal

class Refusal(ValueError): pass
LIB='usr/lib/x86_64-linux-gnu/'
LOADER=LIB+'ld-linux-x86-64.so.2'
BINARY='usr/bin/qemu-system-x86_64'
SNAPSHOT='20260927T000000Z'
BINARY_SHA='8a35ccba41582fc6c38b9df85fc9e35fa1d42f414d2d7d8090ee9b2f5e7c0854'
LOADER_SHA='c20a2dc8917c755f02b94049356320fe1f62ac7d9f8994731f807d9df39302da'


def sha(raw: bytes) -> str: return hashlib.sha256(raw).hexdigest()

def records(inventory: dict, read, packages: dict, *, expected_binary_sha: str = BINARY_SHA,
            expected_loader_sha: str = LOADER_SHA,
            expected_archive_sha: str = '5f5e095dbe95d167cf6a141e9ea2bd3a22f8c82c67107f54a82cd96bde8608c9',
            expected_archive_size: int = 420270080) -> dict:
    if set(inventory)!={'source','decisions','included','archive_sha256','archive_size','expanded_bytes','bootstrap_paths'}:
        raise Refusal('inventory_schema')
    source=inventory['source']
    if (inventory['archive_sha256']!=expected_archive_sha or
            inventory['archive_size']!=expected_archive_size):raise Refusal('archive_candidate')
    if not isinstance(source,dict) or len(source)>10000: raise Refusal('inventory_count')
    if source.get(BINARY,{}).get('sha256')!=expected_binary_sha:
        raise Refusal('qemu_candidate')
    if source.get(LOADER,{}).get('sha256')!=expected_loader_sha:
        raise Refusal('loader_candidate')
    completed={}; pending=[BINARY]
    included={x['path'] for x in inventory['included']}
    if len(included)!=len(inventory['included']):raise Refusal('included_duplicate')
    def resolve(path: str):
        seen=[]
        for _ in range(12):
            if path in seen: raise Refusal('link_cycle')
            seen.append(path); row=source.get(path)
            if not isinstance(row,dict):raise Refusal('missing_path')
            if 'link' not in row: return path,seen
            link=row['link']
            if not isinstance(link,str) or not link or link.startswith('/') or '/' in link or link in ('.','..'):
                raise Refusal('link_target')
            target=path.rsplit('/',1)[0]+'/'+link
            target_row=source.get(target)
            if (not isinstance(target_row,dict) or row.get('origin_package')!=target_row.get('origin_package') or
                    row.get('payload_sha256')!=target_row.get('payload_sha256')):raise Refusal('link_provenance')
            path=target
        raise Refusal('link_depth')
    while pending:
        path=pending.pop(0)
        path, chain=resolve(path)
        if path in completed:continue
        if len(completed)>=256:raise Refusal('closure_count')
        row=source[path]
        if set(row)!={'sha256','size','origin_package','payload_sha256'} or not isinstance(row['size'],int) or not 1<=row['size']<=40_000_000:
            raise Refusal('file_row')
        pkg=packages.get(row['origin_package'])
        if (not isinstance(pkg,dict) or pkg.get('SHA256')!=row['payload_sha256'] or
                pkg.get('Architecture') not in ('amd64','all')): raise Refusal('package_binding')
        if path not in included:raise Refusal('archive_inventory')
        raw=read(path)
        if len(raw)!=row['size'] or sha(raw)!=row['sha256']:raise Refusal('file_digest')
        try: interpreter, needed=dependencies(raw)
        except ElfRefusal as exc:raise Refusal('elf_'+str(exc)) from exc
        if interpreter is not None:
            if interpreter!='/lib64/ld-linux-x86-64.so.2':raise Refusal('interpreter')
            pending.append(LOADER)
        for name in needed:
            options=[prefix+name for prefix in (LIB,'lib/x86_64-linux-gnu/') if prefix+name in source]
            if len(options)!=1:raise Refusal('dependency_resolution')
            pending.append(options[0])
        completed[path]={'sha256':row['sha256'],'size':row['size'],
                         'origin_package':row['origin_package'],'deb_sha256':row['payload_sha256'],
                         'needed':list(needed),'interpreter':interpreter}
    return {'schema':'symbols.qemu-static-closure-candidate.v1','snapshot':SNAPSHOT,
            'qemu':BINARY,'runtime_complete':False,
            'limitation':'dynamic modules, dlopen, data/firmware, and execution-host closure not yet reconciled',
            'files':{path:completed[path] for path in sorted(completed)}}


def main():
    import argparse
    import tarfile
    p=argparse.ArgumentParser()
    p.add_argument('--inventory',type=Path,required=True)
    p.add_argument('--tar',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    if a.out.exists():raise Refusal('out_exists')
    inventory=json.loads(a.inventory.read_bytes())
    # The archive and inventory are already checked by the builder gate; remeasure.
    if a.tar.stat().st_size!=inventory['archive_size']:
        raise Refusal('archive_size')
    h=hashlib.sha256()
    with a.tar.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    if h.hexdigest()!=inventory['archive_sha256']:raise Refusal('archive_digest')
    source_manifest=json.loads((Path(__file__).parents[1]/'builder_rootfs'/'noble-20260927-source.json').read_bytes())
    if (source_manifest['snapshot_id']!=SNAPSHOT or
            len(source_manifest['packages'])!=142 or
            len({r['Package'] for r in source_manifest['packages']})!=142):raise Refusal('source_snapshot')
    packages={r['Package']:r for r in source_manifest['packages']}
    with tarfile.open(a.tar,'r:') as archive:
        members={m.name:m for m in archive}
        if len(members)!=len(archive.getmembers()):raise Refusal('archive_duplicate')
        included={x['path'] for x in inventory['included']}
        if set(members)!=included:raise Refusal('archive_inventory_set')
        def read(path):
            m=members.get(path)
            if m is None or not m.isfile():raise Refusal('archive_file')
            return archive.extractfile(m).read(40_000_001)
        result=records(inventory,read,packages)
    a.out.write_bytes((json.dumps(result,sort_keys=True,separators=(',',':'))+'\n').encode())
    print(f'candidate {len(result["files"])} files {sha(a.out.read_bytes())}')
if __name__=='__main__':main()
