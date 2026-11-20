# Namespace verification plan and execution record

This plan implements the acceptance requirements in [namespace.md](../namespace.md).
Only completed commands with captured results count as executed verification.

## Build and baseline

Build baseline and candidate with the same GCC 13.4 builder, Ninja, release preset
and cache options, SIMD configuration, test visibility, and execution-thread settings. Baseline
sources must predate feature changes. Preserve baseline binaries and reports.
The delivered implementation includes Linux function binding in its default link
options; record that code change explicitly. Compare the delivered build against
the frozen original release for acceptance, and retain a separately relinked
baseline with matching binding as an attribution control. Neither baseline is
overwritten by an experiment.

```sh
cmake -S . -B build/namespace-baseline -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_STATIC_RIAG=OFF \
  -DBUILD_SINGLE_FILE_HEADER=OFF -DAUTO_UPDATE_GRAMMAR=OFF -DENABLE_BACKTRACES=OFF
cmake --build build/namespace-baseline --parallel 4 --target e2e_test riag_shell
```

The executed builds use `/namespace/baseline-source`, `/namespace/baseline`,
`/namespace/candidate-source`, and `/namespace/candidate` on the dedicated builder's
Linux filesystem. Regenerate the grammar with `scripts/antlr4/generate_grammar.cmake`
before building the candidate. Build `types_test`, `e2e_test`, and `riag_shell`.
A successful configure or a header-only test is not an engine build.

## Correctness matrix

| Area | Required cases |
| --- | --- |
| Compatibility | Existing global-only queries, legacy files, unchanged IDs, result shapes, and schema exports. |
| Catalog | Namespace create/drop/existence modifiers; identifiers/quoting; global reservation; stable drop/recreate identity; rename keeping identity, members, policies and local keys; finite/all/future table policies; restrictive removal. |
| Selection | Unrestricted default, global-only, finite lists, reset, outer override, inline intersection, unknown names, empty intersections, graph/database switches. |
| Patterns | Labels/types/properties, anonymous patterns, repeated variables, optional null behavior, nested queries, UNION, WHERE and parameter changes. |
| Relationships | Cross-namespace endpoints; third-namespace edge; forward/backward and direct scans; anonymous endpoints. |
| Recursion | Every intermediate node and edge, inline per-hop edge restriction, shortest/weighted shortest, zero-hop alternatives. |
| Keys | Global duplicates outside selection, local duplicate domains, index probes, multi-domain results, SERIAL, supported index kinds. |
| Writes | Per-element targets/defaults, CREATE/MERGE assertions, ambiguity errors, property updates, moves, collisions, stable edge endpoints. |
| Deletion | Ordinary incident-edge refusal, hidden-edge preflight, inline/WHERE equivalence, atomic multi-target detach. |
| Persistence | Explicit commit/rollback, reconnect/checkpoint/reopen, crash recovery, first promotion, moves, local indexes, format upgrade, old-reader rejection. |
| Concurrency | Drop/remove versus writes, old reader snapshots during promotion/moves, cached plans across all relevant catalog/selection changes. |
| Integration | COPY/CTAS/export/import/backup; attached catalogs; explicit errors on unsupported partitions/storage/GDS/extension combinations. |

Use the existing end-to-end `.test` harness for observable behavior and C++ tests
where snapshot control, crash injection, or plan inspection is necessary. Execute
the worked example as regression coverage. Run the existing applicable suite and
compare failures with the recorded baseline.

## Optimization and performance

1. Inspect EXPLAIN/PROFILE for unrestricted and implicitly global queries: no
   namespace filter, wider key, extra membership projection, or disabled plan cache.
2. Inspect restricted scans/probes and traversal: bind IDs once, prune namespace
   groups, use direct singleton local-key indexes, and filter before shortest-path
   selection. No row-wise namespace string comparisons.
3. Compare baseline/candidate global-only scans, filters/aggregates, point lookups,
   MERGE, one-hop and recursive traversal, property writes, CREATE/DELETE/DETACH,
   COPY, startup, checkpoint, and recovery.
4. Measure named-namespace workloads with uniform/skewed/mixed memberships, many
   unrelated namespaces, duplicate local keys, and high-degree cross-namespace edges.
   Compare equivalent results and work; report selectivity and topology.
5. Record warm/cold runs, interleave baseline/candidate measurements, and report
   distributions and confidence bounds. Separate preparation from cached execution.
   Record memory, index/data/WAL size, and available instruction/allocation metrics.
