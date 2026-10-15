"""Prepare user-owned archives, preserving volumes and a development flat view."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / 'game-data'
EXTENSIONS = {'.SET', '.FLT', '.PRP', '.SND', '.CST', '.PUP', '.SBK', '.MOV', '.TRK', '.STG', '.SHP', '.SFX', '.11K', '.PIC', '.JPG'}
DUST_DIRS = {'DATA', 'UNDER', 'TARGET', 'MOVIES', 'PUPPETS', 'INVEN', 'SALGAMES', 'CHECKERS', 'CRACK', 'DRUGS', 'FIGHT', 'SCORP', 'KID', 'YUNNIBOX'}


def safe_parts(name):
    parts = PurePosixPath(name.replace('\\', '/')).parts
    if not parts or any(p in ('..', '/') or ':' in p for p in parts):
        raise ValueError(f'Unsafe archive path: {name!r}')
    return parts


def resource(name):
    p = PurePosixPath(name)
    # The original engine executables contain the game's named cursor artwork.
    # They are read as resource archives; ScummVM never executes them.
    return p.name.upper() in {'BOOTFILE', 'TI.EXE', 'DF.EXE'} or p.suffix.upper() in EXTENSIONS


def write(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open('wb') as target:
        shutil.copyfileobj(source, target)


def extract_disc(disc, destination, demo):
    listing = subprocess.run(['7z', 'l', '-slt', str(disc)], check=True, capture_output=True, text=True).stdout
    names = []
    # The first section describes the archive itself, not a file in it.
    for block in listing.split('----------', 1)[1].split('\n\n'):
        fields = dict(line.split(' = ', 1) for line in block.splitlines() if ' = ' in line)
        name = fields.get('Path', '')
        if fields.get('Folder') != '-' or not name:
            continue
        parts = safe_parts(name)
        if demo and parts[0].upper() != 'TITANIC':
            continue
        if not demo and parts[0].upper() in {'INSTALL', 'SUPPORT'}:
            continue
        if resource(name.replace('\\', '/')):
            names.append(name)
    if not names:
        raise RuntimeError(f'No game resources found in {disc}')
    # Exact paths avoid flattening same-named files from different directories.
    listfile = DATA / 'discs' / (disc.stem + '-resources.txt')
    listfile.write_text('\n'.join(names), encoding='utf-8')
    subprocess.run(['7z', 'x', str(disc), '@' + str(listfile), '-scsUTF-8', f'-o{destination}', '-y', '-bso0'], check=True)


def prepare_titanic(filename, edition, marker):
    with zipfile.ZipFile(ROOT / filename) as package:
        entries = [e for e in package.infolist() if not e.is_dir()]
        for entry in sorted(entries, key=lambda e: e.filename):
            parts = safe_parts(entry.filename)
            if not entry.filename.lower().endswith('.iso'):
                continue
            disc = DATA / 'discs' / parts[-1]
            if not disc.exists() or disc.stat().st_size != entry.file_size:
                with package.open(entry) as source:
                    write(source, disc)
            volume = '3pacdemo' if edition == 'demo' else ('titanic1' if 'CD1' in disc.stem.upper() else 'titanic2')
            extract_disc(disc, DATA / edition / 'volumes' / volume, edition == 'demo')
        for entry in entries:
            parts = safe_parts(entry.filename)
            if marker not in parts:
                continue
            relative = parts[parts.index(marker) + 1:]
            if not relative or not resource(parts[-1]):
                continue
            if relative[0].upper() in {'LOCAL', 'TOUR'} or len(relative) == 1 and relative[0].upper() in {'BOOTFILE', 'TI.EXE'}:
                with package.open(entry) as source:
                    write(source, DATA / edition / 'installed' / Path(*relative))
    index_resources(edition)


def prepare_dust():
    with zipfile.ZipFile(ROOT / 'dust.dbgl.zip') as package:
        for entry in package.infolist():
            if entry.is_dir():
                continue
            parts = safe_parts(entry.filename)
            if not resource(parts[-1]):
                continue
            destination = None
            if 'DUSTCD' in parts:
                relative = parts[parts.index('DUSTCD') + 1:]
                if relative and relative[0].upper() in DUST_DIRS:
                    destination = DATA / 'dust' / 'volumes' / 'dust' / Path(*relative)
            elif 'WIN31' in parts and 'DUST' in parts:
                relative = parts[parts.index('DUST') + 1:]
                if relative and (relative[0].upper() == 'LOCAL' or len(relative) == 1 and relative[0].upper() in {'BOOTFILE', 'DF.EXE'}):
                    destination = DATA / 'dust' / 'installed' / Path(*relative)
            if destination:
                with package.open(entry) as source:
                    write(source, destination)
    index_resources('dust')


def index_resources(edition):
    folder = DATA / edition
    index = {}
    # This flat compatibility view serves the room viewer and corpus audits.
    # Installed files win; disc 1 wins a disc-vs-disc collision. The original
    # variants remain in volumes/ for the future volume-aware runtime resolver.
    files = sorted((folder / 'installed').rglob('*')) + sorted((folder / 'volumes').rglob('*'))
    for file in files:
        if not file.is_file() or not resource(file.name):
            continue
        name = file.name.upper()
        with file.open('rb') as source:
            digest = hashlib.file_digest(source, 'sha256').hexdigest()
        index.setdefault(name, []).append({'path': file.relative_to(folder).as_posix(), 'bytes': file.stat().st_size, 'sha256': digest})
    for name, variants in index.items():
        shutil.copyfile(folder / variants[0]['path'], folder / name)
    (folder / 'resource-index.json').write_text(json.dumps(index, indent=2) + '\n', encoding='utf-8')
    conflicts = sum(len({v['sha256'] for v in variants}) > 1 for variants in index.values())
    boot = (folder / 'BOOTFILE').read_bytes()
    print(edition, 'resources=', len(index), 'different same-name variants=', conflicts,
          'rooms=', len(list(folder.glob('*.SET'))), 'boot bytes=', len(boot),
          'md5(first 5000)=', hashlib.md5(boot[:5000]).hexdigest(), flush=True)


if __name__ == '__main__':
    prepare_titanic('taoot_en_demo_offline.dbgl.zip', 'demo', 'TITANICD')
    prepare_titanic('taoot_en_offline.dbgl.zip', 'full', 'TITANIC')
    if (ROOT / 'dust.dbgl.zip').exists():
        prepare_dust()
