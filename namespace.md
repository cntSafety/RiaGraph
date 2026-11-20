# RiaGraph namespaces: technical concept

**Status:** implemented and verified for the tested Linux shared release.
The worked example, namespace semantics, compatibility and existing regression
checks pass apart from four reproduced baseline environment failures. Delivered
global-only performance meets the 1% upper-bound tolerance in the measured cases.
This includes the documented Linux linkage optimization; matching-linkage controls
and a dense scoped recursive case retain measurable costs. See the
[implementation plan](docs/namespace-implementation-plan.md) and
[verification record](docs/namespace-verification.md) for scope, evidence and limitations.

## 1. Purpose and scope

A namespace assigns each node and each relationship to one logical group within a
graph. A query can select one namespace, several namespaces, or all namespaces.
Tables retain one shared schema and can contain elements from several namespaces.
Namespaces do not create another database, graph catalog, or physical table.

Namespaces are **optional** in the Cypher dialect. A statement that names no
namespace, in a session that selected none, creates its nodes and relationships in
the default (global) namespace and reads from all namespaces. It must not fail, or
behave differently, because namespaces exist in the catalog or because another
connection changes them.

The primary requirement is **performance neutrality when namespaces are unused**:
an existing application, with every node and relationship in the global namespace,
must retain its existing storage layout, primary-key representation, query plans,
and execution loops. Namespace support must not add work proportional to the number
of rows, edges, traversed neighbors, or index probes in that case.

The first implementation covers native, stored, typed node and relationship tables
in one selected RiaGraph catalog. Namespaces are an organization and query-scoping
feature, not an authorization boundary or a physical partitioning mechanism.

## 2. Review of the original draft

| Inconsistency or gap | Resolution in this concept |
| --- | --- |
| Shared tables with local/global keys coexist with references to `namespace.table`. | One schema and table identity per catalog. Namespace selection filters elements, not table names. |
| `namespace.table` is proposed without considering existing qualification. | Preserve `database.table` for attached databases; introduce no namespace dot qualification. |
| Unqualified `MATCH` means global-only, but other examples expect a `WHERE namespace(...)` predicate to reach named namespaces. | Unrestricted reads see all namespaces by default. An explicit selection narrows them; a predicate never broadens that selection. |
| The meaning of an unset session scope is unspecified. | No selection means all namespaces. `USE NAMESPACE ALL NAMESPACES` restores this unrestricted default. |
| Table creation without `IN` ambiguously means global-only or all namespaces. | Omitted table membership means all existing and future namespaces. This is separate from query scope. |
| The default primary-key scope is missing; “global” could also mean across different tables. | Default to `GLOBAL PRIMARY KEY`, unique across namespaces of that one logical table. |
| Relationship ownership, cross-namespace endpoints, and path visibility are unspecified. | Each relationship has one independent namespace; cross-namespace edges are permitted. Every path element must satisfy its own effective scope. |
| A mixed-namespace path requires many `namespace(...)` predicates. | Allow a final `IN` selector inside each node and relationship pattern, combined with its labels/types and properties. |
| Multi-namespace sessions have no deterministic insertion target. | Resolve one target per new element, using its inline selector, clause default, or session default. Never copy a row into several namespaces. |
| Moving nodes leaves the fate of incident edges and keys unclear. | Preserve IDs and edges, change membership atomically, and validate destination uniqueness. |
| Dropping namespaces and removing table membership have no dependency/data rules. | Restrict both operations; never silently delete or move data. |
| Several DDL forms differ from the current grammar. | Extend existing `ALTER TABLE` and `DROP TABLE`; define placement of new clauses below. |
| Transactions, recovery, cached plans, imports, extensions, and the unused-feature cost are missing. | Specify these as implementation and acceptance requirements. |

The product choices confirmed during review are: unrestricted reads by default,
permitted cross-namespace relationships with endpoint visibility checks, and global
primary keys unless `LOCAL` is explicit. Default writes still target global when no
namespace restriction is selected, preserving the original draft's create behavior.

## 3. Data model and invariants

### 3.1 Namespace identity and ownership

Every catalog has an implicit global namespace. It always exists and cannot be
created, dropped, or renamed. `GLOBAL` identifies it in namespace syntax.
`namespace(n)` and `namespace(r)` return Cypher `NULL` for global membership,
preserving the original draft's convention. This does not mean missing membership.
`namespace(NULL)` also returns `NULL`, as in an unmatched optional pattern.

Named namespaces are catalog entries with stable, unsigned 64-bit IDs. ID zero
represents global membership internally and is not nullable. Named IDs are never
reused within the owning catalog, including after drop/recreate and recovery.
Namespace names follow the existing catalog identifier comparison and quoting
rules. `GLOBAL` is reserved as a namespace name, including when quoted. A literal
string such as `'NULL'` never denotes the global namespace.

Namespace identity is `(owning catalog identity, namespace ID)`. Equal names in
different databases or graphs are unrelated. Existing `USE GRAPH` and `USE <database>`
retain their catalog-selection meaning; switching either clears namespace selection
and restores unrestricted reads with global as the default write target. Namespace
commands operate on the selected native catalog.

### 3.2 Elements and tables

1. Every live node and relationship belongs to exactly one namespace.
2. Namespace membership is system metadata, independent of user properties. A user
   property named `namespace` has no special effect.
3. A table has one name, schema, logical identity, key policy, and index definitions
   across all its namespaces. Different schemas require different tables or graphs.
4. A table's allowed namespaces govern where its elements may reside. They neither
   create copies of a table nor select rows for a query.
5. `IN ALL NAMESPACES` is a dynamic membership policy, not an enumerated snapshot.
6. Node and relationship IDs keep the current `(tableID, offset)` representation.
   Moving membership preserves IDs, property values, and endpoint references.
7. A relationship may connect nodes in different namespaces. Its own namespace may
   differ from either endpoint. Its `FROM`/`TO` table constraints still apply.
8. Moving a node does not move its incident relationships or either endpoint of
   those relationships. Moving a relationship does not move its endpoints.

## 4. Namespace and table DDL

The following syntax extends RiaGraph's existing grammar. Bracketed forms in syntax
summaries are optional; examples are complete proposed statements.

### 4.1 Create, drop, and rename namespaces

```cypher
CREATE NAMESPACE [IF NOT EXISTS] <name>;
DROP NAMESPACE [IF EXISTS] <name>;
ALTER NAMESPACE <name> RENAME TO <new-name>;
```

Creating an existing namespace fails unless `IF NOT EXISTS` is present. Dropping a
missing namespace fails unless `IF EXISTS` is present. These modifiers suppress only
the corresponding existence error.

