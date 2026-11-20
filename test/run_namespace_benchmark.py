"""Interleave identical baseline/candidate API benchmarks and estimate paired slowdown.

Example: python3 test/run_namespace_benchmark.py --baseline /namespace/unit/bench-base
  --candidate /namespace/unit/bench-candidate --output build/namespace-verification/performance.json
Run with no compilation or other test jobs active. A confidence bound above 1% is not a pass.
"""
import argparse
import json
import math
import os
import random
import statistics
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--baseline', required=True)
parser.add_argument('--candidate', required=True)
parser.add_argument('--rounds', type=int, default=20)
parser.add_argument('--seconds', type=float, default=1.0)
parser.add_argument('--rows', type=int, default=131072)
parser.add_argument('--domains', type=int, default=64)
parser.add_argument('--workload', default='all')
parser.add_argument('--layout', choices=['clustered', 'interleaved', 'skew', 'cold'], default='clustered')
parser.add_argument('--fanout', type=int, default=1)
parser.add_argument('--topology', choices=['isolated', 'cross'], default='isolated')
parser.add_argument('--iterations', type=int, default=0,
                    help='Optional fixed measured query count per workload instead of a duration.')
parser.add_argument('--preheat-ms', type=float, default=250,
                    help='Read-only CPU preheat before timed query workloads; setup is excluded.')
parser.add_argument('--execution', choices=['prepared', 'query'], default='prepared')
parser.add_argument('--baseline-mode', default='global')
parser.add_argument('--candidate-mode', default='global')
parser.add_argument('--cpus', help='Optional Linux CPU affinity, e.g. 14,15; affects only this benchmark and its children.')
parser.add_argument('--pin-threads', action='store_true',
                    help='Pin the benchmark client and its single query worker to separate allowed CPUs.')
parser.add_argument('--fixture', type=Path,
                    help='Existing private fixture for the controlled startup probe.')
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if args.rounds < 2 or args.rounds % 2:
    parser.error('--rounds must be even and at least 2 for balanced AB/BA blocks')
if args.cpus:
    os.sched_setaffinity(0, {int(cpu) for cpu in args.cpus.split(',')})
if args.pin_threads and len(os.sched_getaffinity(0)) != 2:
    parser.error('--pin-threads requires exactly two allowed CPUs')
randomizer = random.Random(19453)
samples = []
orders = []
for block in range(args.rounds // 2):
    first = randomizer.randrange(2)
    orders.extend((first, 1 - first))
for round_index in range(args.rounds):
    variants = [('baseline', args.baseline, args.baseline_mode),
                ('candidate', args.candidate, args.candidate_mode)]
    if orders[round_index]:
        variants.reverse()
    for variant, executable, mode in variants:
        command = [executable, mode, str(args.rows), str(args.domains),
                                 str(args.seconds), args.workload, args.layout, str(args.fanout),
                                 str(args.iterations), str(args.preheat_ms), args.execution,
                                args.topology, 'pinned' if args.pin_threads else 'automatic']
        if args.fixture:
            command.append(str(args.fixture))
        result = subprocess.run(command,
                                text=True, capture_output=True,
                                check=False)
        if result.returncode:
            raise RuntimeError(f'{variant} benchmark failed ({result.returncode}): {result.stderr}')
        round_samples = []
        for line in result.stdout.splitlines():
            item = json.loads(line)
            assert item['iterations'] > 0 and item['ns_per_iteration'] > 0, item
            assert math.isfinite(item['ns_per_iteration']), item
            if args.pin_threads:
                assert item['thread_placement'] == 'pinned', item
            item.update(variant=variant, round=round_index)
            round_samples.append(item)
        assert round_samples, f'{variant} produced no benchmark samples'
        if args.workload != 'all':
            assert [sample['workload'] for sample in round_samples] == [args.workload]
        samples.extend(round_samples)
        print(f'Completed round {round_index + 1}/{args.rounds}: {variant}', flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({'samples': samples}, indent=2))
summaries = []
for workload in sorted({sample['workload'] for sample in samples}):
    pairs = []
    for round_index in range(args.rounds):
        pair = {sample['variant']: sample['ns_per_iteration'] for sample in samples
                if sample['workload'] == workload and sample['round'] == round_index}
        pairs.append(math.log(pair['candidate'] / pair['baseline']))
    blocks = [statistics.mean(pairs[i:i + 2]) for i in range(0, len(pairs), 2)]
    bootstrap = sorted(statistics.mean(randomizer.choices(blocks, k=len(blocks)))
                       for _ in range(10000))
    mean_ratio = math.exp(statistics.mean(pairs))
    upper_ratio = math.exp(bootstrap[int(len(bootstrap) * .95)])
    instrumented = any(sample.get('allocations_per_iteration') is not None for sample in samples)
    no_use = args.baseline_mode == args.candidate_mode == 'global'
    summaries.append({'workload': workload, 'candidate_over_baseline': mean_ratio,
                      'upper_95_percent_ratio': upper_ratio,
                      'confidence_blocks': len(blocks),
                      'global_neutrality_pass': upper_ratio <= 1.01 and args.rounds >= 20
                      if no_use and not instrumented else None})
    cpu_pairs = []
    for round_index in range(args.rounds):
        pair = {sample['variant']: sample['cpu_ns_per_iteration'] for sample in samples
                if sample['workload'] == workload and sample['round'] == round_index}
        cpu_pairs.append(math.log(pair['candidate'] / pair['baseline']))
    cpu_blocks = [statistics.mean(cpu_pairs[i:i + 2]) for i in range(0, len(cpu_pairs), 2)]
    cpu_bootstrap = sorted(statistics.mean(randomizer.choices(cpu_blocks, k=len(cpu_blocks)))
                           for _ in range(10000))
    summaries[-1].update(cpu_candidate_over_baseline=math.exp(statistics.mean(cpu_pairs)),
                         cpu_upper_95_percent_ratio=math.exp(cpu_bootstrap[int(len(cpu_bootstrap) * .95)]))
report = {'configuration': {key: str(value) if isinstance(value, Path) else value
                            for key, value in vars(args).items()},
          'environment': {'affinity': sorted(os.sched_getaffinity(0)), 'system': list(os.uname())},
          'protocol': 'Balanced randomized AB/BA blocks; bootstrap resamples complete blocks',
          'summaries': summaries, 'samples': samples}
args.output.write_text(json.dumps(report, indent=2))
print(json.dumps(summaries, indent=2))
