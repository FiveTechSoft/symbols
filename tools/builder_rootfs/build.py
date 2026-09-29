"""Signed-snapshot candidate rootfs assembly. Not a boot image or runtime pin."""
from __future__ import annotations

import argparse
import datetime
import hashlib
import io
import json
import os
import platform
from pathlib import Path
import re
import resource
import subprocess
import tarfile
import tempfile
import lzma
import urllib.request

from policy import Entry, PolicyRefusal, check_entries

BASE = 'https://snapshot.ubuntu.com/ubuntu/20260927T000000Z/'
FINGERPRINT = 'F6ECB3762474EDA9D21B7022871920D1991BC93C'
KEY_SHA = 'bfdb0512804d0740816d836bb05b1ef8b27609771d7b9962c95bcc57bfaac230'
MAX_EXPANDED = 1024 * 1024 * 1024


class Refusal(ValueError):
    pass


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        raise Refusal('redirect')


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fetch(url: str, size: int, sha: str) -> bytes:
    if not url.startswith(BASE) or not 0 < size <= 200_000_000 or not re.fullmatch('[0-9a-f]{64}', sha):
        raise Refusal('fetch_input')
    with urllib.request.build_opener(NoRedirect()).open(url, timeout=40) as response:
        if response.url != url or response.status != 200:
            raise Refusal('fetch_location')
        raw = response.read(size + 1)
    if len(raw) != size or digest(raw) != sha:
        raise Refusal('fetch_digest')
    return raw


def bounded_xz(raw: bytes, limit: int = 20_000_000) -> bytes:
    result = bytearray()
    remaining = raw
    for _ in range(4):
        if not remaining: break
        dec = lzma.LZMADecompressor(memlimit=80_000_000)
        try:
            chunk = dec.decompress(remaining, max_length=limit + 1 - len(result))
        except lzma.LZMAError as exc:
            raise Refusal('index_xz') from exc
        result.extend(chunk)
        if len(result) > limit or not dec.eof: raise Refusal('index_expanded')
        remaining = dec.unused_data
    if remaining or not result: raise Refusal('index_expanded')
    return bytes(result)


def bounded_deb_tar(argv: list[str], max_bytes: int, timeout: int) -> bytes:
    # Direct child stdout to a regular file with a kernel-enforced file-size cap.
    # subprocess.PIPE/capture_output would collect the entire untrusted expansion.
    def cap():
        resource.setrlimit(resource.RLIMIT_FSIZE, (max_bytes, max_bytes))
    with tempfile.TemporaryFile() as out:
        proc = subprocess.run(argv, stdout=out, stderr=subprocess.PIPE, timeout=timeout,
                              preexec_fn=cap)
        size = out.tell()
        if proc.returncode or size >= max_bytes:
            raise Refusal('archive')
        out.seek(0)
        return out.read(max_bytes)


def signed_indexes(manifest: dict, keyring: Path) -> dict[str, bytes]:
    if digest(keyring.read_bytes()) != KEY_SHA or manifest['fingerprint'] != FINGERPRINT:
        raise Refusal('keyring')
    if {r['pocket'] for r in manifest['inrelease']} != {'noble','noble-updates','noble-security'}:
        raise Refusal('release_set')
    if {r['pocket'] for r in manifest['indexes']} != {'noble','noble-updates','noble-security'}:
        raise Refusal('index_set')
    contents = {}
    for release in manifest['inrelease']:
        pocket = release['pocket']
        url = BASE + 'dists/' + pocket + '/InRelease'
        if release['url'] != url:
            raise Refusal('release_url')
        raw = fetch(url, release['size'], release['sha256'])
        with tempfile.NamedTemporaryFile() as temp:
            temp.write(raw); temp.flush()
            proc = subprocess.run(['gpgv', '--status-fd', '1', '--keyring', str(keyring), temp.name],
                                  capture_output=True, text=True, timeout=15)
        valid = re.findall(r'^\[GNUPG:\] VALIDSIG ([0-9A-F]{40})\b', proc.stdout, re.M)
        if proc.returncode or valid != [FINGERPRINT]:
            raise Refusal('signature')
        contents[pocket] = raw
    indexes = {}
    for index in manifest['indexes']:
        pocket, path, size, sha = (index[k] for k in ('pocket','path','size','sha256'))
        prefix = pocket + '/'
        if not path.startswith(prefix):
            raise Refusal('index_path')
        relative = 'main/binary-amd64/Packages.xz'
        if path != prefix + relative:
            raise Refusal('index_path')
        # Compare against the signed InRelease SHA256 section, not the manifest alone.
        text = contents[pocket].decode('utf-8')
        sha_section = text.split('\nSHA256:\n', 1)
        if len(sha_section) != 2:
            raise Refusal('index_signature_binding')
        lines = sha_section[1].split('\n', 1)[1].split('\n') if sha_section[1].startswith(' ') else sha_section[1].split('\n')
        matches = [m for line in lines if (m := re.fullmatch(r'\s*([0-9a-f]{64})\s+([0-9]+)\s+([^\s]+)', line)) and m[3] == relative]
        if len(matches) != 1 or matches[0][1] != sha or int(matches[0][2]) != size:
            raise Refusal('index_signature_binding')
        raw = fetch(BASE + 'dists/' + path, size, sha)
        if len(raw) > 2_000_000: raise Refusal('index_size')
        data = bounded_xz(raw)
        indexes[pocket + '-main.Packages.xz'] = data
    return indexes


