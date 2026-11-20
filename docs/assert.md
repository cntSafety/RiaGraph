# RiaGraph ASSERT: technical concept

**Status:** implemented. Issue: cntSafety/RiaGraph#113. Tests:
`test/test_files/assert/*.test` and `test/common/assert_clause_test.cpp`.

## 1. Purpose and scope

`ASSERT` lets a Cypher statement state a condition that must hold, and fail when it does not:

```cypher
ASSERT <condition> [AS '<message>']
```

The condition is a BOOLEAN expression. The message is a string literal. Three uses motivate it:

1. **Testing the database itself.** A test script states what the engine must have produced,
   e.g. that a `MERGE` did not duplicate a node, and fails on the first statement that breaks
   it, with a message saying what broke.
2. **Testing queries.** A query under development states what it expects of its intermediate
   results (`ASSERT count(b) > 0`) without changing what it returns.
3. **Contracts at application runtime.** A query states its preconditions on parameters and on
   the graph, and its post-conditions and invariants, and refuses to run, or to commit, when
   they are violated ("contract-based design in queries").

`ASSERT` reads; it never writes. It does not filter: a row either passes unchanged or the whole
statement fails. A statement without `ASSERT` plans and runs exactly as before (section 10).

`ASSERT` is not a declarative schema constraint. It is checked when, and only when, the statement
containing it runs; it guards nothing a later statement does. Persistent constraints
(`CREATE CONSTRAINT`) are outside this concept.

## 2. Review of the issue draft

The examples in #113 are sketches. This concept resolves them as follows.

| Issue draft | Resolution |
| --- | --- |
| `ASSERT EXISTS { MATCH (a) } AS '...'` | Supported as written. `EXISTS { MATCH … }` and `COUNT { MATCH … }` are ordinary expressions. |
| `ASSERT NOT EXISTS { MATCH (a {type:'foo'}) }` | Supported as written. |
| `ASSERT EXPRESSION { $v > 0 }` | No `EXPRESSION { … }` form. It would be a new keyword whose only effect is that of parentheses. Write `ASSERT $v > 0` or `ASSERT ($v > 0)`. |
| `ASSERT NOT EXISTS { UNWIND $p AS e WHERE e < 0 }` | `EXISTS { … }` only takes `MATCH`, and `UNWIND … WHERE` is not Cypher. List quantifiers express it: `ASSERT none(e IN $p WHERE e < 0)` or `ASSERT all(e IN $p WHERE e >= 0)`. |
| `ASSERT (count(a)==count(b))` | `==` is not Cypher; equality is `=`. The aggregate semantics are defined in section 5. Note that after `MATCH (a) MATCH (b)` the rows are the cross product, so `count(a) = count(b)` always holds; `count(DISTINCT a) = count(DISTINCT b)` is the meaningful check. |
| `ASSERT count(b)>0 AS "Graph shall not be empty" RETURN a` | Supported: the aggregate is computed over all rows that reach the clause and the rows pass on unchanged. On an empty graph it fails. |
| `CREATE (a{id:42, …}); ASSERT EXISTS { MATCH (a {id: 42}) }` inside `BEGIN TRANSACTION` | Supported: a statement may consist of `ASSERT` alone. A failure rolls back the transaction (section 7). |
| Is `NULL` a failure? Default message? Error type? | Sections 6 and 8. |

## 3. Grammar

### 3.1 EBNF

```ebnf
AssertClause    = "ASSERT" , Expression , [ "AS" , StringLiteral ] ;

ReadingClause   = Match | Unwind | InQueryCall | LoadFrom | AssertClause ;

SinglePartQuery = { ReadingClause } , Return
                | { ReadingClause } , UpdatingClause , { UpdatingClause } , [ Return ]
                | { ReadingClause } , AssertClause ;
```

`ASSERT` is a **reading clause**. It may appear wherever `MATCH`, `UNWIND`, `CALL` or
`LOAD FROM` may, and in addition a query may **end** with it. A query that ends with `ASSERT` has
no result columns, like a query that ends with an updating clause.

As for any reading clause, openCypher's rule applies that a read after a write needs a `WITH`:

```cypher
MATCH (a:Account {id: $id}) SET a.balance = a.balance - $amount
WITH a ASSERT a.balance >= 0 AS 'Account shall not be overdrawn'
RETURN a.balance;
```

