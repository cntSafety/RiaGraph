# Upstream cherry-picks

Every commit taken from [`ladybugdb/ladybug`](https://github.com/ladybugdb/ladybug) after
the fork gets a row here. Without this log the next person cannot tell what has already
been taken, and the same fix gets picked twice or missed entirely.

## Procedure

```bash
# once per clone
git remote add upstream https://github.com/ladybugdb/ladybug.git
git remote set-url --push upstream DISABLED

# per pick
git fetch upstream
git format-patch -1 --stdout <upstream-sha> > /tmp/up.patch
python scripts/riag-rename.py --patch /tmp/up.patch > /tmp/riag.patch
git am -3 /tmp/riag.patch          # or: git apply -3 /tmp/riag.patch
```

`--patch` runs the diff through the same substitution table as the tree, including the
`diff --git` / `---` / `+++` path headers, so a patch against upstream file names lands on
the renamed ones. For a binding submodule add `--profile <python|nodejs|java|rust|wasm>`;
the token table alone gets those repositories wrong in a few places (see `PROFILES` in the
script).

**Read the transformed patch before applying it.** Two things it cannot get right on its
own:

- **Provenance URLs.** An upstream patch carries no `riag-rename:skip` markers, so a
  `github.com/LadybugDB/ladybug/issues/900` link the patch introduces is rewritten into a
  link to a repository that never existed. Restore it and add the marker as part of the
  pick. The short form (`LadybugDB/ladybug#1051`) is rewritten the same way, so look for
  that too. When the trailing marker would push the line past 100 columns, put
  `// riag-rename:skip-next upstream provenance` on its own line above instead:
  clang-format reflows an over-long comment and moves the trailing marker onto the next
  line, which then fails CI's format check and stops protecting the link.
- **Phase 2 territory.** The patch will conflict wherever upstream touched something this
  fork changed semantically — the storage header, the extension installer, the attach
  binding. Those conflicts are the point: they are exactly the places that need human
  judgement rather than a mechanical merge.

After applying, run `python scripts/riag-rename.py --check` before pushing.

## Log

| Date | Upstream SHA | Our SHA | Rationale |
|---|---|---|---|
| 2026-10-03 | `0b4f086ad` | `78e7d728c` | T01: Fix WAL checksum entry-buffer resize dropping buffered bytes |
| 2026-10-03 | `fe34d7865` | `bc7baba19` | T01: Fix Windows-only CardinalityTest and WalTest failures |
| 2026-10-03 | `2c51da1e1` | `023115b0b` | T02: Fix #924: reuse shadow FileHandle across checkpoints |
| 2026-10-03 | `2a2753441` | `107d32cfd` | T02: fix(storage): prevent corrupted HASH PK entries from crashing MERGE |
| 2026-10-03 | `11e3256e7` | `d4998fb38` | T02: Centralize Node Offset Validation in NodeTable::isVisibleNoLock |
| 2026-10-03 | `9f23afeb0` | `35ef82f80` | T02: fix(storage): guard chunk scan against zero-length memcpy on empty chunks |
| 2026-10-03 | `0cb583936` | `ca8736a75` | T02: fix(storage): fail fast on out-of-bounds PIP/header page reads |
| 2026-10-03 | `db7830a28` | `29858ae17` | T02: fix(storage): validate page ranges handed out by the allocator |
| 2026-10-03 | `5384822e9` | `906d10f12` | T02: fix DAC page check |
| 2026-10-03 | `ee9b9725c` | `5fe65781c` | T02: Validate hash index headers and bound DAC chains |
| 2026-10-03 | `3b7f3e8dc` | `30f02b442` | T02: Address review findings on PR 982 |
| 2026-10-03 | `5fa9a95c6` | `60fc1d556` | T02: fix STRING dictionary offsets can underflow and crash the engine |
| 2026-10-03 | `2f956e750` | `8775cfba7` | T02: Harden STRING dictionary offset validation and add regression coverage |
| 2026-10-03 | `e35c1a7ae` | `62ad576ed` | T03: fix(alp): bound the encodable range and the sentinel by ENCODED_TYPE |
| 2026-10-03 | `6953959eb` | `1b0e7ec4a` | T03: fix ALP DOUBLE exception chunk can reach invalid state and crash during checkpoint |
| 2026-10-03 | `7b0ae7618` | `788209dc1` | T03: Fix UBSAN negation overflow on INT_MIN in integer bitpacking |
| 2026-10-03 | `c8ed6c266` | `91f927fc3` | T03: fix(storage): don't collapse float chunks holding NaN/-0.0 to CONSTANT |
| 2026-10-03 | `ac968c408` | `281fb1b43` | T03: fix(storage): keep COPY segment sizing linear for compressible columns |
| 2026-10-03 | `234630406` | `a196538e9` | T04: Fix race between FWD/BWD rel batch inserts sharing one partition cursor |
| 2026-10-03 | `807d1fd00` | `83dee11b3` | T04: Fix data race in OptimisticAllocator during parallel COPY |
| 2026-10-03 | `0e81143d1` | `ef5f8535c` | T04: fix(storage): pin Arrow registry entries with shared_ptr to prevent use-after-free |
| 2026-10-03 | `22780c918` | `42041b772` | T04: Fix use-after-free: reusable ResultSet outlived its Database |
| 2026-10-03 | `8e3a1a0e0` | `700ae1df0` | T04: Fix Parquet writer data race |
| 2026-10-03 | `ea8883874` | `1e0322bc7` | T04: Parallelize Parquet export flush while keeping it race-free |
| 2026-10-03 | `b0187a020` | `e80aa27c5` | T04: Fix IceDisk relation table data race |
| 2026-10-03 | `392a620f1` | `6b8a9acdd` | T04: Make indptr load lock-free after warmup, assert lock held |
| 2026-10-03 | `464824f99` | `638fde5f3` | T04: Fix data race in IceDiskRelTable lazy reader init (#984) |
| 2026-10-03 | `bcf3fb4f1` | `ec4845104` | T04: fix(storage): hold the node-group lock across a delete |
| 2026-10-03 | `96f435dd3` | `00c96c357` | T04: fix(storage): make discard-granule residency safe to grow |
| 2026-10-03 | `09354d3b9` | `021b2e001` | T04: fix(ice-disk): reopen parquet reader when multi-rel scan switches tables |
| 2026-10-03 | `f150bddf7` | `7fedeb7e9` | T05: Fix parameter collection inside subquery WHERE clauses |
| 2026-10-03 | `bc38ef8d3` | `6f13bfaef` | T05: Fix aggregate binding over unresolved parameter placeholders (#962) |
| 2026-10-03 | `88c4854a7` | `d9a31ba30` | T05: Generalize aggregate ANY-placeholder deferral to mixed inputs |
| 2026-10-03 | `ea9ef103c` | `37e7cd492` | T05: Fix stale SKIP/LIMIT on re-executed cached physical plans (#985) |
| 2026-10-03 | `d64ba0e86` | `7276dff5a` | T05: fix(processor): reset node scan shared state on cached plan reuse |
| 2026-10-03 | `4f730f070` | `be2b93ec3` | T05: fix(gds): reset factorized-table pool when reusing cached table-function plans |
| 2026-10-03 | `1bd9b9959` | `0e7c45d68` | T06: fix(storage): read shadow pages for scans running during a checkpoint |
| 2026-10-03 | `926a0ba10` | `67d6c38b6` | T06: fix(storage): keep segment on disk when an out-of-place checkpoint fails |
| 2026-10-03 | `439da80f9` | `03d686548` | T06: style(storage): use [[maybe_unused]] for the released null chunk |
| 2026-10-03 | `0063666e1` | `c371e2bd4` | T06: fix(storage): restore the active WAL when a checkpoint fails |
| 2026-10-03 | `584804061` | `712670293` | T06: fix(storage): keep recovered frozen WAL until it is checkpointed |
| 2026-10-03 | `f49718c11` | `f113659f8` | T06: refactor(storage): scope frozen WAL adoption during recovery |
| 2026-10-03 | `08915c1c8` | `6fc67282e` | T06: fix(storage): assign PK index header pages under one condition |
| 2026-10-03 | `8b393b3e8` | `0f9c3fb98` | T06: fix(storage): publish PK index checkpoint only after commit |
| 2026-10-03 | `5f5c168fd` | `1a70b5e4b` | T06: fix(storage): roll back CSR node group to pre-checkpoint state on failure |
| 2026-10-03 | `4c7a8c5b4` | `912f0f1bf` | T06: fix(storage): snapshot CSR chunks without group version info |
| 2026-10-03 | `e31bb9deb` | `3dd18630d` | T06: fix(storage): tolerate IN_MEMORY chunks in CSR rollback snapshot |
| 2026-10-03 | `6c292d2b8` | `973865fb8` | T07: Fix unaligned string hash access |
| 2026-10-03 | `2092c4284` | `5a5221c03` | T07: Fix UBSan failures and TSan fixture gap from nightly sanitizer run. The `nightly-sanitizers.yml` hunks (fixture generation before the TSan tests) were dropped as the plan says, so the commit message's TSan paragraph does not apply here; that workflow is manual-dispatch only |
| 2026-10-03 | `55a45e520` | `2feaa62f0` | T07: Fix UBSAN misaligned loads and null-pointer bindings |
| 2026-10-03 | `af02fc14e` | `9e1b309ee` | T08: Optimize skewed intersect lists |
| 2026-10-03 | `f24a54a3e` | `51cbfb6c6` | T08: Fix GCC warning in intersect test |
| 2026-10-03 | `51fc3c95c` | `aa5a4f800` | T08: Make intersect kernels header-only, drop LBUG_API exports |
| 2026-10-03 | `03ac456f6` | `ed4672a8a` | T08: Add CRoaring-style batched search as third intersect arm with benchmark |
| 2026-10-03 | `a900d2b40` | `404562333` | T08: Fix -Wformat error in intersect kernels benchmark print |
| 2026-10-03 | `58745bbb8` | `6b43f0b43` | T09: Remove unused variables (fixes #973, #974) |
| 2026-10-03 | `7e5abd7e2` | `87d595aaf` | T09: test: add cast bounds cases for FLOAT and DOUBLE |
| 2026-10-03 | `fafc36265` | `c798dab0a` | T09: Fix UBSan misaligned loads from nightly sanitizer run 35089289811 |
| 2026-10-03 | `02e1c3325` | `058c45f76` | T09: Fix ASan+UBSan nightly build: raise gtest discovery timeout for C API tests |
| 2026-10-03 | `6cd21a2b1` | `706ade458` | T09: Fix remaining UBSan errors from nightly sanitizer run 35993196544 |
| 2026-10-03 | `3940cb137` | `070746b93` | T09: Revert factorized-table 8-byte alignment padding |
| 2026-10-03 | `399903bd8` | `385b72e5e` | T09: fix(common): make int128 add/sub overflow check portable for MSVC |
| 2026-10-03 | `ee94b150a` | `456ec7ad6` | T09: fix: make enum utility switches compiler exhaustive |
| 2026-10-03 | `64cadc26f` | `cf48abe5b` | T10: Sort PackedFilteredCount output and stabilize cardinality comparison |
| 2026-10-03 | `654f24953` | `dd79cc5e1` | T10: Fix MSVC build: avoid comma in EXPECT_EQ macro args |
| 2026-10-03 | `12a6fc3f2` | `bddf40adc` | T10: Fix PackedFilteredCount misattribution on multi-key batches |
| 2026-10-03 | `413447c8f` | `b7322bcad` | T10: fix(list): make quantifier exec slice-aware to fix segfault past 2048 elements |
| 2026-10-03 | `5a22ad4a1` | `8aaf699cf` | T10: fix(optimizer): use primary-key lookup for SERIAL key with implicit cast |
| 2026-10-03 | `974151ebf` | `4250017b0` | T10: fix(optimizer): don't throw on catalog mismatch in secondary ART lookup |
| 2026-10-03 | `91a21c9ef` | `6827dee7a` | T10: fix(optimizer): gate degree top-k rewrite on CSR invariant |
| 2026-10-03 | `f60112199` | `4f316bba3` | T10: fix(order-by): compare trailing keys after string tie in key block merge |
| 2026-10-03 | `20314274b` | `b0c708bea` | T10: fix(scan): stop sub-node-group morsel at range end in NodeGroup::scan |
| 2026-10-03 | `fd94e9471` | `7f5b7492f` | T10: fix(cast): null-safe error messages for empty string casts to temporal/uuid/bool |
| 2026-10-03 | `e82d769f0` (partial) | "fix(optimizer): keep scan+filter for columnar-backed tables" | T10: only the columnar-table guard in `filter_push_down_optimizer.cpp`; the EXPORT feature stays unselected. Needed by `5a22ad4a1`, which otherwise turns `{id: 100}` on an ice-disk node into an index lookup that misses (IceDiskComplexQueries). |
| 2026-10-03 | `a04207ab4` | `fd2aca897` | T11: Extend hash-join SIP with rel-scan semi masks |
| 2026-10-03 | `5a3a52236` | `5c75b26e7` | T11: Fix review findings on rel-scan semi masks |
| 2026-10-03 | `753e821e6` | `06ef615d4` | T11: Skip probe-to-build SIP when probe >= build: avoid useless probe materialization |
| 2026-10-03 | `3205a014b` | `803262906` | T11: fix(mapper): unmap fused-away scans; tolerate unmapped SEMI_MASKER targets |
| 2026-10-03 | `4982d126e` | `f73bd8a91` | T12: fix(exec): hash only selected rows' list entries in vector list hash |
| 2026-10-03 | `96551407c` | `3a7986d60` | T12: fix(main): size default buffer pool from cgroup memory limit when containerized |
| 2026-10-03 | `ff4452e7b` | `cbc56afad` | T12: Honor EXTENSION_STATIC_LINK_LIST in shell targets |
| 2026-10-03 | `87e0a5e11` | `aa7751e4c` | T13: ci(win-arm): install OpenSSL 3.5 via winget, locate VS with vswhere |
| 2026-10-03 | `5743cc548` | `7bf0b39b8` | T13: ci(win-arm): download slproweb ARM64 OpenSSL installer directly |
| 2026-10-03 | `2e2a83330` | `2679e1e93` | T13: ci(win): install OpenSSL on all Windows runners, use msvc-dev-cmd |
| 2026-10-03 | `9508455e4` | `380cdf952` | T13: ci(win): pin msvc-dev-cmd to v1.13.0 SHA |
| 2026-10-03 | `571b129ec` | `5d93a6489` | T13: ci(win-arm): install OpenSSL and use msvc-dev-cmd in python wheel builds. Hand-merged: our Windows matrix entries are commented out, so the new `msvc_arch`/`openssl_exe` keys are added commented as well |
| 2026-10-03 | `2f892aced` | `70af4449e` | T13: ci(macos): relocate OpenSSL dep in httpfs/llm extensions, drop pixi rpath |
| 2026-10-04 | `7824ead` | `d046c93` (extension) | T14: Fix heap-buffer-overflow in HNSW visited bitmap. Applied untransformed: `extension/` has not been through the Phase 6 rename yet, so the patches carry upstream spelling |
| 2026-10-04 | `e14664b` | `ca5f3ac` (extension) | T14: fix(vector): use index metric and efc for post-build HNSW inserts |
| 2026-10-04 | `62aa925` | `a309f5b` (extension) | T14: fix(fts): honor custom ignore_pattern (and tokenizer) when normalizing queries (#910) |
| 2026-10-04 | `dd285ce` | `09c6bd0` (python_api) | T15: Fix 0-row result when re-executing a cached prepared statement scanning a python object |
| 2026-10-04 | `548c183` | `56535ef` (python_api) | T15: fix(connection): segregate implicit prepare cache by full list element types |
| 2026-10-04 | `6bfe826` | `c10c94e` (nodejs_api) | T16: fix: share a single lazy init across concurrent Connection uses (issue #24) |
| 2026-10-04 | `e25b293` | `91d3dc6` (nodejs_api) | T16: test: cover concurrent first-use of a fresh Connection (issue #24) |
| 2026-10-04 | `3f60765` | `82c7dd9` (rust_api) | T17: build: link libatomic on Linux static builds |
| 2026-10-04 | `b7749f3` | `30bd0be` (rust_api) | T17: build: include the Linux variant in the prebuilt cache key |
| 2026-10-04 | `c941bbb` | `1d298b2` (rust_api) | T17: build: compute Linux variant suffix upfront, drop mut |
| 2026-10-04 | `47bfd12` | `a8ea83a` (rust_api) | T17: build: keep the prebuilt cache out of the crate source tree; opt-in localization of bundled C symbols. `src/lib.rs` hunk applied by hand (our docs differ) |
| 2026-10-04 | `95e1f6934` | `93b8222da` | W5: Generalize COUNT_REL_TABLE metadata counting to packed extend chains |
| 2026-10-04 | `31c6354fd` | `086b96395` | W5: Support directional chain hops in anti-edge count rewrite |
| 2026-10-04 | `11a991bce` | `408e68f11` | W5: logical_operator: add missing COUNT_ANTI_EDGE_CHAIN to type-to-string; make switch exhaustive. The 3 `optimizer.cpp` hunks belong to the unselected `LBUG_DUMP_LOGICAL` dumper and were dropped |
| 2026-10-04 | `595e1bd98` | `e454368fe` | W5: Gate the anti-edge chain count rewrite on native storage |
| 2026-10-04 | `9d007e01d` | `7d8caaa84` | W5: Gate suffix-hop endpoints and add a regression test |
| 2026-10-04 | `a05e3c0c9` | `5c1d8abc8` | W5: Match the LSQB q9 shape in the anti-edge regression test |
| 2026-10-04 | `7e91732e7` | `9ad882b4a` | W5: fix the native case to match LSQB q9 more precisely |
| 2026-10-04 | `58c10a424` | `85c350127` | W5: fix(planner): selective correlated-optional unnest, PK lookup in OPTIONAL, recursive seed costing |
| 2026-10-04 | `009e0b492` | `5919f85aa` | W5: fix(planner): COUNT over whole nodes via internal IDs; collect-list group-key reduction |
| 2026-10-04 | `4155e44d2` | `7b3062a93` | W5: fix(planner): keep MERGE existence checks and join hints on unnest path |
| 2026-10-04 | `efcdbc0c0` | `e822c501a` | W5: fix(planner): keep legacy subquery analysis for MERGE checks and join hints |
| 2026-10-04 | `f328ee005` | `c6636d9da` | W5: fix(planner): unnest COLLECT-then-IN membership into pair joins |
| 2026-10-04 | `273adad53` | `59256f904` | W5: fix(planner): staged distinct pre-aggregation over LEFT-join chains |
| 2026-10-04 | `8caf03bf9` | `e09efe342` | W5: fix(planner): degree-stats join order fan-out and staged-agg cardinality |
| 2026-10-04 | `44c21aa39` | `d07206bbd` | W5: fix(planner): keep factorized cardinality on extend to avoid fan-out double-count |
| 2026-10-04 | `ad0397859` | `8bcfcbdb1` | W5: fix(optimizer): accept bushy triangle join in anti-edge chain rewrite |
| 2026-10-04 | `37bf0dc83` | `db5400c6f` | W5: fix(planner): bind by-value set once in correlated-optional unnest |
| 2026-10-05 | `8c2d52e8f` | `0d2d21bfb` | 0.22-dev fixes: fix(storage): refresh node group scan state after a checkpoint |
| 2026-10-05 | `872b06530` | `8f5595675` | 0.22-dev fixes: fix(storage): validate disk array and overflow page indices |
| 2026-10-05 | `b300d59fb` | `9ba670df9` | 0.22-dev fixes: test(storage): fill the corrupt PIP through void* for GCC class-memaccess |
| 2026-10-05 | `fe32b0f8e` | `b4cb4411f` | 0.22-dev fixes: fix(aggregate): return [] instead of null for collect() on empty/all-null input. Provenance link restored with a skip marker |
| 2026-10-05 | `cb3f6e7c9` | `1edd4785c` | 0.22-dev fixes: fix(tests): update expectations for collect() empty-list behaviour |
| 2026-10-05 | `23172196f` | `c55f1ccda` | 0.22-dev fixes: fix(binder): allow implicit cast STRING -> UUID (upstream #1102). Provenance link restored with a skip marker |
| 2026-10-05 | `1f5e096c6` | `ecc8f68c4` | 0.22-dev fixes: fix(parser): reset standalone-call rewrite query per statement (the bug-fix half of upstream PR #1104; its catalog and extension-state features are not taken) |
| 2026-10-05 | `e1a68c062` | `4a24554b3` | 0.22-dev fixes: fix(binder): set alias on bound path expressions |
| 2026-10-05 | `b883fa2d0` | `14883ac99` | Open upstream PR #1114, not merged upstream: fix(storage): don't zero CSR header and length-chunk buffers (upstream issue #1110) |
| 2026-10-05 | `a7ef29373` | `e934252b4` | Open upstream PR #1118, not merged upstream: fix(binder): allow aggregating a value derived from an earlier aggregate (upstream issue #1111) |
| 2026-10-05 | `31f5bbd95` | `48cd8f486` | Open upstream PR #1107, not merged upstream: fix(storage): harden checkpoint crash recovery (upstream issue #1098). Follow-up `c4a378386` adds the test and corrects two messages |
| 2026-10-05 | `da6ffb3e6` | `a2d88d95f` | Open upstream PR #1107: style commit |
| 2026-10-05 | `01b9cfa80` | `bdae3dc6e` | Open upstream PR #1108, not merged upstream: fix(storage): detect stale pre-0.21 string hash indexes loudly (upstream issue #1092). Follow-up `b5a8017d4` makes the legacy hash sign-extend on every host and pins its values; follow-up `c0fd8b8c1` makes the check reach the hash index the old hash selects (as ported it almost never fired) and tests it against a database written by 0.20.2 |
| 2026-10-05 | `4908050f9` | `c9183e082` | Open upstream PR #1108: style commit |
| 2026-10-20 | `aa26196de` | `294f1feb4` | R20: c_api: add riag_value_create_blob function |
| 2026-10-20 | `e1ad26cf4` | `a359ac845` | R20: fix(c_api): format riag_value_create_blob and guard null buffer |
| 2026-10-20 | `f781716ae` | `312053566` | R20: feat(catalog): function lookup falls back from graph catalogs to main |
| 2026-10-20 | `b1fb87821` | `fc599e1f3` | R20: feat(extension): per-database extension state slots (setData/getData) |
| 2026-10-20 | `86b746995` | `c0b5584f0` | R20: fix(storage): harden checkpoint crash recovery. shadow_file.cpp takes upstream's size check in place of the one from our port of PR #1107 (48cd8f486); EmptyShadowFileAfterDurableCheckpointRecordFailsCleanly (c4a378386) now expects upstream's message. local_file_system.cpp keeps the .riag sidecar comment. |
| 2026-10-20 | `a456025bd` | `42e31a6c9` | R20: test(transaction): read ANY-graph JSON property via toString() |
| 2026-10-20 | `5d3975b15` | `d6e2ce073` | R20: fix(storage): harden checkpoint crash recovery (fixes #1098). this is the merged form of PR #1107, which we ported as 48cd8f486; only the replay error messages differ, and upstream's are taken. |
| 2026-10-20 | `08c978964` | `1f81aafa8` | R20: fix(storage): bounds-check disk array updates in release builds |
| 2026-10-20 | `95dbf5ed1` | `648d84891` | R20: fix(csv): remove shadowed variable in dialect detection |
| 2026-10-20 | `a1a25c32c` | `98afc0e63` | R20: fix(gds): emit single length-0 row when varlen source reaches itself |
| 2026-10-20 | `bf2a62edf` | `aa4d529cb` | R20: opt(count): grouped aggregate over variable-length paths without materializing walks. the optimizer.cpp hunk only extends the RIAG_DUMP_LOGICAL plan dumper, which this tree does not carry (dropped with 11a991bce in Wave 5), so it is left out. |
| 2026-10-20 | `da16e34ab` | `7980a5e16` | R20: fix(gds): gate the grouped walk-count rewrite on a full-table source |
| 2026-10-20 | `99ae4efe9` | `f1834fcbe` | R20: fix(review): path-node predicates, int64 count cap, prune-site contract |
| 2026-10-20 | `706acc2ca` | `a363beb56` | R20: fix(planner): range selectivity from observed column min/max |
| 2026-10-20 | `aa52d84c1` | `5ceb3e2cf` | R20: fix(storage): match member init order in ColumnStats copy ctor |
| 2026-10-20 | `366230602` | `0f393c95d` | R20: fix(posix): query F_GETLK before close and split lock error messages. upstream's fix for the PID diagnostic replaces ours from fix/file-lock-holder-pid; the contention message keeps this tree's "Another process may have the database open." instead of the docs.ladybugdb.com link. |
| 2026-10-20 | `4887e4465` | `d7507d919` | R20: fix(processor): keep the child's selection vector intact in hash join build. Upstream's fix for #1109; our planner-side fix `90a8b5b42` is reverted in `2de996fdf`, its test stays and passes |
| 2026-10-20 | `63649be19` | `5c4ea0efe` | R20: fix(opt): skip grouped reachable-count rewrite with rel predicate |
| 2026-10-20 | `a7ab0b81c` | `94c2d141e` | R20: fix(storage): clean stale spill file lazily after lock is held |
| 2026-10-20 | `d61fe1f47` | `223a690e6` | R20: fix(gds): emit single length-0 row when closed walks fail node predicate |
| 2026-10-20 | `6f134bed0` | `cfb362b41` | R20: fix(storage): drop unused selVector capture in ColumnStats::updateMinMax |
| 2026-10-20 | `f80e6bce5` | `9199d5a6a` | R20: fix(storage): keep rel scans on the CSR state they started with. conflict only in scanCommittedInMemSequential, where upstream's pinned in-memory groups replace the locked lookup. |
| 2026-10-20 | `18f04e4cd` | `525a23791` | R20: style(storage): format CSR scan checkpoint changes |
| 2026-10-20 | `0f97e3ebf` | `44446eb79` | R20: fix(storage): pin the live CSR group and keep pins across repeated checkpoints |
| 2026-10-20 | `29919de39` | `9f4c579f7` | R20: test(storage): keep the failed-checkpoint scan test off deleted in-memory rels |
| 2026-10-20 | `72934d338` | `2c277d0a8` | R20: fix(test): make vfs + checkpoint tests work on Windows |
| 2026-10-20 | `e17c531d1` | `483e4a2e5` | R20: style(test): join expectedNeedle to satisfy CI clang-format-18 |
| 2026-10-20 | `c99a20993` | `51719c617` | R20: fix(planner): slim scalar-seeded chain for correlated PK equalities |
| 2026-10-20 | `96dd51eb3` | `24b81a431` | R20: fix(planner): filter scalar-seeded residuals after leg properties attach |
| 2026-10-20 | `b48af7226` | `ba7e62bc0` | R20: fix(storage): pad uncompressed flush buffer to allocated page range on disk |
| 2026-10-20 | `b5e90ebbe` | `fbd9be066` | R20: fix(storage): clean unused var, run clang-format, and add ALP float test coverage |
| 2026-10-20 | `efe1fc846` | `b28670c80` | R20: fix(processor): don't re-materialize probe-side exprs as join payloads. Upstream's fix for #1121, the defect our `eb096064e` fixed; ours is reverted in `2c24dec8e`, its test stays and passes |
| 2026-10-20 | `6057fc561` | `eb497e16f` | R20: fix(processor): do not compact flat join keys' selection vectors on null |
| 2026-10-20 | `33ef97afe` | `bfede88f2` | R20: refactor(processor): restore the key selection vector inside appendVectors |
| 2026-10-20 | `d50f8cf51` | `1837a00a5` | R20: fix(planner): reject hash joins with no join condition |
| 2026-10-20 | `de3ac16a2` | `9c90c1599` | R20: refactor(binder): make the nested-aggregate check state its invariant |
| 2026-10-20 | `36e07a80d` | `8c08b9c4a` | R20: fix(wal): tag records with their owning catalog and route replay per owner |
| 2026-10-20 | `52c411220` | `64c5f7f71` | R20: test(transaction): cover graph-owned WAL record replay across owner catalogs |
| 2026-10-20 | `32b816584` | `78fbe29e8` | R20: fix(storage): silence unused tableID parameter in NodeTable::initScanState |
| 2026-10-20 | `26c489794` | `1e8abae97` | R20: fix(transaction): restore table-ID isUnCommitted/getLocalRowIdx overloads |
| 2026-10-20 | `527557fb8` | `6e62d90e3` | R20: fix(wal): translate UPDATE_SEQUENCE through replayed entry IDs |
| 2026-10-20 | `458be0e82` | `38944187c` | R20: fix(storage): make the graph registry shared lock reentrant |
| 2026-10-20 | `82d82152a` | `963f38c41` | R20: fix(wal): tag UPDATE_SEQUENCE records with the sequence name |
| 2026-10-20 | `d5a2f1d5f` | `86af959b6` | R20: fix(storage): skip undo version records for graphs dropped mid-transaction |
| 2026-10-20 | `9c59925e2` | `e45fca1ac` | R20: fix(wal): encode named UPDATE_SEQUENCE records as a distinct record type |
| 2026-10-20 | `d536229c7` | `431eaa27b` | R20: fix(storage): key graph registry recursion and liveness checks per manager |
| 2026-10-20 | `a8a43f64d` | `6ffae3da0` | R20: fix(wal): cover legacy sequence record replay with hand-written WAL bytes |
| 2026-10-20 | `6788c12d0` | `8b304ccc4` | R20: fix(storage): retire dropped graph catalogs after their records are applied |
| 2026-10-20 | `d65586682` | `c4032ad0a` | R20: fix(wal): record and translate ALTER connection endpoints across replay |
| 2026-10-20 | `0156c6406` | `7e22bf1a1` | R20: fix(wal): translate recovered rel-insert endpoints through replayed entry IDs |
| 2026-10-20 | `c8db54ec9` | `284cfd4e7` | R20: fix(wal): reject torn owner trailers instead of misrouting them |
| 2026-10-20 | `eb5f0725b` | `eb660a6a9` | R20: fix(storage): reject null bytes in graph names |
| 2026-10-20 | `a86f5f7a8` | `7d387332f` | R20: test(transaction): pin crafted recovery bytes for endpoints, torn trailers, and reopen |
| 2026-10-20 | `20fed58d9` | `8faf67992` | R20: fix(fs): resolve UTF-8 paths as Unicode on Windows |
| 2026-10-20 | `59b07bcba` | `012e631b8` | R20: fix(storage): version column min/max stats as STORAGE_VERSION_48. the version map keeps this tree's 0.21.0 entry (the first RiaGraph release, storage 47) and adds 0.22.0 -> 48; upstream's 0.21.1/0.21.2 entries are ladybug releases RiaGraph never shipped. |
| 2026-10-20 | `305d80e81` | `d6972de44` | R20: Bump version to 0.22.0 (dev). Not picked as such: our own bump, made after `59b07bcba` because storage version 48 is keyed to release 0.22.0 |
| 2026-10-20 | `3d1e00a0d` | `dbae8598c` | R20: fix(processor): null the current row when SET returns a NULL value (fixes #1136) |
| 2026-10-20 | `2a95cb45c` | `7cc45cc2d` | R20: test(processor): add mixed-label and null-expression coverage for SET return row |
| 2026-10-20 | `5943adc3e` | `f0249d670` | R20: test(processor): use typed NULL expressions and reset SET fixtures |
| 2026-10-20 | `dd4089846` | `6e647c87c` | R20: perf(wal): cache last owner catalog across replayed records |
| 2026-10-20 | `1c74be702` | `670af8eb0` | R20: style(wal): rewrap owner cache code for CI clang-format-18 |
| 2026-10-20 | `15a2314ab` | `92a7e8f89` | R20: fix(wal): clear the owner cache after inline graph checkpoint recovery |
| 2026-10-20 | `65cf1ac90` | `e42340c86` | R20: test(transaction): pin owner-cache invalidation against registry-mutating extension replay |
| 2026-10-20 | `bc5bc5ea6` | `6f3178c49` | R20: fix(wal): clear the owner name and failed-owner set beside the owner catalog |
| 2026-10-20 | `544f93a99` | `fc40d3a2f` | R20: feat(binder): fold arithmetic identities to expose index scans |
| 2026-10-20 | `35cf8db34` | `420070665` | R20: feat(function): add list_has_any for list overlap checks |
| 2026-10-20 | `1fb4f5517` | `aff8ef543` | R20: perf(transaction): resolve a node's local row with one local-table lookup |
| 2026-10-20 | `f4879c6ae` | `5f6f34f39` | R20: fix(planner): resolve segfault in mapExpressionsScan when enable_plan_optimizer is false (fixes #1142) |
| 2026-10-20 | `508ad1b5d` | `013638165` | R20: fix(planner): resolve correlated scans through factorization when optimizer is disabled |
| 2026-10-20 | `2b004c704` | `b4761efa1` | R20: fix(planner): skip list membership unnest and property seeded chain when optimizer is disabled |
| 2026-10-20 | `c10313fc9` | `bb63ba2f5` | R20: docs(optimizer): explain optimizer-disabled rewrites |
| 2026-10-20 | `72f0b7e29` | `da80c3eb1` | R20: fix(main): reset useInternalCatalogEntry on exception paths (fixes #1162) |
| 2026-10-20 | `0194bc9f3` | `99ec9f78d` | R20: feat(function): add list_min and list_max for list extrema |
| 2026-10-20 | `5585716d0` | `70c5b604f` | R20: fix(function): limit list_min/list_max null fallback to untyped NULL |
| 2026-10-20 | `63eb3589c` | `645fa6859` | R20: fix(catalog): look up replayed macro drops in the macros set (fixes #1163) |
| 2026-10-20 | `89b1f50cd` | `af193954a` | R20: fix(table): reset file-scan shared state on cached plan reuse (fixes #1161) |
| 2026-10-20 | `d0db25918` | `2125a4c40` | R20: fix(prepared): rebind cached plans when catalog version changes (fixes #1158) |
| 2026-10-20 | `a95c6ba0b` | `deefad34a` | R20: fix(wal): remove storage index holder when replaying DROP INDEX (fixes #1152) |
| 2026-10-20 | `0346153a0` | `fc6501af7` | R20: fix(catalog): guard columnIDs against checkpoint vacuum race (fixes #1160) |
| 2026-10-20 | `1f8bc2c57` | `29654057d` | R20: perf(binder): leave large constant lists unfolded |
| 2026-10-20 | `c3b1dfe94` | `2c9be67dc` | R20: fix(parquet): do not write field values for a NULL STRUCT |
| 2026-10-20 | `ee48fb6d7` | `713e9b5d8` | R20: refactor(parquet): narrow the struct position and note why it maps to the fields |
| 2026-10-20 | `415af47b4` | `ecafec923` | R20: fix(arrow): keep child arrays aligned for NULL nested values |
| 2026-10-20 | `5a9903197` | `89b558ea0` | R20: fix(arrow): keep offsets for a NULL UUID exported as text |
| 2026-10-20 | `9ec4b1d8c` | `f0ab14cf3` | R20: fix(storage): read ART primary-key indexes written by 0.17 |
| 2026-10-20 | `6116e5e26` | `e457147b8` | R20: fix(storage): bounds-check the 0.17 ART entry list before reading it |
| 2026-10-20 | `09239be32` | `91c81632a` | R20: fix: read fields of a NULL STRUCT as NULL |
| 2026-10-20 | `be5fbe12f` | `cb50976ee` | R20: perf(struct): null a NULL struct's fields when the struct is set NULL |
| 2026-10-20 | `d6223cf8b` | `d290c04cb` | R20: fix(struct): export StructVector::setNullFieldsOfNullStructs |
| 2026-10-20 | `02d1c0e62` | `7457def5f` | R20: fix(struct): keep a NULL union's tag and members as they are |
| 2026-10-20 | `a88df3c90` | `2de29c8ba` | R20: docs(struct): state the null contract between a struct and its fields |
| 2026-10-20 | `213d9036b` | `cb27149e1` | R20: storage: fix CSR is_sorted assertion on scan after failed checkpoint (#1140) |
| 2026-10-20 | `031458ff0` | `04cc7cc38` | R20: fix(binder): keep folding lists written out in the query |
| 2026-10-20 | `c2d8ac532` | `71970b47c` | R20: refactor(binder): list every physical type when sizing a folded constant |
| 2026-10-20 | `12ec99564` | `1a491efb1` | R20: test(binder): pin the fold limit and run an unfolded constant list |
| 2026-10-20 | `34042ac72` | `367f5729f` | R20: fix(storage): index checkpoint columns by position after a dropped column |
| 2026-10-20 | `b8a716bcf` | `a0c7241b3` | R20: fix(storage): keep checkpointed relationships when a failed commit rolls back |
| 2026-10-20 | `c924b7db8` | `e73b3a314` | R20: fix(wal): replay DROP_PROPERTY storage epoch bump on crash recovery |
| 2026-10-20 | `cee11c608` | `d481941d7` | R20: test(wal): label endpoints in RelDropPropertyReplayedFromWALThenCheckpointThenAdd |
| 2026-10-20 | `fdeab08e7` | `f66d8ca4d` | R20: fix(parquet): fail loudly on INT128 and microsecond-precision INTERVAL export |
| 2026-10-20 | `59a512b60` | `4821c1444` | R20: fix(parquet): adapt export tests to loud INT128/microsecond-INTERVAL rejection |
| 2026-10-20 | `94b1700f1` | `7b6386653` | R20: fix(pk-index): probe legacy sub-index for stale non-ASCII STRING PKs. Replaces our #1092 follow-up `c0fd8b8c1` (engine part reverted in `f3fae662f`; the 0.20.2 fixture test stays and passes). Our `b5a8017d4` stays: upstream's legacy hash still goes through plain char |
| 2026-10-20 | `0003997da` | `4c6abfe59` | R20: fix(partition): rebuild partitioned tables on crash WAL replay (fixes #1186). version map: 0.22.0 -> 49 as upstream, keeping this tree's 0.21.0 entry. |
| 2026-10-20 | `39202dba7` | `13ff46595` | R20: fix(storage): route PK index slot writes through the shadow file |
| 2026-10-20 | `b02a6e31b` | `300a6db9a` | R20: fix(optimizer): keep all operands and respect NULL under NOT in bool folding |
| 2026-10-20 | `cf236ad5d` | `6131e0147` | R20: fix(test): link registry-mutating test extension against riag_shared on Windows |
| 2026-10-20 | `feb6f2fd2` | `378216462` | R20: chore(submodules): update submodules to latest main |
| 2026-10-20 | `e698d1e67` | `94cf608b4` | R20: fix(gql): resolve current-graph ANY check via exported isDefaultGraphAny |
| 2026-10-20 | `1b95b5b32` | `a22f4ec50` | R20: fix(core): export LogicalType::containsAny for extensions |
| 2026-10-20 | `3fa882054` | `501f420a2` | R20: fix(planner): dedupe rel internal ID in recursive path probe pipeline. takes upstream's block whole, including the older foreign-rel _ID guard this tree did not have. |
| 2026-10-20 | `49e313ffe` | `116c922f5` | R20: fix(any): make multi-hop path materialization work on ANY graphs |
| 2026-10-20 | `21c85a886` | `7e2bbda5b` | R20: test(any): update whole-rel expectation for _ID rename |
| 2026-10-20 | `5229e794c` | `64bc47aba` | R20: fix(txn): wait for pre-existing read txns before checkpoint resets versions |
| 2026-10-20 | `556e81083` | `9d56420da` | R20: fix(txn): retain MVCC versions across checkpoint instead of waiting on readers |
| 2026-10-20 | `2e96fd3da` | `10caffbc3` | R20: fix(txn): avoid unused structured binding in horizon recompute |
| 2026-10-20 | `a872b3078` | `b56863408` | R20: fix(storage): compare against the segment span end in base-value reads |
| 2026-10-20 | `7914542a7` | `2b60f535d` | R20: fix(storage): harden retention paths found by runtime checks |
| 2026-10-20 | `6d3718a57` | `bdcf3521e` | R20: fix(txn): collapse retained deletes on the wire; reclaim retention on unpinned checkpoints |
| 2026-10-20 | `1215f36ef` | `86c9966b7` | R20: fix(txn): retain pre-checkpoint CSR snapshots for parked readers |
| 2026-10-20 | `ef58de2b2` | `08d5dc8fd` | R20: fix(txn): pin WAL retirement on the snapshot horizon with pin observability |
| 2026-10-20 | `ed1b28860` | `b152687ca` | R20: fix(recovery): tolerate missing shadow for pinned active-WAL checkpoints |
| 2026-10-20 | `669f43d47` | `587186184` | R20: fix(recovery): replay the suffix after the last CHECKPOINT in active and frozen WALs |
| 2026-10-20 | `b8d9babce` | `c902bf25c` | R20: fix(txn): drop stderr pin-observability log from pinned checkpoints |
| 2026-10-20 | `0ba61c4a9` | `56747ed50` | R20: fix(core): clean up attached-database state on DETACH |
| 2026-10-20 | `5d124791d` | `ee3185e87` | R20: perf(optimizer): find complementary operands in hash sets |
| 2026-10-20 | `d0e671045` | `ee7e5f3f6` | R20: fix(optimizer): flatten nested ORs before folding |
| 2026-10-20 | `be364eece` | `90ae4e9b7` | R20: fix(optimizer): cancel a double negation that folding produces |
| 2026-10-20 | `8432c9556` | `0b09b55ad` | R20: fix(optimizer): flatten an AND or OR that folding produces |
| 2026-10-20 | `bd56b1349` | `a07986704` | R20: fix(binder): give each random() and gen_random_uuid() call its own column |
| 2026-10-20 | `a39c873bb` | `7d67b6b6a` | R20: perf(function): compile a regexp pattern once while it stays the same |
| 2026-10-20 | `0f26809ef` | `340ac6af9` | R20: fix(binder): don't re-evaluate a WITH aggregate referenced inside a CASE |
| 2026-10-20 | `93daf6cb6` | `00027372d` | R20: fix(processor): count only relationships the transaction can see |
| 2026-10-20 | `1b6f4707e` | `ddfc6050f` | R20: chore(deps): bump extension submodule for SQLite CDC stack |
| 2026-10-20 | `a65f28051` | `866754060` | R20: fix(core): export attached-database enumeration for extensions |
| 2026-10-20 | `77b262145` | `7d269f70c` | R20: fix(txn): close horizon gap for readers beginning during checkpoint |
| 2026-10-20 | `a89c7a922` | `c7867ed44` | R20: fix(storage): retain CSR snapshots for parked readers despite in-flight scans |
| 2026-10-20 | `b73d8a55e` | `1d92924eb` | R20: fix(recovery): require pinned-checkpoint marker before treating missing shadow as durable |
| 2026-10-20 | `cf864ec62` | `2c08639be` | R20: docs(main): pin ClientContext::interrupt() as async-signal-safe |
| 2026-10-20 | `e66ac38ad` | `5c3ba0136` | R20: perf(processor): copy Intersect's right-hand lists into a buffer sized once |
| 2026-10-20 | `0052cace3` | `585fb670a` | R20: fix(function): ignore NULL list elements in list_contains/list_position |
| 2026-10-20 | `1ecca0a96` | `4bad8122f` | R20: test(function): pin NULL list elements in the other ListPosition callers |
| 2026-10-20 | `e61bf5fff` | `72e67625e` | R20: docs(function): note that IN over a list follows list_contains semantics |
| 2026-10-20 | `9b0411453` | `97446ab1e` | R20: fix(binder): let ORDER BY name an aliased item by its expression after aggregation |
| 2026-10-20 | `8172e8561` | `51886f276` | R20: test(binder): pin ORDER BY random() after an aggregate |
| 2026-10-20 | `7bd282551` | `8fd31ce55` | R20: fix(processor): count rels from the node group a scan would read |
| 2026-10-20 | `9be3ae4bf` | `bf91677b9` | R20: fix(main): rebind cached statements after a checkpoint renumbers columns |
| 2026-10-20 | `6b0c00690` | `7708b284c` | R20: perf(storage): reuse freed node-group-sized buffers across queries |
| 2026-10-20 | `9f6ba90eb` | `130ff9add` | R20: perf(storage): sum rel degrees in one pass instead of a hash map. rel_degree_test.cpp joins this tree's rel_tests target, which also carries rel_checkpoint_test.cpp. |
| 2026-10-20 | `4a5168160` | `4a1318aa3` | R20: docs: list the remaining make build options and how to build like a release |
| 2026-10-20 | `631ccd6a9` | `e8fee4af3` | R20: perf(processor): grow Intersect's right-hand buffer instead of trusting its size |
| 2026-10-20 | `1f865e163` | `67fa93802` | R20: fix(processor): subtract rel deletions that sit behind CSR gaps from count(*) |
| 2026-10-20 | `8e9f36f5a` | `b736418bc` | R20: perf(storage): apply a sparse semi-mask before a node scan reads its columns |
| 2026-10-20 | `e642479c2` | `7d8af9c7e` | R20: fix(core): 'Projected graph does not exists' typo |
| 2026-10-20 | `a0def4fd7` | `ded6e9b9c` | R20: fix(core): require explicit label for multi-table bare CREATE |
| 2026-10-20 | `165b2664b` | `e1cb78fc9` | R20: fix(core): named and typed function parameters in signatures |
| 2026-10-20 | `98acb269f` | `cb607971b` | R20: fix(storage): make DiskArray WriteIterator exception-safe during checkpoint |
| 2026-10-20 | `4cac74ec9` | `a2997beed` | R20: fix(sanitizers): resolve ASan + TSan nightly failures |
| 2026-10-20 | `0fda440d3` | `208584408` | R20: fix(build): add /bigobj for MSVC to fix C1128 on checkpoint_test |
| 2026-10-20 | `b8a29e1bd` | `0e810d6f6` | R20: perf(main): reuse the plan of a statement prepared without parameter values |
| 2026-10-20 | `e3168001a` | `ace999ab6` | R20: perf(storage): decode only the CSR header span a rel scan's bound nodes need. Replaces our #1117 fix `4da7881f6` (engine part reverted in `028a1b533`; its test stays and passes) |
| 2026-10-20 | `9d85744b4` | `1b99bb78a` | R20: perf(storage): decide the early semi-mask per batch |
| 2026-10-20 | `f003884aa` | `e44fa44bf` | R20: chore(storage): pin MemoryManager's buffer-reuse invariants |
| 2026-10-20 | `fc57b0129` | `67a66697c` | R20: fix(build): pin OpenSSL to 3.x and fix httplib for OpenSSL 4 |
| 2026-10-20 | `25fbbdf4f` | `41c423e12` | R20: fix(ci): pin macOS precompiled-bin builds to OpenSSL 3 |
| 2026-10-20 | `e9a45da8e` | `06050638a` | R20: fix(ci): pin macOS Java build to OpenSSL 3 |
| 2026-10-20 | `57c7e256b` | `2bda5f11e` | R20: fix(ci): pin macOS Node.js and Python wheel builds to OpenSSL 3 |
| 2026-10-20 | `11ea45e65` | `bce9cb985` | R20: Revert "pin macOS Python wheel builds to OpenSSL 3" |
| 2026-10-20 | `a7e923e96` | `2d4314f8d` | R20: fix(mapper): fall back from packed filtered count when a predicate input is computed |
| 2026-10-20 | `49f892b03` | `c278c905c` | R20: fix(planner): give a re-evaluated expression its own slot in the group |
| 2026-10-20 | `66f3544fc` | `2a01f1753` | R20: fix(storage): keep casts that change stats layout out of zone-map predicates |
| 2026-10-20 | `eb660b493` | `5b37634c6` | R20: feat(planner): lookup join for inner equi-joins on node ID |
| 2026-10-20 | `4ac7b3d86` | `e63055d58` | R20: fix(planner): allow lookup join to reuse bound node ID schema |
| 2026-10-20 | `654c48ccf` | `39b955d6f` | R20: fix(shell-test): use up-arrow for row movement in prev_history_multiline |
| 2026-10-20 | `63af99164` | `43282e851` | R20: fix(planner): cost-competitive lookup join; fix parquet export of flat results |
| 2026-10-20 | `af9a99fdc` | `9484a3692` | R20: fix(binder): keep a variable's name when an arithmetic identity returns it |
| 2026-10-20 | `c36467ff0` | `be0eed798` | R20: fix(binder): give a new aggregate its own name when an earlier clause has it |
| 2026-10-20 | `aab321bdb` | `1d6c8dce0` | R20: fix(storage): read CSR length via Column in getDegreeForOffset |
| 2026-10-20 | `a3721ccee` | `290e24d76` | R20: perf(processor): remove per-execute mallocs on prepared-statement fast path |
| 2026-10-20 | `2a642f5cb` | `8ce7249b7` | R20: perf(processor): pool cloned physical plans per thread on the fast path |
| 2026-10-20 | `a2c5bce10` | `e09d41a91` | R20: perf(processor): make operator init paths idempotent plus SFO-bound chunk functors |
| 2026-10-20 | `65a8894f9` | `a0d2dd8b9` | R20: fix(processor): clear pooled-plan state on rebind plus missed operator resets |
| 2026-10-20 | `d9967f563` | `61f13a199` | R20: fix(processor): harden pooled-plan reuse for review |
| 2026-10-20 | `77b6a349a` | `b13cca9e8` | R20: feat(processor): make physical plan pool depth configurable |
| 2026-10-20 | `16df8bfe2` | `1eb1178eb` | R20: perf(processor): zero factorized table blocks as tuples are claimed |
| 2026-10-20 | `48e6c98bf` | `9f9a9bed3` | R20: refactor(storage): count a native rel table's rels in RelTable |
| 2026-10-20 | `b9f0f3a84` | `b7ca2dc12` | R20: fix(storage): count only the rels a transaction can see in CSR degrees and count(*) |
| 2026-10-20 | `6087b4b42` | `cd8e24f29` | R20: fix(types): format negative decimals with fewer digits than scale |
| 2026-10-20 | `9a23a4778` | `4d58e809e` | R20: perf(storage): filter on cheap scan predicates before reading strings and lists |
| 2026-10-20 | `9f64325` | `590d80b` (python_api) | R20: fix(pybind): load each import-cache entry once, under a lock. Renamed with `--profile python` |
| 2026-10-20 | `ec0be15` | `7b0c509` (python_api) | R20: chore(deps): fix uv audit vulnerabilities. Renamed with `--profile python` |
| 2026-10-20 | `97acee8` | `67cd42d` (python_api) | R20: fix(async): propagate cancellation and interrupt only the cancelled query. Renamed with `--profile python` |
| 2026-10-20 | `429a9d7` | `5f3a521` (python_api) | R20: test: regression test for LadybugDB/ladybug#1067 ORDER BY STRING then numeric. Renamed with `--profile python` |
| 2026-10-20 | `60d8c75` | `1477a40` (python_api) | R20: fix(connection): make close() safe while a query is running. Renamed with `--profile python` |
| 2026-10-20 | `c852805` | `5b49171` (python_api) | R20: fix(connection): make close() idempotent and abort-safe. Renamed with `--profile python` |
| 2026-10-20 | `9094f23` | `26fb425` (nodejs_api) | R20: fix: make un-awaited getAll() fail loudly instead of yielding {}. Renamed with `--profile nodejs` |
| 2026-10-20 | `a5c78ee` | `8472502` (extension) | R20: Update icebug to 13.4. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `056fabf` | `00d9b3b` (extension) | R20: chore: copy the LICENSE from main repo. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `98bbb9d` | `0c045a6` (extension) | R20: Iceberg/Unity Catalog ATTACH support and SQL pushdown generalization. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `4154fda` | `f378866` (extension) | R20: Address review findings: tiered column lookup, connector dedup, hermetic pushdown test. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `318f59d` | `9cdb2d9` (extension) | R20: Add DuckLake storage extension with SQL pushdown support. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `3307c12` | `b59ded4` (extension) | R20: CI: build the ducklake extension. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `cfb0501` | `b62cfce` (extension) | R20: Fix shadow-table scan link and add SQL pushdown coverage. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `2d5f07a` | `3b39313` (extension) | R20: fix attaching an Iceberg database through a REST catalog. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `bf74ab6` | `eb66019` (extension) | R20: Alias hf:// to xet:// in httpfs. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `1123fb1` | `d35c2a3` (extension) | R20: adbc: add CATALOG support with qualified scans and schema probe fallback. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `b0488d3` | `741045e` (extension) | R20: feat(gql): TCK-verified transformer rework (161 dual-run cases, opengql/tck 190/206). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `a43038f` | `198907c` (extension) | R20: chore(gql): ignore generated e2e_test case index (test_list). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `560ea13` | `6bb8845` (extension) | R20: fix(gql): review follow-ups part 2 — new dual-run pins (is_simple guard, exact-decimal min/max, long-number sortkey, to_json non-finite, delimited idents). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `c2ba67c` | `6c294d2` (extension) | R20: fix(gql): review follow-ups part 3 — harness integrity hard gates (no green-on-total-failure), portable e2e discovery, REPORT links opengql/tck#9. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `6d243aa` | `7d86953` (extension) | R20: fix(gql): review follow-ups part 3 — harness integrity hard gates (no green-on-total-failure), portable e2e discovery, REPORT links opengql/tck#9. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `01b0920` | `1b57ee2` (extension) | R20: fix(gql): review follow-ups part 4 — exact decimal min/max (compareDecimalTexts), uncapped self-delimiting sortkey, non-finite to_json guard. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `4dba4d1` | `7020a61` (extension) | R20: chore(gql): drop generated e2e case index (test_list) from the tree — stale machine-local cache, runner regenerates it. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `3b8b9e0` | `91b6445` (extension) | R20: fix(gql): review follow-ups part 5 — delimited-identifier funnel at the Transform exit. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `571333a` | `a64800b` (extension) | R20: fix(gql): complete review-followup tree — full gql_transformer.cpp (delimited-ident funnel), byte-exact copies of run_tck/REPORT/json_functions. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `e145715` | `ec89642` (extension) | R20: fix(fts): snapshot FTS aux info in bind data (Fixes ladybugdb/ladybug#1105). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `2b9d5f4` | `3faff60` (extension) | R20: fix(fts): invalidate cached FTS plan on index drop/recreate. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `a06852c` | `720cb57` (extension) | R20: fix(vector): use owner-scoped transaction overloads for embedding scans. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `4a4c15e` | `72b7925` (extension) | R20: fix(vector): read on-disk embeddings through the scan state's transaction. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `47699df` | `8f1ce5f` (extension) | R20: fix(neo4j): fix NEO4J_MIGRATE crash on multi-label nodes. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `115551e` | `ea56209` (extension) | R20: fix(fts): cache jieba instances process-wide to avoid per-query dict reload (fixes #98). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `d65b06c` | `bac1857` (extension) | R20: docs(vector): note QUERY_VECTOR_INDEX rows are unordered without ORDER BY distance. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `6ac5453` | `aa86829` (extension) | R20: fix(gql): avoid dangling-reference in path node ID layout check. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `036c1d9` | `439812d` (extension) | R20: fix(gql): use DatabaseManager::isDefaultGraphAny for current-graph check. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `459c4cc` | `8a20f71` (extension) | R20: feat(sqlite): read SQLite files through the native C API instead of DuckDB. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `fabfd1e` | `0f8594b` (extension) | R20: feat(sqlite): WAL tailer + b-tree page decoder for log-based CDC (phase 1). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `5c332eb` | `905bc9a` (extension) | R20: feat(sqlite): op resolver turning WAL commits into row ops (CDC phase 2). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `033ab49` | `38882da` (extension) | R20: feat(sqlite): convention mapping + Cypher builder for CDC apply (phase 3a)\n\nSync mapping follows pg_ladybug register_node/register_edge semantics:\nlabel <-> table, id column, endpoint columns, props. node_/rel_/csr_rel_\nnaming auto-populates the catalog; explicit register_* calls (3b)\noverride it. Unprefixed tables stay foreign-scanned; PK-less, WITHOUT\nROWID, and FK-less edge tables skip with reasons. Edge endpoints\nresolve through FK metadata + src/dst convention to mapped nodes.\n\nStatement builder emits CREATE ... IF NOT EXISTS DDL, UNWIND-batched\nMERGE/DELETE DML, and the cursor write. Strict value rendering\nmirrors scan enforcement (structural mismatches are builder errors;\nbad date strings pass through to fail at execution with context).\nDeletes carry resolver-retained PK identities for explicit-PK tables.\n\n13 pure-string unit tests; resolver extended with PK identity\nretention (explicit-PK delete test added). 27/27 CDC tests green.. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `74286bd` | `3eafea5` (extension) | R20: feat(sqlite): sync engine executing WAL replication into native tables (phase 3b). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `5b4ab21` | `f6b4e74` (extension) | R20: docs(sqlite): polish CDC design doc to match the built system. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `8874aa9` | `ef8478b` (extension) | R20: feat(sqlite): background sync daemon (no polling loop required). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `67a65e7` | `d60b570` (extension) | R20: fix(sqlite): uint32 cell offsets to satisfy -Werror=sign-compare. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `1d32ada` | `6e2edfd` (extension) | R20: fix(fts): make index lifecycle tolerant to interrupted CREATE/DROP (fixes #77). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `d422aed` | `5d0e0a0` (extension) | R20: fix(fts): schema-confirm residue + preserve user tables on failed CREATE (fixes #77). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `7fefc91` | `fd6a5cc` (extension) | R20: fix(fts): keep the case of ignore_pattern. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `5065115` | `ea1813e` (extension) | R20: fix(fts): count query terms, not dictionary terms, in conjunctive queries. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `d0b6896` | `45f8e15` (extension) | R20: fix(fts): skip rows COPY rejects with ignore_errors. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `e539827` | `7154d31` (extension) | R20: fix(sqlite): use std::memcpy for MSVC compat in btree decoder. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `88020fd` | `9ced2d4` (extension) | R20: fix(algo): name projected-graph parameter in GDS signatures. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `6d33f60` | `ab0c608` (extension) | R20: fix(algo): declare PPR sources as typed LIST(INT64). Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `c1a5048` | `b6fbbe7` (extension) | R20: Fix LogicalType vector init: use moves not initializer-list copy. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `d275ccd` | `894fef1` (extension) | R20: fix(algo): infer nested input types for GDS extra args. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `62084ce` | `d9915ef` (extension) | R20: algo: table-driven GDS registry for icebug algorithms. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `2f9839a` | `fb13843` (extension) | R20: fix(algo): add template disambiguator for macOS Clang build. Applied untransformed (Phase 6 rename not done; not build-verified) |
| 2026-10-20 | `3d06a5393` | - | R20: not picked: the merged form of open PR #1108, already in as `bdae3dc6e` + `c9183e082` (byte-identical) |
| 2026-10-20 | `20e26f419`, `520de542c` | - | R20: not picked: the merged forms of PRs #1114 and #1118, already in as `14883ac99` and `e934252b4` (no diff left) |
| 2026-10-20 | `c064a5e38`, `60412357d` | - | R20: submodule-only commits; the gitlinks are set by our own pointer commit (see the binding and extension rows below) |
| 2026-10-27 | `333662813` | - | R26: not picked: removed in the final pass: CI-only (Windows Chocolatey and DuckDB download retries); cannot be verified without a CI run |
| 2026-10-27 | `2edcb3fe3` | - | R26: not picked: removed in the final pass: CI-only (Arrow 26 pin for the Linux extension builds); cannot be verified without a CI run and an extension build |
| 2026-10-27 | `c68c231c1` | `47dc83d5d` | R26: perf(processor): run a one-thread task on the calling thread |
| 2026-10-27 | `60443ffa0` | `719b5d565` | R26: perf(processor): new worker thread inherits the caller's slot |
| 2026-10-27 | `f8fe073ff` | `871ca1867` | R26: perf(binder): count a node's primary key as its internal id |
| 2026-10-27 | `6bb6ac488` | `1c40e82ce` | R26: test(optimizer): count the lookup join's properties as reads in the count tests |
| 2026-10-27 | `e946fe6d0` | `a765688c6` | R26: fix(processor): rebind cached plans after dropping the built-in PK index |
| 2026-10-27 | `da10d19d4` | - | R26: not picked: removed in the final pass: CI (DuckDB 2.0.0 assets) plus the core AttachedDatabase::executeQuery return type that only DuckDB 2.0.0 extensions need; neither can be verified without a CI run and a renamed extension build (pairs with extension 119912e, deferred) |
| 2026-10-27 | `b043723dd` | `0a41756b1` | R26: perf(parser): append sorted intervals without scanning the set. RiaGraph pick of upstream b043723dd: test registration merged next to our timer_test.cpp |
| 2026-10-27 | `6818e1733` | `8832fb399` | R26: fix(parser): check the appended interval without adjacent() |
| 2026-10-27 | `8c12ff299` | `c86ba152d` | R26: chore(third_party): track the ANTLR runtime patches as a port |
| 2026-10-27 | `0df764758` | `786993ec3` | R26: fix(graph): skip deleted rows in on-disk vertex scan chunks |
| 2026-10-27 | `0fe816e99` | `9bb8eab9f` | R26: fix(copy): rollback rel COPY without dropping committed rows (#1227) |
| 2026-10-27 | `64fa22511` | `0a8d218c5` | R26: test(copy): keep rel COPY endpoints within seeded node ids |
| 2026-10-27 | `3cc52b37c` | `7436d69b5` | R26: fix(copy): defer rel COPY flush in manual transactions (#1227) |
| 2026-10-27 | `1e320795d` | `32a526de6` | R26: style(copy): clang-format rel_batch_insert |
| 2026-10-27 | `9c26b0539` | `2805d207e` | R26: fix(copy): use TransactionContext pointer from Get() |
| 2026-10-27 | `f6e0963bb` | `9bc3c96b1` | R26: fix(copy): avoid gap rows when deferring new-group flush |
| 2026-10-27 | `5e519acdd` | `89995d3df` | R26: fix(copy): clear stale in-mem CSR state on ROLLBACK |
| 2026-10-27 | `74292dd62` | `ed036325d` | R26: test(copy): keep checkpoint enabled in rel COPY rollback regression |
| 2026-10-27 | `b630237bd` | `a2241bdbb` | R26: fix(processor): zero tuples handed out again after a shrinking resize |
| 2026-10-27 | `6af4adf2a` | `aa944a296` | R26: perf(storage): pause the early scan filter on dense in-memory batches |
| 2026-10-27 | `dc7629236` | `b2d31a4d9` | R26: fix(catalog): uppercase macro names before looking them up |
| 2026-10-27 | `921186b89` | `86163a6f1` | R26: fix(ddl): return from DROP MACRO IF EXISTS when the macro is missing |
| 2026-10-27 | `56a7ea57b` | `41db9a9e5` | R26: fix(c_api): cast mixed-width integers to the common type of a list or map |
| 2026-10-27 | `748bd6a08` | `0c1535cb4` | R26: fix(c_api): keep SERIAL element types and test every integer pair |
| 2026-10-27 | `8f167dc5d` | `a19e7f470` | R26: fix(storage): guard the sequential CSR index read in countVisibleInMemRows |
| 2026-10-27 | `3d83f61dd` | `a704d7ff5` | R26: perf(storage): let rel degree fast paths skip deletes a reader cannot see |
| 2026-10-27 | `8dfca463b` | `051edc762` | R26: fix(storage): copy a pinned CSR index before appending rels to it |
| 2026-10-27 | `e9a9fd129` | `783b29572` | R26: fix(storage): keep rel scans from reading table columns while a checkpoint replaces them |
| 2026-10-27 | `f2e3b5526` | `a9efcfc8d` | R26: test(storage): start table scans while checkpoint and ADD change the column vectors. RiaGraph pick of upstream f2e3b5526: test registration merged into the single rel_tests target (ours rel_checkpoint_test.cpp kept, table_columns_concurrency_test.cpp added) |
| 2026-10-27 | `ab187f34d` | `2dd9f6bff` | R26: test(storage): compare the scan results as text in the column race tests |
| 2026-10-27 | `a324a0a10` | `0a7566b05` | R26: fix(storage): stop multi-hop scans losing rels after a node with no rel node group |
| 2026-10-27 | `bdd271c00` | `90f87439c` | R26: test: rel scans after a node past the rel table's last node group |
| 2026-10-27 | `a6dc670ba` | `80ff32282` | R26: perf(storage): decode sparse CSR headers in clusters and bound the neighbour refill |
| 2026-10-27 | `319bb9214` | `42f620753` | R26: fix(storage): check cluster coverage in release builds |
| 2026-10-27 | `b3e8133e8` | `60e1271f0` | R26: fix(storage): give the CHECKPOINT retention lookup a memory manager |
| 2026-10-27 | `b3a219ee0` | `a75d64b70` | R26: test(storage): pin how much of the CSR header a rel scan decodes |
| 2026-10-27 | `9f53dd9d4` | `df1f8f9d6` | R26: fix(parser): preserve negative sign in integer and double literal raw names (fixes #1346) |
| 2026-10-27 | `071d2d1dc` | `e59b1a033` | R26: fix(test): correct table_info column name and double formatting in showMacroWithNegativeLiteral |
| 2026-10-27 | `6cc85b23c` | `24b202b06` | R26: fix(export): preserve column default expressions in exported cypher schema (#1347) |
| 2026-10-27 | `c81ec66c6` | `e79c2f83b` | R26: test(export): cover negative, rel and ALTER ADD defaults in export round-trip |
| 2026-10-27 | `e6fc1c3cc` | `8ef13833a` | R26: fix(copy): populate secondary indexes during node batch insert |
| 2026-10-27 | `52e7f521a` | `94dd0e73b` | R26: fix(test): avoid binder alias collision in CreateSecondaryArtIndex COPY |
| 2026-10-27 | `54b89aee3` | - | R26: not picked: removed in the final pass: its regression test ice_disk_list_property_skip needs the icebug-disk CSR EXPORT (upstream #916, not taken) and fails here (Cannot open file .../export/nodes_nodes.parquet) |
| 2026-10-27 | `9e3c61961` | `d7a483a78` | R26: fix(optimizer): keep the join that drops rows an OPTIONAL MATCH left NULL |
| 2026-10-27 | `f59eeeccc` | `0e1ebf19b` | R26: test(optimizer): pin re-matching a node after chained OPTIONAL MATCHes |
| 2026-10-27 | `6135986af` | `0dad78288` | R26: test(optimizer): pin a labelled MATCH on a node OPTIONAL MATCH left NULL |
| 2026-10-27 | `09aff4df1` | `a1da5d74f` | R26: perf(planner): return a hash join's conditions by const reference |
| 2026-10-27 | `a8739f514` | `1082fe2d1` | R26: style(optimizer): say why the NULL-key walk passes other operators through |
| 2026-10-27 | `7bcc47d3b` | `35d4950fb` | R26: fix(planner): cap a lookup join by its flat outer rows |
| 2026-10-27 | `339bd6dda` | `cb66f4587` | R26: perf(processor): reuse Intersect's per-probe scratch buffers |
| 2026-10-27 | `b14e2f88e` | `3a7ed18d4` | R26: perf(optimizer): mask Intersect's probe scans with its build-side keys |
| 2026-10-27 | `250988482` | `292eaac09` | R26: test(copy): add reproducer for COPY FROM subquery reusing a variable |
| 2026-10-27 | `134dcda8c` | `842bc29d0` | R26: fix(binder): map COPY FROM subquery columns by position |
| 2026-10-27 | `72838e6b7` | `5d01944d5` | R26: fix(function): route scalar list-slice through STRING implicit cast. Upstream adds no regression test; verified with the commit message's reproducers, which fail before the pick and pass after it |
| 2026-10-27 | `d348c6cf7` | `f9f3dc0d4` | R26: fix(function): throw BinderException for non-path LENGTH() input. Upstream adds no regression test; verified with the commit message's reproducers, which fail before the pick and pass after it |
| 2026-10-27 | `4ca22377b` | `2aae5db15` | R26: fix(storage): widen both zone-map bounds when a write exceeds both |
| 2026-10-27 | `7a088fe33` | `27aefc7a9` | R26: fix(optimizer): keep writes under a filter that folds to false |
| 2026-10-27 | `7fc04d41c` | `fece08ac1` | R26: fix(optimizer): remove a primary-key DISTINCT before placing flattens |
| 2026-10-27 | `caedb52ae` | `1e5093bc5` | R26: chore(optimizer): assert that the DISTINCT removal pass gets a flat plan |
| 2026-10-27 | `377569d43` | `d49b6d5a3` | R26: test(optimizer): pin that a removed primary-key DISTINCT leaves no FLATTEN |
| 2026-10-27 | `5a3799868` | `a2415aa36` | R26: fix(storage): don't claim a page group the size cap refused |
| 2026-10-27 | `25284c870`, `e7f681670`, `fd63804b0`, `b9db31207` | - | R26: submodule-only commits; the python_api gitlink is set by our own pointer commit, the extension gitlink stays (see the extension rows below) |
| 2026-10-27 | `e4254c41a` | - | R26: not picked: extension version bump (LBUG_EXTENSION_VERSION 0.21.0 to 0.22.0); RiaGraph keeps its own `RIAG_EXTENSION_VERSION` (0.20.0), to be settled with the release automation |
| 2026-10-27 | `4f6137953` | - | R26: not picked: CI-speed change (-g1 Linux test build, ccache saving) that no taken commit needs |
| 2026-10-27 | `7bcc788` | `6187190` (python_api) | R26: fix(backend): report why the _riag extension failed to load (#72). Renamed with `--profile python` |
| 2026-10-27 | `db8604b` | `a17a6c9` (python_api) | R26: fix(pybind): let Ctrl-C interrupt a running query. Renamed with `--profile python` |
| 2026-10-27 | `5b5f991` | `0f8125e` (python_api) | R26: fix(pybind): keep the connection alive across a Ctrl-C guarded call. Renamed with `--profile python` |
| 2026-10-27 | `0738739` | `d9b9bac` (python_api) | R26: fix(pybind): wait for in-flight SIGINT handlers before releasing the context. Renamed with `--profile python` |
| 2026-10-27 | `d1d0462` | `d7534c5` (python_api) | R26: fix(connection): don't block in set_query_timeout() while a query runs. Renamed with `--profile python` |
| 2026-10-27 | `a04832b` | `4483ab4` (python_api) | R26: fix(connection): validate settings in the setter and keep one timeout value. Renamed with `--profile python` |
| 2026-10-27 | `5d07d29` | `b9cff59` (python_api) | R26: fix(connection): keep the UDF close() error when the UDF runs on the query thread. Renamed with `--profile python` |
| 2026-10-27 | `49837b1` | `1bca362` (python_api) | R26: test(capi): mark the folded-UDF close() scenario as a known C-API failure. Renamed with `--profile python` |
| 2026-10-27 | `2e1adc0` | `fd45d79` (python_api) | R26: test(timeout): give the settings tests' database the suite's 1 GiB max_db_size. Renamed with `--profile python` |
| 2026-10-27 | `26167b0`, `0bee921`, `882fa79`, `2b77cfd`, `2ff8fee`, `913b6b0`, `1a1f735`, `9e20064`, `21ecd58`, `14af298`, `119912e` | - | R26: deferred (extension): fixes behind upstream's extension bumps (FTS lifecycle, quoting and escaping; Arrow 26 and DuckDB 2.0.0 CI). Cherry-picked on RiaGraphExtension `deferred/picks-2026-10-26`, but the extension is not renamed or built yet (Phase 6), so none of them can be verified; the extension gitlink stays at `fb1384326` |
| 2026-10-27 | `408431a` | - | R26: not picked (extension): feat(llm): add AI_EXTRACT_RECORD for schema-driven structured extraction; feature (not in scope) |
| 2026-10-27 | `14e0572` | - | R26: not picked (extension): test(llm): assert NULL struct via IS NULL in ai_extract_record test; test of feature 408431a (AI_EXTRACT_RECORD, not in scope) |
| 2026-10-27 | `d5b78bc` | - | R26: not picked (extension): fix(llm): validate endpoint at bind; NULL mistyped numerics in AI_EXTRACT_RECORD; only touches ai_extract_record.cpp, which exists only with feature 408431a |
| 2026-10-27 | `802a1ee` | - | R26: not picked (extension): chore(algo): bump icebug prebuilt to 13.5; icebug 13.5 prebuilt bump; needed only by feature 06ff834 (GDS_PARALLEL_CORE_DECOMPOSITION) |
| 2026-10-27 | `06ff834` | - | R26: not picked (extension): feat(algo): wire ParallelCoreDecomposition as GDS_PARALLEL_CORE_DECOMPOSITION; feature (not in scope) |
