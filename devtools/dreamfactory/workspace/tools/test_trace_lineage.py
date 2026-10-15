# SPDX-License-Identifier: GPL-3.0-or-later
"""Checkpoint ancestry boundaries; synthetic bytes are not gameplay evidence."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('lineage', Path(__file__).with_name('audit-trace-lineage.py'))
lineage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lineage)


class LineageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.addCleanup(self.tmp.cleanup)

    def record(self, name, source=None, output=None, **changes):
        directory = self.root / name
        directory.mkdir()
        data = dict(schema=1, status='passed', continuityEligible=True,
                    edition='dust', binarySha256='a' * 64, traceToolSha256='b' * 64,
                    restorePath='', artifacts={})
        if source is not None:
            (directory / 'input.dfsv').write_bytes(source)
            data['restorePath'] = str(directory / 'input.dfsv')
            data['artifacts']['input'] = dict(file='input.dfsv', sha256=lineage.digest(directory / 'input.dfsv'))
        log = 'BOOT READY\nBOOT CAPTURE PASS\n'
        if output is not None:
            (directory / 'output.dfsv').write_bytes(output)
            data['artifacts']['output'] = dict(file='output.dfsv', sha256=lineage.digest(directory / 'output.dfsv'))
            log += 'BOOT ACTION PASS index=1 action=savecheckpoint room=town!\n'
        (directory / 'native.log').write_text(log)
        (directory / 'config.ini').write_text('dreamfactory_boot_restore=' + data['restorePath'] + '\n')
        for kind, file in [('log', 'native.log'), ('config', 'config.ini')]:
            data['artifacts'][kind] = dict(file=file, sha256=lineage.digest(directory / file))
        data.update(changes)
        path = directory / 'record.json'
        path.write_text(json.dumps(data))
        return path

    def test_boot_two_checkpoints_and_terminal_without_save(self):
        root = self.record('root', output=b'first')
        child = self.record('child', b'first', b'second')
        terminal = self.record('terminal', b'second')
        records = [lineage.validate(p) for p in [terminal, child, root]]
        self.assertEqual(lineage.prove(records, terminal), [str(p.resolve()) for p in [root, child, terminal]])

    def test_unearned_save_does_not_create_a_root(self):
        target = self.record('unearned', b'legacy', b'new')
        with self.assertRaises(ValueError):
            lineage.prove([lineage.validate(target)], target)

    def test_save_cycle_without_boot_is_unproven(self):
        first = self.record('one', b'b', b'a')
        second = self.record('two', b'a', b'b')
        with self.assertRaises(ValueError):
            lineage.prove([lineage.validate(p) for p in [first, second]], second)

    def test_wrong_edition_and_executable_cannot_bridge(self):
        root = self.record('root', output=b'first')
        for name, changes in [('edition', {'edition': 'full'}), ('binary', {'binarySha256': 'c' * 64})]:
            child = self.record(name, b'first', **changes)
            with self.assertRaises(ValueError):
                lineage.prove([lineage.validate(p) for p in [root, child]], child)

    def test_failed_and_external_load_traces_are_rejected(self):
        for name, changes in [('failed', {'status': 'failed'}), ('load', {'continuityEligible': False})]:
            with self.assertRaises(ValueError):
                lineage.validate(self.record(name, output=b'first', **changes))

    def test_changed_input_bytes_reject(self):
        path = self.record('changed', b'first')
        (path.parent / 'input.dfsv').write_bytes(b'changed')
        with self.assertRaises(ValueError):
            lineage.validate(path)

    def test_config_must_load_the_archived_input_it_hashes(self):
        path = self.record('wrong-file', b'first')
        data = json.loads(path.read_text())
        data['restorePath'] = str(path.parent / 'other.dfsv')
        (path.parent / 'config.ini').write_text('dreamfactory_boot_restore=' + data['restorePath'] + '\n')
        data['artifacts']['config']['sha256'] = lineage.digest(path.parent / 'config.ini')
        path.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            lineage.validate(path)

    def test_stale_output_without_native_save_rejects(self):
        path = self.record('stale', output=b'first')
        data = json.loads(path.read_text())
        (path.parent / 'native.log').write_text('BOOT READY\nBOOT CAPTURE PASS\n')
        data['artifacts']['log']['sha256'] = lineage.digest(path.parent / 'native.log')
        path.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            lineage.validate(path)

    def test_config_load_rejects_even_if_eligibility_flag_is_wrong(self):
        path = self.record('hidden-load', output=b'first')
        data = json.loads(path.read_text())
        with (path.parent / 'config.ini').open('a') as stream:
            stream.write('dreamfactory_boot_actions=  LOADSLOT 90\n')
        data['artifacts']['config']['sha256'] = lineage.digest(path.parent / 'config.ini')
        path.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            lineage.validate(path)

    def test_native_cp1252_caption_keeps_ascii_markers_valid(self):
        path = self.record('caption', output=b'first')
        data = json.loads(path.read_text())
        with (path.parent / 'native.log').open('ab') as stream:
            stream.write(b'BOOT CHOICE text=They\x92re here\xd5\n')
        data['artifacts']['log']['sha256'] = lineage.digest(path.parent / 'native.log')
        path.write_text(json.dumps(data))
        record = lineage.validate(path)
        self.assertEqual(lineage.prove([record], path), [str(path.resolve())])


if __name__ == '__main__':
    unittest.main()
