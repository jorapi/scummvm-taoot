#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare/run a resumable six-frame, two-model Topaz pilot with a credit cap.

Dry-run by default. --execute requires an explicit credit cap and credentials
from TOPAZ_API_KEY or a hidden stdin prompt. Keys and signed URLs are never
written to artifacts. Paid submissions are not automatically retried.
"""
import argparse
import getpass
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
_validator_spec = importlib.util.spec_from_file_location(
    'enhancement_validator', Path(__file__).with_name('verify-enhancement-images.py'))
_validator = importlib.util.module_from_spec(_validator_spec)
_validator_spec.loader.exec_module(_validator)
png_size = _validator.png_size

ROOT = Path(__file__).resolve().parent.parent
API = 'https://api.topazlabs.com/image/v1'
MODELS = [('High Fidelity V2', 'high-fidelity-v2'), ('CGI', 'cgi')]


def write(path, data):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(data, indent=2) + '\n')
    temporary.replace(path)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class SameHostRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urllib.parse.urlparse(newurl).netloc != 'api.topazlabs.com':
            raise RuntimeError('Refusing an authenticated cross-host redirect')
        return super().redirect_request(req, fp, code, msg, headers, newurl)


class Client:
    def __init__(self, key):
        self.key = key
        self.opener = urllib.request.build_opener(SameHostRedirects())

    def json(self, url, data=None, multipart=None):
        headers = {'X-API-Key': self.key, 'Accept': 'application/json',
                   'User-Agent': 'DreamFactory-Enhancement-Pilot/1.0'}
        if data is not None:
            data = json.dumps(data).encode()
            headers['Content-Type'] = 'application/json'
        if multipart:
            fields, image = multipart
            boundary = 'dreamfactory-' + uuid.uuid4().hex
            pieces = []
            for name, value in fields.items():
                pieces.append(f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"\r\n\r\n{value}\r\n'.encode())
            pieces.append(f'--{boundary}\r\nContent-Disposition: form-data; name="image"; filename="frame.png"\r\nContent-Type: image/png\r\n\r\n'.encode())
            pieces.extend([image.read_bytes(), f'\r\n--{boundary}--\r\n'.encode()])
            data = b''.join(pieces)
            headers['Content-Type'] = 'multipart/form-data; boundary=' + boundary
        try:
            with self.opener.open(urllib.request.Request(url, data=data, headers=headers), timeout=90) as response:
                return json.loads(response.read(2 * 1024 * 1024))
        except urllib.error.HTTPError as error:
            # Restrict diagnostics to short, redacted error fields, never whole responses.
            detail = ''
            try:
                body = json.loads(error.read(4096))
                if isinstance(body, dict):
                    safe = {k: body[k] for k in ('code', 'message', 'detail')
                            if isinstance(body.get(k), (str, int))}
                    detail = ' ' + json.dumps(safe).replace(self.key, '[redacted]')[:300]
            except (ValueError, UnicodeError):
                detail = ' (non-JSON error response)'
            raise RuntimeError(f'Topaz HTTP {error.code} at {urllib.parse.urlparse(url).path}{detail}') from None

    def balance(self):
        result = self.json('https://api.topazlabs.com/account/v1/credits/balance')
        for name in ('available_credits', 'balance', 'credits', 'remaining_credits'):
            value = result.get(name)
            if isinstance(value, (int, float)) and not isinstance(value, bool):
                return value
        shape = {k: type(v).__name__ for k, v in result.items()}
        raise RuntimeError('Unrecognized balance response fields: ' + json.dumps(shape) + '; no paid job submitted')


def prepare():
    visual = ROOT / 'build/enhancements/visual-pilot'
    manifest = json.loads((visual / 'manifest.json').read_text())
    sequence = next(s for s in manifest['sequences'] if s['name'] == 'GSTAIR2/Road58/0')
    frames = sequence['frames'][:6]
    assert len(frames) == 6
    target = ROOT / 'build/enhancements/topaz-pilot'
    target.mkdir(parents=True, exist_ok=True)
    jobs = []
    for model, slug in MODELS:
        directory = target / slug
        (directory / 'original').mkdir(parents=True, exist_ok=True)
        (directory / 'enhanced').mkdir(parents=True, exist_ok=True)
        assets = [manifest['assets'][f['asset']] for f in frames]
        subset = {**manifest, 'assets': assets, 'sequences': [{**sequence,
            'frames': [{**f, 'asset': i} for i, f in enumerate(frames)]}]}
        write(directory / 'manifest.json', subset)
        template = (ROOT / 'scummvm/devtools/dreamfactory/enhancement-compare.html').read_text()
        (directory / 'compare.html').write_text(template.replace('/* MANIFEST */ null', json.dumps(subset).replace('<', '\\u003c')))
        for i, asset in enumerate(assets):
            source = visual / 'original' / asset['file']
            assert png_size(source) == (asset['width'], asset['height'])
            shutil.copy2(source, directory / 'original' / asset['file'])
            jobs.append(dict(id=f'{slug}-{i}', model=model, slug=slug, file=asset['file'],
                inputSHA256=sha(source), input_width=asset['width'], input_height=asset['height'],
                output_width=asset['width'] * 2, output_height=asset['height'] * 2))
    plan = dict(sequence=sequence['name'], jobs=jobs, submissionCount=len(jobs),
                estimatedCreditsFromPublishedPricing=12, authorizationCap=12)
    write(target / 'plan.json', plan)
    return target, jobs


def run(args):
    target, jobs = prepare()
    print('Prepared 12 requests: six consecutive staircase frames, two models, 2x PNG.', flush=True)
    if not args.execute and not args.check_access:
        print('Dry run: no API requests or charges. Plan:', target / 'plan.json')
        return
    if args.execute and (args.max_credits is None or not 0 < args.max_credits <= 12):
        raise RuntimeError('This pilot requires an explicit cap between 1 and 12 credits')
    key = getpass.getpass('Topaz API key (hidden): ') if args.key_stdin else os.environ.get('TOPAZ_API_KEY')
    if not key:
        raise RuntimeError('Set TOPAZ_API_KEY locally or use --key-stdin; no jobs submitted')
    client = Client(key)
    if args.check_access:
        job = jobs[0]
        fields = {k: job[k] for k in ('model', 'input_width', 'input_height', 'output_width', 'output_height')}
        fields.update(category='Enhance', output_format='png', crop_to_fill=False)
        estimate = client.json(API + '/estimate', data=fields)
        print('Image estimate access succeeded. Estimated credits:', estimate.get('credits'), flush=True)
        print('Account credit balance:', client.balance(), flush=True)
        return
    ledger_path = target / 'ledger.json'
    ledger = json.loads(ledger_path.read_text()) if ledger_path.exists() else {'jobs': {}, 'reservedCredits': 0}
    balance = client.balance()
    ledger.setdefault('balanceBefore', balance)
    print('Topaz credit balance:', balance, flush=True)
    estimates = {}
    for job in jobs:
        fields = {k: job[k] for k in ('model', 'input_width', 'input_height', 'output_width', 'output_height')}
        fields.update(category='Enhance', output_format='png', crop_to_fill=False)
        fingerprint = json.dumps(fields, sort_keys=True)
        if fingerprint not in estimates:
            estimate = client.json(API + '/estimate', data=fields)
            cost = estimate.get('credits')
            if not isinstance(cost, int) or cost < 1:
                raise RuntimeError('Unrecognized credit estimate; no new paid jobs submitted')
            estimates[fingerprint] = cost
        job['estimatedCredits'] = estimates[fingerprint]
    new_cost = sum(j['estimatedCredits'] for j in jobs if j['id'] not in ledger['jobs'])
    if ledger['reservedCredits'] + new_cost > args.max_credits or balance <= new_cost:
        raise RuntimeError('Estimated total exceeds cap or would exhaust balance; no new jobs submitted')
    ledger['estimates'] = estimates
    write(ledger_path, ledger)
    for job in jobs:
        image = target / job['slug'] / 'original' / job['file']
        output = target / job['slug'] / 'enhanced' / job['file']
        if sha(image) != job['inputSHA256']:
            raise RuntimeError('Pilot input changed; refusing submission')
        record = ledger['jobs'].get(job['id'])
        if record is not None and any(record.get(k) != job[k] for k in
                ('model', 'inputSHA256', 'output_width', 'output_height')):
            raise RuntimeError('Existing ledger differs from this pilot; refusing submission')
        if record is None:
            record = {**job, 'status': 'Submitting'}
            ledger['reservedCredits'] += job['estimatedCredits']
            ledger['jobs'][job['id']] = record
            write(ledger_path, ledger)  # Reserve before POST; ambiguous submissions cannot be repeated.
            fields = {k: job[k] for k in ('model', 'output_width', 'output_height')}
            fields.update(output_format='png', crop_to_fill='false', sharpen='0.1', denoise='0.05',
                          strength='0.25', faceEnhancement='false')
            if job['model'] == 'CGI':
                fields['deblurStrength'] = '0.1'
            response = client.json(API + '/enhance/async', multipart=(fields, image))
            record.update(processID=response['process_id'], status='Submitted', requestFields=fields)
            write(ledger_path, ledger)
        if record['status'] == 'Downloaded':
            if not output.exists() or sha(output) != record['outputSHA256']:
                raise RuntimeError('Completed output missing or changed; recover download without resubmission')
            continue
        if 'processID' not in record:
            raise RuntimeError('Earlier submission outcome unknown; inspect ledger before any retry')
        deadline = time.monotonic() + 600
        while True:
            state = client.json(API + '/status/' + record['processID'])
            record['status'] = state['status']
            write(ledger_path, ledger)
            print(job['id'], record['status'], flush=True)
            if record['status'] == 'Completed':
                break
            if record['status'] in ('Failed', 'Cancelled') or time.monotonic() > deadline:
                raise RuntimeError('Job failed or timed out; no automatic paid retry')
            time.sleep(3)
        link = client.json(API + '/download/' + record['processID'])['download_url']
        if urllib.parse.urlparse(link).scheme != 'https':
            raise RuntimeError('Refusing non-HTTPS download')
        # Signed CDN downloads deliberately receive no API-key header.
        with urllib.request.urlopen(link, timeout=90) as response:
            image_bytes = response.read(32 * 1024 * 1024 + 1)
        if len(image_bytes) > 32 * 1024 * 1024:
            raise RuntimeError('Pilot output unexpectedly large')
        temporary = output.with_suffix('.part')
        temporary.write_bytes(image_bytes)
        if png_size(temporary) != (job['output_width'], job['output_height']):
            raise RuntimeError('Topaz returned unexpected dimensions')
        temporary.replace(output)
        record.update(status='Downloaded', outputSHA256=sha(output))
        ledger['balanceAfter'] = client.balance()
        ledger['observedCreditDelta'] = ledger['balanceBefore'] - ledger['balanceAfter']
        write(ledger_path, ledger)
        if ledger['observedCreditDelta'] > args.max_credits:
            raise RuntimeError('Observed balance change exceeded cap; no further jobs submitted')
    print('Pilot complete. Reserved credits:', ledger['reservedCredits'],
          'observed balance change:', ledger['observedCreditDelta'], flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--check-access', action='store_true', help='Only query a non-processing estimate and balance')
    parser.add_argument('--max-credits', type=int)
    parser.add_argument('--key-stdin', action='store_true')
    try:
        run(parser.parse_args())
    except Exception as error:
        print(f'Pilot stopped: {type(error).__name__}: {error}', flush=True)
        raise SystemExit(1)