6. Accept global-only neutrality only with unchanged hot-path work and storage and
   no repeatable regression; apply the concept's upper 95% slowdown bound of 1%.
   Insufficient precision is inconclusive, never a passing result.

## Execution record

Verification completed on 29 October 2026 (Berlin), in the dedicated Linux Docker
builder. The delivered Linux shared build includes local binding of internal
function definitions (`-Bsymbolic-functions`); its public exports and data/RTTI
remain available. Core-function interposition changes as described in the concept.
This build optimization is part of the measured implementation, not an undisclosed
difference in benchmark settings. Original binaries remain frozen; separately
relinked baseline binaries provide matching-linkage attribution controls.

| Check | Result / evidence |
| --- | --- |
| Release build | GCC 13.4, Ninja, release preset, AVX2, test visibility and one query worker. Baseline and candidate use native Linux source/build snapshots in the dedicated container. |
| Common and namespace tests | 153/153 passed, including 68 namespace cases (1.392 s). Covers scope intersections, local/global keys, moves, hidden-edge deletion guards, snapshots, cached-plan rebinding, concurrency, group-history reclamation, dense masks and offsets beyond 32 bits. `types-tests.{xml,log}`. |
| Full existing regression suite | 2,051 executed; 2,047 passed; the same 4 environment failures as the frozen baseline; 258 disabled (758.449 s). No additional failures. `candidate-e2e-bound-final.{xml,log}`; failure names/counts compared automatically with `baseline-e2e.xml`. |
| Baseline environment failures | One missing Parquet fixture (`AnonymousParquetDeleteReload`) and three extension installation/loading tests whose remote endpoints return HTTP 404. These failures are retained, not reported as passes. |
| Persistence and old readers | Seven compatibility/recovery checks passed: legacy reads, unchanged unused/no-op GLOBAL format, two-phase upgrade, strict and lenient old-reader rejection without file changes, abrupt-exit WAL replay, and checkpoint reopen. `storage-compatibility.json`. Namespace tests also cover LOCAL COPY, CTAS, relocated cross-namespace CSV/Parquet export/import, graph switching and multiple-writer conflicts. |
| Plans and hot layouts | Four global scan/key/MERGE/relationship-count plans match baseline exactly, with no namespace operator/filter. Named scans use numeric membership masks and LOCAL probes use NAMESPACE_HASH. All 16 checked legacy hot member offsets match baseline. `plans.json`, saved plan text and `hot-layouts.json`. |
| Executable concept | Section 11 executes verbatim and passes all five final-state checks. `worked-example.json`. |
| Benchmark correctness | 104 exact-result fixtures passed across global, empty, occupied, named, mixed, LOCAL, clustered/interleaved/skewed layouts, ordinary/prepared execution, writes, independent edge ownership and cross-namespace recursion. Includes four degree-4 recursive fixtures. `benchmark-fixtures.json`. |
| Shared-library integration | A separately loaded module and its host pass shared query/write state, Value ownership, callback RTTI/dynamic_cast and engine exceptions in both directions, for both snapshots. `shared-library-abi.json`. This verifies consumers compiled against the corresponding snapshot, not every prebuilt third-party extension. |
| Generation and repository checks | Forced isolated ANTLR generation reproduces all seven artifacts byte for byte, including shell keywords. TypeSpec WAL code matches generated output apart from an existing comment-wrap difference. Clang-format-18, fork rename guard, Python syntax and diff whitespace checks pass; inherited generated ANTLR whitespace is excluded from the latter. |
| Isolation | Only `riagraph-namespaces-work` and its own processes/files were used. No published ports, host installations, global cache eviction, kernel/security changes or modifications to the parallel clone. Hardware counters were unavailable; instrumented profiles are separate from latency evidence. |

### Delivered global-only release comparison

Each cell is **geometric mean latency ratio / one-sided upper 95% ratio**.
A ratio below 1 is faster. Every acceptance entry below meets the upper bound
of 1.01; the machine-readable selection and source reports are recorded in
`build/namespace-verification/release-acceptance.json`.

| Workload | Prepared | Ordinary query |
| --- | ---: | ---: |
| scan | 0.90823 / 0.91324 | 0.89429 / 0.90819 |
| point | 0.88882 / 0.89480 | 0.84636 / 0.85890 |
| merge | 0.90514 / 1.00786 | 0.95381 / 0.99151 |
| update | 0.92491 / 0.92952 | 0.90609 / 0.91733 |
| traverse | 0.95287 / 0.96165 | 0.92371 / 0.95384 |
| recursive | 0.86420 / 0.89599 | 0.85178 / 0.86391 |
| create | 0.89118 / 0.90946 | 0.87663 / 0.89017 |
| churn | 0.94458 / 0.96640 | 0.91620 / 0.93444 |
| copy | 0.97019 / 0.98079 | 0.98078 / 0.99239 |
| detach | 0.92527 / 0.92989 | 0.92716 / 0.95189 |

