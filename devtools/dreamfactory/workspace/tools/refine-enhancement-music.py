#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Find repeated musical passages and build a separate, auditionable loop pack.

No original downloads or installed pack are overwritten. Signal similarity is
not musical approval: every result explicitly retains listeningReviewed=false.
Requires NumPy (workspace dependencies/image-processing is also supported).
"""
import argparse
import configparser
import hashlib
import html
import json
from pathlib import Path
import sys
import wave

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'dependencies/image-processing'))
import numpy as np


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def candidate(pcm, rate):
    # Spectral context spans four seconds on each side of the candidate seam.
    # Matching an isolated note is insufficient; compare a whole local phrase.
    mono = pcm.astype(np.float32).mean(axis=1)[::max(1, rate // 4000)]
    sample_rate = rate / max(1, rate // 4000)
    hop = round(sample_rate / 4)
    size = 2048
    frames = np.lib.stride_tricks.sliding_window_view(mono, size)[::hop]
    spectra = np.abs(np.fft.rfft(frames * np.hanning(size), axis=1))
    frequencies = np.fft.rfftfreq(size, 1 / sample_rate)
    edges = np.geomspace(60, 1800, 33)
    features = np.stack([np.sqrt(np.mean(spectra[:, (frequencies >= a) & (frequencies < b)] ** 2, axis=1))
                         for a, b in zip(edges[:-1], edges[1:])], axis=1)
    features = np.log1p(features)
    features -= features.mean(axis=1, keepdims=True)
    features /= np.maximum(np.linalg.norm(features, axis=1, keepdims=True), 1e-8)
    context = 16
    windows = np.lib.stride_tricks.sliding_window_view(features, context, axis=0).reshape(-1, 32 * context).copy()
    windows /= np.maximum(np.linalg.norm(windows, axis=1, keepdims=True), 1e-8)
    times = (np.arange(len(windows)) * hop + size / 2 + context * hop / 2) / sample_rate
    energy = np.sqrt(np.mean(frames ** 2, axis=1))[:len(windows)]
    similarity = windows @ windows.T
    durations = times[None, :] - times[:, None]
    good = ((durations >= 20) & (times[:, None] >= 5) & (times[None, :] <= len(pcm) / rate - 7)
            & (energy[:, None] > np.max(energy) * .15) & (energy[None, :] > np.max(energy) * .15))
    similarity[~good] = -1
    peak = float(np.max(similarity))
    if peak < .97:
        return None, dict(bestSimilarity=peak, status='No sufficiently similar repeated phrase; original album loop retained')
    # Among similarly good matches, prefer a longer phrase rather than a short
    # repeating accompaniment figure. This remains an audition candidate.
    eligible = similarity >= max(.97, peak - .004)
    lengths = np.where(eligible, durations, -1)
    start_index, end_index = np.unravel_index(np.argmax(lengths), lengths.shape)
    start, end = round(times[start_index] * rate), round(times[end_index] * rate)
    # Refine the end by waveform continuity over 20 ms, without moving by more
    # than a quarter-second from the spectral phrase boundary.
    width = round(rate * .02)
    reference = pcm[start:start + width].astype(np.float32)
    best = (float('inf'), end)
    for shift in range(-round(rate * .125), round(rate * .125), max(1, rate // 4000)):
        at = end + shift
        segment = pcm[at:at + width].astype(np.float32)
        if segment.shape != reference.shape:
            continue
        loss = float(np.mean((segment - reference) ** 2))
        if loss < best[0]:
            best = (loss, at)
    return (start, best[1]), dict(bestSimilarity=peak, chosenSimilarity=float(similarity[start_index, end_index]),
                                proposedStartSeconds=start / rate, proposedEndSeconds=best[1] / rate,
                                status='Automatically matched repeated phrase; musical listening required')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'game-data/full/enhancements')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/enhancements/music-refined')
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        parser.error('Output must differ from the installed source pack')
    report = json.loads((args.source / 'music-pack-report.json').read_text())
    ini = configparser.ConfigParser(interpolation=None)
    ini.read(args.source / 'pack.ini')
    ini['pack']['title'] = 'Louis Stever HQ soundtrack - matched loop audition pack'
    ini['pack']['loop_review'] = 'automatic_matching_complete_listening_pending'
    (args.output / 'music').mkdir(parents=True, exist_ok=True)
    (args.output / 'audition').mkdir(parents=True, exist_ok=True)
    results = []
    for track in report['tracks']:
        source = args.source / track['file']
        assert sha(source) == track['sha256'], 'Source WAV changed'
        with wave.open(str(source)) as audio:
            rate, channels = audio.getframerate(), audio.getnchannels()
            assert audio.getsampwidth() == 2 and channels == 2
            pcm = np.frombuffer(audio.readframes(audio.getnframes()), dtype='<i2').reshape(-1, channels).copy()
        points, analysis = candidate(pcm, rate)
        start, end = points if points else (0, len(pcm))
        overlap = round(rate * .006) if points else 0
        output = pcm[:end].copy()
        if overlap:
            weight = np.linspace(0, 1, overlap, dtype=np.float32)[:, None]
            output[end - overlap:end] = np.rint(output[end - overlap:end] * (1 - weight) + pcm[start:start + overlap] * weight).astype('<i2')
        target = args.output / track['file']
        with wave.open(str(target), 'wb') as audio:
            audio.setnchannels(channels); audio.setsampwidth(2); audio.setframerate(rate); audio.writeframes(output.tobytes())
        seam = np.concatenate((output[max(start + overlap, end - 5 * rate):end], output[start + overlap:min(end, start + overlap + 5 * rate)]))
        with wave.open(str(args.output / 'audition' / (track['video'] + '.wav')), 'wb') as audio:
            audio.setnchannels(channels); audio.setsampwidth(2); audio.setframerate(rate); audio.writeframes(seam.tobytes())
        for section in ini.sections():
            if section.startswith('music-') and ini[section].get('file') == track['file']:
                ini[section]['loop_start'] = str(start + overlap)
                ini[section]['loop_end'] = str(end)
        results.append(dict(video=track['video'], title=track['title'], file=track['file'], rate=rate,
                            sourceSHA256=track['sha256'], outputSHA256=sha(target), loopStart=start + overlap,
                            loopEnd=end, seamCrossfadeFrames=overlap, listeningReviewed=False, **analysis))
        print(track['title'], analysis['status'], flush=True)
    with (args.output / 'pack.ini').open('w') as handle:
        ini.write(handle)
    (args.output / 'loop-review.json').write_text(json.dumps(dict(scope=__doc__, tracks=results), indent=2) + '\n')
    pack_report = dict(report)
    pack_report['scope'] = 'Private automatically matched music loop pack; musical listening review remains pending.'
    pack_report['tracks'] = [{**track, 'sha256': result['outputSHA256'], 'frames': result['loopEnd'],
                              'loopStart': result['loopStart'], 'loopEnd': result['loopEnd'],
                              'loopStatus': result['status'], 'listeningReviewed': False}
                             for track, result in zip(report['tracks'], results)]
    (args.output / 'music-pack-report.json').write_text(json.dumps(pack_report, indent=2) + '\n')
    cards = []
    for result in results:
        cards.append(f'<article><h2>{html.escape(result["title"])}</h2><p>{html.escape(result["status"])}</p>'
                     f'<p>Intro: {result["loopStart"] / result["rate"]:.2f}s; loop ends at {result["loopEnd"] / result["rate"]:.2f}s.</p>'
                     f'<p>Boundary preview (five seconds on each side):</p><audio controls preload="none" src="audition/{result["video"]}.wav"></audio>'
                     f'<p>Full intro/track:</p><audio controls preload="none" src="{result["file"]}"></audio></article>')
    (args.output / 'index.html').write_text('<!doctype html><meta charset="utf-8"><title>Soundtrack loop audition</title>'
        '<style>body{background:#17202b;color:#eee;font:16px system-ui;max-width:900px;margin:30px auto;padding:20px}article{border-top:1px solid #567;padding:15px 0}h2{font-size:19px}audio{width:100%}</style>'
        '<h1>Soundtrack loop audition</h1><p>Automatically matched candidates, not listening-approved loops. '
        'Compare phrasing, rhythm and reverb across the preview boundary. No gain normalization was applied.</p>' + ''.join(cards))
    credits = (args.source / 'CREDITS.txt').read_text()
    credits = credits.replace('Current track loops repeat the full album track. Musical loop refinement\nand listening checks remain pending. Original music is the default.',
                              'This derived audition pack uses automatically matched repeats where found.\nMusical listening review remains pending. Original music is the engine default.')
    (args.output / 'CREDITS.txt').write_text(credits)
    print('Separate audition pack:', args.output, flush=True)


if __name__ == '__main__':
    main()
