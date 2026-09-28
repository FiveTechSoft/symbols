"""Pure semantic verifier of measured Slice B lock records, never launch authority."""
import datetime
import re
import posixpath

CAP_PACKAGES = 256
CAP_FILES = 10000
CAP_EXPANDED = 1024**3
EXCLUSION_REASON = 'setuid account-management helpers; guest runtime has no auth flow or account management'
EXCLUSIONS = {
    ('passwd', 'usr/bin/chage'), ('passwd', 'usr/bin/chfn'),
    ('passwd', 'usr/bin/chsh'), ('passwd', 'usr/bin/expiry'),
    ('passwd', 'usr/bin/gpasswd'), ('passwd', 'usr/bin/passwd'),
    ('libpam-modules-bin', 'usr/sbin/pam_extrausers_chkpwd'),
    ('libpam-modules-bin', 'usr/sbin/unix_chkpwd'),
}
TOP = set('architecture builder directories exclusions files image packages provenance review_by_utc schema snapshot_id state'.split())
BUILDER = set('state extractor_sha256 readelf_sha256 loader_closure_sha256 source_inventory_sha256'.split())
IMAGE = set('state kernel_sha256 qemu_sha256 busybox_sha256 initramfs_sha256 supervisor_sha256 image_id qemu_argv_sha256 compiler_flags_sha256'.split())
PROVENANCE = set('signing_fingerprint inrelease_sha256 packages_indexes_sha256 sources_indexes_sha256 acquired_at_utc valid_until_policy crosscheck_preflight_run'.split())
PACKAGE = set('name version architecture pocket filename size sha256 source_name source_version source_index_sha256'.split())
FILE = set('path origin_package type mode size sha256 link'.split())
DIRECTORY = {'path', 'mode'}
EXCLUSION = {'package', 'path', 'reason'}
HEX = re.compile(r'[0-9a-f]{64}\Z', re.ASCII)
FINGERPRINT = re.compile(r'[0-9A-F]{40}\Z', re.ASCII)
SNAPSHOT = re.compile(r'[0-9]{8}T[0-9]{6}Z\Z', re.ASCII)
NAME = re.compile(r'[a-z0-9][a-z0-9+.-]*\Z', re.ASCII)


class LockRefusal(ValueError):
    def __init__(self, code):
        self.code = code
        super().__init__(code)


def _require(condition, code):
    if not condition:
        raise LockRefusal(code)


def _fields(obj, keys):
    _require(type(obj) is dict and set(obj) == keys, 'fields')


def _digest(value, nullable=False):
    _require((nullable and value is None) or
             (type(value) is str and HEX.fullmatch(value) is not None), 'type')


def _text(value, limit=1024):
    _require(type(value) is str and 1 <= len(value) <= limit and
             value.isascii() and not any(ord(c) < 32 or ord(c) == 127 for c in value), 'type')


def _path(value):
    _text(value, 511)
    _require(not value.startswith('/') and '\\' not in value and
             all(p not in ('', '.', '..') for p in value.split('/')), 'path')


def _utc(value):
    _require(type(value) is str and re.fullmatch(r'[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z', value), 'type')
    try:
        return datetime.datetime.strptime(value, '%Y-%m-%dT%H:%M:%SZ').replace(tzinfo=datetime.timezone.utc)
    except ValueError as exc:
        raise LockRefusal('type') from exc


def _ordered(values, key):
    _require(type(values) is list, 'type')
    seq = [key(item) for item in values]
    _require(seq == sorted(seq) and len(seq) == len(set(seq)), 'order')


def _index_map(obj, count):
    _require(type(obj) is dict and len(obj) == count, 'fields')
    for key, digest in obj.items():
        _path(key)
        _digest(digest)


