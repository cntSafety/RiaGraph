# RiaGraph Fork — Renaming & Reconfiguration Guide

**Status:** authoritative plan. Read this in full before touching any file.
**Audience:** coding agents executing the fork of `ladybugdb/ladybug` into `cntSafety/RiaGraph`.
**Rule:** this document is the single source of truth for every naming decision. If something is
not covered here, it is an *open item* — add it to §16 rather than inventing an answer.

---

## 0. Decision register (locked — do not re-litigate)

| # | Question | Decision |
|---|----------|----------|
| D1 | Upstream relationship | **Occasional cherry-picks** from `ladybugdb/ladybug`. Not a tracking fork; not a clean-room hard fork. See §14 for the cherry-pick mechanism that survives the rename. |
| D2 | Rename depth in C/C++ core | **Full rename**, including the root C++ namespace, all macros, all identifiers, all file names. No compatibility aliases in the C/C++ API. |
| D3 | On-disk compatibility | **New magic bytes `RIAG`, accept `LBUG` on read.** Storage format itself is unchanged. RiaGraph opens ladybug databases; ladybug cannot open RiaGraph databases (one-way). |
| D4 | Distribution | **GitHub-only for now.** GitHub Releases + GitHub Packages + GHCR under `cntSafety`. Package *names* are set now; public-registry publication is deferred. |
| D5 | Naming set | **`riag` short form** for code identifiers, `RiaG` for CamelCase, `RIAG` for SCREAMING_CASE, `RiaGraph` for prose/repos/titles, `riagraph` for distribution package names. Full table in §3. |
| D6 | Versioning | **Continue from ladybug 0.20.2.** First RiaGraph release is **0.21.0**. Storage version stays **47**. |
| D7 | Local layout | Core repo at repo root with all eight submodules checked out. Already done — see §2. |
| D8 | Scope | **Rename only.** No new CI gates, no fuzzing, no coverage thresholds. The quality/reliability programme is planned separately. |
| D9 | Extension hosting | **GitHub Pages** from `cntSafety/RiaGraphExtension` → `https://cntsafety.github.io/RiaGraphExtension/`. Existing `/<version>/<platform>/<name>/` path layout is kept unchanged. |
| D10 | Copyright | Add `Copyright (c) 2026 The RiaGraph Contributors`. **Retain** the Kùzu Inc. and Ladybug Memory Inc. lines — MIT requires it. |
| D11 | Inherited CI needing infra we lack | **Mark, do not decide.** Rename identifiers inside them, add a `RIAG-TODO(infra)` marker, and register them in `docs/riagraph-open-infra.md`. Do not delete, do not stand up new infrastructure. |
| D12 | Environment variables | All to `RIAG_*`. **No fallback** to `LBUG_*` / `LADYBUG_*`. |

---

## 1. What RiaGraph is

