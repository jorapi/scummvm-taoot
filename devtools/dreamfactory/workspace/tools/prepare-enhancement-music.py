#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a private optional soundtrack pack from the supplied playlist download.

Requires ffmpeg and yt-dlp's per-video info JSON and best-audio downloads.
Original game files are only read. Album-track repeats are deliberately labelled
as drafts: authored musical loop points need audition against the remaster.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import wave

ROOT = Path(__file__).resolve().parent.parent
TRACKS = [
    ('FipvCXPeA18', ['BEDRAD1.TRK']),
    ('Crf4YVL9ADU', ['DECKC.TRK']),
    ('E0GIKz8AhVs', ['DECKA.TRK', 'DECKA.11K']),
    ('C-cDJb8MbHw', ['DECKB.TRK', 'DECKB.11K']),
    ('ItLokxtk1Vw', ['DECKD.TRK']),
    ('tCTMhrX9Q14', ['DECKE.TRK', 'DECKE.11K']),
    ('VLoKXckKX-s', ['DECKF.TRK', 'DECKF.11K']),
    ('KVxQ2z-GhOQ', ['CARGO.TRK', 'CARGO.11K']),
    ('8JvHBJTJfas', ['SMSTACK.TRK']),
    ('hgaqj-BTOlQ', ['SINK0.11K']),
    ('sHrwF6ZRwmc', ['SINK0.TRK']),
    ('8gAWHCIM9wc', ['SINK1.TRK', 'SINK1.11K']),
    ('si9P-TDnOPE', ['SINK2.TRK', 'SINK2.11K']),
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'build/enhancements/soundtrack-source')
    parser.add_argument('--game', type=Path, default=ROOT / 'game-data/full')
    parser.add_argument('--output', type=Path, default=ROOT / 'game-data/full/enhancements')
    args = parser.parse_args()
    index = json.loads((args.game / 'resource-index.json').read_text())
    identity = hashlib.md5((args.game / 'BOOTFILE').read_bytes()).hexdigest()
    music = args.output / 'music'
    music.mkdir(parents=True, exist_ok=True)
    ini = ['[pack]', 'format=1', f'boot_md5={identity}',
           'title=Louis Stever HQ soundtrack - draft album loops',
           'author=Louis Stever', 'loop_review=pending', '']
    report = dict(scope='Private soundtrack enhancement pack; album-loop boundaries pending musical audition.',
                  bootMD5=identity, tracks=[], mappings=[])
    for video, banks in TRACKS:
        info = json.loads((args.source / f'{video}.info.json').read_text(encoding='utf-8'))
        inputs = [p for p in args.source.glob(f'{video}.*') if p.suffix in ('.webm', '.m4a', '.opus', '.ogg')]
        if len(inputs) != 1:
            raise ValueError(f'Expected one downloaded audio file for {video}: {inputs}')
        output = music / f'{video}.wav'
        if not output.exists() or output.stat().st_mtime < inputs[0].stat().st_mtime:
            subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-nostdin', '-y',
                            '-i', str(inputs[0]), '-vn', '-ar', '44100', '-ac', '2',
                            '-c:a', 'pcm_s16le', str(output)], check=True)
        with wave.open(str(output)) as wav:
            assert wav.getnchannels() == 2 and wav.getsampwidth() == 2 and wav.getframerate() == 44100
            frames = wav.getnframes()
        assert output.stat().st_size <= 64 * 1024 * 1024 and frames * 2 <= 32 * 1024 * 1024
        report['tracks'].append(dict(video=video, title=info['title'], creator=info.get('uploader'),
            sourceURL=info.get('webpage_url'), sourceFile=inputs[0].name, sourceSHA256=sha(inputs[0]),
            file=f'music/{output.name}', sha256=sha(output), frames=frames, rate=44100, channels=2,
            loopStatus='whole album track repeats; seamless musical loop not yet approved'))
        for bank in banks:
            for variant in index.get(bank, []):
                source = variant['path'].removeprefix('volumes/')
                if not source.startswith(('installed/', 'titanic1/', 'titanic2/')):
                    continue
                ini += [f'[music-{len(report["mappings"]):03d}]', f'source={source}',
                        f'file=music/{output.name}', 'loop_start=0', f'loop_end={frames}', '']
                if bank == 'BEDRAD1.TRK':
                    ini.insert(len(ini) - 1, 'preserve_original_intro=true')
                report['mappings'].append(dict(source=source, originalSHA256=variant['sha256'], video=video))
        print(f'Prepared {info["title"]}', flush=True)
    (args.output / 'pack.ini').write_text('\n'.join(ini), encoding='utf-8')
    (args.output / 'music-pack-report.json').write_text(json.dumps(report, indent=2) + '\n')
    (args.output / 'CREDITS.txt').write_text(
        'Titanic: Adventure Out of Time HQ soundtrack remakes by Louis Stever.\n'
        'Original soundtrack composed by Erik Holt.\n'
        'Creator description: remade using original E-MU Proteus samples.\n'
        'Playlist: https://www.youtube.com/playlist?list=PLGcaWrQt0MmmiRmJUk7sIC2Vm_TK4vJvy\n\n'
        'These are best-available YouTube downloads decoded to stereo PCM WAV;\n'
        'conversion to WAV does not recover information lost in the source encoding.\n'
        'Private local pack; not included in the engine source overlay or APK.\n'
        'Current track loops repeat the full album track. Musical loop refinement\n'
        'and listening checks remain pending. Original music is the default.\n')
    print(f'Pack: {args.output}; {len(report["tracks"])} tracks, {len(report["mappings"])} exact resource mappings.')


if __name__ == '__main__':
    main()
