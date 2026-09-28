"""Fail-closed path policy. No archives are unpacked and no files are opened here."""
from __future__ import annotations

import posixpath
from dataclasses import dataclass


class PolicyRefusal(ValueError):
    pass


@dataclass(frozen=True)
class Entry:
    package: str
    path: str
    kind: str
    mode: int
    link: str | None = None


# The optional entries are omitted from the host scratch root and candidate
# non-bootable builder rootfs. They do not become ambient exclusions.
ABSOLUTE_LINKS = {
    ("libpython3.12-minimal", "usr/lib/python3.12/sitecustomize.py", "/etc/python3.12/sitecustomize.py"): "restore",
    ("tar", "etc/rmt", "/usr/sbin/rmt"): "omit_optional",
    ("tzdata", "usr/share/zoneinfo/localtime", "/etc/localtime"): "omit_optional",
}


def _path(value: str) -> str:
    if (not isinstance(value, str) or not value or value.startswith("/") or
            "\\" in value or "\x00" in value or any(p in ("", ".", "..") for p in value.split("/")) or
            posixpath.normpath(value) != value):
        raise PolicyRefusal("path")
    return value


def check_entries(entries: list[Entry], *, permitted_setuid: frozenset[tuple[str, str]] = frozenset()) -> dict[str, str]:
    """Return exact disposition for the three absolute symlinks, or refuse.

    `permitted_setuid` names preapproved account-helper exclusions only. Such
    rows never reach an extracted root; callers must record source package and
    digest separately. This function does not authenticate its caller's rows.
    """
    if type(entries) is not list or len(entries) > 10000:
        raise PolicyRefusal("count")
    by_path: dict[str, Entry] = {}
    decisions: dict[str, str] = {}
    for row in entries:
        if type(row) is not Entry or not row.package or not isinstance(row.package, str):
            raise PolicyRefusal("entry")
        path = _path(row.path)
        if path in by_path:
            raise PolicyRefusal("duplicate")
        if type(row.mode) is not int or not 0 <= row.mode <= 0o7777:
            raise PolicyRefusal("mode")
        if row.mode & 0o6000:
            if (row.package, path) in permitted_setuid:
                decisions[path] = "exclude_setuid"
                by_path[path] = row
                continue
            raise PolicyRefusal("setuid")
        if row.kind not in ("dir", "file", "symlink"):
            raise PolicyRefusal("kind")
        if row.kind != "symlink":
            if row.link is not None:
                raise PolicyRefusal("link")
        else:
            if not isinstance(row.link, str) or not row.link or "\\" in row.link or "\x00" in row.link:
                raise PolicyRefusal("link")
            if row.link.startswith("/"):
                action = ABSOLUTE_LINKS.get((row.package, path, row.link))
                if action is None or ".." in row.link.split("/") or "//" in row.link:
                    raise PolicyRefusal("absolute_link")
                _path(row.link[1:])
                decisions[path] = action
            else:
                resolved = posixpath.normpath(posixpath.join(posixpath.dirname(path), row.link))
                if resolved in ("..", ".") or resolved.startswith("../") or resolved.startswith("/"):
                    raise PolicyRefusal("link_escape")
                _path(resolved)
        by_path[path] = row
    for path in by_path:
        parent = posixpath.dirname(path)
        while parent:
            ancestor = by_path.get(parent)
            if ancestor and (ancestor.kind != "dir" or parent in decisions):
                raise PolicyRefusal("symlink_parent")
            parent = posixpath.dirname(parent)
    site = "usr/lib/python3.12/sitecustomize.py"
    if site in decisions:
        target = by_path.get("etc/python3.12/sitecustomize.py")
        if target is None or target.kind != "file" or target.package != "libpython3.12-minimal":
            raise PolicyRefusal("sitecustomize_target")
    return decisions