`DROP NAMESPACE` is restrictive. It fails if any live node or relationship belongs
to the namespace, or any table's finite allowed set explicitly references it.
An `ALL NAMESPACES` policy is not a blocking reference. Empty the namespace and remove
finite memberships explicitly before dropping it. There is no `CASCADE` in this
version. Existing transactions retain their consistent snapshots; reclamation waits
until no snapshot can reference the dropped entry or its metadata.

`ALTER NAMESPACE ... RENAME TO` changes only the name of the namespace definition. The
namespace keeps its ID, so everything keyed by that ID stays attached without being
rewritten: member nodes and relationships, finite table policies, `LOCAL` primary-key
domains, and session selections. The statement changes one catalog entry; its cost does
not depend on the number of members, tables, or indexes. It is transactional catalog DDL
like `CREATE`/`DROP NAMESPACE`: visible inside its own transaction, undone by `ROLLBACK`,
logged to the WAL and persisted by checkpoint, and refused in a read-only transaction or
database. It operates on the namespaces of the selected native catalog only.

Renaming fails if `<name>` does not exist or `<new-name>` already exists, under the
catalog identifier comparison rules of section 3.1. As for `ALTER TABLE ... RENAME TO`,
the current name, in any casing, already exists, and there is no `IF EXISTS` form.
`GLOBAL` is neither a valid source nor target, in any casing and quoted or not: the
implicit global namespace cannot be renamed, and no namespace can be renamed to it.
The name of a dropped namespace is free and can be the target of a rename. After a
rename, the old name is free as well: `CREATE NAMESPACE` with it creates a new identity
with a fresh ID and no members, and nothing bound to the renamed namespace attaches to it.
Two names are swapped through a third, free name, for example in one transaction.

Names are bound to IDs when a statement is bound; section 10 states how plans and
selections behave across a rename. Drop/recreate, by contrast, creates a new identity,
so old prepared statements and selections cannot attach to the new object.

### 4.2 Create tables

```cypher
CREATE NODE TABLE [IF NOT EXISTS] <table>
    [IN <namespace-list> | IN ALL NAMESPACES] (
    <property-definitions>, <primary-key-definition>
);

CREATE REL TABLE [GROUP] [IF NOT EXISTS] <table>
    [IN <namespace-list> | IN ALL NAMESPACES] (
    FROM <source-table> TO <target-table>,
    <optional-properties-and-existing-constraints>
);
```

Here `<namespace-list>` is a nonempty comma-separated list of named identifiers
and/or `GLOBAL`. Every named namespace must already exist. Omission means
`IN ALL NAMESPACES`, regardless of the session's read selection. Existing tables
opened from a database without namespace metadata receive this policy implicitly.

For example, both namespaces share the same `Person` schema:

```cypher
CREATE NODE TABLE Person IN GLOBAL, team_a, team_b (
    id INT64 LOCAL PRIMARY KEY,
    name STRING
);
CREATE REL TABLE Knows IN ALL NAMESPACES (FROM Person TO Person);
```

`IF NOT EXISTS` uses the existing table name and catalog identity. It does not create
another per-namespace table, change membership, or reconcile differing schemas.
Changing properties or indexes affects the shared table everywhere.

Relationship-table membership constrains the relationship itself. Endpoint tables
need not admit that same namespace, because cross-namespace endpoints are allowed.
The existing relationship multiplicity constraints remain table-wide across
namespaces for a given endpoint; namespace selection does not relax them.

### 4.3 Change allowed namespaces

Use the existing `ALTER TABLE` form for both table kinds:

```cypher
ALTER TABLE Person ADD TO NAMESPACE team_c, GLOBAL;
ALTER TABLE Person REMOVE FROM NAMESPACE team_a;
ALTER TABLE Person SET NAMESPACES GLOBAL, team_b;
ALTER TABLE Person SET NAMESPACES ALL NAMESPACES;
```

`ADD` unions a finite set; an already-present namespace is a no-op. Adding to `ALL`
is also a no-op after validating the supplied names. `REMOVE` subtracts from a finite
set; an already-absent namespace is a no-op. Removing from `ALL` fails: first use
`SET NAMESPACES` to declare a finite set. The resulting finite set must be nonempty.

Removing or replacing membership fails if it would exclude any live element of
that table. Membership changes do not move or delete rows, change keys, or depend
on the session's read scope. Check all affected namespaces atomically. Endpoint
constraints and cross-namespace relationships remain valid because element IDs and
table identities do not change.

### 4.4 Drop tables

```cypher
DROP TABLE [IF EXISTS] Person;
```

This drops the shared table and its data in every namespace, subject to existing
dependency rules. It has the same meaning in every session scope. Neither
`DROP NODE TABLE namespace.table` nor namespace-specific table deletion is introduced.
Deleting the rows in one namespace is a DML operation.

## 5. Primary keys and indexes

Each node table still requires exactly one non-null primary key of an existing
supported type. Both current declaration styles gain an optional scope:

```cypher
CREATE NODE TABLE Asset (id INT64 GLOBAL PRIMARY KEY, name STRING);
CREATE NODE TABLE Person (id INT64 LOCAL PRIMARY KEY, name STRING);
CREATE NODE TABLE Device (id INT64, name STRING, LOCAL PRIMARY KEY (id));
```

| Declaration | Uniqueness domain | Can the same key occur in `team_a` and `team_b`? |
| --- | --- | --- |
| `PRIMARY KEY` | This table across all namespaces; equivalent to `GLOBAL PRIMARY KEY`. | No |
| `GLOBAL PRIMARY KEY` | This table across all namespaces, including global. | No |
| `LOCAL PRIMARY KEY` | This table within one namespace, including global as its own domain. | Yes |

“Global key” does not mean uniqueness across different tables or graph catalogs.
For a local key, the logical uniqueness key is `(namespace ID, primary-key value)`.
Global membership uses ID zero, not a nullable component that could bypass
uniqueness checking. A multi-namespace key lookup may return one row per selected
namespace; never pick an arbitrary first match.

`CREATE`, `MERGE`, bulk loading, and moves enforce these rules against all relevant
rows, including rows outside the read selection. A hidden duplicate still causes a
global-key conflict. Key scope is fixed at table creation in this version; changing
it later requires a validated migration. Existing primary-key update restrictions
remain in effect. `SERIAL` retains its table-wide allocator and is not reset per
namespace; explicit local keys can still repeat across namespaces.

Index definitions remain table-wide and inherit the table's key policy. A global
index hit must still pass namespace visibility checks. Local-key indexes must
support singleton and multi-namespace probes. Ordinary secondary-index results
must also respect scope; this concept does not add new secondary uniqueness syntax.

## 6. Read selection and query semantics

### 6.1 Session and clause scope

```cypher
USE NAMESPACE GLOBAL;
USE NAMESPACE team_a;
USE NAMESPACE GLOBAL, team_a, team_b;
USE NAMESPACE ALL NAMESPACES;
```

