<p align="center">
  <a href="https://github.com/cntSafety/RiaGraph/actions">
    <img src="https://github.com/cntSafety/RiaGraph/actions/workflows/ci-workflow.yml/badge.svg?branch=main" alt="GitHub Actions Badge"></a>
</p>

# RiaGraph
RiaGraph is an embedded graph database built for query speed and scalability. RiaGraph is optimized for handling complex analytical workloads
on very large databases and provides a set of retrieval features, such as a full text search and vector indices. Our core feature set includes:

- Flexible Property Graph Data Model and Cypher query language
- Embeddable, serverless integration into applications
- Native full text search and vector index
- Columnar disk-based storage
- Columnar sparse row-based (CSR) adjacency list/join indices
- Vectorized and factorized query processor
- Novel and very fast join algorithms
- Multi-core query parallelism
- Serializable ACID transactions
- Wasm (WebAssembly) bindings for fast, secure execution in the browser

RiaGraph's emphasis is **quality and reliability**. It is a fork of
[ladybug](https://github.com/ladybugdb/ladybug), whose emphasis is feature breadth, and <!-- riag-rename:skip lineage, plan section 1 -->
ladybug is itself a fork of [Kùzu](https://github.com/kuzudb/kuzu). RiaGraph inherits the <!-- riag-rename:skip lineage, plan section 1 -->
engine from both and diverges on what it prioritises, not on what it is.

RiaGraph is available under a permissive license. Try it out and help us make it better --
feedback and feature requests are welcome.

### Compatibility with ladybug databases <!-- riag-rename:skip lineage, plan section 1 -->

RiaGraph reads databases written by ladybug and by Kùzu: the on-disk layout is unchanged <!-- riag-rename:skip lineage, plan section 1 -->
and only the four header magic bytes differ. The exchange is **one-way**. Once RiaGraph
writes to a database the header carries `RIAG`, and ladybug can no longer open it. Copy <!-- riag-rename:skip lineage, plan section 1 -->
anything you still need to open with ladybug before writing to it with RiaGraph. <!-- riag-rename:skip lineage, plan section 1 -->

## Installation

| Language | Installation                                                           |
| -------- |------------------------------------------------------------------------|
RiaGraph is **not published to any public package registry yet**. Package names are
reserved in the build system, but PyPI, npm, crates.io and Maven Central publication is
deferred; see open item O3 in [the fork plan](docs/riagraph-fork-plan.md). Until then,
build from source or take a binary from
[Releases](https://github.com/cntSafety/RiaGraph/releases/latest).

| Language | Package name (not yet published) |
| -------- | -------------------------------- |
| Python   | `riagraph` (imported as `riag`)   |
| Node.js  | `@riagraph/core`                  |
| Wasm     | `@riagraph/wasm-core`             |
| Rust     | `riag`                            |
| Java     | `com.riagraph`                    |
| C/C++    | `libriag` / `riag.h`              |
| CLI      | `riag`                            |

## Build from Source

`make release` builds the core library, the shared library and the `riag` shell. See
[`CONTRIBUTING.md`](CONTRIBUTING.md) for prerequisites and the full target list.

## Contributing
We welcome contributions to RiaGraph. If you are interested in contributing to RiaGraph, please read our [Contributing Guide](CONTRIBUTING.md).

## License
By contributing to RiaGraph, you agree that your contributions will be licensed under the [MIT License](LICENSE).

## Contact
Open an [issue](https://github.com/cntSafety/RiaGraph/issues) or start a
[discussion](https://github.com/cntSafety/RiaGraph/discussions).
