#!/usr/bin/env python3
from __future__ import annotations

import argparse
import codecs
import os
import re
import sys
from pathlib import Path


SOURCE_TEXT = "renderdoc"
TARGET_TEXT = "riderduck"
SOURCE_SEGMENTS = ("render", "doc")
TARGET_SEGMENTS = ("rider", "duck")
SKIP_DIRECTORIES = {".git", ".hg", ".svn", "__pycache__"}
TEXT_EXTENSIONS = {
    ".bat",
    ".c",
    ".cc",
    ".cmake",
    ".cpp",
    ".cs",
    ".css",
    ".h",
    ".hpp",
    ".hlsl",
    ".htm",
    ".html",
    ".inl",
    ".json",
    ".md",
    ".natvis",
    ".pro",
    ".props",
    ".py",
    ".qrc",
    ".rc",
    ".rc2",
    ".rst",
    ".sh",
    ".sln",
    ".targets",
    ".txt",
    ".ui",
    ".vcxproj",
    ".wxl",
    ".xml",
    ".yml",
}
BINARY_EXTENSIONS = {
    ".7z",
    ".a",
    ".aps",
    ".bin",
    ".bmp",
    ".class",
    ".cur",
    ".dat",
    ".db",
    ".dll",
    ".dylib",
    ".exe",
    ".gif",
    ".gz",
    ".ico",
    ".ilk",
    ".jar",
    ".jpg",
    ".jpeg",
    ".lib",
    ".lz4",
    ".mp3",
    ".mp4",
    ".obj",
    ".ogg",
    ".otf",
    ".pdb",
    ".pdf",
    ".png",
    ".pyc",
    ".pyd",
    ".pyo",
    ".rar",
    ".rtf",
    ".snk",
    ".so",
    ".tar",
    ".ttf",
    ".wav",
    ".webp",
    ".woff",
    ".woff2",
    ".xz",
    ".zip",
}
TEXT_BOMS = (
    (codecs.BOM_UTF8, "utf-8-sig"),
    (codecs.BOM_UTF16_LE, "utf-16"),
    (codecs.BOM_UTF16_BE, "utf-16"),
    (codecs.BOM_UTF32_LE, "utf-32"),
    (codecs.BOM_UTF32_BE, "utf-32"),
)
PATTERN = re.compile(re.escape(SOURCE_TEXT), re.IGNORECASE)
SELF_PATH = Path(__file__).resolve()
SEGMENT_SPLITS = []
offset = 0
for segment in SOURCE_SEGMENTS:
    offset += len(segment)
    SEGMENT_SPLITS.append(offset)


def apply_case(source_fragment: str, replacement_fragment: str) -> str:
    if not source_fragment:
        return replacement_fragment
    if source_fragment.isupper():
        return replacement_fragment.upper()
    if source_fragment.islower():
        return replacement_fragment.lower()
    if source_fragment[0].isupper() and source_fragment[1:].islower():
        return replacement_fragment[:1].upper() + replacement_fragment[1:].lower()

    upper_count = sum(1 for char in source_fragment if char.isupper())
    lower_count = sum(1 for char in source_fragment if char.islower())
    if upper_count > lower_count:
        return replacement_fragment.upper()
    if lower_count > upper_count:
        return replacement_fragment.lower()
    return replacement_fragment


def replace_match(match: re.Match[str]) -> str:
    matched = match.group(0)
    source_parts = []
    start = 0
    for split in SEGMENT_SPLITS:
        source_parts.append(matched[start:split])
        start = split
    if start < len(matched):
        source_parts.append(matched[start:])

    replacement_parts = []
    for index, replacement_fragment in enumerate(TARGET_SEGMENTS):
        source_fragment = source_parts[index] if index < len(source_parts) else ""
        replacement_parts.append(apply_case(source_fragment, replacement_fragment))

    if len(source_parts) > len(TARGET_SEGMENTS):
        tail_source = "".join(source_parts[len(TARGET_SEGMENTS) :])
        replacement_parts.append(apply_case(tail_source, ""))

    return "".join(replacement_parts)