Selection is connection-local state. A fresh connection has no restriction: its
reads see all namespaces, including global. `USE NAMESPACE ALL NAMESPACES` clears
any restriction and restores this default; it is also the reset command.
`USE NAMESPACE GLOBAL` narrows reads to global data only. Selecting named namespaces
does not automatically include global data. Lists are sets: duplicates have no
effect. A finite selection must be nonempty and all names must resolve.
`ALL NAMESPACES` means all namespaces visible in the statement's transaction
snapshot, including global. It is not expanded permanently when `USE` executes.

| Session state | `MATCH` without `IN` | `CREATE`/`MERGE` without `IN` |
| --- | --- | --- |
| Unset, or reset through `ALL NAMESPACES` | All namespaces | Global target |
| `GLOBAL` | Global only | Global target |
| `team_a` | `team_a` only | `team_a` target |
| `team_a, team_b` | Both selected namespaces | Error: specify one target |

An inline selector or clause-level `IN` can supply an explicit write target as
described in section 7.1.

`USE NAMESPACE` is allowed between transactions and is rejected inside an explicit
transaction. It is not persistent database state. If an explicitly selected namespace
is subsequently dropped, the next statement fails with a stale-selection error;
there is no silent fallback or rebinding to a recreated name. A replacement
`USE NAMESPACE` or catalog switch remains allowed so the connection can recover;
these commands validate the new selection without requiring the old one to exist.
A selection records namespace identities, not names: after `ALTER NAMESPACE team_a
RENAME TO team_x`, a connection that selected `team_a` keeps reading and writing that
same namespace, which `namespace(...)` now reports as `team_x`. A namespace later
created under the name `team_a` is a different identity and is not selected.

Each `MATCH` or `OPTIONAL MATCH` may override the session selection. Place `IN`
after the complete pattern list, before `WHERE` and existing subsequent clauses:

```cypher
MATCH (p:Person) IN team_a
WHERE p.id = 1
RETURN p;

MATCH (p:Person) IN GLOBAL, team_a, team_b
WHERE namespace(p) IN ['team_a', 'team_b'] OR namespace(p) IS NULL
RETURN namespace(p), p.id;

MATCH (p:Person) IN ALL NAMESPACES
RETURN namespace(p), count(*);
```

An explicit clause selection replaces the session selection for that clause only.
It does not change the connection. Separate clauses can therefore bind elements
from different namespaces and join them by ID or user properties:

```cypher
MATCH (a:Person) IN team_a
MATCH (b:Person) IN team_b
WHERE a.id = b.id
RETURN a.name, b.name;
```

`WHERE namespace(p) = 'team_b'` filters an already scoped pattern. It never expands
a global-only or `team_a` scan. Optimizer rewrites must preserve this rule, including
under `OR`, negation, parameters, and `IS NULL`. Names passed as strings to
`namespace()` comparisons are values: an unknown name produces no match, whereas
an unknown identifier in an `IN` or `USE NAMESPACE` clause is an error. String
comparison retains the existing string equality rules, which may differ from
identifier comparison. Pushdown must preserve the actual string result, including
case, rather than resolving arbitrary string values as namespace identifiers.

### 6.2 Inline namespace selectors

`IN` may appear once at the end of a node or bracketed relationship pattern,
after any labels/types, recursive detail, and property map. All existing pattern
components remain optional and keep their current order. In schematic syntax:

```text
node         := '(' [variable] [labels] [properties] [IN selector] ')'
relationship := '[' [variable] [types] [recursive-detail] [properties] [IN selector] ']'
selector     := namespace-list | ALL NAMESPACES
```

Selectors use the same namespace identifiers, quoting, `GLOBAL`, and comma-separated
lists as clause selection. They are not string literals, variables, or expressions:
`IN n0` names the namespace `n0`, even if a query variable has that name. Parameters
and dynamic expressions remain in `WHERE namespace(...)` predicates in this version.
Use `GLOBAL`, not `NULL`, for global membership. Unknown named identifiers are
binding errors even if another restriction would make the pattern unsatisfiable.

```cypher
MATCH (a0 IN n0)-[r0 IN n1]->(a1 IN n2)-[r1 IN n3]->(a2 IN n4)
RETURN a0, r0, a1, r1, a2;

MATCH (a:my_node_type {foo: 36, bar: 42} IN my_ns)
RETURN a;

MATCH (a:Person IN GLOBAL, team_a)-[r:Knows {since: 2026} IN team_b]->(b)
RETURN a, r, b;
```

Anonymous patterns such as `(:Person IN team_a)` and `[:Knows IN team_b]` are also
valid. Relationship brackets are required to attach a selector to an otherwise
undetailed edge: use `-[IN team_a]->`, rather than adding `IN` beside an arrow.

For reads, first determine the **base scope B** from the clause's outer `IN`, or
from the session when the clause has none. For each element occurrence x, its
effective scope is `E(x) = B intersect I(x)`, where I(x) is its inline selector,
or all namespaces when no inline selector is present. Inline selection narrows B;
it never overrides or broadens it. `IN ALL NAMESPACES` inside a pattern is therefore
a no-op. An empty intersection produces no match, with normal optional-match null
behavior. Sibling patterns do not inherit each other's inline selectors.

For example, with no session restriction this compact form:

```cypher
MATCH (a:Person IN team_a)-[r:Knows IN GLOBAL]->(b:Person IN team_b)
RETURN a, r, b;
```

has the filtering behavior of the following form, using the stored namespace names:

```cypher
MATCH (a:Person)-[r:Knows]->(b:Person)
WHERE namespace(a) = 'team_a' AND namespace(r) IS NULL
  AND namespace(b) = 'team_b'
RETURN a, r, b;
```

In a `team_a`-only session, that pattern has no matches: its inline selectors cannot
expose global edges or `team_b` endpoints. Add an outer `IN GLOBAL, team_a, team_b`
or `IN ALL NAMESPACES` to replace the session restriction for that clause.

Repeated occurrences of the same variable add constraints; their selectors do not
change its membership. `(a IN team_a)` and `(a IN team_b)` in the same match cannot
bind the same entity when the namespaces differ. Inline filters belong to pattern
matching itself, including inside `OPTIONAL MATCH` and nested queries; they must not
be moved to a later filter that would eliminate an optional match's null result.

### 6.3 Relationships, paths, and nested queries

For a pattern evaluated with base scope B and the element scopes above:

- Every bound node must belong to its own effective scope E(node).
- Every bound relationship must belong to E(relationship), while its endpoints
  satisfy their own node scopes. The edge's inline selector does not constrain
  either endpoint to the edge's namespace.
- All elements must also belong to B. Direct relationship scans with no explicit
  endpoint patterns check both endpoints against B.