The broad prepared/ordinary runs use 20 balanced rounds, 0.5-second samples,
16,384 rows and 16 domains. The client and its single query worker are pinned
to CPUs 14 and 15 respectively. COPY uses the prespecified precision runs:
80 rounds, 32 measured batches of 4,096 rows after 32 warm-up batches. Both
engines therefore start and finish with identical row counts; all sample
checksums were checked. Their upper bounds are 0.98079 (prepared) and 0.99239
(ordinary). The original duration-based COPY runs remain diagnostic reports:
prepared 0.99674 / 1.02856, ordinary 1.04648 / 1.06834. They are not called passes.

| Lifecycle case | Warm | Cold-advised |
| --- | ---: | ---: |
| startup | 0.98975 / 1.00303 | 0.99098 / 0.99921 |
| checkpoint | 0.97665 / 0.99483 | 0.96892 / 0.98828 |
| recovery | 0.91892 / 0.92942 | 0.90850 / 0.92856 |

Warm/cold lifecycle runs use 40 balanced rounds. Cold startup uses the controlled
same-fixture probe: 40 rounds / 0.5 seconds on CPU 14, up to 4,096 measured opens
per sample after eight warm-ups. Both engines open the exact same private legacy
database; only that file receives fsync/cache advice. Its SHA-256 is unchanged
after all opens (`startup-controlled-fixture.json`). The earlier copied-fixture
cold-startup run, 1.02231 / 1.07068, remains inconclusive and is retained.
Global degree-4 recursion, 512 rows / 16 domains, also passes: 0.93715 / 0.96015.

The legacy data file is 2,142,208 bytes and pending WAL is 208,971 bytes in both
engines. Implicit global membership adds no per-node/edge/index storage, namespace
pages or wider IDs. The unchanged global scalar-key index and native read loops
are retained. Optional per-table/session/query metadata has bounded costs.

### Scoped performance and attribution

| LOCAL workload vs equivalent property/global-key control | Mean / upper 95% |
| --- | ---: |
| churn | 0.91095 / 0.91621 |
| copy | 0.66164 / 0.67170 |
| create | 0.85664 / 0.86415 |
| detach | 0.91490 / 0.92761 |
| merge | 0.59818 / 0.63260 |
| point | 0.86592 / 0.87547 |
| recursive | 0.82621 / 0.84184 |
| scan | 0.69592 / 0.70143 |
| traverse | 0.96498 / 0.97961 |
| update | 0.90475 / 0.91873 |

| Additional comparison | Mean / upper 95% |
| --- | ---: |
| Skewed scan: 131,072 rows, 64 namespaces, half in the selected namespace | 0.81630 / 0.82149 |
| Independent edge/endpoint namespaces, cross edges, degree 16 | 0.95073 / 0.96262 |
| Dense scoped recursion, degree 4, 512 rows / 16 namespaces | 1.06219 / 1.08366 |

The dense scoped recursive case is **6.2% slower** than its property-filter control
(upper ratio 1.08366). Its memberships share a small number of groups and do not
offer the same pruning benefit as larger clustered data. This cost is retained
as a limitation, not hidden by the faster selective cases. Namespace-aware use
is not claimed universally cost-free. LOCAL also opts into native index domains:
the 16-domain fixture had approximately 113 MiB process RSS versus 58 MiB for
the property/global-key control. This is an opt-in index-capacity cost, not
per-element overhead on unused/global-key tables.

Empty namespace definitions and global tables alongside an unrelated named table
were measured separately (`performance-bound-empty.json`, `...-occupied.json`).
All their workload upper bounds are below 1.01. The earlier complete named/mixed
matrix and larger clustered/interleaved scan reports remain available as staged
evidence; they are not substituted for the final build results above.

Matching-linkage controls (`performance-bound-matched-{prepared,query}.json`)
isolate namespace-related costs from ELF binding. Several statement-level means
are about 1-2% slower and their bounds exceed 1%; these controls do **not** prove
that namespace code alone is free. The release acceptance above applies to the
delivered Linux shared build including its explicit linkage optimization.
Static libraries, Windows and other configurations have not been timed here.