def check_package_rows(manifest: dict, indexes: dict[str, bytes]) -> None:
    packages = manifest['packages']
    if (len(packages) != 142 or len({r['Package'] for r in packages}) != 142 or
            sum(int(r['Size']) for r in packages) != 154352140 or
            manifest['total_compressed_bytes'] != 154352140 or
            len(manifest['indexes']) != 3 or len(manifest['inrelease']) != 3):
        raise Refusal('package_set')
    parsed = {}
    for key, raw in indexes.items():
        for block in raw.decode('utf-8').split('\n\n'):
            fields = {}
            for line in block.splitlines():
                if line.startswith(' ') or ': ' not in line: continue
                k, v = line.split(': ', 1)
                if k in fields: raise Refusal('duplicate_field')
                fields[k] = v
            if 'Filename' in fields:
                identity = (key, fields['Filename'])
                if identity in parsed: raise Refusal('duplicate_index_row')
                parsed[identity] = fields
    for row in packages:
        fields = parsed.get((row['index'], row['Filename']))
        if fields is None or any(fields.get(k) != str(row[k]) for k in ('Package','Version','Architecture','Filename','Size','SHA256')):
            raise Refusal('package_index_binding')
        if (row['Architecture'] not in ('amd64','all') or
                not re.fullmatch(r'pool/[a-z0-9+.-]+(?:/[a-z0-9+.-]+)+/[A-Za-z0-9+.:~_-]+\.deb',row['Filename'])):
            raise Refusal('package_path')


def stage_packages(manifest: dict, stage: Path, *, loader: Path, executable: Path, lib: Path, local_debs: Path | None = None) -> tuple[dict, dict]:
    entries = {}; dirs = {}; source = {}; expanded = 0
    exclusions = {(x['package'], x['path']): x for x in manifest['setuid_exclusions']}
    if len(exclusions) != 8: raise Refusal('exclusions')
    for row in manifest['packages']:
        sha, size, package = row['SHA256'], int(row['Size']), row['Package']
        url = BASE + row['Filename']
        deb = local_debs / (sha + '.deb') if local_debs else None
        if deb and deb.stat().st_size != size: raise Refusal('package_size')
        raw = deb.read_bytes() if deb else fetch(url, size, sha)
        if len(raw) != size or digest(raw) != sha: raise Refusal('package_digest')
        with tempfile.NamedTemporaryFile(suffix='.deb') as tmp:
            tmp.write(raw);tmp.flush()
            tar_bytes = bounded_deb_tar([str(loader),'--library-path',str(lib),str(executable),'--fsys-tarfile',tmp.name],100_000_000,120)
        with tarfile.open(fileobj=io.BytesIO(tar_bytes), mode='r:*') as tar:
            package_entries = 0
            for member in tar:
                package_entries += 1
                if package_entries > 30000: raise Refusal('package_entries')
                path = member.name.removeprefix('./').rstrip('/')
                if path in ('', '.'): continue
                if not isinstance(member.mode,int) or member.mode < 0 or member.mode > 0o7777:
                    raise Refusal('mode')
                kind = 'dir' if member.isdir() else 'file' if member.isfile() else 'symlink' if member.issym() else 'other'
                # Keep every source row in a canonical manifest; dedupe only identical directories.
                e = Entry(package,path,kind,member.mode,member.linkname if kind=='symlink' else None)
                if member.size > 100_000_000: raise Refusal('member_limit')
                if kind == 'dir':
                    if path in entries or (path in dirs and dirs[path].mode != e.mode): raise Refusal('dir_collision')
                    dirs[path] = e
                    source.setdefault(path, {'kind':'dir','mode':e.mode,'origins':[]})['origins'].append({'origin_package':package,'payload_sha256':sha})
                    continue
                if path in entries or path in dirs: raise Refusal('file_collision')
                if kind == 'file':
                    expanded += member.size
                    if expanded > MAX_EXPANDED or member.size > 100_000_000: raise Refusal('expanded_limit')
                    data = tar.extractfile(member).read(member.size+1)
                    if len(data) != member.size: raise Refusal('file_size')
                    sh = digest(data)
                    blob = stage / sh
                    if not blob.exists(): blob.write_bytes(data)
                    source[path] = {'sha256':sh,'size':len(data),'origin_package':package,'payload_sha256':sha}
                else:
                    source[path] = {'link':member.linkname,'origin_package':package,'payload_sha256':sha}
                entries[path] = e
        print(f'{package} verified {size} {sha}', flush=True)
    if len(dirs) + len(entries) > 10000: raise Refusal('path_count')
    all_entries=list(dirs.values())+list(entries.values())
    # Exclusions are the eight exact setuid rows; every one must be present and no others.
    approved=frozenset(exclusions)
    decisions=check_entries(all_entries,permitted_setuid=approved)
    actual={(e.package,e.path) for e in all_entries if e.mode&0o6000}
    if actual != approved or any(
            entries[k[1]].kind != 'file' or entries[k[1]].mode != exclusions[k]['mode'] or
            exclusions[k]['payload_sha256'] != source[k[1]]['payload_sha256']
            for k in approved):
        raise Refusal('setuid_exclusions')
    expected_absolute={'usr/lib/python3.12/sitecustomize.py':'restore','etc/rmt':'omit_optional','usr/share/zoneinfo/localtime':'omit_optional'}
    if {k:v for k,v in decisions.items() if v!='exclude_setuid'} != expected_absolute: raise Refusal('absolute_link_set')
    return {**dirs,**entries}, {'source':source,'decisions':decisions,'expanded_bytes':expanded}


