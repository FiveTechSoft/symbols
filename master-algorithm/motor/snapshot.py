"""Recoverable binary snapshot of everything the motor has learned.

Format MAKB1: magic + gzip(JSON of file texts). Stdlib only.
Dump ↔ load roundtrip is the test that RAM is not the source of truth.
"""
from __future__ import annotations

import gzip
import hashlib
import json
import re
import time
from pathlib import Path
from typing import Any

MOTOR_DIR = Path(__file__).resolve().parent
ARCHIVE_DIR = MOTOR_DIR / "archive"
RUNS_DIR = MOTOR_DIR / "runs"
BIN_NAME = "learned.bin"
MAGIC = b"MAKB1\0"
FORMAT = 1

# Learned state (not chat logs). Relpaths from MOTOR_DIR.
LEARNED_FILES = (
    "archive/theory.pl",
    "archive/meta.json",
    "archive/skin.json",
    "runs/latest.json",
    "runs/curiosity-latest.json",
    "runs/xfer-battery.json",
)


def bin_path(archive_dir: Path | None = None) -> Path:
    d = Path(archive_dir) if archive_dir else ARCHIVE_DIR
    return d / BIN_NAME


def _sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _count_verified(theory: str) -> int:
    return len(re.findall(r"^verified\(", theory, flags=re.M))


def _count_rejected(theory: str) -> int:
    return len(re.findall(r"^rejected\(", theory, flags=re.M))


def collect(motor_dir: Path | None = None) -> dict[str, str]:
    root = Path(motor_dir) if motor_dir else MOTOR_DIR
    files: dict[str, str] = {}
    for rel in LEARNED_FILES:
        p = root / rel
        if p.is_file():
            files[rel] = p.read_text(encoding="utf-8")
    return files


def build_payload(files: dict[str, str]) -> dict[str, Any]:
    theory = files.get("archive/theory.pl", "")
    return {
        "format": FORMAT,
        "created_unix": int(time.time()),
        "n_verified": _count_verified(theory),
        "n_rejected": _count_rejected(theory),
        "files": files,
        "sha256": {k: _sha(v) for k, v in files.items()},
    }


def encode(payload: dict[str, Any]) -> bytes:
    raw = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    return MAGIC + gzip.compress(raw, compresslevel=9)


def decode(blob: bytes) -> dict[str, Any]:
    if not blob.startswith(MAGIC):
        raise ValueError("not a MAKB1 learned.bin (bad magic)")
    payload = json.loads(gzip.decompress(blob[len(MAGIC):]).decode("utf-8"))
    if int(payload.get("format", 0)) != FORMAT:
        raise ValueError(f"unsupported snapshot format {payload.get('format')}")
    return payload


def dump(motor_dir: Path | None = None, dest: Path | None = None) -> dict[str, Any]:
    root = Path(motor_dir) if motor_dir else MOTOR_DIR
    files = collect(root)
    if "archive/theory.pl" not in files:
        raise FileNotFoundError(f"no theory.pl under {root / 'archive'}")
    payload = build_payload(files)
    blob = encode(payload)
    out = Path(dest) if dest else (root / "archive" / BIN_NAME)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(blob)
    return {
        "path": str(out),
        "bytes": len(blob),
        "n_files": len(files),
        "n_verified": payload["n_verified"],
        "n_rejected": payload["n_rejected"],
        "sha256": payload["sha256"],
    }


def load(src: Path | None = None) -> dict[str, Any]:
    path = Path(src) if src else bin_path()
    return decode(path.read_bytes())


def restore(dest_motor: Path, src: Path | None = None) -> dict[str, Any]:
    """Write snapshot files onto dest_motor (creates archive/ and runs/)."""
    payload = load(src)
    dest = Path(dest_motor)
    written = []
    for rel, text in payload["files"].items():
        p = dest / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
        written.append(rel)
    return {
        "dest": str(dest),
        "written": written,
        "n_verified": payload["n_verified"],
        "n_rejected": payload["n_rejected"],
    }


def verify_roundtrip(motor_dir: Path | None = None, scratch: Path | None = None) -> dict[str, Any]:
    """Dump live → restore to empty dir → compare hashes and PrologArchive counts."""
    import tempfile

    from motor.prolog.bridge import PrologArchive

    root = Path(motor_dir) if motor_dir else MOTOR_DIR
    dumped = dump(root)
    payload = load(root / "archive" / BIN_NAME)
    live_files = collect(root)
    mismatches = []
    for rel, text in live_files.items():
        got = payload["files"].get(rel)
        if got is None:
            mismatches.append(f"missing in bin: {rel}")
        elif _sha(got) != _sha(text):
            mismatches.append(f"hash mismatch: {rel}")
    tmp = Path(scratch) if scratch else Path(tempfile.mkdtemp(prefix="makb-restore-"))
    restore(tmp, src=root / "archive" / BIN_NAME)
    arch = PrologArchive(theory_path=tmp / "archive" / "theory.pl", meta_path=tmp / "archive" / "meta.json")
    n_v = arch.count_verified()
    n_r = arch.count_rejected()
    count_ok = (n_v == payload["n_verified"] and n_r == payload["n_rejected"])
    return {
        "ok": (not mismatches) and count_ok,
        "dumped": dumped,
        "restored_dir": str(tmp),
        "n_verified_live": payload["n_verified"],
        "n_verified_reloaded": n_v,
        "n_rejected_live": payload["n_rejected"],
        "n_rejected_reloaded": n_r,
        "mismatches": mismatches,
        "count_ok": count_ok,
    }


def dump_quietly(archive_dir: Path) -> None:
    """Called from PrologArchive.save — best-effort, never break a tick."""
    try:
        # archive_dir is motor/archive; motor_dir is parent
        dump(motor_dir=Path(archive_dir).parent)
    except Exception:
        pass