For example, an edge owned by `team_a` between a `team_a` node and a `team_b` node
is visible in base scope `team_a, team_b`, and invisible in either singleton base
scope. Global nodes require explicit inclusion of `GLOBAL` in a named base selection.

On a variable-length relationship, its inline selector applies to **every traversed
edge**, not to the path/list value. The explicit start and end node selectors apply
to their endpoints; intermediate nodes use B, with any additional existing recursive
comprehension predicates. For example, in an unrestricted session:

```cypher
MATCH (a:Person IN team_a)-[r:Knows*1..3 IN team_a]->(b:Person IN team_b)
RETURN a, r, b;
```

requires every traversed edge to belong to `team_a`, but does not force intermediate
nodes into `team_a` or `team_b`. An outer clause selector can restrict all intermediate
nodes too. Zero-hop paths must satisfy both endpoint selectors; their edge selector
is vacuously satisfied because they contain no edges.

Traversal restrictions apply during expansion, before shortest-path or recursive
algorithms choose results. Finding a path in the unrestricted graph and filtering
its final endpoints is incorrect. Relationship-only scans must enforce endpoint
visibility too, in both storage directions.

Unlabeled patterns, label unions, optional matches, `EXISTS`/`COUNT` subqueries,
aggregations, and graph algorithms use the same scoping rules. An optional pattern
with no visible match yields the existing null result behavior. A nested pattern
inherits its enclosing clause selection unless it supplies an explicit `IN`.
Each `UNION` branch starts from the statement's session selection. A later top-level
`MATCH` without `IN` also returns to the session selection.

`WITH` carries previously bound values without rebinding them. Reusing an entity
in another `MATCH` checks it against that occurrence's base and inline scopes.
Scalar projection and `RETURN` do not reapply a namespace filter to values already
bound by a clause.

`namespace(x)` accepts node or relationship values and returns the catalog's stored
namespace name as a `STRING`, or `NULL` for global/null input; other input types are errors. It is
read-only and cannot be assigned. An unmatched optional variable cannot be distinguished
from a global element by this function alone; test the variable itself for null.
Existing labels, relationship types, property maps, and default API result shapes
do not acquire another field implicitly. Applications request membership explicitly.

## 7. Writes, moves, and deletion

### 7.1 Deterministic write targets

```cypher
CREATE (p:Person {id: 1, name: 'Alice'}) IN team_a;
CREATE (p:Person {id: 2, name: 'Shared'}) IN GLOBAL;
MERGE (p:Person {id: 1}) IN team_a
ON CREATE SET p.name = 'Alice';
```

Resolve one target for each new/unbound node and relationship of `CREATE` or `MERGE`
in this order:

1. Its inline `IN <one-namespace>` supplies its target.
2. Otherwise, an outer clause `IN <one-namespace>` supplies the default target.
3. An unrestricted session, including after `USE NAMESPACE ALL NAMESPACES`, defaults
   to global. All-namespace visibility does not mean all-namespace insertion.
4. A finite singleton session selection supplies its one namespace.
5. A finite selection of several namespaces requires an inline or clause target
   for every unbound element, even if only one selected namespace contains data.

A preceding `MATCH ... IN ...` does not change this write default. A write target
can be outside the session's read selection; the explicit target expresses that intent.

An outer write-clause target is a default, so more specific inline targets can
differ. Each new element's table must admit its resolved target. For example:

```cypher
CREATE (a:Person {id: 10, name: 'Ann'} IN team_a)
       -[:Knows IN GLOBAL]->(b:Person {id: 11, name: 'Ben'} IN team_b);

MERGE (p:Person {id: 1} IN team_a)
ON CREATE SET p.name = 'Alice';
```

These write selectors supply targets rather than intersecting the session's read
selection. A newly bound `MERGE` pattern matches each element in its resolved target
and creates missing elements there. All unbound elements must have deterministic
singleton targets before execution, even if `MERGE` could find a complete match.
`MERGE` never adopts an equal key from another namespace; a global key conflict
outside its element's target is an error. Normal `ON MATCH`/`ON CREATE` behavior
otherwise remains unchanged.

Previously bound endpoints retain their identities and membership; an outer write
target does not constrain them. An inline selector on a bound entity in a write
pattern is an assertion about its existing membership. A mismatch fails the
statement atomically; it does not move the entity. Conflicting inline targets on
repeated occurrences of a new variable also fail. Use `SET ... IN ...` for moves.

Inline and outer selectors in `CREATE` or `MERGE` must each name exactly one
namespace, including when asserting a bound entity's membership. Lists and
`ALL NAMESPACES` are rejected in write patterns. Property updates do not change
namespace membership. Write patterns remain subject to existing table and endpoint
constraints; inline syntax does not bypass key checks or table membership policies.

### 7.2 Move membership

```cypher
MATCH (p:Person) IN team_a
WHERE p.id = 1
SET p IN team_b
RETURN namespace(p), p.id;

MATCH (p:Person) IN team_b
WHERE p.id = 1
SET p IN GLOBAL;
```

`SET <node-or-relationship-variable> IN <one-namespace>` is a dedicated membership
update. `SET p IN NULL` is not supported; use `GLOBAL` consistently with other syntax.
An optional unmatched null variable is a no-op, consistent with property assignment.

Resolve the destination and validate table membership before mutating anything.
For nodes with local keys, reserve/check the destination key, remove the old index
entry, insert the new one, and update membership in the same transaction. A global
key's value and uniqueness entry remain unchanged. Moving to the current namespace
is a no-op. Conflicting rows, including conflicts among the statement's own moved
rows, fail the statement atomically; there is no last-writer-wins behavior or
implicit swap of local keys.

Moving a node leaves incident relationships in place with stable endpoints. It may
make an edge visible or invisible under a particular scope. A node move does not
scan or rewrite all incident edges. This follows from permitting cross-namespace
relationships and is important for high-degree nodes.

Membership changes are visible to subsequent operations in the same transaction.
`RETURN` can return an element just moved outside its original match scope. The
scope selects bindings; it is not an output redaction or security mechanism.

### 7.3 Delete without hidden side effects

`DELETE` selects entities through the normal scoped `MATCH`; ordinary node deletion
still fails if incident relationships exist, including relationships outside scope.

`DETACH DELETE` uses the base scope B of the most recent `MATCH` that bound or
revalidated its target variable, before applying inline selectors or `WHERE`.
Both forms filter bindings without narrowing the scope for incident-edge deletion.
For example, `MATCH (p:Person IN team_a) IN ALL NAMESPACES DETACH DELETE p` can
delete incident edges across namespaces, just as the equivalent `WHERE` form can.
`WITH` and aliases preserve that base scope; variables introduced by `CREATE` or
`MERGE` start with their singleton target scope. This is
binding metadata, not another stored property of the entity.