def assemble(entries: dict, audit: dict, stage: Path, out: Path) -> None:
    included=[]
    with tarfile.open(out, mode='w', format=tarfile.PAX_FORMAT) as archive:
        for path in sorted(entries):
            e=entries[path]; action=audit['decisions'].get(path)
            if action in ('exclude_setuid','omit_optional'): continue
            info=tarfile.TarInfo(path + ('/' if e.kind=='dir' else ''))
            info.uid=info.gid=0;info.uname=info.gname='';info.mtime=0;info.mode=e.mode & 0o777
            if e.kind=='dir': info.type=tarfile.DIRTYPE
            elif e.kind=='symlink': info.type=tarfile.SYMTYPE;info.linkname=e.link
            else:
                record=audit['source'][path];info.size=record['size'];info.type=tarfile.REGTYPE
            if e.kind=='file':
                blob=stage / record['sha256']
                if digest(blob.read_bytes()) != record['sha256']: raise Refusal('stage_drift')
                with blob.open('rb') as stream: archive.addfile(info,stream)
            else: archive.addfile(info)
            included.append({'path':path,'kind':e.kind,'mode':info.mode,'source':audit['source'].get(path)})
    audit['included']=included
    audit['archive_sha256']=digest(out.read_bytes())
    audit['archive_size']=out.stat().st_size


def main() -> None:
    parser=argparse.ArgumentParser()
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--local-debs',type=Path,help='Local exact SHA-named deb cache; still verify every byte')
    parser.add_argument('--offline-metadata',type=Path,help='Local signed metadata fixture for tests only, never hosted workflow')
    args=parser.parse_args()
    manifest_path=Path(__file__).with_name('noble-20260927-source.json')
    keyring=Path(__file__).with_name('ubuntu-signing-key.gpg')
    manifest=json.loads(manifest_path.read_bytes())
    if manifest['schema']!='symbols.builder-rootfs-source.v1' or manifest['snapshot_id']!='20260927T000000Z' or manifest['keyring_sha256']!=KEY_SHA or manifest['review_by']!='2026-10-27':
        raise Refusal('manifest')
    if datetime.datetime.now(datetime.timezone.utc).date() > datetime.date.fromisoformat(manifest['review_by']):
        raise Refusal('review_expired')
    if platform.machine() != 'x86_64' or (not args.offline_metadata and not Path('/etc/os-release').read_text().startswith('PRETTY_NAME="Ubuntu 24.04')):
        raise Refusal('runner_os')
    if args.offline_metadata:
        # Development check only: no claim to reverify a signature on this route.
        indexes={x['pocket']+'-main.Packages.xz':(args.offline_metadata/(x['pocket']+'-main.Packages.xz')).read_bytes() for x in manifest['indexes']}
        indexes={k:bounded_xz(v) for k,v in indexes.items()}
    else: indexes=signed_indexes(manifest,keyring)
    check_package_rows(manifest,indexes)
    args.out.mkdir(exist_ok=False)
    with tempfile.TemporaryDirectory(dir=args.out) as stage_dir:
        from bootstrap import bootstrap
        stage=Path(stage_dir)
        root=stage/'bootstrap';root.mkdir()
        measured=bootstrap(manifest,root,local_debs=args.local_debs)
        lib=root/'usr/lib/x86_64-linux-gnu'
        entries,audit=stage_packages(manifest,stage,loader=lib/'ld-linux-x86-64.so.2',executable=root/'usr/bin/dpkg-deb',lib=lib,local_debs=args.local_debs)
        audit['bootstrap_paths']=measured
        archive=args.out/'rootfs.tar'
        assemble(entries,audit,stage,archive)
    (args.out/'inventory.json').write_text(json.dumps(audit,sort_keys=True,separators=(',',':'))+'\n')
    print(f"archive {audit['archive_size']} {audit['archive_sha256']}",flush=True)


if __name__=='__main__': main()
