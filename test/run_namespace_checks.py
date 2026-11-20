"""Build shared-source API probes and run namespace compatibility/plan/example checks.

Requires existing baseline and candidate release builds. This script does not
replace the common tests, full regression suite, or performance acceptance runs.
All compilation/execution uses explicit argument lists, never a shell.
"""
import argparse
import json
import shlex
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--baseline-source', type=Path, required=True)
parser.add_argument('--baseline-build', type=Path, required=True)
parser.add_argument('--candidate-source', type=Path, required=True)
parser.add_argument('--candidate-build', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--binary-directory', type=Path)
parser.add_argument('--cxx', default='g++')
parser.add_argument('--build-only', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
binary_directory = args.binary_directory or args.output / 'bin'
binary_directory.mkdir(parents=True, exist_ok=True)
args.output.mkdir(parents=True, exist_ok=True)


def run(*command):
    subprocess.run([str(part) for part in command], check=True)


for variant, source, build in (
        ('baseline', args.baseline_source, args.baseline_build),
        ('candidate', args.candidate_source, args.candidate_build)):
    for probe, filename in (('storage', 'namespace_storage_probe.cpp'),
                             ('plan', 'namespace_plan_probe.cpp'),
                             ('bench', 'namespace_benchmark.cpp'),
                             ('lifecycle', 'namespace_lifecycle_benchmark.cpp'),
                             ('startup', 'namespace_startup_benchmark.cpp')):
        library = build.resolve() / 'src'
        run(args.cxx, '-std=c++20', '-O3', '-I' + str(source / 'src/include'),
            '-I' + str(build / 'src/include'), root / 'test' / filename,
            '-L' + str(library), '-lriag', '-Wl,-rpath,' + str(library),
            '-o', binary_directory / (probe + '-' + variant))
        print('Built ' + probe + '-' + variant, flush=True)
run(args.cxx, '-std=c++20', '-O3', '-shared', '-fPIC',
    root / 'test/namespace_allocation_probe.cpp', '-o', binary_directory / 'allocations.so')
layouts = {}
linkage = {}
for variant, source, build in (
        ('baseline', args.baseline_source, args.baseline_build),
        ('candidate', args.candidate_source, args.candidate_build)):
    library = build.resolve() / 'src'
    common_flags = [args.cxx, '-std=c++20', '-O3', '-I' + str(source / 'src/include'),
                    '-I' + str(build / 'src/include')]
    dependencies = ['-L' + str(library), '-lriag', '-Wl,-rpath,' + str(library)]
    plugin = binary_directory / ('linkage-plugin-' + variant + '.so')
    host = binary_directory / ('linkage-host-' + variant)
    run(*common_flags, '-DNAMESPACE_LINKAGE_PLUGIN', '-shared', '-fPIC',
        root / 'test/namespace_linkage_probe.cpp', *dependencies, '-o', plugin)
    run(*common_flags, root / 'test/namespace_linkage_probe.cpp', *dependencies,
        '-ldl', '-o', host)
    if not args.build_only:
        linkage[variant] = subprocess.check_output([str(host), str(plugin)], text=True).strip()
if linkage:
    (args.output / 'shared-library-abi.json').write_text(json.dumps(linkage, indent=2))
    print('Baseline and candidate shared-library API/callback/RTTI/exception checks passed.', flush=True)
for variant, source, build in (
        ('baseline', args.baseline_source, args.baseline_build),
        ('candidate', args.candidate_source, args.candidate_build)):
    binary = binary_directory / ('layout-' + variant)
    commands = json.loads((build / 'compile_commands.json').read_text())
    command = next(item for item in commands if item['file'].endswith('/storage/table/table.cpp'))
    tokens = iter(command.get('arguments') or shlex.split(command['command']))
    includes = []
    for token in tokens:
        if token.startswith(('-I', '-D')):
            includes.append(token)
        elif token == '-isystem':
            includes.extend((token, next(tokens)))
    run(args.cxx, '-std=c++20', '-O2', '-fno-access-control', '-Wno-invalid-offsetof',
        *includes,
        root / 'test/namespace_layout_probe.cpp', '-o', binary)
    layouts[variant] = subprocess.check_output([str(binary)], text=True)
if layouts['baseline'] != layouts['candidate']:
    raise AssertionError('Legacy hot member offsets differ:\n' + json.dumps(layouts, indent=2))
(args.output / 'hot-layouts.json').write_text(json.dumps(layouts, indent=2))
print('All 16 legacy hot member offsets match baseline.', flush=True)
if args.build_only:
    raise SystemExit(0)
run(sys.executable, root / 'test/verify_namespace_storage.py',
    '--baseline', binary_directory / 'storage-baseline',
    '--candidate', binary_directory / 'storage-candidate',
    '--output', args.output / 'storage-compatibility.json')
run(sys.executable, root / 'test/verify_namespace_plans.py',
    '--baseline', binary_directory / 'plan-baseline',
    '--candidate', binary_directory / 'plan-candidate', '--output-directory', args.output)
run(sys.executable, root / 'test/verify_namespace_example.py',
    '--probe', binary_directory / 'storage-candidate', '--concept', root / 'namespace.md',
    '--output', args.output / 'worked-example.json')
run(sys.executable, root / 'test/verify_namespace_benchmarks.py',
    '--baseline', binary_directory / 'bench-baseline',
    '--candidate', binary_directory / 'bench-candidate',
    '--output', args.output / 'benchmark-fixtures.json')
