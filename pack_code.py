#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import fnmatch
import os
from pathlib import Path


DEFAULT_EXTENSIONS = {
    ".h",
    ".hpp",
    ".hh",
    ".hxx",
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".inl",
    ".ipp",
}


DEFAULT_EXCLUDE_DIRS = {
    ".git",
    ".idea",
    ".vs",
    ".vscode",
    "build",
    "cmake-build-debug",
    "cmake-build-release",
    "out",
    "bin",
    "lib",
    "third_party",
    "3rdparty",
    "vendor",
    "deps",
}


DEFAULT_EXCLUDE_FILES = {
    "*.pb.h",
    "*.pb.cc",
    "*.pb.cpp",
    "*.generated.*",
}


def is_match_any(name: str, patterns: set[str]) -> bool:
    for pattern in patterns:
        if fnmatch.fnmatch(name, pattern):
            return True
    return False


def should_skip_dir(dirname: str, exclude_dirs: set[str]) -> bool:
    return dirname in exclude_dirs


def should_skip_file(filename: str, exclude_files: set[str]) -> bool:
    return is_match_any(filename, exclude_files)


def read_file_safe(path: Path) -> str:
    encodings = ["utf-8", "gbk", "utf-16"]

    for encoding in encodings:
        try:
            return path.read_text(encoding=encoding)
        except Exception:
            pass

    return f"<<<< READ FAILED: {path} >>>>"


def collect_files(
    root: Path,
    extensions: set[str],
    exclude_dirs: set[str],
    exclude_files: set[str],
) -> list[Path]:
    result = []

    for current_root, dirs, files in os.walk(root):
        dirs[:] = [
            d for d in dirs
            if not should_skip_dir(d, exclude_dirs)
        ]

        current_path = Path(current_root)

        for file in files:
            if should_skip_file(file, exclude_files):
                continue

            file_path = current_path / file

            if file_path.suffix.lower() not in extensions:
                continue

            result.append(file_path)

    result.sort()
    return result


def build_output(root: Path, files: list[Path]) -> str:
    lines = []

    lines.append("=" * 80)
    lines.append("PROJECT SOURCE EXPORT")
    lines.append("=" * 80)
    lines.append(f"ROOT: {root.resolve()}")
    lines.append(f"FILE COUNT: {len(files)}")
    lines.append("")

    for file_path in files:
        relative_path = file_path.relative_to(root)

        lines.append("")
        lines.append("=" * 80)
        lines.append(f"FILE: {relative_path}")
        lines.append("=" * 80)
        lines.append("")

        content = read_file_safe(file_path)
        lines.append(content.rstrip())
        lines.append("")

    return "\n".join(lines)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Pack C/C++ source files into one txt file for LLM."
    )

    parser.add_argument(
        "root",
        help="Project root directory",
    )

    parser.add_argument(
        "-o",
        "--output",
        default="project_dump.txt",
        help="Output txt file",
    )

    parser.add_argument(
        "--ext",
        nargs="*",
        default=[],
        help="Extra file extensions, example: --ext .proto .txt",
    )

    parser.add_argument(
        "--exclude-dir",
        nargs="*",
        default=[],
        help="Extra exclude directories",
    )

    parser.add_argument(
        "--exclude-file",
        nargs="*",
        default=[],
        help="Extra exclude file patterns",
    )

    return parser.parse_args()


def main():
    args = parse_args()

    root = Path(args.root).resolve()

    if not root.exists():
        raise FileNotFoundError(f"Directory not found: {root}")

    extensions = set(DEFAULT_EXTENSIONS)
    extensions.update(args.ext)

    exclude_dirs = set(DEFAULT_EXCLUDE_DIRS)
    exclude_dirs.update(args.exclude_dir)

    exclude_files = set(DEFAULT_EXCLUDE_FILES)
    exclude_files.update(args.exclude_file)

    files = collect_files(
        root=root,
        extensions=extensions,
        exclude_dirs=exclude_dirs,
        exclude_files=exclude_files,
    )

    output = build_output(root, files)

    output_path = Path(args.output).resolve()
    output_path.write_text(output, encoding="utf-8")

    total_size = output_path.stat().st_size / 1024 / 1024

    print(f"Done.")
    print(f"Files: {len(files)}")
    print(f"Output: {output_path}")
    print(f"Size: {total_size:.2f} MB")


if __name__ == "__main__":
    main()