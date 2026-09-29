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


    def test_structured_trace_diagnostic_is_fixed_vocabulary_only(self):
        from collect import structured_trace_details
        raw=['812  openat(AT_FDCWD, "/etc/ld.so.preload", O_RDONLY) = -1 ENOENT (No such file or directory)',
             '812  access("/etc/selinux/config", F_OK) = -1 EACCES (Permission denied)',
             '812  openat(AT_FDCWD, "relative", O_RDONLY) = -1 ENOENT (No such file or directory)',
             '812  readlink("/proc/self/exe", "\\x01private", 4096) = 12',
             '812  openat(AT_FDCWD, "/private/SECRET_TOKEN", O_RDONLY) = -1 EVIL (private)',
             '812  --- SIGCHLD {si_pid=99, si_addr=0x123} ---',
             '812  +++ killed by SIGTERM +++',
             '812  <... openat resumed> ) = 3']
        r=structured_trace_details(raw,[2,3,4,5,6,7,8])
        self.assertEqual([x['line'] for x in r['unsupported']],[2,3,4,5,6,7,8])
        self.assertEqual(r['unsupported'][0]['known_probe_paths'],['/etc/selinux/config'])
        self.assertEqual(r['unsupported'][1]['rejection'],'no_absolute_quoted_path')
        self.assertEqual(r['unsupported'][2]['rejection'],'escape')
        self.assertEqual(r['unsupported'][3]['errno_class'],'other')
        self.assertEqual([x['rejection'] for x in r['unsupported'][4:]],['signal','killed','resumed'])
        self.assertEqual([(x['path'],x['errno_class'],x['parser_supported']) for x in r['host_attempts']],
                         [('/etc/ld.so.preload','ENOENT',True),('/etc/selinux/config','EACCES',False),
                          ('/proc/self/exe','none',False)])
        for forbidden in ('private','SECRET_TOKEN','EVIL','si_pid','si_addr','SIGCHLD','SIGTERM','812'):
            self.assertNotIn(forbidden,repr(r))
        self.assertLessEqual(len(structured_trace_details(raw*20,list(range(1,161)))['host_attempts']),100)

    def test_missed_path_shapes_and_signal_boundaries_are_bounded(self):
        from collect import structured_trace_details
        raw=['52  openat(AT_FDCWD, "relative-name", O_RDONLY) = 3',
             '52  access("relative-name", F_OK) = -1 ENOENT (No such file or directory)',
             '52  openat(3</private/dir>, "rel", O_RDONLY) = 4',
             '52  openat(AT_FDCWD, "", O_RDONLY) = -1 ENOENT (No such file or directory)',
             '52  openat(AT_FDCWD, "\\x41private", O_RDONLY) = 5',
             '52  statfs("/sys/fs/selinux", {f_type=1}) = -1 ENOENT (No such file or directory)',
             '52  statfs64("/selinux", 88, {}) = -1 ENOENT (No such file or directory)',
             '52  --- SIGUSR1 {secret=/private/dir} ---',
             '52  --- SIGUSR1 {secret=/private/dir}',
             '52  +++ killed by SIGKILL +++',
             '52  +++ exited with 1 +++']
        r=structured_trace_details(raw,list(range(1,len(raw)+1)))
        x=r['unsupported']
        self.assertEqual([(v['dirfd_class'],v['path_class'],v['path_length'],v['has_escape']) for v in x[:5]],
                         [('AT_FDCWD','relative','1-16',False),('not_applicable','relative','1-16',False),
                          ('descriptor','relative','1-16',False),('AT_FDCWD','empty','0',False),
                          ('AT_FDCWD','other','1-16',True)])
        self.assertEqual([v['call'] for v in x[5:7]],['statfs','statfs64'])
        self.assertEqual([v['event_shape'] for v in x[7:]],
                         ['signal_delimited','signal_unterminated','killed','exit'])
        for forbidden in ('private','relative-name','SIGUSR1','SIGKILL','secret','52'):
            self.assertNotIn(forbidden,repr(r))

    @unittest.skipUnless(sys.platform=='linux' and hasattr(os,'O_PATH'),'openat2 diagnostic is Linux-only')
    def test_relative_open_diagnostic_does_not_publish_arbitrary_name(self):
        from collect import relative_open_diagnostic
        from tempfile import TemporaryDirectory
        import hashlib
        with TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'public').write_bytes(b'public snapshot bytes')
            (root/'SECRET-agent-file').write_bytes(b'private bytes')
            (root/'escape').symlink_to(root/'SECRET-agent-file')
            source={'public':{'size':len(b'public snapshot bytes'),'sha256':hashlib.sha256(b'public snapshot bytes').hexdigest()}}
            lines=['77  openat(AT_FDCWD, "public", O_RDONLY) = 3',
                   '77  openat(AT_FDCWD, "SECRET-agent-file", O_RDONLY) = 4',
                   '77  openat(AT_FDCWD, "escape", O_RDONLY) = 5',
                   '77  openat(AT_FDCWD, "../outside-secret", O_RDONLY) = 6',
                   '77  openat(AT_FDCWD, "gone", O_RDONLY) = 7']
            r=relative_open_diagnostic(lines,[1,2,3,4,5],root,source)
            self.assertEqual(r['launch_cwd'],'stage')
            self.assertEqual(r['candidates'][0]['inventory_path'],'public')
            self.assertEqual(r['candidates'][0]['sha256'],source['public']['sha256'])
            self.assertEqual(r['candidates'][1]['sha256'],hashlib.sha256(b'private bytes').hexdigest())
            self.assertNotIn('inventory_path',r['candidates'][1])
            self.assertEqual([x['state'] for x in r['candidates'][2:]],['unresolved','path_unsafe','unresolved'])
            self.assertNotIn('SECRET-agent-file',repr(r))
            self.assertNotIn('outside-secret',repr(r))
            moved=relative_open_diagnostic(['77  chdir("/private") = 0',lines[0]],[2],root,source)
            self.assertEqual(moved['candidates'][0]['state'],'cwd_changed')

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

    def test_microvm_bios_mapped_only_from_exact_inventory_bytes(self):
        from tempfile import TemporaryDirectory
        from unittest.mock import patch
        import hashlib
        import collect
        data=b'signed snapshot fixture'
        checksum=hashlib.sha256(data).hexdigest()
        src_name=collect.MICROVM_BIOS_SOURCE
        dest_name=collect.MICROVM_BIOS_DATA
        with patch.object(collect,'MICROVM_BIOS_SHA',checksum),patch.object(collect,'MICROVM_BIOS_SIZE',len(data)):
            with TemporaryDirectory() as tmp:
                stage=Path(tmp)
                source=stage/src_name;source.parent.mkdir(parents=True)
                source.write_bytes(data)
                row={'sha256':checksum,'size':len(data)}
                collect.stage_microvm_bios({src_name:row},stage)
                self.assertEqual((stage/dest_name).read_bytes(),data)
                if os.name=='posix':
                    self.assertEqual((stage/dest_name).stat().st_mode & 0o777,0o644)
                with self.assertRaisesRegex(Refusal,'bios_source_or_destination'):
                    collect.stage_microvm_bios({src_name:row},stage)
                (stage/dest_name).unlink()
                source.write_bytes(b'other snapshot fixture')
                with self.assertRaisesRegex(Refusal,'bios_source_or_destination'):
                    collect.stage_microvm_bios({src_name:row},stage)
                self.assertFalse((stage/dest_name).exists())
                with self.assertRaisesRegex(Refusal,'bios_inventory'):
                    collect.stage_microvm_bios({src_name:row,dest_name:row},stage)
                with self.assertRaisesRegex(Refusal,'bios_inventory'):
                    collect.stage_microvm_bios({src_name:{**row,'size':12}},stage)

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
