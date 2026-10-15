#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check CI packages for private build paths and prepare release checksums."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import zipfile

EXPECTED = {
    "dreamfactory-windows-x64-unsigned.zip",
    "dreamfactory-macos-arm64-adhoc.zip",
    "dreamfactory-macos-x86_64-adhoc.zip",
    "dreamfactory-ios-arm64-unsigned.ipa",
    "dreamfactory-android-arm64-v8a-unsigned.apk",
    "dreamfactory-android-armeabi-v7a-unsigned.apk",
    "dreamfactory-android-x86_64-unsigned.apk",
    "dreamfactory-psp-experimental.zip",
}
HOME = re.compile(rb"(?:(?i:[A-Za-z]:[/\\]Users[/\\])|/(?:Users|home)/)([^/\\\x00\s]+)")
PUBLIC_BUILD_USERS = {b"runner", b"runneradmin", b"vsts", b"build", b"builder"}
CREDENTIAL = re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|\bgh[pousr]_[A-Za-z0-9]{20,}|\bgithub_pat_[A-Za-z0-9_]{20,}")


def scan(data, label, depth=0):
    # Decode contiguous UTF-16 strings only. Removing every NUL joins unrelated
    # native string-table entries and invents usernames after /home/runner.
    payloads = [data]
    payloads.extend(match.group()[::2] for match in re.finditer(rb"(?:[\x20-\x7e]\x00){6,}", data))
    for payload in payloads:
        if CREDENTIAL.search(payload):
            raise ValueError(f"Credential-like material in {label}")
        for match in HOME.finditer(payload):
            if match.group(1).lower() not in PUBLIC_BUILD_USERS:
                raise ValueError(f"Personal home-directory path in {label}")
    if depth < 3 and zipfile.is_zipfile(io.BytesIO(data)):
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            if sum(i.file_size for i in archive.infolist()) > 1024 ** 3:
                raise ValueError(f"Archive too large to inspect: {label}")
            for entry in archive.infolist():
                if entry.is_dir():
                    continue
                if entry.filename.startswith(("/", "\\")) or ".." in entry.filename.replace("\\", "/").split("/"):
                    raise ValueError(f"Unsafe archive path in {label}")
                scan(archive.read(entry), label + ":" + entry.filename, depth + 1)


def check(paths):
    for path in paths:
        if not path.is_file():
            raise ValueError(f"Missing package: {path.name}")
        if path.suffix in (".zip", ".apk", ".ipa") and not zipfile.is_zipfile(path):
            raise ValueError(f"Not a valid package archive: {path.name}")
        scan(path.read_bytes(), path.name)
        print(f"Privacy/contents check passed: {path.name}")


def prepare(directory):
    packages = sorted(p for p in directory.iterdir() if p.suffix in (".zip", ".apk", ".ipa"))
    if {p.name for p in packages} != EXPECTED:
        raise ValueError("Release requires exactly the eight expected platform packages")
    check(packages)
    manifest = {"repository": os.environ["GITHUB_REPOSITORY"], "sourceCommit": os.environ["GITHUB_SHA"],
                "runUrl": f"https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}",
                "validation": "CI compilation, package contents and privacy checks; no device or full-game playthrough",
                "assets": [{"file": p.name, "bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in packages]}
    metadata = directory / "manifest.json"
    metadata.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    checksums = "".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in packages + [metadata])
    (directory / "SHA256SUMS.txt").write_text(checksums, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check", "prepare"))
    parser.add_argument("paths", nargs="+", type=Path)
    args = parser.parse_args()
    if args.command == "check":
        check(args.paths)
    elif len(args.paths) == 1:
        prepare(args.paths[0])
    else:
        parser.error("prepare takes one artifact directory")


if __name__ == "__main__":
    main()
