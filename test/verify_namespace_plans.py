"""Compare global physical plans and verify namespace access paths."""
import argparse
import difflib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--baseline', required=True)
parser.add_argument('--candidate', required=True)
parser.add_argument('--output-directory', type=Path, required=True)
args = parser.parse_args()
args.output_directory.mkdir(parents=True, exist_ok=True)
plans = {}
for name, executable, mode in [('baseline-global', args.baseline, 'global'),
                               ('candidate-global', args.candidate, 'global'),
                               ('candidate-local', args.candidate, 'local')]:
    result = subprocess.run([executable, mode], text=True, capture_output=True, check=True)
    plans[name] = result.stdout
    (args.output_directory / (name + '-plans.txt')).write_text(result.stdout)
assert 'NAMESPACE' not in plans['candidate-global']
if plans['baseline-global'] != plans['candidate-global']:
    diff = '\n'.join(difflib.unified_diff(plans['baseline-global'].splitlines(),
                                        plans['candidate-global'].splitlines()))
    (args.output_directory / 'global-plan-diff.txt').write_text(diff)
    raise AssertionError('Global physical plans differ; inspect global-plan-diff.txt')
assert 'namespace IDs' in plans['candidate-local']
assert 'Index: NAMESPACE_HASH' in plans['candidate-local']
checks = ['Global scan, scalar-key lookup, MERGE, and relationship-count plans match baseline exactly',
          'Global plans contain no namespace operator or filter',
          'Named scans use the numeric namespace access path',
          'LOCAL equality lookups use the namespace hash directory']
(args.output_directory / 'plans.json').write_text(json.dumps({'checks': checks}, indent=2))
print('\n'.join(checks))
