# Namespace implementation plan

The contract is [namespace.md](../namespace.md). This plan tracks implementation;
an unchecked item is not implemented or verified. Keep the existing engine usable
at every stage. Do not replace the contract with a query-text preprocessor or
namespace-specific physical copies of user tables.

## Stages

1. **Baseline and build environment**
   - [x] Inspect parser, catalog, MVCC, WAL, index, scan, traversal, and write integration points.
   - [x] Locate the existing `riagraph-build-env:latest` Docker image (GCC 13.4, CMake, Ninja, Java 17).
   - [x] Build the unchanged release engine and end-to-end runner in the isolated builder.
   - [x] Freeze the unchanged source/build; record its regression results and interleaved measurements.
2. **Namespace values and binding**
   - [x] Represent global as numeric ID zero; implement numeric set intersection and singleton specialization.
   - [x] Add optional AST selectors to outer clauses and node/relationship patterns.
   - [x] Extend grammar at the source; regenerate using the existing ANTLR generator.
   - [x] Bind base and per-element scopes separately; intersect numeric sets once.
   - [x] Resolve singleton write targets and bound-variable assertions.
   - [x] Keep absent/unrestricted scope on the existing planning and execution path.
3. **Transactional catalog and persistence**
   - [x] Add namespace catalog entries, stable IDs, namespace DDL, and restrictive drop.
   - [x] Rename a namespace in place of its ID (`ALTER NAMESPACE ... RENAME TO`), logged as the
         existing catalog-entry drop and create records; no member, policy, or index rewrite.
   - [x] Persist table allowed sets and key scope as optional metadata.
   - [x] Implement session selection/reset, catalog-switch reset, and cached-plan dependencies.
   - [x] Add explicitly versioned catalog/WAL serialization and safe legacy defaults.
   - [x] Test commit, rollback, reopen, recovery, and concurrent catalog changes.
4. **Membership and primary keys**
   - [x] Implement lazy, snapshot-correct node/relationship membership with stable element IDs.
   - [x] Preserve the implicit-global representation and native scalar-key index when unused.
   - [x] Route local-key probes and writes through per-namespace index domains.
   - [x] Implement atomic moves and destination-conflict checks without incident-edge rewrites.
   - [x] Maintain numeric namespace/group directories, vector scan masks, and selectivity estimates.
5. **Read and write operators**
   - [x] Enforce node scopes on scans, primary/secondary probes, and reused variables.
   - [x] Enforce edge and endpoint scopes on both traversal directions and direct rel scans.
   - [x] Restrict recursive/shortest traversal before path selection; preserve zero-hop behavior.
   - [x] Implement `namespace()` projection without string materialization for inline selectors.
   - [x] Implement CREATE, MERGE, property updates, DELETE, and guarded DETACH DELETE.
   - [x] Implement COPY/CTAS/export/import and explicit errors for unsupported combinations.
6. **Integrations and delivery**
   - [x] Audit graph/database switching, attached tables, partitions, alternative storage, GDS,
         projected graphs, raw CSR/Arrow, FTS, vector indexes, and unloaded extensions.
   - [x] Add diagnostics and actionable validation errors.
   - [x] Execute functional, regression, recovery, compatibility, and plan checks; record baseline environment failures.
   - [x] Complete the measured Linux shared-release acceptance in [the verification plan](namespace-verification.md), including fixed-work COPY and controlled cold startup; report attribution controls and scoped costs separately.
   - [x] Update the feature status and documentation to match verified behavior and documented limitations.

## Design constraints

- Resolve catalog identifiers to numeric IDs before execution. Do not compare namespace
  strings for each row, edge, or index probe. Only explicit `namespace()` output needs names.
- Preserve unrestricted/global-only operator specialization and physical-plan reuse.
  Do not introduce an always-true generic filter or widen stored element IDs.
- Use existing MVCC, undo, WAL, checkpoints, and storage ownership. Every persistent
  change needs recovery coverage before it is considered complete.
- Optimize restricted queries with namespace/group pruning and direct local-key index
  routing. A singleton lookup must not probe unrelated namespaces.
- An unsupported integration must reject namespace-aware use before returning results.
  Existing global-only functionality remains supported.
- Performance with namespaces depends on selectivity and topology. Report comparable
  work and measured overhead; do not claim universal zero cost for scoped graph queries.

## Working environment

The host is Windows. CMake/Ninja/compiler are not on its default PATH, and the
installed Ubuntu WSL distribution has no C++ toolchain. The existing Docker builder
provides the toolchain without host installation. Mount only this workspace at
`/workspace`; keep build directories and verification artifacts under `build/`.
Use identical release flags and builder image for baseline and candidate. The
dedicated container `riagraph-namespaces-work` holds a frozen baseline source/build
at `/namespace/baseline-source` and `/namespace/baseline`, and candidate source/build
at `/namespace/candidate-source` and `/namespace/candidate`. Compilation occurs on
the container's Linux filesystem to avoid Windows-mounted build overhead. Export
verification logs and reports to `build/namespace-verification/` in the workspace.

The builder publishes no ports. Commands target this container by its exact name;
other local clones and containers are left untouched. Host-side installation, if
needed, must use the user's `riacore` conda environment. None has been needed.
