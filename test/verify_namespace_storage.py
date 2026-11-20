"""Verify legacy compatibility and old-reader rejection using separate engines.

Compile namespace_storage_probe.cpp twice with baseline/candidate includes and
libraries, then pass the two executable paths. All databases are temporary.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--baseline', required=True)
parser.add_argument('--candidate', required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
checks = []


def run(executable, database, mode, query=None, success=True):
    command = [executable, str(database), mode]
    if query is not None:
        command.append(query)
    result = subprocess.run(command, text=True, capture_output=True, timeout=60)
    if success and result.returncode != 0:
        raise AssertionError(result.stderr)
    if not success and result.returncode == 0:
        raise AssertionError(f'Old reader did not reject namespace format: {result}')
    return result.stdout.strip()


def hashes(directory):
    return {str(path.relative_to(directory)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in directory.rglob('*') if path.is_file()}


with tempfile.TemporaryDirectory(prefix='riag_namespace_format_') as temporary:
    directory = Path(temporary)
    legacy = directory / 'legacy'
    run(args.baseline, legacy, 'create-global')
    assert run(args.candidate, legacy, 'query', 'MATCH (n:P) RETURN count(*)') == '1'
    checks.append('Candidate reads legacy database')
    global_database = directory / 'global'
    run(args.candidate, global_database, 'create-global')
    assert run(args.baseline, global_database, 'query', 'MATCH (n:P) RETURN count(*)') == '1'
    assert struct.unpack_from('<Q', global_database.read_bytes(), 4)[0] == 49
    assert global_database.stat().st_size == legacy.stat().st_size
    checks.append('Unused namespace feature preserves legacy format and old-engine readability')
    run(args.candidate, global_database, 'query', 'MATCH (n:P) SET n IN GLOBAL')
    run(args.candidate, global_database, 'query', 'DROP NAMESPACE IF EXISTS absent')
    run(args.candidate, global_database, 'query', 'CREATE NODE TABLE Q(id INT64 PRIMARY KEY)')
    run(args.candidate, global_database, 'query', 'CHECKPOINT')
    assert struct.unpack_from('<Q', global_database.read_bytes(), 4)[0] == 49
    assert run(args.baseline, global_database, 'query', 'MATCH (n:P) RETURN count(*)') == '1'
    checks.append('No-op global move and absent namespace drop preserve the legacy format')
    # Simulate a crash after writing the header extension but before publishing the
    # capability word. There can be no namespace WAL before that capability sync.
    phase_one = directory / 'phase_one'
    shutil.copyfile(global_database, phase_one)
    with phase_one.open('r+b') as output:
        output.seek(60)  # magic + version + four page-range words + UUID
        output.write(bytes([3]))
        output.seek(69)  # format byte + existing data-file page count
        output.write(struct.pack('<Q', 49))
    assert run(args.baseline, phase_one, 'query', 'MATCH (n:P) RETURN count(*)') == '1'
    assert run(args.candidate, phase_one, 'query', 'MATCH (n:P) RETURN count(*)') == '1'
    checks.append('Header-upgrade first phase remains readable by both engines')
    run(args.candidate, legacy, 'promote-crash')
    assert struct.unpack_from('<Q', legacy.read_bytes(), 4)[0] == 50
    before = hashes(directory)
    run(args.baseline, legacy, 'query', 'RETURN 1', success=False)
    assert hashes(directory) == before, 'Rejected old reader modified database/WAL files'
    run(args.baseline, legacy, 'query-lenient', 'RETURN 1', success=False)
    assert hashes(directory) == before, 'Lenient old reader modified database/WAL files'
    checks.append('Old reader rejects namespace WAL before recovery without modifying files')
    assert run(args.candidate, legacy, 'query', 'MATCH (n:L {id:1}) RETURN count(*)') == '2'
    assert run(args.candidate, legacy, 'query',
               'MATCH (a:L IN red)-[r:R IN blue]->(b:L IN blue) RETURN count(*)') == '1'
    checks.append('Committed namespace/local-key/relationship-move WAL recovers after abrupt exit')
    run(args.candidate, legacy, 'query', 'CHECKPOINT')
    before = hashes(directory)
    run(args.baseline, legacy, 'query', 'RETURN 1', success=False)
    assert hashes(directory) == before
    assert run(args.candidate, legacy, 'query', 'MATCH (n:L IN red) RETURN count(*)') == '1'
    checks.append('Checkpointed namespace catalog rejects old readers and reopens correctly')

args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps({'passed': len(checks), 'checks': checks}, indent=2))
print('\n'.join(checks))
