#!/usr/bin/env python3
"""Exploratory, secret-free KVM and QEMU microvm capability probe. No workspace code."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import selectors
import signal
import stat
import subprocess
import sys
import tempfile
import time

KVM_GET_API_VERSION = 0xAE00
KVM_CREATE_VM = 0xAE01
MARKER = b'SYMBOLS_PREFLIGHT_BOOT_OK_84736'
MAX_OUTPUT = 65536

class ProbeError(Exception):
    pass


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def package(path):
    found = subprocess.run(['dpkg-query', '-S', str(path)], capture_output=True,
                           text=True, timeout=10, check=False)
    if found.returncode:
        return {'name': None, 'version': None, 'lookup_error': found.stderr.strip()[:200]}
    name = found.stdout.split(': ', 1)[0].split(', ', 1)[0]
    ver = subprocess.run(['dpkg-query', '-W', '-f=${Version}', name], capture_output=True,
                         text=True, timeout=10, check=False)
    return {'name': name, 'version': ver.stdout.strip() if ver.returncode == 0 else None}


def cpio_entry(name, data, mode, inode):
    name_bytes = name.encode() + b'\0'
    fields = [inode, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(name_bytes), 0]
    result = b'070701' + b''.join(f'{field:08x}'.encode() for field in fields) + name_bytes
    result += b'\0' * (-len(result) % 4)
    result += data + b'\0' * (-len(data) % 4)
    return result


def initrd(path, busybox):
    program = b'#!/bin/sh\necho SYMBOLS_PREFLIGHT_BOOT_OK_84736\nexec /bin/busybox sleep 600\n'
    entries = [('bin', b'', stat.S_IFDIR | 0o755),
               ('bin/busybox', Path(busybox).read_bytes(), stat.S_IFREG | 0o755),
               ('bin/sh', b'busybox', stat.S_IFLNK | 0o777),
               ('init', program, stat.S_IFREG | 0o755),
               ('TRAILER!!!', b'', stat.S_IFREG)]
    with open(path, 'wb') as target:
        for i, (name, data, mode) in enumerate(entries, 1):
            target.write(cpio_entry(name, data, mode, i))
    return digest(path)


def shutdown(proc):
    if proc is None:
        return None
    started = time.monotonic()
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        os.killpg(proc.pid, signal.SIGKILL)
        proc.wait(timeout=3)
    finally:
        if proc.stdout:
            proc.stdout.close()
        if proc.stderr:
            proc.stderr.close()
    return round((time.monotonic() - started) * 1000, 3)


def boot(qemu, kernel, image, timeout):
    command = [str(qemu), '-M', 'microvm', '-accel', 'kvm', '-cpu', 'host', '-m', '128M',
               '-smp', '1', '-kernel', str(kernel), '-initrd', str(image),
               '-append', 'console=ttyS0 rdinit=/init panic=1', '-nodefaults',
               '-no-user-config', '-nographic', '-serial', 'stdio', '-monitor', 'none',
               '-net', 'none', '-no-reboot']
    proc = None
    started = time.monotonic()
    data = bytearray()
    try:
        proc = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, start_new_session=True)
        with selectors.DefaultSelector() as selector:
            for channel in (proc.stdout, proc.stderr):
                os.set_blocking(channel.fileno(), False)
                selector.register(channel, selectors.EVENT_READ)
            while True:
                if MARKER in data:
                    return {'boot_ms': round((time.monotonic() - started) * 1000, 3),
                            'kill_ms': None, 'pass': True}, proc
                if time.monotonic() - started >= timeout:
                    raise ProbeError('boot_timeout')
                if proc.poll() is not None and not selector.get_map():
                    raise ProbeError(f'qemu_exited_{proc.returncode}_before_marker')
                for key, _ in selector.select(timeout=0.1):
                    chunk = os.read(key.fileobj.fileno(), 4096)
                    if not chunk:
                        selector.unregister(key.fileobj)
                    elif len(data) < MAX_OUTPUT:
                        data.extend(chunk[:MAX_OUTPUT - len(data)])
                    elif MARKER not in data:
                        raise ProbeError('serial_output_limit')
    except Exception:
        shutdown(proc)
        raise


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--kernel', required=True, type=Path)
    p.add_argument('--busybox', default='/bin/busybox', type=Path)
    p.add_argument('--qemu', default='/usr/bin/qemu-system-x86_64', type=Path)
    p.add_argument('--boot-timeout', default=20, type=int)
    p.add_argument('--json-out', type=Path)
    args = p.parse_args()
    result = {'schema': 'symbols.kvm-hosted-preflight.v1', 'status': 'fail',
              'classification': 'exploratory-capability-only', 'environment': {},
              'phases': [], 'boot_attempts': [], 'p50_boot_ms': None, 'error': None}
    phase = 'environment'
    try:
        if not (1 <= args.boot_timeout <= 60):
            raise ProbeError('invalid_boot_timeout')
        if os.uname().machine != 'x86_64':
            raise ProbeError('unsupported_arch')
        for label, path in [('qemu', args.qemu), ('kernel', args.kernel), ('busybox', args.busybox)]:
            if not path.is_file():
                raise ProbeError(f'{label}_missing')
            result['environment'][label] = {'path': str(path), 'sha256': digest(path),
                                            'package': package(path)}
        version = subprocess.run([str(args.qemu), '--version'], capture_output=True,
                                 text=True, timeout=10, check=False)
        result['environment']['qemu_version'] = version.stdout.splitlines()[0][:200] if version.stdout else None
        result['environment']['runner_image'] = os.environ.get('ImageVersion', '')[:100]
        result['phases'].append({'phase': phase, 'pass': True, 'error': None})
        phase = 'kvm_device'
        dev = os.open('/dev/kvm', os.O_RDWR | os.O_CLOEXEC)
        try:
            version = fcntl.ioctl(dev, KVM_GET_API_VERSION, 0)
            if version != 12:
                raise ProbeError(f'kvm_api_{version}_not_12')
            result['environment']['kvm_api_version'] = version
            result['phases'].append({'phase': phase, 'pass': True, 'error': None})
            phase = 'create_vm'
            vm = fcntl.ioctl(dev, KVM_CREATE_VM, 0)
            if vm < 0:
                raise ProbeError('create_vm_invalid_fd')
            os.close(vm)
            result['phases'].append({'phase': phase, 'pass': True, 'error': None})
        finally:
            os.close(dev)
        phase = 'initramfs'
        with tempfile.TemporaryDirectory(prefix='symbols-kvm-probe-') as temp:
            image = Path(temp) / 'initramfs.cpio'
            result['environment']['initramfs_sha256'] = initrd(image, args.busybox)
            result['phases'].append({'phase': phase, 'pass': True, 'error': None})
            for attempt in range(1, 4):
                phase = f'boot_kill_{attempt}'
                record, proc = boot(args.qemu, args.kernel, image, args.boot_timeout)
                try:
                    record['kill_ms'] = shutdown(proc)
                    if proc.poll() is None:
                        raise ProbeError('qemu_not_reaped')
                except Exception:
                    record['pass'] = False
                    raise
                result['boot_attempts'].append(record)
                result['phases'].append({'phase': phase, 'pass': True, 'error': None})
        vals = sorted(x['boot_ms'] for x in result['boot_attempts'])
        result['p50_boot_ms'] = vals[1]
        result['status'] = 'pass'
    except Exception as exc:
        error = str(exc) if isinstance(exc, ProbeError) else f'{type(exc).__name__}: {exc}'
        result['error'] = {'phase': phase, 'code': error[:240]}
        result['phases'].append({'phase': phase, 'pass': False, 'error': error[:240]})
    text = json.dumps(result, sort_keys=True, separators=(',', ':')) + '\n'
    if args.json_out:
        args.json_out.write_text(text, encoding='utf-8')
    sys.stdout.write(text)
    return 0 if result['status'] == 'pass' else 1

if __name__ == '__main__':
    sys.exit(main())
