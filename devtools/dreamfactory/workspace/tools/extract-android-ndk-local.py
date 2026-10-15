"""Install the already downloaded Linux NDK on NTFS with Windows-native I/O."""
import hashlib
import pathlib
import posixpath
import stat
import sys
import urllib.request
import zipfile

archive = pathlib.Path('C:/Android/downloads/android-ndk-r23c-linux.zip')
target = pathlib.Path('C:/Android/linux-sdk/ndk/23.2.8568313')
if not archive.exists():
    archive.parent.mkdir(parents=True, exist_ok=True)
    urllib.request.urlretrieve('https://dl.google.com/android/repository/android-ndk-r23c-linux.zip', archive)
assert hashlib.sha1(archive.read_bytes()).hexdigest() == 'e5053c126a47e84726d9f7173a04686a71f9a67a'
with zipfile.ZipFile(archive) as source:
    def contents(entry):
        if stat.S_ISLNK(entry.external_attr >> 16):
            link = source.read(entry).decode()
            name = posixpath.normpath(posixpath.join(posixpath.dirname(entry.filename), link))
            return contents(source.getinfo(name))
        return source.read(entry)

    for entry in source.infolist():
        if '--repair-links' in sys.argv and not stat.S_ISLNK(entry.external_attr >> 16):
            continue
        relative = pathlib.PurePosixPath(entry.filename).relative_to('android-ndk-r23c')
        assert '..' not in relative.parts
        dest = target.joinpath(*relative.parts)
        if entry.is_dir():
            dest.mkdir(parents=True, exist_ok=True)
        else:
            dest.parent.mkdir(parents=True, exist_ok=True)
            # Resolve archive symlinks to real files for Windows/WSL interoperability.
            dest.write_bytes(contents(entry))
assert '23.2.8568313' in (target / 'source.properties').read_text()
print('Installed checksum-verified Linux NDK:', target)
