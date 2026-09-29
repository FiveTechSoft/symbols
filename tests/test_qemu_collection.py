"""Collector classification tests; never run a VM in nominal CI."""
from pathlib import Path
import sys
import unittest
import ast
import re
import os
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'/'qemu_closure'))
from collect import diagnostic,MARKER,checked_kvm_identity,Refusal,bounded_failure,exec_probe,loader_preflight

class CollectionTests(unittest.TestCase):
    def test_bounded_preboot_categories_never_echo_arbitrary_data(self):
        data=b'/home/runner/secret TOKEN=x error while loading shared libraries: libfoo.so.1: cannot open shared object file'
        r=bounded_failure(data)
        self.assertIn('loader_missing_library',r['categories'])
        self.assertIn('loader_missing_file',r['categories'])
        self.assertEqual(r['sonames'],['libfoo.so.1'])
        self.assertNotIn('secret',repr(r))
        self.assertEqual(bounded_failure(b'guest says secret.so.2')['sonames'],[])
        self.assertEqual(bounded_failure(b'strace: exec: No such file or directory')['categories'],
                         ['exec_missing','strace_exec_failure'])

    def test_exec_probe_only_exact_loader_binary(self):
        lines=['execve("/stage/ld.so", ["/stage/ld.so"], 0x0) = 0',
               '[pid 12] execve("/stage/qemu", ["/stage/qemu"], 0x0) = -1 ENOENT (No such file or directory)',
               'execve("/some/other", [], 0x0) = 0','not an exec line']
        self.assertEqual(exec_probe(lines,'/stage/ld.so','/stage/qemu'),
                         {'loader_success':1,'loader_failure':0,'qemu_success':0,'qemu_failure':1})

    @unittest.skipUnless(os.name=='posix','bounded loader preflight uses Linux preexec_fn')
    def test_loader_preflight_is_nonboot_and_bounded(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as tmp:
            root=Path(tmp)
            loader=root/'loader';loader.write_text('#!/bin/sh\nprintf "libfoo.so.1 => not found\n"\nexit 127\n')
            loader.chmod(0o755)
            r=loader_preflight(loader,'/stage/lib',root/'qemu',env={'PATH':'/usr/bin:/bin'},cwd=tmp)
            self.assertEqual(r['status'],'blocked')
            self.assertEqual(r['exit_status'],127)
            self.assertEqual(r['missing_sonames'],['libfoo.so.1'])
            self.assertNotIn(tmp,repr(r))

    def test_kvm_identity_requires_active_nonroot_group(self):
        data=dict(uid=1001,euid=1001,egid=1001,groups=[1001,994],device_gid=994,device_mode=0o660,kvm_gid=994)
        self.assertEqual(checked_kvm_identity(**data)['active_gids'],[994,1001])
        for change in ({'groups':[1001]},{'euid':0},{'uid':0},{'device_gid':995},
                       {'device_mode':0o600},{'kvm_gid':995}):
            with self.subTest(change=change),self.assertRaisesRegex(Refusal,'kvm_identity'):
                checked_kvm_identity(**{**data,**change})

    def test_selected_snapshot_firmware_names(self):
        rule=r'[A-Za-z0-9._+,/-]+'
        for name in ('usr/share/qemu/QEMU,VGA.bin','usr/share/qemu/QEMU,cgthree.bin','usr/share/qemu/QEMU,tcx.bin'):
            self.assertIsNotNone(re.fullmatch(rule,name))
        for name in ('../etc/passwd','usr/share/qemu/evil\\name','/etc/ld.so.cache'):
            self.assertTrue(name.startswith('/') or '..' in name.split('/') or not re.fullmatch(rule,name))

    def test_resource_is_linux_only_main_path(self):
        source=(Path(__file__).resolve().parents[1]/'tools'/'qemu_closure'/'collect.py').read_text()
        tree=ast.parse(source)
        top_imports=[n for n in tree.body if isinstance(n,(ast.Import,ast.ImportFrom))]
        self.assertNotIn('resource',[a.name for n in top_imports for a in getattr(n,'names',[])])
        self.assertIn('import resource  # Linux-only',source)
        self.assertNotIn('grp',[a.name for n in top_imports for a in getattr(n,'names',[])])

    def test_ambient_and_unknown_are_visible(self):
        result=diagnostic(['execve("/stage/usr/bin/qemu-system-x86_64", ["qemu"], 0x0) = 0',
                           'openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY) = 3',
                           'openat(AT_FDCWD, "/stage/usr/share/qemu/kvmvapic.bin", O_RDONLY) = 4',
                           'newfstatat(AT_FDCWD, "/etc/nsswitch.conf", 0, 0) = 0',
                           '+++ exited with 0 +++',
                           'strace: Process 99 attached'],'/stage')
        self.assertIn('/etc/ld.so.cache',result['outside'])
        self.assertIn('/etc/nsswitch.conf',result['outside'])
        self.assertEqual(result['unsupported_count'],1)
        self.assertIn('/stage/usr/share/qemu/kvmvapic.bin',result['opened'])
        self.assertEqual(MARKER,b'SYMBOLS_BOOT_ONLY_SUPERVISOR_READY_v1\r\n')
        self.assertIn('/stage/usr/bin/qemu-system-x86_64',result['opened'])
        self.assertNotIn('/stage/usr/bin/qemu-system-x86_64',result['outside'])

if __name__=='__main__':unittest.main()
