"""Check benchmark result equivalence before collecting timing evidence."""
import argparse
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--baseline', required=True)
parser.add_argument('--candidate', required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
checks = []
rows, domains = 1024, 4
for executable, mode in [(args.baseline, 'global'), (args.baseline, 'property'),
                         (args.candidate, 'global'), (args.candidate, 'empty'),
                         (args.candidate, 'occupied'),
                         (args.candidate, 'named'), (args.candidate, 'mixed'),
                         (args.candidate, 'local')]:
    for layout in ('clustered', 'interleaved', 'skew'):
        selected = rows if mode in ('global', 'empty', 'occupied') else rows // (2 if layout == 'skew' else domains)
        for workload in ('scan', 'point'):
            result = subprocess.run([executable, mode, str(rows), str(domains), '0.01', workload,
                                     layout, '1', '0', '0'], text=True, capture_output=True, check=False)
            assert result.returncode == 0, result.stderr
            sample = json.loads(result.stdout)
            count = sample['iterations'] + 32
            expected = (sum(i % 97 for i in range(selected)) * count if workload == 'scan'
                        else sum((i % selected) % 97 for i in range(count)))
            assert sample['checksum'] == expected, (mode, layout, workload, sample, expected)
            checks.append({'mode': mode, 'layout': layout, 'workload': workload,
                           'correct': True})
    for workload in ('traverse', 'recursive'):
        result = subprocess.run([executable, mode, str(rows), str(domains), '0.01', workload,
                                 'clustered', '2', '0', '0'], text=True, capture_output=True, check=False)
        assert result.returncode == 0, result.stderr
        sample = json.loads(result.stdout)
        count = sample['iterations'] + 32
        if workload == 'recursive':
            expected = 30 * count  # Two choices at each hop, lengths 1..4.
        else:
            selected = rows if mode in ('global', 'empty', 'occupied') else rows // domains
            block = rows // domains
            expected = 0
            for iteration in range(count):
                source = iteration % selected
                start = source // block * block
                expected += sum((start + (source - start + step) % block) % 97
                                for step in (1, 2))
        assert sample['checksum'] == expected, (mode, workload, sample, expected)
        checks.append({'mode': mode, 'layout': 'clustered', 'workload': workload,
                       'fanout': 2, 'correct': True})
args.output.parent.mkdir(parents=True, exist_ok=True)
for executable, mode in [(args.baseline, 'global'), (args.candidate, 'global'),
                         (args.candidate, 'empty'), (args.candidate, 'occupied'),
                         (args.candidate, 'named'), (args.candidate, 'local')]:
    for workload in ('scan', 'point'):
        result = subprocess.run([executable, mode, str(rows), str(domains), '0.01', workload,
                                 'clustered', '1', '100', '0', 'query'],
                                text=True, capture_output=True, check=False)
        assert result.returncode == 0, result.stderr
        sample = json.loads(result.stdout)
        selected = rows if mode in ('global', 'empty', 'occupied') else rows // domains
        expected = (sum(i % 97 for i in range(selected)) * 132 if workload == 'scan'
                    else sum(i % 97 for i in range(132)))
        assert sample['checksum'] == expected, (mode, workload, sample, expected)
        assert sample['execution'] == 'query', sample
        checks.append({'mode': mode, 'workload': workload, 'execution': 'query', 'correct': True})
for executable, mode in ((args.baseline, 'property'), (args.candidate, 'named')):
    result = subprocess.run([executable, mode, str(rows), str(domains), '0.01', 'traverse',
                             'clustered', '8', '100', '0', 'prepared', 'cross'],
                            text=True, capture_output=True, check=False)
    assert result.returncode == 0, result.stderr
    sample = json.loads(result.stdout)
    block = rows // domains
    expected = sum(sum(((iteration + step) % block) % 97 for step in (1, 5))
                   for iteration in range(132))
    assert sample['checksum'] == expected, (mode, sample, expected)
    checks.append({'mode': mode, 'workload': 'traverse', 'fanout': 8,
                   'topology': 'cross', 'correct': True})
    result = subprocess.run([executable, mode, str(rows), str(domains), '0.01', 'recursive',
                             'clustered', '8', '100', '0', 'prepared', 'cross'],
                            text=True, capture_output=True, check=False)
    assert result.returncode == 0, result.stderr
    sample = json.loads(result.stdout)
    assert sample['checksum'] == 30 * 132, (mode, sample)
    checks.append({'mode': mode, 'workload': 'recursive', 'fanout': 8,
                   'topology': 'cross', 'correct': True})
for executable, mode in ((args.baseline, 'global'), (args.baseline, 'property'),
                         (args.candidate, 'named'), (args.candidate, 'local'),
                         (args.candidate, 'mixed')):
    for workload, iterations in (('merge', 100), ('create', 5), ('churn', 5), ('copy', 2)):
        result = subprocess.run([executable, mode, str(rows), str(domains), '0.01', workload,
                                 'clustered', '1', str(iterations), '0'],
                                text=True, capture_output=True, check=False)
        assert result.returncode == 0, (mode, workload, result.stderr)
        sample = json.loads(result.stdout)
        count = iterations + 32
        expected = (sum(i % 97 for i in range(count)) if workload == 'merge'
                    else count * (count - 1) // 2 if workload == 'create'
                    else count if workload == 'churn' else count * 4096)
        assert sample['checksum'] == expected, (mode, workload, sample, expected)
        assert sample['tenant_write_control'] == (mode == 'property'), sample
        assert sample['primary_key_scope'] == ('LOCAL' if mode == 'local' else 'GLOBAL'), sample
        checks.append({'mode': mode, 'workload': workload, 'correct': True})
for executable, mode in ((args.baseline, 'global'), (args.baseline, 'property'),
                         (args.candidate, 'global'), (args.candidate, 'named')):
    result = subprocess.run([executable, mode, '512', '16', '0.01', 'recursive',
                             'clustered', '4', '2', '0'],
                            text=True, capture_output=True, check=False)
    assert result.returncode == 0, (mode, result.stderr)
    sample = json.loads(result.stdout)
    # Four choices globally; every fourth edge belongs to another namespace.
    choices = 4 if mode == 'global' else 3
    expected = sum(choices ** depth for depth in range(1, 5)) * 34
    assert sample['checksum'] == expected, (mode, sample, expected)
    checks.append({'mode': mode, 'workload': 'recursive', 'fanout': 4, 'correct': True})
args.output.write_text(json.dumps({'passed': len(checks), 'checks': checks}, indent=2))
print(f'{len(checks)} read/write benchmark fixtures return the expected results.')
