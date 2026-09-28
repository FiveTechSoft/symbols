import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools" / "builder_rootfs"))
from policy import Entry, PolicyRefusal, check_entries


class PolicyTests(unittest.TestCase):
    def test_exact_absolute_dispositions(self):
        e = [
            Entry("libpython3.12-minimal", "etc/python3.12/sitecustomize.py", "file", 0o644),
            Entry("libpython3.12-minimal", "usr/lib/python3.12/sitecustomize.py", "symlink", 0o777, "/etc/python3.12/sitecustomize.py"),
            Entry("tar", "etc/rmt", "symlink", 0o777, "/usr/sbin/rmt"),
            Entry("tzdata", "usr/share/zoneinfo/localtime", "symlink", 0o777, "/etc/localtime"),
        ]
        self.assertEqual(check_entries(e), {"usr/lib/python3.12/sitecustomize.py": "restore", "etc/rmt": "omit_optional", "usr/share/zoneinfo/localtime": "omit_optional"})
        for i in (1, 2, 3):
            changed = e.copy()
            old = changed[i]
            changed[i] = Entry("other", old.path, old.kind, old.mode, old.link)
            with self.assertRaises(PolicyRefusal):
                check_entries(changed)
        with self.assertRaisesRegex(PolicyRefusal, "sitecustomize_target"):
            check_entries(e[1:])

    def test_escape_duplicate_and_special_refuse(self):
        bad = [
            Entry("p", "../escape", "file", 0o644),
            Entry("p", "/absolute", "file", 0o644),
            Entry("p", "a//b", "file", 0o644),
            Entry("p", "a\\b", "file", 0o644),
            Entry("p", "x", "symlink", 0o777, "../escape"),
            Entry("p", "x", "symlink", 0o777, "/etc/passwd"),
            Entry("p", "x", "device", 0o644),
            Entry("p", "x", "file", 0o4755),
            Entry("p", "x", "file", True),
        ]
        for row in bad:
            with self.subTest(row=row), self.assertRaises(PolicyRefusal):
                check_entries([row])
        with self.assertRaisesRegex(PolicyRefusal, "duplicate"):
            check_entries([Entry("p", "a", "file", 0o644)] * 2)
        with self.assertRaisesRegex(PolicyRefusal, "symlink_parent"):
            check_entries([Entry("p", "a", "symlink", 0o777, "b"), Entry("p", "a/c", "file", 0o644)])

    def test_relative_links_inside_root(self):
        self.assertEqual(check_entries([Entry("p", "usr/lib/a", "symlink", 0o777, "../share/b")]), {})
        with self.assertRaisesRegex(PolicyRefusal, "link_escape"):
            check_entries([Entry("p", "usr/lib/a", "symlink", 0o777, "../../../etc/passwd")])


if __name__ == "__main__":
    unittest.main()