def _shape(lock):
    _fields(lock, TOP)
    for name, keys in [('builder', BUILDER), ('image', IMAGE), ('provenance', PROVENANCE)]:
        _fields(lock[name], keys)
    for name, keys in [('packages', PACKAGE), ('files', FILE),
                       ('directories', DIRECTORY), ('exclusions', EXCLUSION)]:
        _require(type(lock[name]) is list, 'fields')
        for row in lock[name]:
            _fields(row, keys)
    _fields(lock['provenance']['inrelease_sha256'], {'noble', 'noble-updates', 'noble-security'})
    _index_map(lock['provenance']['packages_indexes_sha256'], 12)
    _index_map(lock['provenance']['sources_indexes_sha256'], 12)


def _types_caps_order(lock):
    _require(lock['schema'] == 'symbols.slice-b-lock.v1' and type(lock['schema']) is str, 'schema')
    _require(lock['architecture'] == 'amd64' and type(lock['architecture']) is str, 'architecture')
    _require(type(lock['snapshot_id']) is str and SNAPSHOT.fullmatch(lock['snapshot_id']), 'snapshot')
    try:
        datetime.datetime.strptime(lock['snapshot_id'], '%Y%m%dT%H%M%SZ')
    except ValueError as exc:
        raise LockRefusal('snapshot') from exc
    for name in ('packages', 'files', 'directories', 'exclusions'):
        _ordered(lock[name], lambda x: x['name'] if name == 'packages' else x['path'])
    _require(len(lock['packages']) <= CAP_PACKAGES and len(lock['files']) <= CAP_FILES and
             len(lock['directories']) <= CAP_FILES and
             len(lock['files'])+len(lock['directories']) <= CAP_FILES, 'count_limit')
    names = set()
    for row in lock['packages']:
        for key in ('name', 'source_name'):
            _require(type(row[key]) is str and NAME.fullmatch(row[key]), 'type')
        for key in ('version', 'source_version'):
            _text(row[key], 200)
        _require(row['architecture'] in ('amd64', 'all') and type(row['architecture']) is str, 'type')
        _require(row['pocket'] in ('noble', 'noble-updates', 'noble-security') and type(row['pocket']) is str, 'type')
        _path(row['filename'])
        _require(row['filename'].startswith('pool/'), 'path')
        _require(type(row['size']) is int and 1 <= row['size'] <= 200_000_000, 'type')
        _digest(row['sha256']);_digest(row['source_index_sha256'])
        names.add(row['name'])
    all_paths = set()
    directory_paths = set()
    symlinks = {}
    expanded = 0
    for row in lock['directories']:
        _path(row['path'])
        _require(type(row['mode']) is int and 0 <= row['mode'] <= 0o777, 'type')
        all_paths.add(row['path'])
        directory_paths.add(row['path'])
    for row in lock['files']:
        _path(row['path'])
        _require(row['path'] not in all_paths, 'duplicate')
        all_paths.add(row['path'])
        _require(row['origin_package'] in names and type(row['origin_package']) is str, 'type')
        _require(type(row['mode']) is int and 0 <= row['mode'] <= 0o777, 'type')
        _require(row['type'] in ('file', 'symlink') and type(row['type']) is str, 'type')
        if row['type'] == 'file':
            _require(type(row['size']) is int and 0 <= row['size'] <= CAP_EXPANDED and row['link'] is None, 'type')
            _digest(row['sha256']);expanded += row['size']
        else:
            _require(row['size'] is None and row['sha256'] is None, 'type')
            _text(row['link'], 511)
            _require(not row['link'].startswith('/') and '\\' not in row['link'], 'path')
            symlinks[row['path']] = row['link']
    _require(expanded <= CAP_EXPANDED, 'byte_limit')
    # Every parent must be an explicit directory; symlinks may not be parents.
    for path in all_paths:
        parts=path.split('/')
        for i in range(1,len(parts)):
            _require('/'.join(parts[:i]) in directory_paths, 'path')
    for path, target in symlinks.items():
        resolved=posixpath.normpath(posixpath.join('/'+posixpath.dirname(path),target))
        _require(resolved.startswith('/') and not resolved.startswith('/../'), 'path')
    for row in lock['exclusions']:
        _path(row['path']);_text(row['package']);_text(row['reason'])
    for key in ('signing_fingerprint',):
        value=lock['provenance'][key]
        _require(type(value) is str and FINGERPRINT.fullmatch(value), 'type')
    for digest in lock['provenance']['inrelease_sha256'].values():_digest(digest)
    for key in ('acquired_at_utc',):_utc(lock['provenance'][key])
    for key in ('valid_until_policy', 'crosscheck_preflight_run'):_text(lock['provenance'][key], 512)
    for key in BUILDER-{'state'}:_digest(lock['builder'][key],nullable=True)
    _text(lock['builder']['state'])
    for key in IMAGE-{'state','image_id'}:_digest(lock['image'][key],nullable=True)
    _text(lock['image']['state'])
    if lock['image']['image_id'] is not None:_text(lock['image']['image_id'])
    if lock['review_by_utc'] is not None:_utc(lock['review_by_utc'])
    if lock['state'] not in ('measured_payloads_only','pinned_image') or type(lock['state']) is not str:
        raise LockRefusal('state')
    return all_paths, names


