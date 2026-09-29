"""Collector classification tests; never run a VM in nominal CI."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'/'qemu_closure'))
from collect import diagnostic,MARKER

class CollectionTests(unittest.TestCase):
    def test_ambient_and_unknown_are_visible(self):
        result=diagnostic(['execve("/stage/usr/bin/qemu-system-x86_64", ["qemu"], 0x0) = 0',
                           'openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY) = 3',
                           'openat(AT_FDCWD, "/stage/usr/share/qemu/kvmvapic.bin", O_RDONLY) = 4',
                           'newfstatat(AT_FDCWD, "/etc/nsswitch.conf", 0, 0) = 0',
                           '+++ exited with 0 +++',
                           'strace: Process 99 attached'],Path('/stage'))
        self.assertIn('/etc/ld.so.cache',result['outside'])
        self.assertIn('/etc/nsswitch.conf',result['outside'])
        self.assertEqual(result['unsupported_count'],1)
        self.assertIn('/stage/usr/share/qemu/kvmvapic.bin',result['opened'])
        self.assertEqual(MARKER,b'SYMBOLS_BOOT_ONLY_SUPERVISOR_READY_v1\r\n')
        self.assertIn('/stage/usr/bin/qemu-system-x86_64',result['opened'])
        self.assertNotIn('/stage/usr/bin/qemu-system-x86_64',result['outside'])

if __name__=='__main__':unittest.main()