### 3.2 Changes to `Cypher.g4`

```antlr
oC_SinglePartQuery
    : ( oC_ReadingClause SP? )* oC_Return
        | ( ( oC_ReadingClause SP? )* oC_UpdatingClause ( SP? oC_UpdatingClause )* ( SP? oC_Return )? )
        | ( oC_ReadingClause SP? )* iC_Assert
        ;

oC_ReadingClause
    : oC_Match
        | oC_Unwind
        | iC_InQueryCall
        | iC_LoadFrom
        | iC_Assert
        ;

iC_Assert
    : ASSERT SP? oC_Expression ( SP AS SP StringLiteral )? ;
```

`ASSERT` is added to `src/antlr4/keywords.txt` (and therefore to the generated
`tools/shell/include/keywords.h`) and to `iC_NonReservedKeywords`, so existing queries that use
`assert` as a variable, property, alias or function name keep parsing. `AS` after an expression
already ends a projection item, so `ASSERT x AS 'm'` is unambiguous. Section 3.1 keeps a query
from ending with any other reading clause: `MATCH (a)` alone is still a parse error.

The generated contexts get an `ASSERT()` accessor, which the `ASSERT(condition)` macro of
`common/assert.h` would expand; the two headers that include the generated parser undefine the
macro around that include (`#pragma push_macro`), as `logical_operator.h` does for `DELETE`.

The parser artifacts are regenerated as `docs/grammar.md` describes.

## 4. Forms

### 4.1 Statement level

A query that consists of `ASSERT` alone runs over one empty row, like `RETURN 1`:

```cypher
ASSERT EXISTS { MATCH (a) } AS 'Graph shall contain at least one node';
ASSERT NOT EXISTS { MATCH (a:Item {type: 'foo'}) } AS 'Graph shall not contain items of type foo';
ASSERT $v > 0 AS 'Parameter v shall be greater than 0';
ASSERT none(e IN $p WHERE e < 0) AS 'No negative elements';
ASSERT COUNT { MATCH (:Person)-[:Knows]->() } < 1000;
```

Several conditions can be chained; they are checked in order:

```cypher
ASSERT $from <> $to AS 'Distinct accounts' ASSERT $amount > 0 AS 'Positive amount'
MATCH (a:Account {id: $from}), (b:Account {id: $to}) ...
```

### 4.2 Clause level, per row

Inside the clause pipeline a condition without aggregates is checked for **every row that reaches
the clause**. Rows pass on unchanged: the next clause receives exactly the rows `ASSERT`
received, in the same number.

```cypher
MATCH (p:Person) ASSERT p.age >= 0 AS 'Age shall not be negative' RETURN p.name;
MATCH (a)-[r:Transfer]->(b) ASSERT a <> b AS 'No self transfers' RETURN count(r);
MATCH (p:Person) ASSERT EXISTS { MATCH (p)-[:LivesIn]->(:City) } RETURN p;
```

The condition sees every variable in scope, plus parameters. `EXISTS { … }` and `COUNT { … }`
may correlate with them as anywhere else. Zero rows reaching the clause means nothing to check:
the clause passes.

### 4.3 Clause level, aggregating

A condition that contains an aggregate (`count`, `sum`, `min`, `collect`, … with or without
`DISTINCT`) is checked **once, over all rows that reach the clause**, and the rows then pass on
unchanged. See section 5.

```cypher
MATCH (a:Person) MATCH (b:City)
ASSERT count(DISTINCT a) >= count(DISTINCT b) AS 'At least one person per city'
ASSERT count(*) > 0 AS 'Graph shall not be empty'
RETURN a.name, b.name;
```

## 5. Aggregate semantics

1. **Group.** All rows that reach the clause form one group; there are no grouping keys. An
   aggregating `ASSERT` is the check `WITH <aggregates> WHERE <condition>` would express, but it
   does not replace the rows with the group.
2. **Pass-through.** The rows leave the clause as they entered it, all of them, with the same
   variables in scope. The following clauses' cardinality is unchanged. The aggregate values are
   not visible after the clause; to use one, compute it with `WITH`.
3. **Empty input.** Over zero rows `count` is 0 and `sum`, `min`, `max`, `avg` are NULL, as in
   `RETURN count(*)` over nothing. `MATCH (b) ASSERT count(b) > 0` therefore fails on an empty
   graph, which is what a non-emptiness check is for, and `ASSERT max(b.x) < 10` fails too
   (NULL, section 6).