def _context(lock, trusted):
    # Caller must supply independently verified complete manifest; a lock cannot attest itself.
    _require(type(trusted) is dict and set(trusted) == {'packages','files','directories','exclusions','provenance','image','snapshot_id','raw_setuid_paths'}, 'context')
    for name in ('packages','files','directories','provenance','image','snapshot_id'):
        _require(type(lock[name]) is type(trusted[name]) and lock[name] == trusted[name], 'context')


def _exclusions(lock, trusted, all_paths):
    _require({(x['package'],x['path']) for x in lock['exclusions']} == EXCLUSIONS and
             all(x['reason'] == EXCLUSION_REASON for x in lock['exclusions']), 'exclusions')
    _require(type(trusted['exclusions']) is list and lock['exclusions'] == trusted['exclusions'], 'exclusions')
    _require(not any(path in all_paths for _,path in EXCLUSIONS), 'exclusions')
    # The raw package scan must be independent, not the already filtered manifest.
    _require(type(trusted['raw_setuid_paths']) is list and len(trusted['raw_setuid_paths']) == len(EXCLUSIONS) and
             all(type(p) is str for p in trusted['raw_setuid_paths']) and
             set(trusted['raw_setuid_paths']) == {path for _,path in EXCLUSIONS}, 'setuid')


def verify_lock(lock, *, trusted, now_utc, review_required=False):
    """Evidence-only result. Never grants build, VM launch or observation authority."""
    _shape(lock)
    all_paths, names = _types_caps_order(lock)
    _context(lock,trusted)
    _exclusions(lock,trusted,all_paths)
    _require(type(now_utc) is str and type(review_required) is bool, 'type')
    now=_utc(now_utc)
    acquired=_utc(lock['provenance']['acquired_at_utc'])
    if lock['state'] == 'measured_payloads_only':
        _require(lock['builder']['state'] == 'unverified' and
                 lock['image']['state'] == 'not_built' and
                 lock['review_by_utc'] is None and
                 all(lock['builder'][k] is None for k in BUILDER-{'state'}) and
                 all(lock['image'][k] is None for k in IMAGE-{'state','kernel_sha256','qemu_sha256','busybox_sha256'}) and
                 lock['image']['image_id'] is None, 'state')
        return {'status':'validated_evidence_only','launch_eligible':False}
    _require(lock['review_by_utc'] is not None, 'pin_incomplete')
    review=_utc(lock['review_by_utc'])
    _require(review <= acquired+datetime.timedelta(days=30) and review>=acquired, 'pin_expired')
    _require(now<=review,'pin_expired')
    _require(not review_required,'pin_review_required')
    # Pinned shape is not enough until independent builder, remeasurement and security gates.
    raise LockRefusal('pin_incomplete')