Inspect all incident relationships and their endpoints before deleting. If any
incident relationship is outside that scope, or has either endpoint outside it,
fail the whole statement. Do not delete hidden relationships
silently or leave dangling edges. Use an explicit `MATCH ... IN ALL NAMESPACES` and
a restrictive `WHERE` to authorize the necessary incident-edge deletions. With
several delete targets, validate every target and its incident relationships before
mutating any of them. This check may cost degree-proportional work when namespaces
are in use; ordinary global-only deletion retains its existing implementation.

## 8. Loading, export, and other features

- **COPY FROM:** extend the table target as `COPY <table> [(<columns>)]
  [IN <one-namespace>] FROM ...`, with existing options following the source. The
  same target-resolution rules as `CREATE` apply. Every row in one COPY has one
  target; missing namespace metadata never means “all.” Local-key relationship
  endpoint lookups use `(target namespace, endpoint key)`; global-key lookups use
  the key and must still find endpoints in that target. COPY of cross-namespace
  edges is not supported initially; use scoped `MATCH` plus `CREATE`. Do not guess
  an endpoint when keys repeat.
- **CREATE TABLE AS:** retain existing node/relationship `AS` forms and insert table
  membership before the body/`AS` as above. Source queries obey their own scopes;
  loaded rows use global in an unrestricted session or the singleton selected
  namespace, independent of the table's allowed set. A finite multi-namespace
  session is an error for CTAS in this version. The load target is not adjusted to
  the policy: when the declared policy excludes it, as `IN team_b` excludes global
  in an unrestricted session, the statement is refused and creates no table. Select
  the target first (`USE NAMESPACE team_b`), or create the table and load it with
  `COPY ... IN team_b`. Rel CTAS uses
  the same single-namespace endpoint resolution as COPY. Source membership is never
  inherited implicitly, and the inferred node primary key defaults to global scope.
- **COPY TO / query export:** export selected data according to query scope. Include
  `namespace(...)` explicitly when exporting membership as an application column.
  A database-level export/import or backup/restore must instead preserve all namespace
  entries, table policies, key scopes, memberships, and identities independently of
  session scope. Logical import into a new catalog remaps internal IDs consistently.
- **ATTACH and graphs:** operate namespace-specific scopes after selecting the owning
  native catalog. For statements spanning catalogs, the first version supports
  unrestricted and global-only scopes, and rejects scopes containing named
  namespaces. Unrestricted queries read all data in each referenced native catalog;
  a global-only scope selects ID zero independently in each catalog. To select a
  named namespace in an attached database, first select its owning catalog. Never
  silently apply one catalog's named namespace IDs to another.
- **Partitioned tables:** namespaces and physical partitions are independent axes.
  Correct full support would require shared membership policies, parent-wide
  local/global uniqueness, and endpoint routing over children. Until those exist,
  reject `LOCAL PRIMARY KEY`, finite membership policies, and named inserts/moves
  for partitioned parents and children. Global-only partitioned tables retain all
  existing behavior. A named-only selection sees no rows in them; a scope containing
  global sees their existing rows.
- **Foreign/Arrow/Iceberg storage and ANY graphs:** initially expose only implicit
  global membership. Named writes, moves, and finite policies require explicit
  support and otherwise fail. A selected scope excluding global produces no matches;
  it must not bypass scope through a foreign scan.
- **GDS, FTS, vector search, and raw CSR/Arrow paths:** each integration must enforce
  the same scope before namespace-aware use is enabled. Projected graphs capture
  their scope and snapshot; reusing one under another scope requires rebuilding or
  an error. Search filters must be applied within candidate generation where needed
  for correct top-k semantics. Falling back to a correct scoped scan is acceptable;
  returning an unfiltered result is not. Unsupported paths fail explicitly.

These limits concern combinations with the new feature. Existing global-only paths
remain usable and retain their existing semantics and limitations.

## 9. Storage and execution design

### 9.1 Implicit global representation

A table without element-membership storage means every element is global, under
the relevant transaction snapshot. Do not add a stored namespace column, nullable
bitmap, wider element ID, namespace prefix in a global key, per-row allocation, or
namespace index to such a table. Creating an empty named namespace alone must not
convert any table or rebuild its indexes.

Represent nondefault catalog metadata through optional structures. The absence of
metadata supplies `ALL NAMESPACES` table policy and global primary-key scope for
legacy tables. Explicit `LOCAL` keys opt into a numeric index directory; its global
domain uses the existing scalar-key index implementation. Ordinary global keys
retain the original index without that directory.

Namespace-aware physical operators are selected while binding/mapping or initializing
a statement. When every participating table and relationship is implicitly global:

- Read element scopes containing global use existing scans, lookups, joins, and
  traversal; global write targets retain existing insert/update/delete, COPY, and
  index implementations.
- Any required read element scope excluding global makes that pattern empty without
  reading data pages. Preserve optional-match null results and any permitted
  zero-hop alternatives whose endpoint scopes are satisfied.
- `namespace(x)` is a constant-null projection, respecting null input.
- The original execution loop has no namespace branch, indirect namespace predicate,
  membership read, extra vector, hash prefix, lock, or catalog lookup per element.

Feature-state checks may occur at statement planning/initialization, outside existing
hot loops. Namespace-free statements must retain plan reuse. A generic filter with
an always-true predicate does not meet this requirement.

