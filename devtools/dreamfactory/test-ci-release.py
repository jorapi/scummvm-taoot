#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("ci_release", Path(__file__).with_name("ci-release.py"))
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class PrivacyChecks(unittest.TestCase):
    def test_personal_build_paths(self):
        for path in (r"C:\Users\PrivatePerson\project", "/home/PrivatePerson/project", "/Users/PrivatePerson/project"):
            for encoding in ("utf-8", "utf-16le"):
                with self.subTest(path=path, encoding=encoding), self.assertRaises(ValueError):
                    release.scan(path.encode(encoding), "synthetic")

    def test_public_runner_paths(self):
        release.scan(b"/home/runner/work/project C:\\Users\\runneradmin\\work", "runner")

    def test_nested_archive_leak(self):
        data = io.BytesIO()
        with zipfile.ZipFile(data, "w") as archive:
            archive.writestr("program", b"/home/PrivatePerson/project")
        with self.assertRaises(ValueError):
            release.scan(data.getvalue(), "nested")

    def test_private_key(self):
        with self.assertRaises(ValueError):
            release.scan(b"-----BEGIN PRIVATE KEY-----", "synthetic")

    def test_archive_traversal(self):
        data = io.BytesIO()
        with zipfile.ZipFile(data, "w") as archive:
            archive.writestr("../outside", b"data")
        with self.assertRaises(ValueError):
            release.scan(data.getvalue(), "synthetic")

    def test_incomplete_release_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                release.prepare(Path(directory))

    def test_invalid_package_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "broken.apk"
            path.write_bytes(b"not an APK")
            with self.assertRaises(ValueError):
                release.check([path])


if __name__ == "__main__":
    unittest.main()
