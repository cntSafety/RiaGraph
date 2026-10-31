# Upstream integration plan — ladybug 0.20.2 → 0.21.2 (October 2026)

**Status:** Waves 1-5 applied and verified on `upstream/integration-2026-10` (2026-10-04), reviewed and finalized 2026-10-05; see §11 for the outcome. **Date:** 2026-10-03.
**Range reviewed:** `191b90ca3` (fork point, ladybug 0.20.2) .. `c473940c7` (ladybug `main`, 0.21.2) —
151 core commits, plus the binding and extension submodule commits those bumps carry.
**Scope decisions:** bug fixes, hardening and generic performance first. The LDBC/LSQB planner
series is deferred to its own final wave (Wave 5). No upstream *features* are taken.

Every pick follows [upstream-picks.md](upstream-picks.md) and gets a row in its log.

---

## 1. Summary

| | commits |
|---|---|
| Selected core commits, Waves 1–4 | 86 |
| Selected core commits, Wave 5 (deferred planner series) | 17 |
| Selected submodule commits (extension, python, nodejs, rust) | 11 |
| Not selected (core) | 48 — see §8 |

**Dry run.** All 103 selected core commits were applied in the order below onto `origin/main`
(`ed3cca737`), in a throwaway worktree, through `riag-rename.py --patch` + `git apply -3`.
**92 apply cleanly. 11 have rejected hunks, and none of the 11 is a real semantic conflict.**
They fall into three groups (details in the task tables):

- `test/storage/CMakeLists.txt` / `test/common/CMakeLists.txt` registration lines. Our tree registers
  `rel_checkpoint_test.cpp` and `database_magic_test`, so upstream's context lines no longer match.
  Add the new test line by hand.
- Appends to `test/test_files/cast/cast_error.test` (ours grew with the type-parser fixes) and one
  include line in `src/main/prepared_statement.cpp`.
- CI YAML and `LBUG_DUMP_LOGICAL` dumper hunks, which we drop (see the per-task notes).

The dry run was textual only and was not compiled. Each task still needs a build and the test suite.

**Our own post-fork fixes are kept, and upstream has not fixed the same problems differently:**

