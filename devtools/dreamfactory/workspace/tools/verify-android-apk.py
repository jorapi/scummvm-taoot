"""Verify the locally built DreamFactory Android packages, without a device."""
import hashlib
import datetime
import json
import pathlib
import re
import struct
import argparse
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--build-log', default='build/android-arm64-build-final.log')
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parent.parent
build = root / 'scummvm/build-dreamfactory/android-arm64-v8a'
artifacts = root / 'scummvm/build-dreamfactory/artifacts'
assert re.search(r'^ENABLE_DREAMFACTORY\s*[:?+]?=\s*STATIC_PLUGIN',
                 (build / 'config.mk').read_text(), re.M)
assert '#define USE_FREETYPE2' in (build / 'config.h').read_text()
results = []
for variant in ('unsigned', 'debug'):
    apk = artifacts / f'dreamfactory-android-arm64-v8a-{variant}.apk'
    with zipfile.ZipFile(apk) as archive:
        assert archive.testzip() is None
        names = archive.namelist()
        assert 'AndroidManifest.xml' in names
        assert 'classes.dex' in names
        assert 'assets/assets/fonts.dat' in names
        assert 'assets/doc/OBOE-LICENSE.txt' in names
        assert 'assets/doc/FREETYPE-LICENSE.txt' in names
        assert any(n.endswith('scummmodern.zip') for n in names)
        native = archive.read('lib/arm64-v8a/libscummvm.so')
        assert native[:5] == b'\x7fELF\x02'
        assert struct.unpack_from('<H', native, 18)[0] == 183  # AArch64
        phoff = struct.unpack_from('<Q', native, 32)[0]
        phsize, phnum = struct.unpack_from('<HH', native, 54)
        load_alignments = [struct.unpack_from('<Q', native, phoff + i * phsize + 48)[0]
                           for i in range(phnum)
                           if struct.unpack_from('<I', native, phoff + i * phsize)[0] == 1]
        assert load_alignments and all(a >= 16384 for a in load_alignments)
        assert b'CyberFlix DreamFactory' in native
        assert b'dreamfactory' in native
        assert not any(n.startswith('assets/assets/games/') for n in names)
        results.append(dict(file=apk.name, bytes=apk.stat().st_size,
                            sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
                            nativeSha256=hashlib.sha256(native).hexdigest(),
                            abi='arm64-v8a', variant=variant,
                            zipCrcValid=True, dreamfactoryPresent=True,
                            freetypeEnabled=True, fontsAndThemePresent=True,
                            elfLoadAlignments=load_alignments))
report = dict(scope='Android ARM64 compile and package verification; no device runtime test.',
              createdAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              ndkVersion='23.2.8568313', sdkPlatform='android-37.0',
              oboeVersion='1.10.0', freetypeVersion='2.13.3',
              configSha256=hashlib.sha256((build / 'config.mk').read_bytes()).hexdigest(),
              buildLog=args.build_log,
              buildLogSha256=hashlib.sha256((root / args.build_log).read_bytes()).hexdigest(),
              artifacts=results)
(root / 'build/android-package-audit.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
