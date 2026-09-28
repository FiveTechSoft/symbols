"""Pure, bounded byte framing for Slice B. No process, VM or filesystem access."""
import hashlib


def _hash(data):
    return hashlib.sha256(data).digest()


class FrameRefusal(ValueError):
    def __init__(self, code):
        self.code = code
        super().__init__(code)


def _bytes(data):
    if type(data) is not bytes:
        raise FrameRefusal('type')
    return data


def _path(path):
    _bytes(path)
    if not 1 <= len(path) <= 511:
        raise FrameRefusal('path_length')
    if (path.startswith(b'/') or b'\\' in path or
            any(b < 32 or b > 126 for b in path) or
            any(part in (b'', b'.', b'..') for part in path.split(b'/'))):
        raise FrameRefusal('path')
    return path


def _digest(value):
    _bytes(value)
    if len(value) != 32:
        raise FrameRefusal('digest_length')
    return value


def _integer(value, lo, hi, code):
    if type(value) is not int or not lo <= value <= hi:
        raise FrameRefusal(code)
    return value


def encode_manifest(*, path, contract_sha256, workspace_sha256, before_sha256,
                    after_sha256, source, goal, timeout_ms, nonce):
    """Context is already validated; caller still binds the decoded frame."""
    _path(path)
    _bytes(source)
    _bytes(goal)
    _integer(len(source), 1, 65536, 'source_limit')
    _integer(len(goal), 0, 127, 'goal_limit')
    _integer(timeout_ms, 5000, 5000, 'timeout')
    _bytes(nonce)
    if len(nonce) != 16 or nonce == bytes(16):
        raise FrameRefusal('nonce')
    if _digest(after_sha256) != _hash(source):
        raise FrameRefusal('source_digest')
    p = len(path)
    body = (b'SBM1' + (260 + p).to_bytes(2, 'big') + p.to_bytes(2, 'big') + path +
            _digest(contract_sha256) + _digest(workspace_sha256) +
            _digest(before_sha256) + after_sha256 + len(source).to_bytes(4, 'big') +
            _hash(source) + len(goal).to_bytes(4, 'big') + _hash(goal) +
            timeout_ms.to_bytes(4, 'big') + nonce)
    return body + _hash(body)


def decode_manifest(frame, *, expected=None):
    """Decode wire, optionally bind each value to trusted expected raw-byte context."""
    _bytes(frame)
    if len(frame) < 4 or frame[:4] != b'SBM1':
        raise FrameRefusal('magic')
    if len(frame) < 8:
        raise FrameRefusal('truncated')
    length = int.from_bytes(frame[4:6], 'big')
    if length > 771:
        raise FrameRefusal('length_limit')
    if len(frame) != length:
        raise FrameRefusal('truncated' if len(frame) < length else 'trailing')
    p = int.from_bytes(frame[6:8], 'big')
    if not 1 <= p <= 511:
        raise FrameRefusal('path_length')
    if length != 260 + p:
        raise FrameRefusal('length')
    path = _path(frame[8:8+p])
    i = 8 + p
    fields = {}
    for name in ('contract_sha256', 'workspace_sha256', 'before_sha256', 'after_sha256'):
        fields[name] = frame[i:i+32]; i += 32
    source_len = int.from_bytes(frame[i:i+4], 'big'); i += 4
    if not 1 <= source_len <= 65536:
        raise FrameRefusal('source_limit')
    fields['source_sha256'] = frame[i:i+32]; i += 32
    goal_len = int.from_bytes(frame[i:i+4], 'big'); i += 4
    if goal_len > 127:
        raise FrameRefusal('goal_limit')
    fields['goal_sha256'] = frame[i:i+32]; i += 32
    timeout = int.from_bytes(frame[i:i+4], 'big'); i += 4
    if timeout != 5000:
        raise FrameRefusal('timeout')
    nonce = frame[i:i+16]; i += 16
    if nonce == bytes(16):
        raise FrameRefusal('nonce')
    if frame[i:] != _hash(frame[:i]):
        raise FrameRefusal('digest')
    if fields['after_sha256'] != fields['source_sha256']:
        raise FrameRefusal('source_digest')
    result = dict(path=path, source_len=source_len, goal_len=goal_len,
                  timeout_ms=timeout, nonce=nonce, **fields)
    if expected is not None:
        if type(expected) is not dict or set(expected) != set(result):
            raise FrameRefusal('context')
        for key, value in result.items():
            if type(expected[key]) is not type(value) or expected[key] != value:
                raise FrameRefusal('context')
    return result


def encode_source_frame(source):
    _bytes(source)
    if not 1 <= len(source) <= 65536:
        raise FrameRefusal('source_limit')
    if any(b > 127 or b == 0 for b in source):
        raise FrameRefusal('source_encoding')
    return b'SBI1' + len(source).to_bytes(4, 'big') + _hash(source) + source


def decode_source_frame(frame):
    _bytes(frame)
    if len(frame) < 4 or frame[:4] != b'SBI1':
        raise FrameRefusal('magic')
    if len(frame) < 40:
        raise FrameRefusal('truncated')
    length = int.from_bytes(frame[4:8], 'big')
    if not 1 <= length <= 65536:
        raise FrameRefusal('source_limit')
    if len(frame) != 40 + length:
        raise FrameRefusal('truncated' if len(frame) < 40 + length else 'trailing')
    source = frame[40:]
    if frame[8:40] != _hash(source):
        raise FrameRefusal('digest')
    if any(b > 127 or b == 0 for b in source):
        raise FrameRefusal('source_encoding')
    return source


def decode_result_frame(frame):
    _bytes(frame)
    if len(frame) < 4 or frame[:4] != b'SBO1':
        raise FrameRefusal('magic')
    if len(frame) < 9:
        raise FrameRefusal('truncated')
    status = frame[4]
    if status not in (0, 1):
        raise FrameRefusal('status')
    length = int.from_bytes(frame[5:9], 'big')
    if status == 1 and length != 0:
        raise FrameRefusal('phase_length')
    if length > 127:
        raise FrameRefusal('length_limit')
    if len(frame) != 41 + length:
        raise FrameRefusal('truncated' if len(frame) < 41 + length else 'trailing')
    payload = frame[41:]
    if frame[9:41] != _hash(payload):
        raise FrameRefusal('digest')
    return {'status': status, 'stdout': payload, 'complete': True,
            'started': status == 0}
