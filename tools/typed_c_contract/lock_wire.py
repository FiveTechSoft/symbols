"""Pure Slice B SBL1 framing and canonical JSON. Not a semantic lock verifier."""
import hashlib
import json

MAGIC = b'SBL1'
MAX_FRAME = 4 * 1024 * 1024
MAX_PAYLOAD = MAX_FRAME - 40


class LockWireRefusal(ValueError):
    def __init__(self, code):
        self.code = code
        super().__init__(code)


def _pairs(items):
    result = {}
    for key, value in items:
        if key in result:
            raise LockWireRefusal('duplicate_key')
        result[key] = value
    return result


def _constant(value):
    raise LockWireRefusal('constant')


def _float(value):
    raise LockWireRefusal('float')


def _surrogates(value):
    if isinstance(value, str):
        if any(0xd800 <= ord(ch) <= 0xdfff for ch in value):
            raise LockWireRefusal('surrogate')
    elif isinstance(value, list):
        for item in value:
            _surrogates(item)
    elif isinstance(value, dict):
        for key, item in value.items():
            _surrogates(key)
            _surrogates(item)


def _canonical(value):
    _surrogates(value)
    try:
        result = json.dumps(value, sort_keys=True, separators=(',', ':'),
                            ensure_ascii=True, allow_nan=False).encode('ascii')
    except (TypeError, ValueError, OverflowError, UnicodeError) as exc:
        raise LockWireRefusal('json_type') from exc
    if len(result) > MAX_PAYLOAD:
        raise LockWireRefusal('length_limit')
    return result


def encode_lock_wire(value):
    """Encode canonical JSON, not attest, authorize or launch an image."""
    payload = _canonical(value)
    body = MAGIC + len(payload).to_bytes(4, 'big') + payload
    return body + hashlib.sha256(body).digest()


def decode_lock_wire(frame):
    """Decode full bytes, reject noncanonical JSON; schema verification is separate."""
    if type(frame) is not bytes:
        raise LockWireRefusal('type')
    if len(frame) < 4 or frame[:4] != MAGIC:
        raise LockWireRefusal('magic')
    if len(frame) < 8:
        raise LockWireRefusal('truncated')
    length = int.from_bytes(frame[4:8], 'big')
    if length > MAX_PAYLOAD:
        raise LockWireRefusal('length_limit')
    total = 40 + length
    if len(frame) != total:
        raise LockWireRefusal('truncated' if len(frame) < total else 'trailing')
    body, digest = frame[:-32], frame[-32:]
    if hashlib.sha256(body).digest() != digest:
        raise LockWireRefusal('digest')
    payload = frame[8:-32]
    try:
        text = payload.decode('ascii')
    except UnicodeError as exc:
        raise LockWireRefusal('encoding') from exc
    try:
        value = json.loads(text, object_pairs_hook=_pairs,
                           parse_constant=_constant, parse_float=_float)
    except LockWireRefusal:
        raise
    except (ValueError, UnicodeError, RecursionError) as exc:
        raise LockWireRefusal('json') from exc
    if _canonical(value) != payload:
        raise LockWireRefusal('noncanonical')
    return value