4. **No mixing.** Outside its aggregates the condition may not read the row: neither a variable,
   a property, nor a subquery correlated with one. `ASSERT count(*) > 0 AND a.x > 0` is a binder
   error that names `a.x`; it has no single value. Parameters, literals and uncorrelated
   subqueries are allowed. Grouped checks are written with `WITH` first:

   ```cypher
   MATCH (p:Person)-[:LivesIn]->(c:City)
   WITH c, count(p) AS residents
   ASSERT residents < 1e6 AS 'City too large'
   RETURN c.name, residents;
   ```

   An aggregate an earlier `WITH` computed is a plain value in scope, not an aggregate of this
   clause: `WITH count(*) AS n ASSERT n > 0` is a per-row check on the one row `WITH` produced.
5. **Nesting** is rejected as in a projection: `ASSERT sum(count(*)) > 0` is a binder error.
6. **Cost.** An aggregating `ASSERT` cannot let a row pass before it has seen the last one, so it
   materializes its input once (section 10).

## 6. NULL

`ASSERT` succeeds only when its condition is TRUE. **FALSE and NULL both fail.** A contract that
cannot be evaluated is not met: `ASSERT $v > 0` with `$v` NULL, or `ASSERT p.age >= 0` for a person
without an age, fails. A condition that should tolerate NULL says so:

```cypher
ASSERT p.age IS NULL OR p.age >= 0
ASSERT coalesce(p.age >= 0, true)
```

This differs from `WHERE`, which drops NULL rows silently, and from SQL `CHECK` constraints,
which accept UNKNOWN; both of those filter or constrain data, while `ASSERT` states something the
author believes to be true.

The condition must have type BOOLEAN after the usual implicit cast; `ASSERT 1` is a binder error
(`Expression 1 has data type INT64 but expected BOOL. Implicit cast is not supported.`). A parameter of unknown type is bound as
BOOLEAN where it is the whole condition (`ASSERT $flag`).

## 7. Errors and transactions

A failed `ASSERT` raises `AssertionException` (`common/exception/assertion.h`) with the message

```text
Assertion failed: <message>
```

where `<message>` is the `AS` string, verbatim, or, when `AS` is absent, the condition as written
in the query: `MATCH (p) ASSERT p.age >= 0` fails with `Assertion failed: p.age >= 0`. The text
neither depends on the failing row nor distinguishes FALSE from NULL, so it is the same on every
run and whichever worker thread meets the failing row first. Clients see it as the query result's
error message, like any other error; the prefix is stable for applications that want to tell a
contract violation from other errors. It is not an `InternalException`: those report broken engine
invariants (`DASSERT`), and their text starts `Assertion failed in file`.

The statement fails as a whole. No result rows are returned; the engine materializes the result
before returning it.

`ASSERT` behaves exactly like every other runtime error (`ClientContext::TransactionHelper`):

- **Auto-commit.** The statement's transaction is rolled back. Writes of earlier clauses of the
  same statement are undone: `CREATE (n:T {id: 1}) WITH n ASSERT n.id = 2` creates nothing.
- **Explicit transaction.** The **whole transaction** is rolled back, including the statements
  before the failing one, and the connection returns to auto-commit mode. A following `COMMIT`
  reports `No active transaction for COMMIT.` This is what a violated post-condition or
  invariant should do: the transaction does not commit.

  ```cypher
  BEGIN TRANSACTION;
  CREATE (:Person {id: 42, name: 'klaus'});
  ASSERT EXISTS { MATCH (:Person {id: 42}) } AS 'Person 42 shall exist';
  COMMIT;
  ```

- **Read-only.** `ASSERT` itself is read-only, so a statement of reading clauses and `ASSERT` runs
  in a `BEGIN TRANSACTION READ ONLY` and against a read-only database. A condition that calls a
  writing function (e.g. `nextval`) makes the statement a write, as in `WHERE`.

## 8. Decisions made by default

The issue left these open. Each has a sensible default, recorded here:

