"""Collector classification tests; never run a VM in nominal CI."""
from pathlib import Path
import sys
import unittest
import ast
import re
import os
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'/'qemu_closure'))
from collect import diagnostic,MARKER,checked_kvm_identity,Refusal,bounded_failure,exec_probe,loader_preflight,needed_aliases

class CollectionTests(unittest.TestCase):
    def test_bounded_preboot_categories_never_echo_arbitrary_data(self):
        data=b'/home/runner/secret TOKEN=x error while loading shared libraries: libfoo.so.1: cannot open shared object file'
        r=bounded_failure(data)
        self.assertIn('loader_missing_library',r['categories'])
        self.assertIn('loader_missing_file',r['categories'])
        self.assertEqual(r['sonames'],['libfoo.so.1'])
        self.assertNotIn('secret',repr(r))
        self.assertEqual(r['redacted']['status'],'redacted')
        self.assertNotIn('TOKEN',repr(r))
        from collect import redacted_stderr
        self.assertEqual(redacted_stderr(b'qemu-system-x86_64: /private/token: No such file or directory')['words'][-5:],
                         ['no','such','file','or','directory'])
        self.assertEqual(redacted_stderr(b'qemu: could not load microvm configuration')['words'],
                         ['qemu','could','not','load','microvm','configuration'])
        self.assertEqual(redacted_stderr(b'kvm: secretword 123 /tmp/secret')['words'],
                         ['kvm','[unknown]','[value]','[value]'])
        self.assertEqual(redacted_stderr(b'\x00secret')['status'],'suppressed')
        self.assertEqual(bounded_failure(b'guest says secret.so.2')['sonames'],[])
        self.assertIn('bios_qboot_named',bounded_failure(b'qemu: could not load microvm bios qboot.rom')['categories'])
        self.assertNotIn('bios_qboot_named',bounded_failure(b'qemu: could not load microvm bios other.rom')['categories'])

        self.assertEqual(bounded_failure(b'qemu: could not open kernel image /private/secret-kernel: No such file or directory')['categories'],
                         ['exec_missing','qemu_kernel_open','qemu_file_open','error_kernel'])
        self.assertEqual(bounded_failure(b'qemu: -initrd could not load initrd /private/guest')['categories'],
                         ['qemu_initrd_load','error_initrd'])
        self.assertNotIn('/private/',repr(bounded_failure(b'qemu: could not open /private/token')))

        self.assertEqual(bounded_failure(b'strace: exec: No such file or directory')['categories'],
                         ['exec_missing','strace_exec_failure','strace_prefix'])

    def test_redacted_trace_shapes_no_paths_or_arbitrary_names(self):
        from collect import trace_shapes
        raw=['1234 openat(AT_FDCWD, "/home/private/token", O_RDONLY) = 3',
             '[pid 1234] newfstatat(AT_FDCWD, "/etc/secret", ...) = 0',
             '1234 strange_agent_instruction("do something") = 0',
             '1234 +++ exited with 1 +++']
        shapes=trace_shapes(raw,[1,2,3])
        self.assertEqual(sum(x['count'] for x in shapes['counts']),len(raw))
        self.assertIn({'prefix':'decimal_pid','kind':'syscall','call':'openat','count':1},shapes['counts'])
        self.assertIn({'prefix':'bracket_pid','kind':'syscall','call':'newfstatat','count':1},shapes['counts'])
        self.assertNotIn('private',repr(shapes))
        self.assertNotIn('instruction',repr(shapes))
        self.assertEqual(len(shapes['samples']),3)
        self.assertEqual([x['line'] for x in shapes['samples']],[1,2,3])
        weird=trace_shapes(['1234  openat(AT_FDCWD, "/secret", O_RDONLY) = 3',
                            '1234\topenat(AT_FDCWD, "/private", O_RDONLY) = 3'],[1,2])
        self.assertEqual([(x['gap_length'],x['gap_class'],x['after_gap_call']) for x in weird['samples']],
                         [(2,'space','openat'),(1,'tab_or_mixed','openat')])
        self.assertNotIn('/secret',repr(weird))
        accepted=diagnostic(['1234  execve("/stage/qemu", ["qemu"], 0x0) = 0',
                             '1234  openat(AT_FDCWD, "/stage/usr/share/qemu/qboot.rom", O_RDONLY) = 3',
                             '1234  access("/etc/ld.so.preload", R_OK) = -1 ENOENT (No such file or directory)',
                             '1234  +++ exited with 1 +++'],'/stage')
        self.assertEqual(accepted['unsupported_count'],0)
        self.assertIn('/stage/usr/share/qemu/qboot.rom',accepted['opened'])
        self.assertIn('/etc/ld.so.preload',accepted['outside'])
        self.assertEqual(exec_probe(['1234  execve("/stage/ld.so", [], 0x0) = 0'],'/stage/ld.so','/stage/qemu')['loader_success'],1)
        refused=diagnostic(['1234   openat(AT_FDCWD, "/stage/qboot.rom", O_RDONLY) = 3',
                            '1234\topenat(AT_FDCWD, "/stage/qboot.rom", O_RDONLY) = 4'],'/stage')
        self.assertEqual(refused['unsupported_count'],2)
        self.assertEqual(refused['opened'],[])


    def test_exec_probe_only_exact_loader_binary(self):
        lines=['execve("/stage/ld.so", ["/stage/ld.so"], 0x0) = 0',
               '[pid 12] execve("/stage/qemu", ["/stage/qemu"], 0x0) = -1 ENOENT (No such file or directory)',
               '12345 execve("/stage/ld.so", ["/stage/ld.so"], 0x0) = 0',
               'execve("/some/other", [], 0x0) = 0','not an exec line']
        self.assertEqual(exec_probe(lines,'/stage/ld.so','/stage/qemu'),
                         {'loader_success':2,'loader_failure':0,'qemu_success':0,'qemu_failure':1})

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

    def test_signed_soname_alias_selected_from_needed(self):
        from test_qemu_closure import elf
        from closure import BINARY,LIB,Refusal
        binary=elf(['libfdt.so.1'])
        actual=elf([])
        source={BINARY:{'sha256':'a'},LIB+'libfdt.so.1':{'link':'libfdt-1.7.0.so'},
                LIB+'libfdt-1.7.0.so':{'sha256':'b'}}
        files={BINARY,LIB+'libfdt-1.7.0.so'}
        raw={BINARY:binary,LIB+'libfdt-1.7.0.so':actual}
        self.assertEqual(needed_aliases(files,source,raw.__getitem__),{LIB+'libfdt.so.1'})
        source[LIB+'libfdt.so.1']={'sha256':'c'}
        with self.assertRaisesRegex(Refusal,'dependency_alias'):
            needed_aliases(files,source,raw.__getitem__)
        del source[LIB+'libfdt.so.1']
        with self.assertRaisesRegex(Refusal,'dependency_resolution'):
            needed_aliases(files,source,raw.__getitem__)

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
        numbered=diagnostic(['4123 execve("/stage/usr/bin/qemu-system-x86_64", ["qemu"], 0x0) = 0',
                             '4123 openat(AT_FDCWD, "/stage/usr/lib/libfdt.so.1", O_RDONLY) = 3',
                             '4123 access("/etc/ld.so.preload", R_OK) = -1 ENOENT (No such file or directory)',
                             '4123 +++ exited with 0 +++'], '/stage')
        self.assertEqual(numbered['unsupported_count'],0)
        self.assertIn('/stage/usr/lib/libfdt.so.1',numbered['opened'])
        self.assertIn('/etc/ld.so.preload',numbered['outside'])
        self.assertEqual(diagnostic(['1234567890123 openat(AT_FDCWD, "/stage/x", O_RDONLY) = 3'],'/stage')['unsupported_count'],1)

if __name__=='__main__':unittest.main()