def replace_text(text: str) -> tuple[str, int]:
    return PATTERN.subn(replace_match, text)


def looks_like_text(text: str) -> bool:
    sample = text[:4096]
    if not sample:
        return True

    printable = 0
    for char in sample:
        if char in "\n\r\t\f\b":
            printable += 1
            continue
        if char.isprintable():
            printable += 1

    return printable / len(sample) >= 0.95


def decode_text(data: bytes) -> tuple[str | None, str | None]:
    for bom, encoding in TEXT_BOMS:
        if data.startswith(bom):
            try:
                return data.decode(encoding), encoding
            except UnicodeDecodeError:
                return None, None

    try:
        return data.decode("utf-8"), "utf-8"
    except UnicodeDecodeError:
        pass

    try:
        text = data.decode("cp1252")
    except UnicodeDecodeError:
        return None, None

    if looks_like_text(text):
        return text, "cp1252"
    return None, None


def is_probably_binary(path: Path, data: bytes) -> bool:
    if path.suffix.lower() in TEXT_EXTENSIONS:
        return False

    if path.suffix.lower() in BINARY_EXTENSIONS:
        return True

    if b"\x00" in data:
        for bom, _encoding in TEXT_BOMS[1:]:
            if data.startswith(bom):
                return False
        return True

    return False


def safe_rename(path: Path, new_path: Path, dry_run: bool) -> None:
    if path == new_path:
        return

    if new_path.exists():
        raise FileExistsError(f"Rename target already exists: {new_path}")

    print(f"rename: {path} -> {new_path}")
    if not dry_run:
        try:
            path.rename(new_path)
        except PermissionError as exc:
            raise PermissionError(
                f"Failed to rename {path} -> {new_path}. "
                "Close any programs using the file and try again."
            ) from exc


def collect_paths(root: Path) -> tuple[list[Path], list[Path]]:
    files: list[Path] = []
    directories: list[Path] = []

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if name not in SKIP_DIRECTORIES]
        current_dir = Path(dirpath)
        directories.extend(current_dir / name for name in dirnames)
        files.extend(
            path for path in (current_dir / name for name in filenames) if path.resolve() != SELF_PATH
        )

    return files, directories


def process_file_content(path: Path, dry_run: bool) -> int:
    data = path.read_bytes()
    if is_probably_binary(path, data):
        return 0

    text, encoding = decode_text(data)
    if text is None or encoding is None:
        return 0

    replaced, count = replace_text(text)
    if count == 0:
        return 0

    print(f"update: {path} ({count} replacements)")
    if not dry_run:
        path.write_bytes(replaced.encode(encoding))
    return count


def rename_paths(paths: list[Path], dry_run: bool) -> int:
    renamed = 0
    for path in paths:
        new_name, count = replace_text(path.name)
        if count == 0:
            continue
        safe_rename(path, path.with_name(new_name), dry_run)
        renamed += 1
    return renamed


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Replace renderdoc with riderduck across text file contents, file names, "
            "and directory names while preserving common case styles."
        )
    )
    parser.add_argument(
        "root",
        nargs="?",
        default=".",
        help="Repository root to process. Defaults to the current directory.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show the planned changes without writing anything.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()

    if not root.exists():
        print(f"Root path does not exist: {root}", file=sys.stderr)
        return 1
    if not root.is_dir():
        print(f"Root path is not a directory: {root}", file=sys.stderr)
        return 1

    try:
        files, directories = collect_paths(root)
        content_replacements = 0
        for path in files:
            content_replacements += process_file_content(path, args.dry_run)

        renamed_files = rename_paths(
            sorted(files, key=lambda item: len(item.parts), reverse=True),
            args.dry_run,
        )
        renamed_directories = rename_paths(
            sorted(directories, key=lambda item: len(item.parts), reverse=True),
            args.dry_run,
        )
    except OSError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1

    print(
        "\nSummary: "
        f"{content_replacements} content replacements, "
        f"{renamed_files} files renamed, "
        f"{renamed_directories} directories renamed."
    )
    if args.dry_run:
        print("Dry run only: no files were changed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