| Question | Decision | Consequence |
| --- | --- | --- |
| Is NULL a failure? | Yes, only TRUE passes. | Strict contracts; tolerance is explicit (`IS NULL OR …`). |
| Aggregates mid-pipeline? | One group over all incoming rows, rows pass unchanged. | Issue example 2 works; costs one materialization. |
| Row-dependent terms beside aggregates? | Binder error. | No ambiguous "per row and per group" condition. |
| Message type | String literal only. | Stable, row-independent errors. Expressions or parameters as message are a possible extension. |
| Default message | The condition's source text. | `Assertion failed: p.age >= 0`. |
| `EXPRESSION { … }` | Not added. | Parentheses do the same. |
| Failure in an explicit transaction | Rolls back the transaction. | Same as every runtime error in this engine; no statement-level savepoints exist. |
| `ASSERT` after an update | Needs `WITH`, as every reading clause. | No new grammar rule beside the reading clauses. |
| `ASSERT` inside `EXISTS { … }` | Not supported; those subqueries contain only `MATCH`. | |

## 9. Namespaces, EXPLAIN/PROFILE, prepared statements, parameters

**Namespaces** ([namespace.md](../namespace.md) section 1) are optional and need nothing new.
`ASSERT` reads no table itself. Its subqueries read with the statement's namespace scope like any
other subquery: under `USE NAMESPACE team_a`, `ASSERT EXISTS { MATCH (p:Person) }` checks
`team_a` only, and an inline selector narrows further,
`ASSERT EXISTS { MATCH (p:Person IN team_b) }`. A namespace restriction can never make an
`ASSERT` see more than the statement could otherwise read.

**EXPLAIN / PROFILE.** `ASSERT` is an operator of its own, `ASSERT`, shown with its condition and
message. `EXPLAIN` does not run the statement, so it never fails an assertion. `PROFILE` runs it
and reports the operator's tuple count: the number of rows it checked and let through.

An aggregating `ASSERT` appears as an `ACCUMULATE` of the input, an `ASSERT` over an `AGGREGATE`
over a scan of that accumulated table, and a `CROSS_PRODUCT` that joins its single row back.

**Prepared statements and parameters.** A condition may use parameters anywhere an expression
may. They are bound at execution, so one prepared statement passes for one value and fails for
another, and keeps working after a failure. The message is fixed at preparation.

## 10. Execution and performance

The primary requirement is that `ASSERT` costs nothing when absent. It adds a clause type, a
logical and a physical operator, and a case in the switches that dispatch on them; a statement
without `ASSERT` binds, plans, optimizes and executes along the same paths as before, with the
same plans. The shared code that changes adds constant or per-plan work only: the cross-product
mapper checks a flag no other planner path sets, the semi-mask optimizer walks the plan once to
learn that it has no `ASSERT`, and the planner walks the operators of one pipeline before it
plans a `LIMIT` or the cross product of a `CALL`/`LOAD FROM`. Nothing changes per row.

| Form | Plan | Cost |
| --- | --- | --- |
| Per row | `ASSERT` operator in the pipeline, after the operators producing its input. Evaluates the condition on each chunk, like `FILTER`, without touching the selection. | One expression evaluation per row; a pipelined, parallel operator. |
| Aggregating | `ACCUMULATE` (input materialized with every factorization group flattened, so one table row is one input row), check plan: `EXPRESSIONS_SCAN` → `AGGREGATE` → `ASSERT`, joined back by `CROSS_PRODUCT` with the one row, then a projection that drops the aggregate columns. | One materialization of the input, one extra scan of the columns the aggregates read. |
| Statement level | A one-row dummy scan, then as above. | Constant. |

The plumbing of the aggregating form is that of a correlated subquery: the check plan scans the
accumulated table back through `EXPRESSIONS_SCAN`, so it must run after the accumulation. The
planner marks its `CROSS_PRODUCT` as build-depends-on-probe;
`CorrelatedSubqueryUnnestSolver` then links the scan to the accumulate and the mapper maps the
probe side first, as for an accumulated hash join.

**Every row is checked.** An `ASSERT` that saw only some of its rows would pass or fail depending
on thread scheduling. The optimizer must therefore never let a later operator decide that rows
before the `ASSERT` need not be produced. The logical operator is classified as a side effect
(`LogicalOperatorUtils::isUpdate`), which gives it the treatment writes already get:

- a `FILTER` whose predicate folds to FALSE is not replaced by an empty result over it;
- in a later join the side with the `ASSERT` becomes the build side, which runs to completion
  before the probe side, even when the probe side turns out empty;