Linux shared builds bind references between engine function definitions locally
with `-Bsymbolic-functions`. This keeps added symbols from adding PLT indirection
to existing internal calls. Public function exports, data symbols, and RTTI remain
available to API and extension consumers; registered callbacks retain normal
dispatch. Interposing another definition of a core function no longer replaces
calls to it from inside the engine. This is the
[GNU ELF function-binding behavior](https://sourceware.org/binutils/docs/ld/Options.html),
and cross-module API ownership, RTTI, callbacks, and exceptions are verified separately.

Reads with unrestricted scopes on every element need no membership filtering even
when a table contains named elements. Reuse existing scan/traversal loops in that
case too, unless the query actually requests membership values. Local-key lookups
still need their index
directory to find every matching key across namespaces; global-key probes do not.

### 9.2 Lazy membership storage

On the first non-global insert or move for a native table, create optional,
transactional membership storage keyed by its existing stable element offset.
Existing elements are implicitly ID zero; avoid an eager rewrite of every row.
For relationships, associate membership with the stable relationship ID, consistently
across forward and backward adjacency storage, rather than mutable CSR positions.

Use implicit-global and uniform-namespace encodings for storage groups where possible,
and a compressed membership vector for mixed groups. Membership updates use MVCC
versions and WAL just like properties. A table/global flag or uniform-group marker
must describe the reader's snapshot: a current-state marker cannot override older
membership versions. Promotion is atomic with the first named write. Readers with
older snapshots continue seeing the original global membership.

The implementation uses MVCC catalog groups of 1,024 numeric memberships. Missing
groups mean global. Persisted groups choose uniform, run, or sparse encoding;
execution reads numeric arrays and builds native bitmap masks for named-only scans.
An optional numeric `(table ID, group index)` directory locates the MVCC chain;
catalog names remain the DDL/WAL identity and are not compared for each key probe.
The directory follows replacement, rollback, recovery, and checkpoint load. A
separate `(table ID, namespace ID)` group directory prunes singleton/finite scans
and supports selectivity estimates. Both directories are absent when unused.

A membership reader keeps its current group inline. A singleton index probe or
single-group scan allocates no group-cache map; only visits to additional groups
populate that cache. Transaction-local membership changes invalidate cached groups
by numeric generation, preserving reads after moves and rollback.

Repeated singleton scans cache selected 16-bit offsets within each committed
membership group. The cache is built lazily using numeric SIMD comparisons and
feeds the existing bitmap mask. It belongs to an immutable MVCC version, is neither
serialized nor copied to a writable version, and cannot override a transaction's
uncommitted changes. Readers of older snapshots retain their version's offsets.
Finite scopes containing GLOBAL use the table's numeric group directory and fill
implicit-global gaps between materialized groups. They do not enumerate namespace
names. A table whose snapshot policy still indicates implicit-global membership
keeps its direct scan path without constructing a namespace bitmap.

Dropping or emptying a namespace need not immediately remove its storage. Reclaim
versions only after snapshot safety. Membership writes prune versions older than
the oldest active transaction's visible version, retaining that boundary and every
newer version for cached readers and undo records. The existing transaction horizon
is published atomically, so reclamation never acquires the transaction-manager lock
under a storage lock. Long-lived readers may retain history until they end; the
next write to a group reclaims its obsolete history. Returning a table to the
implicit-global representation is an optional checkpoint optimization, with atomic metadata/index
updates and plan invalidation; it is not required after namespaces have been used.

At commit, finalize only the membership groups touched by that transaction. Native
relationship IDs staged in local storage are remapped to their reserved persistent
offsets before WAL serialization. A node-only write must not enumerate unrelated
table policies, namespace definitions, or membership groups.

The initial implementation versions membership in 1,024-element groups. Concurrent
writers changing the same group can therefore encounter a write-write conflict
even for distinct element offsets; retry the transaction. Writes to distinct groups
retain independent membership versions. Batching inserts or moves amortizes the
group copy and WAL cost; singleton reads do not scan those histories or unrelated
namespaces when a current snapshot and direct directory entry suffice.

### 9.3 Primary-key implementation

Keep the existing index unchanged for global keys. For local keys, implement the
logical composite key through a directory of scalar-key indexes keyed by namespace
ID. An unchanged scalar-key index serves ID zero; create named indexes lazily.
Explicit local keys have the directory lookup cost even when their only domain is
global. Tables with ordinary global keys retain their original representation and
probe path.

Resolve a singleton local-key index once per probe or homogeneous insertion batch.
Reuse the membership reader across a vector of insertions. Multi-namespace
lookups probe the selected namespace indexes and union their results. Writes spanning
several targets, including inline targets within one clause, route each element's
batch to the correct namespace index.
Native point-probe visibility callbacks use inline callable storage and retain both
row visibility and snapshot-correct membership validation; they allocate no wrapper
for each index candidate.
Moves between local-key domains update both indexes transactionally. Apply the same
contract to each supported primary-key implementation, including ART; reject an
unsupported index/key-policy combination at DDL time rather than weakening uniqueness.

### 9.4 Namespace-aware execution

Resolve names to catalog-local IDs once per binding/snapshot. Retain the base scope
and separate effective node/relationship scopes in bound patterns and scan/traversal
state. Do not collapse them into one shared selector or repeated string comparisons.
Compile inline selectors directly to those scopes and push them into scans, key
probes, and traversal; do not materialize `namespace(...)` strings to evaluate them.
Use namespace/group pruning, vectorized membership selection, and
per-namespace cardinalities where available. Include visibility checks on primary
and secondary index hits and on all relationship expansion routes.

Apply node and relationship restrictions before expansion/aggregation as semantics
require. Namespace predicates eligible for pushdown narrow the effective set by
intersection. Keep correct residual evaluation for dynamic or complex expressions;
parameters must not freeze a previous execution's namespace predicate.

When namespaces are used, the additional metadata, index partitions, visibility
checks, and cross-namespace unions have a real cost. The design aims to prune work;
it does not claim that an arbitrary scoped query costs exactly the same as an
unrestricted query. There must be no cost proportional to the count of unrelated
namespaces for a singleton scan/probe when its storage can be pruned directly.

## 10. Transactions, recovery, and plan reuse

Namespace DDL, table policies, membership updates, uniqueness checks, statistics,
and index changes participate in existing transactions. A failed statement leaves
no partial catalog or data change, following the engine's normal rollback rules.
Readers observe one consistent snapshot across membership and graph data. Concurrent
namespace drop, table-policy removal, promotion, and writes must conflict or serialize
so a committed element can never reference an absent or disallowed namespace.

WAL records must carry sufficient owner identity and namespace IDs to replay DDL,
named inserts, moves, and local-key index changes exactly once. Checkpoint includes
namespace catalog state, ID allocation high-water marks, optional table metadata,
membership versions needed by recovery, and index-directory metadata. Crash recovery
must not temporarily expose moved rows in both namespaces or lose their keys/edges.
A rename is logged as the existing catalog-entry drop and create records, in that
order, both carrying the same namespace ID; it needs no new WAL record type and no
storage-version change, and replay restores the ID rather than allocating one.

Prepared statements use the namespace selection current at **execution** for their
base scope, unless a clause supplies explicit outer `IN`. Inline read selectors
still intersect that base; they do not make the statement independent of session
selection. Record the session-selection generation, owner catalog identity,
resolved outer and inline namespace identities, table/key policies,
and membership-storage mode as plan dependencies. Rebind/remap when any dependency
changes; an already running statement keeps its snapshot and chosen state. Use the
existing catalog/plan-version infrastructure, with a session generation for selection
changes. Parameter-dependent predicates are rebound or evaluated per execution.
A plan bound in an earlier transaction that a concurrent schema change made stale
is rebound and run again by the engine rather than returning an error to the caller.
A plan bound in the transaction it runs in, explicit or auto-commit (every statement
of an ad-hoc query, and every rebind), was bound against that transaction's own
snapshot, so another connection's DDL does not make it stale and it is not compared
against the shared catalog version; a rebind therefore always makes progress. A
statement that depends on no namespace (none in its catalog, no session selection)
is not checked at all and runs as it did before namespaces existed.

Creating/dropping/renaming namespaces, changing finite/all policies, or promoting a table
invalidates affected plans. `ALL` plans account for newly created namespaces on the
next execution. Dropping and recreating the same name must never reuse old bindings.
A rename changes the name-to-ID mapping, so a rebind resolves the statement's namespace
names again: a statement whose text names the old name fails on its next execution
with the same unknown-namespace error a fresh statement gets, rather than silently
following the renamed identity; once a new namespace takes the old name, the statement
binds to that new identity. Names that were never resolved as identifiers, such as the
string in `namespace(p) = 'team_a'`, compare against the current name after the rename.
A selection keeps its identities (section 6.1), and `namespace(...)` returns the new
name in every transaction that starts after the rename commits, on any connection.
Transactions that started earlier keep the name of their snapshot.
The namespace-free plan/cache path must not be disabled to simplify implementation.
Physical scan descriptors represent an unrestricted scope by a null pointer; scoped
descriptors share an immutable numeric selection. Unrestricted scans skip namespace
mask initialization, including when their prepared physical plans are reused.
For fully covered bitmap ranges, preserve the existing selection and dense column
scan instead of materializing every row offset. Any active SIP mask still applies.
Treat an equality probe on a LOCAL key as unique when its effective namespace set
is a singleton; unrestricted and multi-namespace probes retain their multi-match
behavior. A changed session selection must rebind that cardinality assumption.

Persisting the new metadata requires an explicitly versioned storage/WAL extension.
The baseline supports storage versions through 49; the rename plan's statement
that version 47 stays unchanged describes its earlier rename-only work. Namespace
metadata uses format 50, with retained readers for existing formats.
Do not append bytes that an old reader could interpret as existing fields.

Open namespace-free files with implicit defaults and no eager row/index rewrite.
Keep their existing format until a namespace-specific persistent change requires
upgrade. Before committing such a change, durably mark the database as requiring
the new reader, using the engine's atomic header/checkpoint protocol; recovery must
recognize the upgrade even if the namespace mutation was only in WAL. Older engines
must reject it safely. Backup before upgrade and logical export are the downgrade
route; no transparent downgrade is promised.

## 11. End-to-end example

Run the following statements in order on an empty database. Comments give
expected behavior; failure examples are separate so execution can continue.

```cypher
CREATE NAMESPACE team_a;
CREATE NAMESPACE team_b;

CREATE NODE TABLE Person IN GLOBAL, team_a, team_b (
    id INT64 LOCAL PRIMARY KEY,
    name STRING
);
CREATE NODE TABLE Asset IN ALL NAMESPACES (
    id INT64 GLOBAL PRIMARY KEY,
    name STRING
);
CREATE REL TABLE Knows (FROM Person TO Person);

// A fresh connection reads all namespaces. Plain CREATE still targets GLOBAL.
CREATE (:Person {id: 1, name: 'Shared contact'});
CREATE (:Person {id: 1, name: 'Alice'}) IN team_a;
CREATE (:Person {id: 1, name: 'Bob'}) IN team_b;

// Local keys allow all three id=1 nodes. Default reads see all three.
MATCH (p:Person)
RETURN p.name ORDER BY p.name;
// Alice
// Bob
// Shared contact

// Select global explicitly to narrow that result.
MATCH (p:Person) IN GLOBAL
RETURN p.name;
// Shared contact

MATCH (p:Person) IN ALL NAMESPACES
RETURN namespace(p), p.name ORDER BY p.name;
// team_a | Alice
// team_b | Bob
// NULL   | Shared contact

// Bind two namespaces in one clause and create an edge owned by team_a.
MATCH (a:Person {id: 1} IN team_a), (b:Person {id: 1} IN team_b)
CREATE (a)-[:Knows IN team_a]->(b);

MATCH (a:Person)-[r:Knows]->(b:Person) IN team_a
RETURN a.name, b.name;
// No rows: Bob is outside this scope.

MATCH (a:Person)-[r:Knows]->(b:Person) IN team_a, team_b
RETURN a.name, namespace(r), b.name;
// Alice | team_a | Bob

// Inline selectors express the precise scope of each path element.
MATCH (a:Person IN team_a)-[r:Knows IN team_a]->(b:Person IN team_b)
RETURN a.name, namespace(r), b.name;
// Alice | team_a | Bob

USE NAMESPACE team_a;
CREATE (:Person {id: 2, name: 'Cara'});
// The singleton session supplies the write target.

MATCH (p:Person) WHERE namespace(p) = 'team_b'
RETURN p.name;
// No rows: WHERE cannot broaden the team_a session scope.

MATCH (p:Person) IN team_a WHERE p.id = 2
SET p IN team_b;
// Cara keeps her ID and key; Person/team_b/id=2 was free.

USE NAMESPACE GLOBAL, team_a, team_b;
MATCH (p:Person)
RETURN namespace(p), count(*);
// NULL | 1; team_a | 1; team_b | 2 (row order unspecified).

CREATE (:Asset {id: 100, name: 'Server'}) IN team_a;
// id=100 is now reserved across Asset in every namespace.

// Knows admits future namespaces; Person has an explicit finite list.
CREATE NAMESPACE team_c;
ALTER TABLE Person ADD TO NAMESPACE team_c;

// The inline selector picks Alice; the outer scope allows incident-edge deletion.
MATCH (p:Person IN team_a) IN ALL NAMESPACES
DETACH DELETE p;
// Deletes Alice and her Knows edge; Bob and Cara remain.

MATCH (a:Asset) IN team_a DELETE a;
ALTER TABLE Person REMOVE FROM NAMESPACE team_a;
DROP NAMESPACE team_a;
USE NAMESPACE ALL NAMESPACES;
// Restores unrestricted reads and global as the default write target.
```

Each of these statements is an independent expected failure at the indicated point:

```cypher
// After Bob exists, moving Alice would collide with Person/team_b/id=1.
MATCH (p:Person) IN team_a WHERE p.id = 1 SET p IN team_b;

// After Server exists, a global key conflicts even in another namespace.
CREATE (:Asset {id: 100, name: 'Other server'}) IN team_b;

// With multiple session namespaces selected, insertion has no implicit target.
CREATE (:Person {id: 3, name: 'Ambiguous'});

// Before cleanup, the namespace still contains elements and a table reference.
DROP NAMESPACE team_a;
```

## 12. Implementation map and delivery

Repository locations relevant to the design are:

| Layer | Existing integration points | Required work |
| --- | --- | --- |
| Grammar/transform | `src/antlr4/Cypher.g4`, `src/antlr4/keywords.txt`, `src/parser/transform/` | Namespace DDL, outer and inline selection, final selector placement in node/relationship patterns, key scope, moves, COPY/CTAS. Keep new keywords nonreserved where possible. Follow [grammar generation](docs/grammar.md); change source and regenerate artifacts through the existing process. |
| AST/binder | `src/include/parser/query/graph_pattern/node_pattern.h`, `rel_pattern.h`, `src/include/binder/`, `src/binder/bind/bind_ddl.cpp`, `bind_graph_pattern.cpp`, `bind_updating_clause.cpp` | Optional inline selectors on node/rel AST entries; separate table allowed sets, base scope, per-element read scopes, and write targets; validate combinations and bind stable IDs. |
| Catalog | `src/include/catalog/catalog.h`, `src/catalog/catalog.cpp`, node/rel table entries | MVCC namespace entries, optional policies, key scope, dependencies, serialization, ID lifetime. |
| Session/cache | `src/include/main/client_context.h`, `src/main/client_context.cpp`, `src/main/database_manager.cpp` | Connection selection, catalog switches, selection generation, preparation/rebinding dependencies. |
| Planner/mapper | `src/planner/`, `src/processor/map/map_scan_node_table.cpp`, `map_index_scan_node.cpp`, `map_recursive_extend.cpp` | Scope propagation, empty/global specialization, namespace-aware scan/index/traversal selection. |
| Storage/index | `src/storage/table/`, `src/storage/local_storage/`, `src/include/storage/index/hash_index.h`, `art_index.h` | Lazy membership storage, local index directory, stable IDs, atomic moves, endpoint visibility. |
| Recovery | `src/storage/wal/`, catalog/table serializers, `src/include/storage/storage_version_info.h` | Versioned persistence, upgrade protocol, replay and checkpoint validation. |
| Integrations | `src/graph/on_disk_graph.cpp`, GDS/projection, COPY/export, extension entry points | Scope enforcement, result metadata on request, support checks and explicit errors. |

Deliver in dependency order: (1) catalog/grammar/session semantics and global-path
specialization; (2) lazy membership persistence and recovery; (3) reads, index access,
all traversal routes, writes and moves; (4) bulk operations, integrations, diagnostics,
and full acceptance. Do not advertise namespace support before all required paths
either enforce scope correctly or reject unsupported combinations explicitly.
Catalog diagnostics must expose namespace names, table membership policy, primary-key
scope, and element counts per namespace without relying on an active read selection.
Namespace DDL and scope are not permission checks; existing database access governs
who may execute them.

## 13. Acceptance and performance verification

### 13.1 Semantic and recovery coverage

Use existing test harnesses with meaningful end-to-end cases for:

- Legacy database open and global-only behavior; default/explicit/all scopes; global
  null semantics; quoted identifiers; duplicate, absent, dropped/recreated, and renamed names.
- Shared schemas; finite/all/future table policies; restrictive namespace/table
  removal; global/local keys including global-domain duplicates and hidden conflicts.
- Multi-scope joins; unlabeled/union patterns; optional/nested/negated predicates;
  parameter changes; repeated prepared execution before and after selection changes.
- Inline selector placement with labels/types/properties and anonymous patterns;
  lists, global/all and quoted names; identifier/variable distinction; repeated
  variables, empty intersections, optional null results, and outer/session precedence.
- Cross-namespace endpoints; edges in a third namespace; both traversal directions;
  separate endpoint and edge selectors; recursive and shortest paths with hidden
  intermediate elements, per-hop edge selection, and zero-hop behavior; direct rel scans.
- CREATE/MERGE/COPY/CTAS targets; ambiguous-target errors; atomic moves, preserved
  IDs/edges, destination collisions, statement rollback, and hidden-edge deletion.
- Inline write targets overriding clause defaults; mixed-namespace creation and
  MERGE in one clause; bound-variable assertions, conflicting targets, and rejected
  list/all write selectors. Equivalent inline/WHERE filters retain the same
  DETACH DELETE scope and atomic failure behavior.
- Concurrent drop/remove/write races; old snapshots during first promotion and moves;
  crash injection before/after header upgrade, WAL commit, index updates, and checkpoint.
- Supported extension/GDS paths and explicit failures for unsupported combinations;
  graph/database switches, database export/import, backup/restore, and old-reader rejection.

The executable regression check `test/verify_namespace_example.py` extracts the
worked example verbatim, executes it, and verifies the resulting data and catalog.

### 13.2 Performance contract

Compare the frozen pre-change release and delivered implementation with the same
toolchain, release preset/cache options, SIMD settings, execution-thread settings,
and hardware/data, using warm/cold runs and repeatable seeds. Include
legacy files and newly created namespace-free files. Measure both normal execution
and cached prepared statements. Keep namespace-free CI/benchmark results as the
baseline rather than comparing different data layouts or query results.
Report intentional changes to default compilation/linking as part of the
implementation, rather than silently changing the benchmark configuration. The
acceptance comparison uses the delivered release against the frozen pre-change
artifact. Also use a baseline with matching ELF function binding as an attribution
control, distinguishing namespace costs from the effect of that optimization.

The no-use baseline has no named namespace definitions or namespace-specific table
policies. Its suite includes scan/filter/aggregate, primary-key point lookup and MERGE,
one-hop and recursive traversal at low/high degree, property updates, CREATE/DELETE/
DETACH DELETE, COPY, checkpoint/recovery, and database startup. Measure latency,
throughput, CPU instructions, allocations, peak memory, index size, and data/WAL size.
Also measure databases with many empty named namespaces but only global elements,
and global-only tables inside a database whose other tables contain named elements.
Their data operations must keep the global specialization wherever applicable.
Creating/loading actual namespace catalog entries has a metadata cost; report it
separately from the unchanged per-element paths rather than claiming it is free.

Acceptance requires:

1. Equivalent global-only plans and unchanged row/edge/index representation. No new
   namespace work in the existing execution loops; verify with plan inspection and
   profiling, including primary-key visibility callbacks and traversal code.
2. No membership pages, per-element bytes, or namespace index allocations for
   implicitly global tables. Optional catalog/session state may add bounded metadata,
   never memory proportional to the number of elements when the feature is unused.
3. No repeatable performance regression on the no-use suite. Report distributions
   and confidence intervals; “not statistically significant” alone is insufficient.
   Require the upper 95% confidence bound on slowdown to be at most 1%, as a
   measurement tolerance, and investigate any stable regression within that margin.
4. No unrelated-namespace scan/probe work for singleton queries when pruning or
   direct index routing applies. Measure mixed/uniform memberships, namespace skew,
   many namespaces, local/global keys, and high-degree cross-namespace edges separately.
5. All existing applicable tests pass, and all new semantic/recovery cases pass.

Absolute zero wall-clock difference cannot be established by a design document or
finite benchmarks. The enforceable guarantee is unchanged hot-path work and storage
for the unused feature, backed by the measurements above. Namespace-aware overhead
must be reported separately; it cannot be hidden in the global-only baseline.
