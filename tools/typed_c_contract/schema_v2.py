"""Freeze the v1 wire shape before any independent evaluator authors tasks."""
import base64
import hashlib
import json

SCHEMA = 'symbols.c-repair-contract.v2'

def strict_pairs(pairs):
    result = {}
    for k, v in pairs:
        if k in result:
            raise ValueError('duplicate_key')
        result[k] = v
    return result

def keys(obj, expected):
    if type(obj) is not dict or set(obj) != set(expected):
        raise ValueError('fields')

def path(p):
    if type(p) is not str or not p or len(p.encode('utf-8')) >= 512 or '\\' in p or any(x in p for x in '*?[]') or p.startswith('/') or any(x in ('', '.', '..') for x in p.split('/')):
        raise ValueError('path')
    if any(ord(ch) < 32 or ord(ch) > 126 for ch in p):
        raise ValueError('path')
    return p

def digest(v):
    if type(v) is not str or len(v) != 64 or any(ch not in '0123456789abcdef' for ch in v):
        raise ValueError('digest')

def parse(raw):
    if not raw or len(raw) > 8192:
        raise ValueError('limit')
    if raw.startswith(b'\xef\xbb\xbf'):
        raise ValueError('utf8_bom')
    obj = json.loads(raw.decode('utf-8'), object_pairs_hook=strict_pairs,
                     parse_constant=lambda _: (_ for _ in ()).throw(ValueError('constant')))
    keys(obj, ('schema','workspace_digest','stdout','exit_code','probe','edit_scope','source_predicates'))
    if type(obj['schema']) is not str or obj['schema'] != SCHEMA: raise ValueError('version')
    digest(obj['workspace_digest'])
    keys(obj['stdout'], ('bytes_b64','length','termination'))
    st=obj['stdout']
    if type(st['bytes_b64']) is not str or type(st['termination']) is not str or st['termination'] != 'exact' or type(st['length']) is not int or not 0 <= st['length'] <= 127:
        raise ValueError('stdout')
    b64=st['bytes_b64']
    try:
        b=base64.b64decode(b64, validate=True)
    except (ValueError, base64.binascii.Error) as exc:
        raise ValueError('base64') from exc
    if base64.b64encode(b).decode('ascii') != b64 or len(b) != st['length']:
        raise ValueError('base64')
    if type(obj['exit_code']) is not int or obj['exit_code'] != 0:
        raise ValueError('exit')
    keys(obj['probe'], ('kind','timeout_ms'))
    if type(obj['probe']['kind']) is not str or type(obj['probe']['timeout_ms']) is not int or obj['probe'] != {'kind':'single-c-main','timeout_ms':5000}:
        raise ValueError('probe')
    keys(obj['edit_scope'], ('allow','deny'))
    allow,deny=obj['edit_scope']['allow'],obj['edit_scope']['deny']
    if type(allow) is not list or type(deny) is not list or not 1 <= len(allow) <= 4 or len(deny)>4:
        raise ValueError('scope')
    names=[path(x) for x in allow+deny]
    if len(set(names)) != len(names): raise ValueError('scope')
    predicates=obj['source_predicates']
    if type(predicates) is not list or len(predicates)>4: raise ValueError('predicates')
    seen=set()
    for item in predicates:
        if type(item) is not dict: raise ValueError('predicate')
        kind=item.get('kind')
        if kind=='file_bytes_equal':
            keys(item,('kind','path','sha256'))
            ident=(kind,path(item['path']))
        elif kind=='span_bytes_equal':
            keys(item,('kind','path','start','length','sha256'))
            start,length=item['start'],item['length']
            if type(start) is not int or type(length) is not int or start<0 or length<=0 or start+length>262144:
                raise ValueError('span')
            ident=(kind,path(item['path']),start,length)
        else: raise ValueError('predicate')
        digest(item['sha256'])
        if ident in seen: raise ValueError('predicate')
        seen.add(ident)
    return obj,b,hashlib.sha256(raw).hexdigest()