Instruction profiles corroborate the absence of membership work on unused paths.
With matching binding, two global recursive queries execute 204,909,701 baseline
instructions versus 204,935,389 candidate instructions (+0.0125%); simulated
mispredicts are 702,388 versus 702,251 (`profile-bound-recursive/`). Ordinary
point-query parsing dropped from 176,674,963 to 172,060,968 instructions after
holding cached ANTLR token sets by const reference. Ordinary-query allocation
counts fell in all ten workloads; point queries saved 91 allocations and 19,184
bytes per execution in the fixed instrumented sample (`allocations-final-query.json`).
Prepared global counts were equal before linking, with bounded extra transient
storage of 72 bytes for a point query. Simulated events and instrumented latency
are not hardware-counter or timing acceptance evidence.

### Excluded and baseline-limited evidence

Initial write controls omitted tenant payloads, and an intermediate graph control
omitted independent relationship membership. Their reports are archived under
`untagged-property-write-control/` and `node-only-property-control/` and excluded.
Corrected controls store tenant payloads on writes, include edge predicates at
each recursive hop and enforce both endpoints. Interrupted placement/query
diagnostics that briefly overlapped during rescheduling are also excluded.
All accepted timing jobs ran without compilation, tests or profiling in parallel.

Persistent `DROP GRAPH` can fail an existing file-removal allowlist check in the
unchanged baseline too. Namespace checkpoint/reopen and in-memory graph deletion
are covered independently. The three unavailable extension tests remain limitations.

## Reproduction

The checked-in API runner compiles identical sources against both existing builds:

```sh
python3 test/run_namespace_checks.py \
  --baseline-source /namespace/baseline-source --baseline-build /namespace/baseline \
  --candidate-source /namespace/candidate-source --candidate-build /namespace/candidate \
  --output /workspace/build/namespace-verification --binary-directory /namespace/unit
```

Run the common tests and the existing suite separately. The latter requires its
fixtures under the source tree; preserve the baseline XML to distinguish existing
environment failures from new failures:

```sh
/namespace/candidate/test/common/types_test \
  --gtest_output=xml:/workspace/build/namespace-verification/types-tests.xml
E2E_TEST_FILES_DIRECTORY=/namespace/candidate-source/test/test_files \
  /namespace/candidate/test/runner/e2e_test --gtest_filter='*' \
  --gtest_output=xml:/workspace/build/namespace-verification/candidate-e2e.xml
```

With compilation/tests stopped, interleave performance samples:

```sh
python3 test/run_namespace_benchmark.py \
  --baseline /namespace/unit/bench-baseline --candidate /namespace/unit/bench-candidate \
  --rounds 20 --seconds 0.5 --rows 16384 --domains 16 --cpus 14,15 --pin-threads \
  --output build/namespace-verification/performance-global.json
```

Repeat with `--execution query` for ordinary execution. To reproduce the equal-work
COPY check, use `--workload copy --iterations 32 --rounds 80`; each measured batch
contains 4,096 rows and the same 32 warm-up batches precede measurement.

`startup-baseline`/`startup-candidate` are the controlled startup probes compiled
by the API runner. Create a private, checkpointed legacy database with 2,048 global
`P(id INT64 PRIMARY KEY, x INT64)` rows using the baseline. Then run:

```sh
python3 test/run_namespace_benchmark.py \
  --baseline /namespace/unit/startup-baseline --candidate /namespace/unit/startup-candidate \
  --workload startup --fixture /path/to/private/database --layout cold \
  --rows 2048 --domains 16 --rounds 40 --seconds 0.5 --cpus 14 \
  --output build/namespace-verification/performance-startup-controlled.json
```

Hash the fixture before and after the entire comparison. Cache advice must target
only this private file; remove it after verification. The recorded controlled run
kept its SHA-256 unchanged. No global cache controls are needed.

Use `lifecycle-baseline`/`lifecycle-candidate` for startup, checkpoint, and recovery
(`--workload all`, with optional `--layout cold`). Cold runs fsync and advise eviction
of each private fixture only; they never clear system caches. Namespace comparisons
use baseline mode `property` and candidate mode `named`, `local`, or `mixed`, with
`--layout clustered`, `interleaved`, or `skew`; `--fanout` varies degree. Mode `empty`
creates namespace definitions while retaining global data. Allocation runs set
`LD_PRELOAD=/namespace/unit/allocations.so`; their reports cannot pass the latency gate.

An additional graph integration probe reproduced an existing baseline limitation:
persistent `DROP GRAPH` can fail the file-removal allowlist check. The same failure
occurs without namespaces in the frozen baseline. Namespace graph checkpoint/reopen
and in-memory selected-graph deletion are covered independently.