- the seeded-chain optimizations that bail out for writes bail out for it too.

`ASSERT` is unknown to the filter, limit and order-by push-down optimizers, so they treat it as a
barrier: a filter or `LIMIT` after it is never moved below it. Two mechanisms would still let an
operator above narrow what the `ASSERT` sees, and both are switched off where an `ASSERT` is
involved:

- **Semi masks.** Sideways information passing restricts a scan below a join to the keys the
  join will keep: `MATCH (a) ASSERT a.age < 80 MATCH (a)-[:knows]->(b) WHERE b.ID > 1000` would
  scan no `a` at all. `HashJoinSIPOptimizer` applies no semi mask to a join, intersect or path
  probe that has an `ASSERT` below it. A plan without `ASSERT` is checked once per plan and
  optimized as before.
- **Early termination.** A `LIMIT` stops its pipeline once it has its rows, and the limit push
  down hands its number to a `DISTINCT` below. `ASSERT` is classified as an update
  (`LogicalOperatorUtils::isUpdate`), so the mechanism that keeps a `SET` or `CREATE` from being
  cut short covers it: `LogicalOperator::hasUpdateInPipeline` finds an `ASSERT` in the same
  pipeline, `planProjectionBody` then materializes the input of a `LIMIT` first (`ACCUMULATE`)
  and the limit push down leaves the `DISTINCT` uncapped, so that the `ASSERT` runs to
  completion. Likewise a cross product with an empty build side never pulls its probe side; a
  `CALL` or `LOAD FROM` after an `ASSERT` puts the `ASSERT` there (`Planner::planReadOp`), and
  the same check materializes its input.

The classification does not make the statement a write: the transaction type is decided on the
parsed statement, where `ASSERT` is a reading clause.

## 11. Implementation map

| Layer | Files |
| --- | --- |
| Grammar | `src/antlr4/Cypher.g4`, `src/antlr4/keywords.txt`; generated `scripts/antlr4/Cypher.g4`, `scripts/antlr4/hash.md5`, `third_party/antlr4_cypher/*`, `tools/shell/include/keywords.h` |
| AST / transform | `parser/query/reading_clause/assert_clause.h`, `transform_reading_clause.cpp`, `transform_query.cpp`, `common/enums/clause_type.h`, statement visitors, `statement_read_write_analyzer.cpp` |
| Binder | `binder/bind/read/bind_assert.cpp`, `bound_assert_clause.h`, `Binder::getNewAggregateExpressions`, `property_collector.cpp` |
| Planner | `planner/plan/plan_assert.cpp`, `logical_assert.{h,cpp}`, the materialization before a `LIMIT` (`plan_projection.cpp`) and before a `CALL`/`LOAD FROM` cross product (`plan_read.cpp`) |
| Optimizer | `logical_operator_visitor`, `projection_push_down_optimizer.cpp`, `factorization_rewriter.cpp`, `correlated_subquery_unnest_solver.cpp`, `acc_hash_join_optimizer.cpp` (no semi masks above an `ASSERT`) |
| Processor | `processor/operator/assert_condition.{h,cpp}`, `map/map_assert.cpp`, `map_cross_product.cpp`, `common/exception/assertion.h` |

## 12. Verification

- `test/test_files/assert/assert.test`: every form; FALSE, NULL and empty input; default and
  custom messages; aggregates with and without `DISTINCT`, mixing errors, empty input,
  pass-through cardinality; `EXISTS`/`COUNT` subqueries, correlated and not; type errors;
  `assert` as an identifier; `EXPLAIN`; joins, `OPTIONAL MATCH`, `LIMIT` and folded filters after
  `ASSERT`.
- `test/test_files/assert/assert_transaction.test`: auto-commit rollback of earlier clauses,
  explicit transactions that commit or are rolled back, read-only transactions.
- `test/test_files/assert/assert_namespace.test`: subqueries under session and inline scopes.
- `test/common/assert_clause_test.cpp`: parameters, prepared statements executed repeatedly,
  NULL parameters, the condition text as written, many morsels on several threads, `LIMIT`,
  rollback of an explicit transaction.

The full release suite (`ctest --test-dir build/release/test`) passes with these tests added,
apart from the three extension tests that need a download and fail on `main` as well.