| Our fix | Upstream overlap | Verdict |
|---|---|---|
| `a7ffdc72a`, `754859861` type-parser bracket segfault / recursion bound | none | keep ours; `fd94e9471` only conflicts on `cast_error.test` appends — keep both blocks |
| `4ba39f3a9` finalize rel checkpoint when no CSR region needs writing (#16, upstream #611) | `5f5c168fd` wraps `CSRNodeGroup::checkpoint` in a snapshot/rollback `try` | compatible, applies cleanly; ours now runs inside upstream's `try`. Keep ours. Run both `rel_checkpoint_test` (ours) and `CSRGroupReadsBackAfterFailedCSRHeaderCheckpoint` (upstream) |
| `3616e405c` bounds-check group index (`ASSERT` in `group_collection.h`) | none | keep ours |

**Open verification findings that upstream fixes** (see `FINDINGS_SUMMARY.md` in the verification repo):

| Finding | Upstream fix | Task |
|---|---|---|
| 4 — shift by 64 in aggregate partitioning | `6cd21a2b1` (+ `fafc36265` simple-aggregate path) | T09 |
| 5 — `ALTER TABLE ADD` null `ResultSet` reference | `55a45e520` | T07 |
| 3 — factorized-table misalignment | partial only (`2092c4284`, `55a45e520`, `fafc36265`, `6cd21a2b1` memcpy fixes). Upstream tried padding, reverted it (`3940cb137`), then turned UBSan alignment checks off in CI (`2d69bd3d7`) | T07/T09 — keep our suppression |
| 1, 2, 6–11 | not fixed upstream (6 is fixed by our own `754859861`) | — |

---

## 2. Procedure

### 2.1 Setup (once)

`upstream` was added to the `RiaGraph` clone during this review, as §Procedure of
`upstream-picks.md` prescribes (fetch-only, push `DISABLED`). Other clones need the same.

**Fix `scripts/riag-rename.py --patch` on Windows first (Task T00).** As written, it fails on Windows
in two ways:

1. `sys.stdout.write` in text mode turns every LF into CRLF, so **no patch applies**.
2. It encodes stdout as cp1252, so any patch with non-ASCII content crashes with `UnicodeEncodeError`
   (e.g. `1bd9b9959`).

Fix: in `run_patch`, write bytes:
`sys.stdout.buffer.write("".join(out).encode("utf-8", "surrogateescape"))`. Until that lands, use
`PYTHONUTF8=1` and pipe through `tr -d '\r'`. (The conda env `riacoredev` works as the interpreter.)

### 2.2 Per task

```bash
git switch -c upstream/<task-id> origin/main
for sha in <task commits, in listed order>; do
  git format-patch -1 --stdout $sha -- . ':(exclude)extension' ':(exclude)tools' \
      ':(exclude)benchmark' ':(exclude)dataset' > /tmp/up.patch
  python scripts/riag-rename.py --patch /tmp/up.patch > /tmp/riag.patch
  git am -3 /tmp/riag.patch    # on failure: git apply --reject /tmp/riag.patch, fix *.rej, git add, git am --continue
done
python scripts/riag-rename.py --check
```

- `git am -3` rarely helps here: the patch's base blobs are ladybug-named, so the 3-way fallback has
  no matching base. Expect plain reject semantics and resolve `.rej` files by hand.
- **Provenance URLs.** These picks add `github.com/LadybugDB/...` links that the rename rewrites into
  dead links. Restore each one and add `// riag-rename:skip upstream provenance`:
  `0b4f086ad` (1), `2c51da1e1` (1), `2f956e750` (1), `ea9ef103c` (6), `d64ba0e86` (2), `2f892aced` (2).
- Keep one RiaGraph commit per upstream commit (`git am` does that), so the log row maps 1:1. Keep
  upstream authorship and add `(cherry picked from ladybug <sha>)` to the message.
- One PR per task. Per AGENTS.md: unpatch the fuzz overlay (`fuzz.ps1 unpatch`) before committing in
  `RiaGraph/`, and ask before pushing.

### 2.3 Verification per task

1. `verify.ps1 build`, then the RiaGraph test suite in the Docker build env (`9fc2cfd8d`), including
   each pick's new regression tests.
2. Per wave: `verify.ps1 run differential -- smoke`, a short `cyphuzz` run, and a `fuzz` run on the
   ASan+UBSan build. The `validation-engineer` agent can run these; storage waves (1) and planner
   waves (5) need the longer campaigns.

---

## 3. Wave 1 — storage integrity and durability (highest priority)

### T01 — WAL record integrity
| SHA | What | Notes |
|---|---|---|
| `0b4f086ad` | WAL checksum buffer resize dropped buffered bytes: **any WAL record > ~4 KB was corrupted** and failed replay (upstream #771) | Critical. Clean. |
| `fe34d7865` | Windows: file lock moved from byte 0 to a byte near `INT64_MAX` (same-process RO handle could not read); PackedFilteredCount deterministic grouping | Clean. Its WalTest needs `0b4f086ad`. The lock byte changes, so an old and a new build on Windows do not see each other's lock. Avoid mixed-version access to one DB. |

### T02 — corrupted-file hardening and a resource leak
| SHA | What | Notes |
|---|---|---|
| `2c51da1e1` | Shadow `FileHandle` leaked VM frame groups per CHECKPOINT; 64 MiB `max_db_size` died after 7 checkpoints (#924) | Clean. Restore 1 provenance URL. |
| `2a2753441` | Corrupted HASH PK entries crash MERGE | Clean |
| `11e3256e7` | Centralize node-offset validation in `NodeTable::isVisibleNoLock` | Clean. Independent of our `3616e405c`. |
| `9f23afeb0` | Zero-length `memcpy` from null on empty chunks (SIGSEGV, #840) | Clean |
| `0cb583936` | Fail fast on out-of-bounds PIP/header page reads (#843) | Clean |
| `db7830a28` | Validate page ranges from the allocator; a corrupted free list grew the file to 892 GiB (#948) | Clean |
| `5384822e9` | DAC page check | Clean |
| `ee9b9725c` | Validate hash-index headers, bound DAC chains | Reject: `test/storage/CMakeLists.txt` — add `disk_array_collection_test` by hand |
| `3b7f3e8dc` | Review follow-up + HashIndexLoadTest | Clean (after `ee9b9725c`) |
| `5fa9a95c6` | STRING dictionary offsets can underflow and crash | Clean |
| `2f956e750` | Harden dictionary offset validation + tests | Reject: CMake test line. Restore 1 URL. |

### T03 — compression correctness
| SHA | What | Notes |
|---|---|---|
| `e35c1a7ae` | ALP float: encodable range and sentinel by `ENCODED_TYPE` (UB, wrong sentinel on arm64) | Reject: `test/common/CMakeLists.txt` line for `alp_encode_test` |
| `6953959eb` | ALP DOUBLE exception chunk reaches an invalid state and crashes checkpoint | Reject: CMake test line |
| `7b0ae7618` | Bitpacking negated `INT64_MIN` (UB) | Clean |
| `c8ed6c266` | **Silent data corruption**: float chunks with NaN / -0.0 collapsed to CONSTANT on checkpoint (#1043) | Reject: CMake test line. Prevents new corruption only; data already written with the bug stays wrong. Add a release note. |
| `ac968c408` | COPY segment sizing was super-linear for compressible DOUBLE/STRING columns; list payloads of removed rows duplicated on disk | Perf + space. Clean. |

### T04 — concurrency (races, use-after-free)
| SHA | What | Notes |
|---|---|---|
| `234630406` | FWD/BWD rel batch insert shared one partition cursor → heap corruption in parallel COPY | Clean |
| `807d1fd00` | `OptimisticAllocator` data race in parallel COPY | Clean |
| `0e81143d1` | Arrow registry returned raw pointers → UAF on concurrent DROP (#933) | Clean |
| `22780c918` | Thread-local reusable `ResultSet` outlived its Database (UAF) | Clean. Replaces the thread-local cache with a per-`QueryProcessor` pool. |
| `8e3a1a0e0`, `ea8883874` | Parquet writer race; then a race-free parallel flush (per-row-group null counts) | Clean; take both |
| `b0187a020`, `392a620f1`, `464824f99` | IceDisk rel table lazy-reader data races (final form: no shared lazy readers) | Clean; apply in order |
| `bcf3fb4f1` | Hold the node-group lock across a delete (scan could dereference null versions) | Clean |
| `96f435dd3` | `VMRegion` discard-granule vector reallocated under concurrent readers | Clean |
| `09354d3b9` | IceDisk multi-rel scan reused the first table's reader → wrong counts (#1066) | Clean |

---

## 4. Wave 2 — query correctness and checkpoint failure safety

### T05 — prepared statements and the cached-plan fast path
| SHA | What | Notes |
|---|---|---|
| `f150bddf7` | Parameters inside a subquery `WHERE` were never collected → silently empty results | Clean |
| `bc38ef8d3`, `88c4854a7` | Aggregates over unresolved `ANY` parameter placeholders failed to prepare via C API (#962) | Clean |
| `ea9ef103c` | Stale `SKIP`/`LIMIT` on re-executed cached plans (#985) | Reject: one include line in `src/main/prepared_statement.cpp` (`binder/expression/parameter_expression.h`). Restore 6 URLs. Behaviour: statements with parameterized SKIP/LIMIT no longer use the physical-plan cache. |
| `d64ba0e86` | Cached rel scan segfaulted after a committed node insert (#1030) | Clean. Restore 2 URLs. |
| `4f730f070` | GDS table-function plan reuse crashed in `FactorizedTable::merge` | Clean |

### T06 — checkpoint failure atomicity and WAL recovery
Run T02 before this task: `1bd9b9959`'s symptom is caught by the dictionary validation in T02.

| SHA | What | Notes |
|---|---|---|
| `1bd9b9959` | Scans running during a checkpoint read stale pages (shadow pages not consulted) | Clean |
| `926a0ba10`, `439da80f9` | A failed out-of-place checkpoint left the segment `IN_MEMORY` → no further checkpoint possible | Clean |
| `0063666e1` | Restore the active WAL when a checkpoint fails (duplicate-PK error on reopen, or **lost writes**) | Clean. **Behaviour change:** after a failure past the CHECKPOINT record, later checkpoints are refused until reopen |
| `584804061`, `f49718c11` | Recovered frozen WAL without a CHECKPOINT record was deleted after replay → lost writes | Clean. **Behaviour change:** a writable open runs a checkpoint when such a WAL is found |
| `08915c1c8` | Empty STRING PK index checkpoint left the DB unopenable (#464) | Reject: CMake test line |
| `8b393b3e8` | Publish the PK index checkpoint only after commit (duplicate PKs after a failed checkpoint, #1050) | Clean |
| `5f5c168fd`, `4c7a8c5b4`, `e31bb9deb` | Roll back the CSR node group on checkpoint failure (rels attached to wrong source nodes, #1051) | Clean. See §1 on the interplay with our `4ba39f3a9` |

### T07 — sanitizer fixes, part A
| SHA | What | Notes |
|---|---|---|
| `6c292d2b8` | Unaligned string hash access | Clean |
| `2092c4284` | ALP decode overflow, null deref in `delete_executor` finalize, memcpy for packed FT rows | Reject: `.github/workflows/nightly-sanitizers.yml` — **drop** those hunks (sanitizer runs live in the verification repo; schedules were removed in `ed3cca737`) |
| `55a45e520` | More memcpy fixes + **ALTER default-value evaluator bound to a null ResultSet** (Finding 5) | Clean. Review point: upstream binds a function-local `static ResultSet` shared by every database and thread. It is safe only while the evaluator never writes to it. Consider a per-operator empty `ResultSet` member as a follow-up. |

### T08 — intersect kernels (perf; must sit between T07 and T09)
`fafc36265` (T09) patches the header-only intersect kernels this task creates, so the order matters.

| SHA | What |
|---|---|
| `af02fc14e`, `f24a54a3e` | Galloping intersect for skewed adjacency lists + test |
| `51fc3c95c` | Kernels header-only, no `RIAG_API` exports |
| `03ac456f6`, `a900d2b40` | CRoaring-style batched-search arm + benchmark test (portable printf) |

All clean.

### T09 — sanitizer fixes, part B, and build hygiene
| SHA | What | Notes |
|---|---|---|
| `58745bbb8` | Unused variables (#973, #974) | Clean |
| `7e5abd7e2` | Cast bounds tests for FLOAT/DOUBLE | Clean |
| `fafc36265` | Remaining misaligned loads (count states, pattern table, intersect, radix ties) | Clean |
| `02e1c3325` | gtest discovery timeout for C API tests under sanitizers | Clean; helps our ASan builds |
| `6cd21a2b1` | int128 overflow, **shift by 64** (Finding 4), more memcpy; CMake `-fno-sanitize=alignment` order | Clean. Its FT 8-byte padding is reverted by the next commit. Take both. |
| `3940cb137` | Revert FT alignment padding (it broke unflat columns) | Clean, mandatory with `6cd21a2b1` |
| `399903bd8` | Portable int128 add/sub (MSVC has no `__int128`) | Clean, mandatory: `6cd21a2b1` breaks the Windows build without it |
| `ee94b150a` | Exhaustive enum switches (turns a missing-case bug class into a build error) | Clean |

**Verification-repo follow-up after T07/T09:** remove the `shift-exponent` and
`null-pointer-dereference` scopes for Findings 4 and 5 from `fuzz/suppressions/ubsan-ignorelist.txt`,
rebuild ASan, rerun the reproducers, and mark both fixed. Keep the `[alignment]` suppression.

### T10 — execution wrong results and crashes
| SHA | What | Notes |
|---|---|---|
| `64cadc26f`, `654f24953`, `12a6fc3f2` | PackedFilteredCount: deterministic order; **wrong counts on multi-key batches** | Clean; keep this order |
| `413447c8f` | List quantifiers (ANY/ALL/NONE/SINGLE) segfault past 2048 elements (#1024) | Clean |
| `5a22ad4a1`, `974151ebf` | SERIAL PK `= literal` fell back to a full scan; companion guard against catalog mismatch | Clean; **must go together** (the first exposes the throw that the second fixes) |
| `91a21c9ef` | Degree top-k rewrite: segfault with STRING PK, wrong keys with non-contiguous INT PK (#1031) | Clean |
| `f60112199` | ORDER BY `<string>, <x>` scrambled the trailing keys when merging runs (#1067) | Clean |
| `20314274b` | Sub-node-group morsel underflow → **duplicate rows** (#1085) | Clean |
| `fd94e9471` | Casting an empty string with a null data pointer segfaulted (#1084) | Reject: `cast_error.test` append — keep our blocks and add theirs |

---

## 5. Wave 3 — performance and operations

### T11 — hash-join SIP with rel-scan semi masks
| SHA | What | Notes |
|---|---|---|
| `a04207ab4` | SIP masks extend output; LDBC SF1 q16 −83 %, q12 −35 %, identical results | Clean. Touches `NodeGroup::scan`, so give it a differential campaign. |
| `5a3a52236` | Review fixes, including a `reinterpret_cast` read of the wrong type in `mapSemiMasker` | Clean, mandatory with `a04207ab4` |
| `753e821e6` | Skip probe-to-build SIP when probe ≥ build | Clean |
| `3205a014b` | Fused ice-disk scan left a dangling operator mapping → `SEMI_MASKER` UAF (#1068) | Clean **only after** `a04207ab4`/`5a3a52236`. Applied earlier, it rejects 4 hunks. |

### T12 — misc performance and operations
| SHA | What | Notes |
|---|---|---|
| `4982d126e` | List hash re-hashed the whole data vector per row (Q10 2.8 s → 0.6 s) | Clean |
| `96551407c` | Size the default buffer pool from the cgroup limit (container OOM kills, #1070) | Clean. Directly relevant: Docker is our default path. |
| `ff4452e7b` | `make shell` honours `EXTENSION_STATIC_LINK_LIST` | Clean |

### T13 — CI: OpenSSL on Windows/macOS runners
Our workflows still run on `windows-2022`, which upstream reports no longer ships OpenSSL.

| SHA | Notes |
|---|---|
| `87e0a5e11`, `5743cc548`, `2e2a83330`, `9508455e4` | `precompiled-bin-workflow.yml`: install OpenSSL 3.5.9, `msvc-dev-cmd` pinned. Clean. |
| `571b129ec` | `python-wheel-workflow.yml`: same. Reject (our renamed workflow diverges) — hand-merge. |
| `2f892aced` | macOS: relocate OpenSSL in httpfs/llm extension builds (extensions shipped via GitHub Pages, D9). Clean. Restore 2 URLs. |

---

## 6. Wave 4 — submodules

The core bump commits point at ladybug repos, so they cannot be taken as-is. Pick the content into our
mirrors (`--profile <python|nodejs|rust>` for bindings), then bump the pointers in RiaGraph in one commit.

| Repo | Pick | What |
|---|---|---|
| `extension` (ours at `89fceca`, which already has the FTS export commits) | `7824ead` | HNSW visited-bitmap heap-buffer-overflow |
| | `e14664b` | HNSW post-build inserts ignored the index metric and `efc` |
| | `62aa925` | FTS ignored a custom `ignore_pattern`/tokenizer when normalizing queries (#910) |
| `tools/python_api` | `dd285ce` | Cached prepared statement scanning a Python object returned 0 rows |
| | `548c183` | Implicit prepare cache keyed by full list element types (wrong signature reuse) |
| `tools/nodejs_api` | `6bfe826`, `e25b293` | Concurrent first use of a fresh Connection raced the lazy init (#24) + test |
| `tools/rust_api` | `3f60765` | Link libatomic on Linux static builds |
| | `b7749f3`, `c941bbb` | Include the Linux variant in the prebuilt cache key (wrong cached lib) |
| | `47bfd12` | Keep the prebuilt cache out of the crate source tree |

The extension and binding changes need the binding test suites, plus `verify.ps1 run explorer -- build`
for nodejs.

---

## 7. Wave 5 — deferred: LDBC/LSQB planner and optimizer series

Take this as **one series, all or nothing**, after Waves 1–4 are verified. It brings large benchmark
wins (Q14 OOM → 0.13 s, Q12 9 s → 0.6 s, LSQB q1 35–50×, q9 95×). Upstream also shipped three
regressions inside it and fixed them in later commits of the same series: MERGE returning empty,
a planner hang or SIGSEGV, and a 22 s plan. A partial pick reintroduces those bugs.

Apply in this order (dry run: all clean except as noted):

| # | SHA | What |
|---|---|---|
| 1 | `95e1f6934` | `COUNT(*)` over packed extend chains (count-only operator) |
| 2 | `31c6354fd` | Directional hops in the anti-edge count rewrite |
| 3 | `11a991bce` | `COUNT_ANTI_EDGE_CHAIN` to-string + exhaustive switch. **Rejects 3 hunks in `optimizer.cpp`:** they belong to the unselected `LBUG_DUMP_LOGICAL` dumper, so drop them and keep `logical_operator.cpp`. |
| 4–7 | `595e1bd98`, `9d007e01d`, `a05e3c0c9`, `7e91732e7` | Gate the rewrites on native storage + tests |
| 8 | `58c10a424` | Selective correlated-OPTIONAL unnesting, PK lookup in OPTIONAL, recursive seed costing |
| 9 | `009e0b492` | COUNT over whole nodes via internal IDs; COLLECT group-key reduction |
| 10–11 | `4155e44d2`, `efcdbc0c0` | Fix: MERGE existence checks and join hints (regression from #8) |
| 12 | `f328ee005` | Unnest COLLECT-then-IN membership; also turns an `ExpressionMapper` UB into a clear error |
| 13 | `273adad53` | Staged COUNT(DISTINCT) pre-aggregation over LEFT-join chains |
| 14 | `8caf03bf9` | Fan-out-aware join costing using ANALYZE degree stats |
| 15 | `44c21aa39` | Fix: fan-out double count (regression from #14: LSQB q9 80 ms → 22 s) |
| 16 | `ad0397859` | Anti-edge rewrite accepts the bushy triangle join (regression from #14) |
| 17 | `37bf0dc83` | Fix: by-value temporaries in `planOptionalMatch` → hang/SIGSEGV on MERGE/OPTIONAL with a property predicate (regression from #8) |

Gate: the full e2e suite, then a long differential + cyphuzz campaign that focuses on `MERGE`,
`OPTIONAL MATCH`, `EXISTS`/`COUNT {}` subqueries, `COLLECT … IN`, and `COUNT(DISTINCT)`.
Upstream itself only validated on LDBC SF1/LSQB.

---

## 8. Not selected (core)

| SHA(s) | Subject | Why not |
|---|---|---|
| `e82d769f0`, `479397394`, `7abd80f8c`, `3eb72a15c`, `effdcea8d` | icebug-disk EXPORT format, IMPORT defaults to native, grammar change | Feature; changes IMPORT/EXPORT behaviour and the grammar (≈3.5k-line ANTLR regen) |
| `713d6b859` | Heap corruption in indptr export finalize | Fixes code that only exists with the export feature above |
| `1d060021b` | Missing paren in exported CREATE REL TABLE | Bug introduced by the export feature; our `toCypher` never emits `WITH (storage…)` |
| `346865d70`, `38ae83bce`, `22bdda38e`, `d008a7629` | `LBUG_DUMP_LOGICAL` plan dumper | Debug feature; `EXPLAIN` covers it |
| `553e2c527`, `0de38747a` | Prepared-statement Arrow schema without execution (C API) | API feature (Flight/ADBC) |
| `814deeedd`, `a0792b97d` | `schema_table` / `schema_rel` system tables | Feature; adds hidden tables to every database |
| `cbe0ff594`, `33567d5f0`, `ed9bfb8e2`, `0a9004527` | SQL pushdown generalization, Iceberg/UC, DuckLake | Feature |
| `231b57480` | Partition routing hooks in the amalgamated header | Feature (distributed ABI) |
| `f84495ef4`, `79d8a1135` | `hf://` remote paths | Feature, paired with an httpfs alias |
| `dd8881a73` | ABI comments for extensions | Comments only |
| `28320ef2a` | Remove unused test code | No functional value |
| `093ce2840`, `17a70e51c`, `c473940c7`, `d7225cea5` | Version bumps 0.21.0–0.21.2 | Own versioning (D6); storage version stays 47 upstream too |
| `f250c2746` | Update production releases script | Upstream release process |
| `d13be4dd2`, `f96d38e53`, `3d15b4f39` | Split debug symbols out of Python wheels | Upstream PyPI packaging; we distribute via GitHub only (D4). Revisit when we publish wheels. |
| `0998466d9` | LSQB benchmark runs the TestPyPI nightly wheel | Upstream infrastructure |
| `5936a18c1`, `300108914`, `bfe9b2e76`, `2d69bd3d7` | Nightly sanitizer workflow tweaks | Sanitizer runs live in the verification repo. `2d69bd3d7`'s decision (no alignment checks) already matches our `[alignment]` suppression. |
| `e2e533b29`, `df20d4a47`, `2161c0f8d` | `workflow_dispatch` "all platforms" option (two of them cancel out) | CI convenience |
| `dad846de4`, `057e5dd95`, `a90c1aa21`, `d135ac159`, `3517aaf98`, `26264904c`, `9d4598a15`, `7a089588d` | Submodule pointer bumps | They point at ladybug repos; the content is handled in Wave 4 |

## 9. Not selected (submodules)

| Repo | SHA(s) | Why not |
|---|---|---|
| extension | `b71aca4`, `89fceca` | Already in our pointer |
| extension | `98bbb9d`, `4154fda`, `318f59d`, `3307c12`, `cfb0501` | Iceberg/UC ATTACH, DuckLake — feature |
| extension | `2d5f07a` | Iceberg REST attach fix; edits the same files as, and depends on, the unselected Iceberg work |
| extension | `bf74ab6` | `hf://` alias — feature |
| extension | `a5c78ee` | icebug 13.4 dependency update; evaluate separately |
| extension | `056fabf` | Copies upstream's LICENSE; we maintain our own (D10) |
| python | `b8b81a5`, `af5052f`, `0176c78` | Debug-symbol split for wheels — packaging |
| rust | `19c48ac`, `8fae3f6` | Need the unselected `getArrowSchema` API |
| rust | `e77dabd`, `ca706e5`, `28aba1c` | Partition-routing shim — needs the unselected `231b57480` |
| nodejs | `bfff3d4`, `e706c1f`, `ed9e9d8`, `4cc3afc`, `735372c`, `313a547` | Windows OpenSSL/MSVC CI for the nodejs repo. Take them if our nodejs CI breaks the same way (likely, see T13). |

---

## 10. Suggested order and effort

| Order | Task | Commits | Expected manual work |
|---|---|---|---|
| 0 | T00 fix `riag-rename.py --patch` on Windows | — | small |
| 1 | T01 WAL | 2 | none |
| 2 | T02 corrupt-file hardening | 11 | 2 CMake lines |
| 3 | T03 compression | 5 | 3 CMake lines |
| 4 | T04 concurrency | 12 | none |
| 5 | T05 cached plans | 6 | 1 include |
| 6 | T06 checkpoint safety | 11 | 1 CMake line; review the behaviour changes |
| 7 | T07 → T08 → T09 sanitizer + intersect | 3 + 5 + 8 | drop CI hunks; verification-repo suppression follow-up |
| 8 | T10 wrong results / crashes | 10 | 1 test-file merge |
| 9 | T11 SIP | 4 | none; differential campaign |
| 10 | T12 misc, T13 CI | 3 + 6 | 1 workflow hand-merge |
| 11 | Wave 4 submodules | 11 | per-repo picks + pointer bump |
| 12 | Wave 5 planner series | 17 | drop 3 dumper hunks; long campaign |

---

## 11. Outcome (2026-10-04, reviewed 2026-10-05)

All five waves are applied on **`upstream/integration-2026-10`**. The wave branches were stacked, so the branch is `upstream/wave-5` plus this section, the review fix and the merged timer fix (see below); it merges into `main` (`ed3cca737`) without conflicts. Every pick has its own commit with upstream authorship and a `(cherry picked from ladybug <sha>)` trailer, and a row in [upstream-picks.md](upstream-picks.md).

| Wave | Tasks | Applied | Notes |
|---|---|---|---|
| 1 | T00-T04 | T00 + 30 core picks | rename-tool fix, WAL, corrupted-file hardening, compression, concurrency |
| 2 | T05-T10 | 43 core picks + 1 partial | cached plans, checkpoint atomicity, sanitizer fixes, intersect kernels, wrong results |
| 3 | T11-T13 | 13 core picks | semi-mask SIP, performance/ops, CI OpenSSL |
| 4 | T14-T17 | 11 submodule picks | extension (3), python (2), nodejs (2), rust (4); pointers bumped in one commit |
| 5 | planner series | 17 core picks + 1 fix of ours | see below |

**Deviations from the plan**

- `e82d769f0` was taken **partially**: only the columnar-table guard in `filter_push_down_optimizer.cpp`, which `5a22ad4a1` needs (otherwise `{id: 100}` on an ice-disk node becomes an index lookup that misses). The EXPORT feature stays unselected.
- `11a991bce` (Wave 5): its three rejected `optimizer.cpp` hunks belong to the unselected `LBUG_DUMP_LOGICAL` dumper and were dropped.
- **One engine fix of our own on top of Wave 5** (`520d09e91`). The imported `COUNT_ANTI_EDGE_CHAIN` rewrite (LSQB q9) returned wrong counts when the query had no `id(a)<>id(c)` filter, and when the end node of the chain had a self-loop. Upstream's LDBC/LSQB data never reaches either case, and the tests on tiny fixtures do not fire the rewrite, so the test suite was green with both present. The fix comes with an e2e test whose expected values were enumerated outside the engine. Worth reporting upstream.
- **Review fix** (`76cf54f73`): the T06 CSR rollback picks cite upstream as the short form `LadybugDB/ladybug#1051`, which the rename turned into a dead `LadybugDB/riagraph#1051` in three places; restored. The provenance markers that pushed comment lines past 100 columns now sit on their own `riag-rename:skip-next` line, so clang-format (`ReflowComments`) no longer flags them. [upstream-picks.md](upstream-picks.md) covers both points in its procedure.
- **One engine fix that is not an upstream pick, merged in** (`fix/timer-steady-clock`, `7900dae66`, merged as `6199221b4`). `common::Timer` measured query timeouts with `high_resolution_clock`, which is the wall clock in libstdc++. When the clock is stepped backwards (a Docker Desktop VM does this about every 30 s), a running query's elapsed time went negative, wrapped to a huge `uint64_t`, and the query was interrupted ("Interrupted.") regardless of its timeout. It now uses `steady_clock`, with a regression test (`TimerTest`). Upstream still has the wall clock. The fix branch is based on `main`, so merging it there on its own stays a no-op for this branch.
- Behaviour changes carried by the picks, as listed in §4 (T06) and §5: after a checkpoint failure past the CHECKPOINT record, later checkpoints are refused until reopen; a writable open checkpoints a recovered frozen WAL; the default buffer pool is sized from the cgroup limit (`96551407c`, so a query that only just fit in a 4 GiB container can now fail cleanly with "buffer pool is full"); the Windows file-lock byte moved (do not mix old and new builds on one database).

**Verification**

- Final branch tip (`6199221b4`) on the Docker build env: 2862 ctest tests run, all pass except three `extension.*` tests that download from a host that returns 404 (no extensions are published there yet). `AnonymousParquetDeleteReload` needs its fixture generated first (`scripts/generate-dictionary-bug-fixture.py`, as `docker/build-env/verify.sh` does). Waves 1-4 were also verified wave by wave, as recorded in the commit messages.
- Review (2026-10-05): every selected pick was re-derived from upstream through `riag-rename.py --patch` and compared with its commit. 94 of 104 core picks and all 11 submodule picks are identical; the other 10 differ only by the deviations above and the logged hand-merges. The verification repo's differential, planner-probe, cyphuzz and ASan+UBSan runs on the final branch report only previously known defects, and the planning time of the fuzzer's timeout inputs is unchanged from `main`.
- Not build-verified: the three extension picks (the extension repo is still `lbug`-named until Phase 6) and the rust `localize-symbols` path. CI has not run on this branch yet, so the Windows/macOS picks (`fe34d7865`, `399903bd8`, `654f24953`, `a900d2b40`, T13) are verified by reading only.
- Still open: a long (multi-hour) ASan+UBSan campaign and a long cyphuzz run focused on MERGE, OPTIONAL MATCH, COLLECT-IN and COUNT(DISTINCT); the review ran a short one of each.