RiaGraph is a fork of [ladybug](https://github.com/ladybugdb/ladybug), which is itself a fork of
[Kùzu](https://github.com/kuzudb/kuzu). Ladybug's emphasis is feature breadth; **RiaGraph's emphasis
is quality and reliability**. That positioning belongs in `README.md`, `CONTRIBUTING.md` and
`SECURITY.md` (Phase 8) but must not drive code changes in this programme — see D8.

Lineage must remain visible and truthful. Never remove:

- the `Kùzu Inc.` and `Ladybug Memory Inc.` copyright lines in `LICENSE`;
- source comments citing `kuzudb/kuzu` issue URLs (they are provenance for real bugs);
- the README sentence describing the fork chain.

---

## 2. Preconditions and current repository state

### 2.1 Already done

- The core working tree has been restored to the repository root
  (`C:\Users\simon\dev\ria\RiaGraph`). It had been nested one level deep in `RiaGraph/`.
- `.gitmodules` has been rewritten to point at `cntSafety/RiaGraph*`. **This change is
  uncommitted** — commit it in Phase 0.
- Seven of eight submodules are initialised and checked out at the recorded commits.

### 2.2 Known defects that block work — fix in Phase 0

| Defect | Detail |
|---|---|
| **`RiaGraphBenchmark` is a mis-mirror** | `cntSafety/RiaGraphBenchmark` contains a copy of the *extensions* repository, not the benchmarks repository. Its history does not contain `42dd2882d319c339d72b1f10f25e237eafaa1a8f`, the commit the superproject records for `benchmark/`. Upstream source is `https://github.com/ladybugdb/benchmarks` (plural), whose `main` is exactly that commit. |
| **Loose clone directories** | `RiaGraphJava/`, `RiaGraphNodejs/`, `RiaGraphPython/`, `RiaGraphRust/`, `RiaGraphWsm/` may still exist at the repo root as untracked duplicates of the submodules. They are at the same commits as the submodules with no unpushed work. Delete them. On Windows a file watcher (VS Code) may hold handles — close the editor first. Note the directory is `RiaGraphWsm` locally but the GitHub repo is `RiaGraphWasm`; the submodule path `tools/wasm` is what matters. |
| **Nested `dataset` submodule** | `tools/python_api/.gitmodules` and `tools/nodejs_api/.gitmodules` still point `dataset` at `https://github.com/ladybugdb/dataset.git`. Re-point to `https://github.com/cntSafety/RiaGraphDataset.git`. |

### 2.3 Repository map

| Path in superproject | GitHub repo | Upstream it was mirrored from |
|---|---|---|
| *(root)* | `cntSafety/RiaGraph` | `ladybugdb/ladybug` |
| `extension/` | `cntSafety/RiaGraphExtension` | `ladybugdb/extensions` |
| `benchmark/` | `cntSafety/RiaGraphBenchmark` | `ladybugdb/benchmarks` ← **currently wrong** |
| `dataset/` | `cntSafety/RiaGraphDataset` | `ladybugdb/dataset` |
| `tools/java_api/` | `cntSafety/RiaGraphJava` | `ladybugdb/ladybug-java` |
| `tools/nodejs_api/` | `cntSafety/RiaGraphNodejs` | `ladybugdb/ladybug-nodejs` |
| `tools/python_api/` | `cntSafety/RiaGraphPython` | `ladybugdb/ladybug-python` |
| `tools/rust_api/` | `cntSafety/RiaGraphRust` | `ladybugdb/ladybug-rust` |
| `tools/wasm/` | `cntSafety/RiaGraphWasm` | `ladybugdb/ladybug-wasm` |

### 2.4 Remote convention (apply in every repo, Phase 0)

```
origin    https://github.com/cntSafety/<RiaGraph repo>.git   # ours, push here
upstream  https://github.com/ladybugdb/<ladybug repo>.git    # fetch-only, never push
```

Several clones currently have this inverted (`origin` → ladybug, `ria` → RiaGraph). Normalise, and
disable pushes to upstream:

```bash
git remote set-url --push upstream DISABLED
```

---

## 3. The naming dictionary

This is the complete substitution table. Apply nothing that is not listed here.

### 3.1 Token forms

| Ladybug form | RiaGraph form | Where |
|---|---|---|
| `lbug` | `riag` | lowercase identifiers, file names, targets |
| `Lbug` | `RiaG` | CamelCase identifiers, class names, CMake project name |
| `LBUG` | `RIAG` | macros, CMake options, env vars, magic bytes, constants |
| `ladybug` | `riagraph` | prose-lowercase, distribution package names |
| `Ladybug` | `RiaGraph` | prose |
| `LADYBUG` | `RIAG` | env var prefixes |
| `ladybugdb` / `LadybugDB` | `cntSafety` when it names the GitHub org; `riagraph` when it names an npm scope or Java group; `RiaGraph` in prose | context-dependent — **never blind-replace** |
| `lbdb` | `riag` | see §3.4, context-dependent |

`lbug` is not a substring of any other token in this codebase (`ladybug` does not contain it), so
ordering between the `lbug` and `ladybug` families is irrelevant. Ordering *within* a family is not:
always replace `LadybugDB` before `Ladybug` before `ladybug`.

### 3.2 C/C++ core

| Thing | Before | After |
|---|---|---|
| Root namespace | `namespace lbug` | `namespace riag` |
| Nested namespaces | `common`, `storage`, `binder`, `main`, … | unchanged |
| C API symbol prefix | `lbug_*` | `riag_*` |
| C API header | `src/include/c_api/lbug.h` | `src/include/c_api/riag.h` |
| C++ umbrella header | `src/include/main/lbug.h` | `src/include/main/riag.h` |
| Forward-decl header | `src/include/main/lbug_fwd.h` | `src/include/main/riag_fwd.h` |
| Generated single header | `lbug.hpp` | `riag.hpp` |
| ANTLR parser files | `src/{include/,}parser/antlr_parser/lbug_cypher_parser.{h,cpp}` | `riag_cypher_parser.{h,cpp}` |
| Export macros | `LBUG_API`, `LBUG_HELPER_DLL_{IMPORT,EXPORT,LOCAL}`, `LBUG_HELPER_DEPRECATED`, `LBUG_DEPRECATED`, `LBUG_EXPORTS`, `LBUG_STATIC_DEFINE` | `RIAG_…` (same suffixes) |
| Version macros | `LBUG_VERSION`, `LBUG_EXTENSION_VERSION` | `RIAG_VERSION`, `RIAG_EXTENSION_VERSION` |
| Page-size macro | `LBUG_PAGE_SIZE` | `RIAG_PAGE_SIZE` |
| Arrow schema name | `"lbug_query_result"` | `"riag_query_result"` |
| HTTP User-Agent | `lbug/v{version}` | `riag/v{version}` |

### 3.3 Build system

| Thing | Before | After |
|---|---|---|
| CMake project | `project(Lbug VERSION 0.20.2 …)` | `project(RiaG VERSION 0.21.0 …)` |
| Library targets | `lbug`, `lbug_shared`, `lbug_link_deps` | `riag`, `riag_shared`, `riag_link_deps` |
| Shell target | `lbug_shell` (OUTPUT_NAME `lbug`) | `riag_shell` (OUTPUT_NAME `riag`) |
| Python ext target | `_lbug` | `_riag` |
| Artefacts | `liblbug.a`, `liblbug.so`, `lbug_shared.dll`, `lbug.lib` | `libriag.a`, `libriag.so`, `riag_shared.dll`, `riag.lib` |
| CMake options | `LBUG_ENABLE_SIMD`, `LBUG_PAGE_SIZE_LOG2`, `LBUG_VECTOR_CAPACITY_LOG2`, `LBUG_NODE_GROUP_SIZE_LOG2`, `LBUG_MAX_SEGMENT_SIZE_LOG2`, `LBUG_DEFAULT_REL_STORAGE_DIRECTION`, `LBUG_API_USE_PRECOMPILED_LIB`, `LBUG_API_PRECOMPILED_LIB_PATH` | same names with `RIAG_` prefix |
| Wasm export name | `-sEXPORT_NAME=lbug` | `-sEXPORT_NAME=riag` |

`LBUG_NODEJS_USE_PRECOMPILED_LIB` and `LBUG_NODEJS_PRECOMPILED_LIB_PATH` are documented in-tree as
deprecated aliases of the `LBUG_API_*` pair. **Drop them** rather than renaming — nothing outside
this repo sets them, and D2 gives no compatibility obligation. Remove the alias-forwarding block in
`CMakeLists.txt` at the same time.

### 3.4 Runtime, storage and user-visible surface

| Thing | Before | After | Notes |
|---|---|---|---|
| Storage magic bytes | `"LBUG"` | `"RIAG"` | `StorageVersionInfo::MAGIC_BYTES`. See §6.1 for the read-both requirement. |
| Storage version | 47 | **47, unchanged** | The byte layout does not change. Only the magic differs. |
| `ATTACH … (DBTYPE …)` | `LBUG` (`common::ATTACHED_LBUG_DB_TYPE`) | `RIAG` (`common::ATTACHED_RIAG_DB_TYPE`), with `LBUG` accepted as an alias | Consistent with D3: if we read ladybug files we should accept the name they are attached by. |
| User data directory | `~/.lbdb/` | `~/.riag/` | Holds `extension/` and the shell's `history.txt`. |
| Extension file suffix | `.lbug_extension` | `.riag_extension` | Forces a rebuild of all extensions. Intended. |
| Default extension repo | `https://extension.ladybugdb.com/` | `https://cntsafety.github.io/RiaGraphExtension/` | Path layout under it is unchanged. |
| Proxy env vars | `LADYBUG_HTTP_PROXY`, `LADYBUG_HTTPS_PROXY`, `LADYBUG_ALL_PROXY`, `LADYBUG_NO_PROXY` | `RIAG_HTTP_PROXY`, `RIAG_HTTPS_PROXY`, `RIAG_ALL_PROXY`, `RIAG_NO_PROXY` | Standard `http_proxy`/`HTTPS_PROXY`/`no_proxy` fallbacks stay untouched. |
| Tuning env vars | `LBUG_SIMD`, `LBUG_CREATE_INDEX_WAL_THRESHOLD`, `LBUG_ART_INDEX_DESTRUCTOR_STATS` | `RIAG_SIMD`, `RIAG_CREATE_INDEX_WAL_THRESHOLD`, `RIAG_ART_INDEX_DESTRUCTOR_STATS` | Error strings naming them must be updated too (`src/function/simd_filter.cpp`). |
| Database file suffix | `.lbdb` | `.riag` | **Convention only.** The sidecar matcher in `local_file_system.cpp` derives the extension from the user's own path (`dbPathP.extension()`); nothing is hardcoded. Change docs, examples and tests; old `.lbdb` files keep working. |

### 3.5 Vendored third-party namespaces

Ladybug prefixed vendored libraries to avoid symbol collisions. These prefixes are ladybug's own
additions, not upstream code, so they are in scope:

| Before | After | Files |
|---|---|---|
| `namespace lbug_mbedtls` | `namespace riag_mbedtls` | 57 files under `third_party/mbedtls/` |
| `namespace lbug_zstd` | `namespace riag_zstd` | `third_party/zstd/` |
| `namespace lbug_fast_float` | `namespace riag_fast_float` | `third_party/fast_float/` |
| `namespace lbug_snappy` | `namespace riag_snappy` | `third_party/snappy/` |
| `lbug_parquet`, `lbug_apache` | `riag_parquet`, `riag_apache` | `third_party/parquet/`, `third_party/thrift/` |
| `LBUG_THRIFT_*` include guards | `RIAG_THRIFT_*` | `third_party/thrift/` |
| also affected | | `third_party/re2/` (42 files), `third_party/miniz/`, `third_party/utf8proc/` |

**The generators must be updated in the same commit**: `third_party/ports/mbedtls/do-patch.py`
(`NAMESPACE = "lbug_mbedtls"`) and `third_party/ports/zstd/do-patch.py`, plus
`third_party/ports/zstd/README.md`. If the generated files and the generator drift, the next
re-vendoring silently reverts the rename.

### 3.6 Language bindings

| | Before | After |
|---|---|---|
| **Python** dist name | `ladybug` | `riagraph` |
| Python import name | `import ladybug` | `import riag` |
| Python package mapping | `packages = ["ladybug"]`, `ladybug = "src_py"` | `packages = ["riag"]`, `riag = "src_py"` |
| Python ext module | `_lbug` (`src_cpp/lbug_binding.cpp`) | `_riag` (`src_cpp/riag_binding.cpp`) |
| Python capi shim | `src_py/_lbug_capi.py` | `src_py/_riag_capi.py` |
| Python build output dir | `build/ladybug` | `build/riag` |
| **npm** core | `@ladybugdb/core` | `@riagraph/core` |
| npm wasm | `@ladybugdb/wasm-core` | `@riagraph/wasm-core` |
| Node native addon | `lbugjs.node` | `riagjs.node` |
| Node typings | `src_js/lbug.d.ts` | `src_js/riag.d.ts` |
| Node native loader | `src_js/lbug_native.js` | `src_js/riag_native.js` |
| Wasm worker/sync | `src_js/lbug_wasm_worker.js`, `src_js/sync/lbug.js` | `src_js/riag_wasm_worker.js`, `src_js/sync/riag.js` |
| **Rust** crate | `lbug` | `riag` |
| Rust vendored source dir | `lbug-src/` | `riag-src/` |
| Rust FFI headers | `include/lbug_rs.h`, `include/lbug_arrow.h` | `include/riag_rs.h`, `include/riag_arrow.h` |
| Rust FFI sources | `src/lbug_rs.cpp`, `src/lbug_arrow.cpp` | `src/riag_rs.cpp`, `src/riag_arrow.cpp` |
| **Java** Maven group | `com.ladybugdb` | `com.riagraph` |
| Java package | `com.lbugdb` | `com.riagraph` |
| Java classes | `LbugList`, `LbugMap`, `LbugStruct` | `RiaGList`, `RiaGMap`, `RiaGStruct` |
| Java native method | `lbugNativeReloadLibrary` | `riagNativeReloadLibrary` |
| JNI source | `src/jni/lbug_java.cpp` | `src/jni/riag_java.cpp` |
| Java version env var | `LBUG_JAVA_VERSION` | `RIAG_JAVA_VERSION` |
| Android example package | `com.ladybugdb` | `com.riagraph` |

`build.gradle` parses the CMake version with the literal
`it.startsWith('project(Lbug VERSION ')` — update it to `project(RiaG VERSION ` or the Java build
silently falls back to `0.0.0-SNAPSHOT`.

### 3.7 Scripts and asset files to rename

```
logo/lbug-logo.png                          -> logo/riagraph-logo.png
scripts/download-liblbug.sh                 -> scripts/download-libriag.sh
scripts/migrate-lbug-db.py                  -> scripts/migrate-riag-db.py
scripts/pgembed_ladybug_sample_query.py     -> scripts/pgembed_riagraph_sample_query.py
tools/rust_api/download-liblbug.sh          -> tools/rust_api/download-libriag.sh
tools/rust_api/scripts/download-liblbug.sh  -> .../download-libriag.sh
tools/rust_api/scripts/download_lbug.{sh,ps1} -> .../download_riag.{sh,ps1}
tools/python_api/scripts/download_lbug.sh   -> .../download_riag.sh
tools/python_api/test/lbug_test_paths.py    -> .../riag_test_paths.py
```

`logo/lbug-logo.png` is a ladybug graphic. Renaming the file does not make it a RiaGraph logo —
see §16 (open item O1).

### 3.8 Do NOT rename

- `kuzudb/kuzu` issue URLs in comments: `src/include/function/list/functions/base_list_sort_function.h`,
  `test/copy/multi_copy_test.cpp`, `test/test_runner/csv_converter.cpp`,
  `test/test_files/lsqb/lsqb_queries{,_parquet}.test`, `test/test_files/optional_match/correlated.test`.
- `LICENSE` copyright lines for Kùzu Inc. and Ladybug Memory Inc.
- The storage-version compatibility table's ladybug/kuzu **version strings** (`"0.12.0"` … `"0.20.2"`).
  They describe which ladybug release wrote a file; rewriting them corrupts the compatibility map.
- `scripts/migrate-lbug-db.py`'s `"ladybug"`, `"real_ladybug"` and `"kuzu"` PyPI package names —
  these are the *source* packages the migration installs to read old databases.
- Anything under `third_party/` that does not contain an `lbug`/`LBUG` token.
- `test/answers/*.csv` — random fixture data produces false positives on case-insensitive greps.
- Cypher keywords, built-in function names, extension names (`duckdb`, `postgres`, `fts`, `json`,
  `httpfs`, `vector`, `iceberg`, …), and the `duckdb`/`benchmark`/`graph` C++ namespaces.

---

## 4. Phase 0 — Topology (blocking; nothing else may start first)

**4.1 Re-mirror the benchmark repository.** Requires push rights to `cntSafety`. This overwrites
`cntSafety/RiaGraphBenchmark` — confirm with the repository owner before running.

```bash
git clone --mirror https://github.com/ladybugdb/benchmarks.git /tmp/riag-benchmarks.git
cd /tmp/riag-benchmarks.git
git remote set-url --push origin https://github.com/cntSafety/RiaGraphBenchmark.git
git push --mirror
```

Verify: `git ls-remote https://github.com/cntSafety/RiaGraphBenchmark main` must print
`42dd2882d319c339d72b1f10f25e237eafaa1a8f`. Then in the superproject:

```bash
git submodule update --init --force benchmark
git submodule status benchmark   # must show no leading '+' or '-'
```

**4.2 Normalise remotes** in the superproject and all eight submodules per §2.4.

**4.3 Re-point the nested `dataset` submodule** in `tools/python_api` and `tools/nodejs_api` to
`https://github.com/cntSafety/RiaGraphDataset.git`; run `git submodule sync` in each.

**4.4 Delete the loose clone directories** listed in §2.2.

**4.5 Commit** the `.gitmodules` change already present in the working tree, with the message
`Point submodules at cntSafety/RiaGraph* mirrors`.

**4.6 Branch convention.** One phase per branch, one phase per PR, named `fork/phase-N-<slug>`.
Never mix a mechanical rename commit with a semantic change commit — §14 depends on that separation.

**Exit criteria:** `git submodule status` shows eight clean entries with no `+`/`-` markers; every
repo has `origin` → cntSafety and `upstream` → ladybugdb; the tree builds unchanged
(`make release` succeeds) before any rename begins. **Record that build's test results as the
baseline** — every later phase is judged against it.

---

## 5. Phase 1 — Mechanical core rename

Write `scripts/riag-rename.py` and commit it. §14 argues for keeping it permanently.

### 5.1 Required behaviour

1. Operate only on files git tracks (`git ls-files -z`), never on untracked or build output.
2. Skip binary files by content sniffing (NUL byte in the first 8 KiB).
3. Apply the token rules of §3.1 **in this order**, as plain substring replacements:
   `LadybugDB` → *(report only)*, `ladybugdb` → *(report only)*, `Ladybug` → `RiaGraph`,
   `LADYBUG` → `RIAG`, `ladybug` → `riagraph`, `Lbug` → `RiaG`, `LBUG` → `RIAG`, `lbug` → `riag`.
4. `ladybugdb` / `LadybugDB` is **not** in the automatic set — it means three different things
   (GitHub org, npm scope, Java group). Emit a report of every occurrence and leave those
   occurrences unchanged; they are resolved by hand in Phases 5–8 using §3.1.
5. `lbdb` is **not** in the automatic set — see §3.4. Handle by hand.
6. Honour an exclusion list. At minimum: `test/answers/`, `dataset/`, `.git/`,
   `docs/riagraph-fork-plan.md`, and every path listed in §3.8.
7. Honour a per-line opt-out marker `# riag-rename:skip` / `// riag-rename:skip` so the §3.8
   provenance URLs can stay put without excluding the whole file.
8. Rename files with `git mv` per §3.7 and the C/C++ file list in §3.2, then fix `#include` paths.
9. Support `--patch <file>`: read a unified diff, apply the same token rules to it, and write the
   transformed diff. This is what makes §14 work. It must transform diff *content* lines and
   `+++` / `---` / `diff --git` path headers alike.
10. Support `--check`: exit non-zero if any in-scope token remains.

### 5.2 Guards

After the rename, this must produce no output:

```bash
git grep -nI -e 'lbug' -e 'LBUG' -e 'Lbug' -- \
  ':!test/answers' ':!docs/riagraph-fork-plan.md' ':!scripts/riag-rename.py'
```

and this must produce only the §3.8 allow-list:

```bash
git grep -nIi -e 'ladybug' -e 'kuzu' -- ':!test/answers' ':!docs/riagraph-fork-plan.md'
```

### 5.3 Scale (measured on the pre-rename tree, for planning only)

| Token | Hits | Files |
|---|---|---|
| `lbug` | 9 446 | 2 009 |
| `LBUG` | 2 713 | 419 |
| `Lbug` | 1 178 | 81 |
| `ladybug` (all cases) | ~294 | ~48 |
| `lbdb` | 30 | 13 |
| third-party `lbug_*` namespaces | — | 146 |

**Exit criteria:** guards clean; `make release` succeeds; full test suite matches the Phase 0 baseline.

---

## 6. Phase 2 — Semantic changes (hand-written, never scripted)

These are the changes the rename script must *not* attempt. Do them in a separate commit so §14 can
skip them when transplanting upstream patches.

**6.1 Storage magic** — `src/include/storage/storage_version_info.h`, `src/storage/database_header.cpp`.
Set `MAGIC_BYTES = "RIAG"`. Add `LEGACY_MAGIC_BYTES = "LBUG"`. `DatabaseHeader` deserialisation must
accept either; serialisation must always write `RIAG`. Keep `canReadStorageVersion` accepting 40–47.
Add a test that opens a fixture written with `LBUG` magic and one written with `RIAG` magic.

**6.2 Version table** — same header. Append `{"0.21.0", STORAGE_VERSION_47}`. Do not renumber, do
not remove the ladybug/kuzu version strings (§3.8).

**6.3 `CMakeLists.txt`** — `project(RiaG VERSION 0.21.0 LANGUAGES CXX C)`. Leave
`RIAG_EXTENSION_VERSION="0.20.0"` alone for now; it gets its own line when the first RiaGraph
extension build ships (§16 O4).

**6.4 ATTACH DBTYPE** — `src/include/common/constants.h`, `src/binder/bind/bind_attach_database.cpp`,
`src/main/attached_database.cpp`, `src/processor/operator/simple/attach_database.cpp`.
`ATTACHED_RIAG_DB_TYPE = "RIAG"`, plus `ATTACHED_LEGACY_DB_TYPE = "LBUG"` accepted at all three
comparison sites. Update `test/test_files/graph/attach_use.test` to the new spelling and add one
case asserting the legacy spelling still attaches.

**6.5 Extension identity** — `src/include/extension/extension.h`, `src/extension/extension.cpp`,
`src/extension/extension_installer.cpp`.
`OFFICIAL_EXTENSION_REPO = "https://cntsafety.github.io/RiaGraphExtension/"`,
`EXTENSION_FILE_SUFFIX = "riag_extension"`, User-Agent `riag/v{version}`, proxy env vars per §3.4.

**6.6 User directory** — `src/main/client_context.cpp` (`getExtensionDir`),
`tools/shell/shell_runner.cpp`, `tools/shell/test/Makefile`. `~/.lbdb/` → `~/.riag/`. There is no
migration: a fresh extension download and an empty shell history on first run are acceptable and
must be noted in the release notes.

**6.7 Migration script** — `scripts/migrate-riag-db.py`. It reads `catalog.lbdb` at a fixed offset
and installs an old PyPI package to export legacy databases. Extend, do not rewrite: add RiaGraph as
a possible *target*, keep `ladybug`/`real_ladybug`/`kuzu` as *sources* (§3.8), and teach it the
`.riag` suffix alongside `.lbdb`.

**6.8 Drop the deprecated CMake aliases** per §3.3.

---

## 7. Phase 3 — Build, install and headers

- `src/CMakeLists.txt`: `install(TARGETS riag riag_shared)`, `install(FILES …/c_api/riag.h)`,
  `install(FILES ${CMAKE_CURRENT_BINARY_DIR}/riag.hpp)`.
- `scripts/collect-single-file-header.py`: `START_POINT = MAIN_HEADER_PATH / "riag.h"`,
  `OUTPUT_PATH = "riag.hpp"`.
- `cmake/templates/system_config.h.in`: namespace and `@RIAG_*@` substitution variables.
- `Makefile`: every recipe that names an artefact or a directory. Target *names*
  (`release`, `debug`, `python`, `java`, …) are generic — leave them.
- `scripts/check-include-guards.sh` and `scripts/headers.txt` if they encode header names.
- `.gitignore`: `tools/python_api/ladybug.egg-info` → `tools/python_api/riagraph.egg-info`.

Verify the installed surface explicitly:

```bash
cmake --install build --prefix /tmp/riag-install
test -f /tmp/riag-install/include/riag.h
ls /tmp/riag-install/lib | grep -E 'libriag|riag_shared'
```

---

## 8. Phase 4 — Vendored third-party namespaces

Per §3.5. Two rules:

1. Change generated sources **and** their generator (`third_party/ports/*/do-patch.py`) in the same
   commit.
2. Do not touch upstream third-party code that carries no ladybug token. If a file needs an
   unrelated fix, that is a separate PR.

Verify by symbol table, not by grep:

```bash
nm -C build/release/src/libriag.a | grep -c 'lbug_'    # must be 0
```

---

## 9. Phase 5 — Language bindings

One PR per binding. Each is a submodule with its own repository and its own workflows. Apply §3.6
plus, in every one: `README.md`, `LICENSE`, homepage and repository URLs, example code, test
fixtures, and the `download-lib*` scripts that fetch the core library by artefact name.

**Order matters** — the core library must be renamed and buildable first, because every binding
links against `libriag` and includes `riag.h`.

| Binding | Extra care |
|---|---|
| `tools/python_api` | `pyproject.toml` `packages` / `package-dir` / `package-data` triple; the `ruff` per-file-ignore paths; `CMakeLists.txt` output directories `build/ladybug`. |
| `tools/nodejs_api` | `package.json` `name`, `types`, `exports`, `files` — all four name the artefact. Nested `dataset` submodule (§2.2). |
| `tools/java_api` | The `build.gradle` CMake-version regex (§3.6). Package directory moves under `src/main`, `src/test/java`, `src/test/kotlin`, and both Android example source sets. JNI method names are encoded in native symbol names — rename `src/jni/riag_java.cpp` and regenerate headers via `make java_native_header`. |
| `tools/rust_api` | `Cargo.toml` `name` and `include` list (paths change with `lbug-src` → `riag-src`), `build.rs` rerun-if-changed paths. `.cache/` is build output — delete it, do not rename it. |
| `tools/wasm` | `-sEXPORT_NAME` in the core `CMakeLists.txt` must match what `src_js` expects. |

After each binding merges, bump the superproject's submodule pointer in a follow-up commit.

---

## 10. Phase 6 — Extension repository and GitHub Pages

- `extension/` (`cntSafety/RiaGraphExtension`): rename per §3, then rebuild all extensions so they
  carry the `.riag_extension` suffix.
- `scripts/setup-extension-repo.py` builds the `extension/repo/v<version>/<os>_<arch>/` tree. That
  layout is what `OFFICIAL_EXTENSION_REPO` expects; keep it and publish that directory as the
  Pages root.
- Enable GitHub Pages on `cntSafety/RiaGraphExtension` serving `gh-pages`, and add a workflow that
  publishes the built tree there. This workflow is **new**, so D11 does not apply — write it.
- Verify end to end from a built shell: `INSTALL json; LOAD json;` must download from
  `cntsafety.github.io` and land in `~/.riag/extension/`.

---

## 11. Phase 7 — CI workflows

Rename identifiers in all 21 core workflows plus each binding's workflows. Then, for every workflow
that depends on infrastructure that does not exist yet, add a marker as the first line of the file
and register it:

```yaml
# RIAG-TODO(infra-<topic>): <what is missing, what would unblock it>
```

Create `docs/riagraph-open-infra.md` with one row per marker. Per D11: **do not delete these
workflows, do not disable their triggers, do not build the missing infrastructure.**

| Workflow | Missing infrastructure |
|---|---|
| `benchmark-workflow.yml` | dedicated benchmark runners |
| `lsqb-benchmark-workflow.yml` | benchmark runners; installs the `ladybug` wheel from PyPI |
| `gen-docs.yml` | a `LadybugDB/api-docs` equivalent; `ci@ladybugdb.com` identity |
| `build-and-deploy.yml` | `ghcr.io/ladybugdb/ubuntu-22.04-gcc13` builder image |
| `java-workflow.yml`, `nodejs-workflow.yml` | the same builder image, as a default input value |
| `deploy-extension.yml`, `purge-extension.yml`, `build-and-deploy-extension.yml`, `get-extensions-from-ghcr.yml` | `ghcr.io/ladybugdb/extension-repo` and its deploy target — superseded by Phase 6; reconcile there |
| `ci-musl-test-workflow.yml` | hardcodes `/__w/ladybug/ladybug`; the runner workspace path derives from the repo name and becomes `/__w/RiaGraph/RiaGraph` |

`ci-workflow.yml`, `codeql.yml`, `nightly-sanitizers.yml`, `zizmor.yml`, `wasm-workflow.yml`,
`python-wheel-workflow.yml`, `precompiled-bin-workflow.yml`, `release-artifacts.yml` and
`build-extensions.yml` should work after renaming — get them green.

Also update `.github/ISSUE_TEMPLATE/*.yml`, `.github/CODEOWNERS`,
`.github/docs/pull_request_template.md`, `.github/docker/ubuntu-22.04-gcc13/Dockerfile` and
`.github/workflows/lcov_exclude`.

---

## 12. Phase 8 — Branding, docs and licence

- `LICENSE`: add `Copyright (c) 2026 The RiaGraph Contributors` **below** the existing two lines.
- `README.md`: RiaGraph identity, the quality/reliability positioning, and an accurate lineage
  paragraph naming both Kùzu and ladybug with links.
- `AGENTS.md`: retitle, and add a pointer to this document.
- `CONTRIBUTING.md`, `SECURITY.md`, `docs/*.md`, `security/threat_model.md`.
- `docs/incidents/` and `security/incidents/`: **historical records — rename the product name in
  prose but never rewrite dates, findings or commit hashes.**
- `pixi.toml`: workspace name `ladybug-adbc` → `riagraph-adbc`.
- All `ladybugdb.com` / `docs.ladybugdb.com` URLs: there is no RiaGraph domain (§16 O2). Replace
  with the GitHub repository URL, or drop the link if there is nothing to point at. **Do not invent
  a domain.**
- `logo/`: file rename only; a real logo is an open item.

---

## 13. Phase 9 — Verification

Run on Linux, macOS and Windows before declaring the fork complete.

```bash
make release
make test
make shell-test
make pytest
make nodejstest
make javatest
make rusttest
make wasmtest
make extension-test
```

Plus the identity assertions:

```bash
./build/release/tools/shell/riag --version                      # prints 0.21.0
xxd -l 4 /tmp/new.riag | grep RIAG                              # new magic on write
./build/release/tools/shell/riag /path/to/an/old-ladybug.lbdb   # legacy magic still opens
git grep -nI -e lbug -e LBUG -e Lbug -- ':!test/answers'        # empty
```

Zero test regressions against the Phase 0 baseline is the release gate. A pre-existing failure stays
a pre-existing failure — record it, do not fix it in this programme (D8).

---

## 14. Cherry-picking from upstream after the rename

This is why §5.1 items 9 and 10 exist, and why Phase 1 and Phase 2 are separate commits.

```bash
# once
git remote add upstream https://github.com/ladybugdb/ladybug.git
git remote set-url --push upstream DISABLED

# per pick
git fetch upstream
git format-patch -1 --stdout <upstream-sha> > /tmp/up.patch
python scripts/riag-rename.py --patch /tmp/up.patch > /tmp/riag.patch
git am -3 /tmp/riag.patch          # or: git apply -3 /tmp/riag.patch
```

The transformed patch applies cleanly wherever upstream touched code we only renamed. It will
conflict where upstream touched something Phase 2 changed semantically (storage header, extension
installer, attach binding) — those are exactly the places that *should* need human judgement.

Record every pick in `docs/upstream-picks.md`: upstream SHA, our SHA, date, one-line rationale.
Without that log, the next person cannot tell what has already been taken.

---

## 15. Rules for agents

1. **Never invent a name.** If §3 does not cover it, add it to §16 and ask.
2. **One phase per PR.** Never mix mechanical rename with semantic change — §14 depends on it.
3. **Never hand-edit what the script covers.** Fix the script and re-run.
4. **Run the §5.2 guards before every push.**
5. **Build and test at every phase boundary.** A phase that leaves the tree unbuildable is not done.
6. **Do not touch `third_party/` upstream code** beyond the ladybug-added prefixes in §3.5.
7. **Do not rewrite history** in incident reports, the storage-version table, or provenance comments.
8. **Do not force-push to any `cntSafety` repository** without explicit confirmation. §4.1 is the
   one exception and it is called out there.
9. **Scope is rename only** (D8). Bugs you find go in an issue, not in your PR.
10. **Update the submodule pointer** in the superproject after every submodule PR merges.

---

## 16. Open items (deliberately unresolved)

| # | Item | Why it is open |
|---|---|---|
| O1 | Brand assets | `logo/lbug-logo.png` is a ladybug. RiaGraph needs its own logo, and the README image links need a host. |
| O2 | Domain | No `riagraph.*` domain exists. All `ladybugdb.com` links become GitHub URLs or are dropped (§12). Revisit if a domain is acquired. |
| O3 | Public registry names | D4 defers PyPI / npm / crates.io / Maven Central. `riagraph`, `@riagraph`, `riag` and `com.riagraph` have **not** been checked for availability, and Maven Central requires proving ownership of `riagraph.*` or using `io.github.cntsafety`. |
| O4 | Extension version line | `RIAG_EXTENSION_VERSION` stays `0.20.0` until the first RiaGraph extension build (§6.3). |
| O5 | Inherited CI infrastructure | Every `RIAG-TODO(infra)` marker from Phase 7, tracked in `docs/riagraph-open-infra.md`. |
| O6 | `cntSafety` capitalisation | Repo URLs use `cntSafety`; GitHub is case-insensitive but Pages URLs are lowercase (`cntsafety.github.io`). Both spellings in this document are deliberate. |
| O7 | Quality programme | D8 defers it. Sanitiser gates, coverage thresholds, fuzzing, a stability/deprecation policy and the reliability positioning in CONTRIBUTING are a separate plan. |
| O8 | Provenance markers vs. the §5.2 guards | Execution found a category §3.8 did not enumerate: upstream issue and pull-request URLs (`LadybugDB/ladybug#611`), third-party repositories that happen to carry a token (`Ladybug-Memory/icebug-format`, `fabubaker/lbug`, `ericpolo/lbug_cs848`, the upstream Hugging Face dataset path) and the ladybug social handle. Renaming them produces links to repositories that never existed — the same reasoning §3.8 applies to the `kuzudb/kuzu` URLs. They carry `riag-rename:skip` markers, so the §5.2 grep guards must exclude marked lines; `scripts/riag-rename.py --check` is the authoritative guard and already does. |
| O9 | `.github/CODEOWNERS` | Still names upstream maintainers, who cannot review in `cntSafety`. Replacing them is an organisational decision, not a rename, so it is left alone rather than guessed at. |
| O10 | Binding naming profiles | §3.6 has rows the token table cannot express, because one spelling means two things: `ladybug` is both the Python distribution name (`riagraph`) and its import name (`riag`); `lbug`/`ladybug` sit inside the Java package (`com.riagraph`) and the JNI symbols that must track it. These live in `PROFILES` in `scripts/riag-rename.py`, selected with `--profile`. Adding a binding means adding a profile. |
| O11 | Extension-repo workflows superseded | Phase 6 publishes extensions to GitHub Pages, which makes `deploy-extension.yml`, `purge-extension.yml`, `build-and-deploy-extension.yml` and `get-extensions-from-ghcr.yml` describe a mechanism that no longer applies. D11 forbids deleting them during the rename; reconcile once Pages is verified end to end. |
