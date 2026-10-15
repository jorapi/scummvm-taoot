#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prove a native trace descends from fresh boot using archived checkpoint bytes.

This proves recorded checkpoint continuity, not coverage of unplayed branches.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def validate(record_path):
    record_path = Path(record_path)
    record = json.loads(record_path.read_text(encoding='utf-8-sig'))
    if not isinstance(record, dict):
        raise ValueError('record must be an object')
    if record.get('schema') != 1 or record.get('status') != 'passed':
        raise ValueError('record is not a successful schema-1 trace')
    if not record.get('continuityEligible'):
        raise ValueError('trace can load state outside its recorded checkpoint input')
    for name in ('binarySha256', 'traceToolSha256'):
        if not re.fullmatch(r'[0-9a-fA-F]{64}', record.get(name, '')):
            raise ValueError(f'invalid {name}')
    artifacts = record['artifacts']
    if not isinstance(artifacts, dict) or not {'log', 'config'} <= artifacts.keys():
        raise ValueError('record must archive its native log and startup config')
    for entry in artifacts.values():
        if not isinstance(entry, dict) or not isinstance(entry.get('file'), str) or not isinstance(entry.get('sha256'), str) or not re.fullmatch(r'[0-9a-fA-F]{64}', entry['sha256']):
            raise ValueError('invalid artifact descriptor')
        path = record_path.parent / entry['file']
        if path.resolve().parent != record_path.parent.resolve():
            raise ValueError('artifact must be directly inside the trace directory')
        if digest(path).lower() != entry['sha256'].lower():
            raise ValueError(f'archived bytes changed: {entry["file"]}')
    # Native diagnostics can mix system text with raw CP1252 game captions.
    # Markers are ASCII; a byte-preserving decode keeps them exact without
    # rejecting a valid trace for an accented or typographic caption byte.
    log = (record_path.parent / record['artifacts']['log']['file']).read_text(encoding='latin-1')
    if 'BOOT READY' not in log or 'BOOT CAPTURE PASS' not in log or re.search(r'BOOT (STOP|ACTION FAIL)', log):
        raise ValueError('native log does not establish a successful trace')
    config = (record_path.parent / record['artifacts']['config']['file']).read_text(encoding='utf-8-sig')
    settings = dict(re.findall(r'^([a-z0-9_]+)=(.*)$', config, re.M))
    if settings.get('dreamfactory_boot_gui_keys', '').strip() or int(settings.get('dreamfactory_boot_dialog_slot', '-1')) >= 0:
        raise ValueError('config can load a native slot outside the checkpoint chain')
    for action in settings.get('dreamfactory_boot_actions', '').split(';'):
        if re.match(r'^(loadcheckpoint|loadslot|rejectcheckpoint|clickon (button|prop) (load|restore)[^\s]*)(\s|$)', action.strip(), re.I):
            raise ValueError('config can load state outside the checkpoint chain')
    restore = re.search(r'^dreamfactory_boot_restore=(.*)$', config, re.M)
    if restore is None or restore.group(1).strip() != record['restorePath']:
        raise ValueError('config does not identify the recorded restore path')
    if bool(record['restorePath']) != ('input' in record['artifacts']):
        raise ValueError('restore must have archived input bytes; fresh boot must not')
    if record['restorePath'] and Path(record['restorePath']).resolve() != (record_path.parent / record['artifacts']['input']['file']).resolve():
        raise ValueError('native restore path does not use the archived input bytes')
    if 'output' in record['artifacts'] and not re.search(r'BOOT ACTION PASS .*?action=savecheckpoint(?: room=|!)', log):
        raise ValueError('output checkpoint was not saved by the native trace')
    record['id'] = str(record_path.resolve())
    return record


def prove(records, target):
    """Find a same-edition, same-executable fresh-boot ancestry for target."""
    target = str(Path(target).resolve())
    by_id = {r['id']: r for r in records}
    if target not in by_id:
        raise ValueError('target is missing or invalid')
    wanted = by_id[target]
    records = [r for r in records if r['edition'] == wanted['edition']
               and r['binarySha256'].lower() == wanted['binarySha256'].lower()]
    checkpoint_paths = {}
    record_paths = {}
    pending = list(records)
    while pending:
        remaining = []
        for record in pending:
            source = record['artifacts'].get('input')
            ancestry = [] if source is None else checkpoint_paths.get(source['sha256'].lower())
            if ancestry is None:
                remaining.append(record)
                continue
            path = ancestry + [record['id']]
            record_paths[record['id']] = path
            output = record['artifacts'].get('output')
            if output:
                checkpoint_paths.setdefault(output['sha256'].lower(), path)
        if len(remaining) == len(pending):
            break
        pending = remaining
    if target not in record_paths:
        raise ValueError('target has no verified checkpoint chain from fresh boot on this executable')
    return record_paths[target]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('history', type=Path)
    parser.add_argument('target', type=Path, help='target trace record.json, including terminal traces with no output save')
    args = parser.parse_args()
    records, rejected = [], []
    for path in args.history.glob('*/record.json'):
        try:
            records.append(validate(path))
        except (ValueError, KeyError, TypeError, OSError) as error:
            rejected.append({'record': str(path), 'reason': str(error)})
    try:
        chain = prove(records, args.target)
    except ValueError as error:
        print(json.dumps({'passed': False, 'error': str(error), 'rejected': rejected}, indent=2))
        return 1
    print(json.dumps({'passed': True, 'scope': 'Recorded native startup-to-target checkpoint continuity only.',
                      'chain': chain, 'rejected': rejected}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
