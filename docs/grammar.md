## Changing the grammar

source: src/antlr4/Cypher.g4
generated: scripts/antlr4/Cypher.g4 (via keywordhandler.py)
generated: third_party/antlr4_cypher/cypher_parser.{cpp,h}

Running make will autogenerate, but manual generation can be done via

(cd scripts/antlr4 && cmake -D ROOT_DIR=../.. -P generate_grammar.cmake)

The namespace syntax and binding rules are specified in
[the namespace concept](../namespace.md). `iC_NamespaceStatement` holds the namespace
commands: `CREATE NAMESPACE`, `DROP NAMESPACE`, `ALTER NAMESPACE <name> RENAME TO
<new-name>` and `USE NAMESPACE`. `iC_NamespaceScope` and `iC_NamespaceList`
are shared by clause selectors, inline node/relationship selectors, table policies,
COPY, and namespace commands. Inline `IN` follows labels/types, recursive bounds,
and properties:

```cypher
MATCH (a:Person {id:36} IN team_a)-[r:Knows IN shared]->(b IN team_b)
IN ALL NAMESPACES RETURN a, r, b;
```

The outer selector supplies the base read scope; inline selectors intersect it.
Selectors contain identifiers, `GLOBAL`, or `ALL NAMESPACES`, not expressions or
string literals. Keep the source grammar authoritative and regenerate the keyword
tables, lexer, parser, and transformed grammar together.

`ASSERT <condition> [AS '<message>']` (`iC_Assert`) is specified in
[the ASSERT concept](assert.md). It is a reading clause, and the third alternative of
`oC_SinglePartQuery` lets a query end with it. `ASSERT` is a non-reserved keyword.
Its generated `ASSERT()` accessors collide with the `ASSERT(condition)` macro of
`common/assert.h`; the two headers that include `cypher_parser.h` undefine the macro
around that include, so include the generated parser only through them.
