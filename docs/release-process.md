# Release process

Development happens in the `<Name>Dev` repositories. Every published one has a
public twin without the suffix; a stable release copies the released state
over. `RiaGraphExplorerDev` and `RiaGraphVerificationDev` have no public twin
and are never published.

| Dev repository | Public repository | Submodule path |
|---|---|---|
| RiaGraphDev | RiaGraph | (root) |
| RiaGraphExtensionDev | RiaGraphExtension | `extension` |
| RiaGraphBenchmarkDev | RiaGraphBenchmark | `benchmark` |
| RiaGraphDatasetDev | RiaGraphDataset | `dataset` |
| RiaGraphJavaDev | RiaGraphJava | `tools/java_api` |
| RiaGraphNodejsDev | RiaGraphNodejs | `tools/nodejs_api` |
| RiaGraphPythonDev | RiaGraphPython | `tools/python_api` |
| RiaGraphRustDev | RiaGraphRust | `tools/rust_api` |
| RiaGraphWasmDev | RiaGraphWasm | `tools/wasm` |

## Releasing

1. Run **Release (all repos)** (`release-all-repos.yml`) with the version, e.g.
   `v0.22.0`, and `kind`. It releases every Dev repository, pins the
   submodules of RiaGraphDev to the released commits (`release/<version>`) and
   creates the RiaGraphDev release.
2. `kind: prerelease` stops there. Everything stays in the Dev repositories,
   and nothing is built.
3. `kind: release` continues with **Publish to public repositories**
   (`publish-public.yml`): the released tag is published to the nine public
   repositories (below). The public RiaGraph release is created as a draft.
4. Then it starts **Release build** (`release-build.yml`) in the public
   RiaGraph repository, on the published tag. The build uses the targets and
   platforms ticked in step 1 (all by default), attaches the results and
   `SHA256SUMS` to the draft release and publishes it. If any selected build
   fails, the release stays a draft. The step-1 run only starts the build; tick
   `follow_build` to keep it open until the build is done (that uses private
   Actions minutes, the build itself uses none).

`dry_run` on step 1 creates nothing and publishes nothing. To rehearse the
public side, run **Publish to public repositories** by hand with `ref: main`,
any version (`v0.0.1-test`) and `dry_run` ticked: it reads everything, builds
and checks the public trees, and pushes nothing.

## Release build

`release-build.yml` runs in the public repository and builds from the tag alone.
Its submodules are the public repositories, so a release artifact cannot contain
a source that was not released. The `plan` job checks this (every submodule must
be a public RiaGraph repository) and attaches `SOURCES.txt`, which lists the
commit of every submodule that went in.

| Target | Result | Platforms |
|---|---|---|
| `libriag` | shared + static libriag, `riag` CLI archives | selected platforms |
| `python` | sdist, wheels for CPython 3.10 to 3.14 | selected platforms |
| `nodejs` | `riagraph-nodejs-<version>-<platform>.tar.gz` (static OpenSSL, glibc >= 2.28) | selected platforms |
| `java` | multiplatform jar, Maven bundle | needs `linux_x64`; natives for the selected platforms |
| `rust` | the `.crate`, built against the engine | needs `linux_x64` |
| `wasm` | `riag-wasm.tar.gz`. **Disabled:** the build refuses it until `RiaGraphExtension` is ported from namespace `lbug` to `riag`; then re-enable in `release-build.yml` (see `package-wasm.yml`) | none (Emscripten) |

Platforms: `linux_x64` (manylinux_2_28), `macos_arm64`, `windows_x64`. Adding a
platform means adding a row to the table in the `plan` job of `release-build.yml`
and a case in `.github/actions/fetch-engine`.

The engine is compiled once per platform (`build-engine.yml`). Python, Node.js,
Java and Rust link that build (`.github/actions/fetch-engine`) and compile only
their own binding. WebAssembly cross-compiles the engine with Emscripten. The
build runs the Java tests and the Node.js smoke test; the other test suites run
in the CI of the component repositories.

Nothing is uploaded to PyPI, npm, crates.io or Maven Central. The files are on
the GitHub release.

To rehearse the build, run **Release build** by hand in RiaGraphDev with
`publish` off and any branch as `version`: it checks out the private submodules
with `GH_TOKEN_RIAGRAPH` and attaches nothing. It costs private Actions
minutes; use few platforms and targets. In the public repository the same run
is free.

The workflows come from the branch the build is dispatched on, the sources from
`version`. A tag cut before this pipeline existed (up to v0.30.0) lacks the
helper actions and the Python packaging fix, so the `plan` job rejects it at
once; build a newer tag, or a branch.

A rehearsal can skip the engine builds by naming an earlier run of the same
repository in `engine_run_id` (artifacts are kept for 90 days). That is only
sound while the engine sources have not changed since that run, and it works
without `publish`.

## What publishing does

Per repository, in the order of `.github/public-release.json` (a repository
after every repository it has a submodule on, RiaGraph last):

- one commit on the public `main` containing the released Dev tree. The public
  history is one commit per release; the commit message is `Release <version>`
  and carries `[skip ci]`.
- the tree is complete except for what `public-release.json` excludes. At the
  moment that is the release machinery of RiaGraphDev, which only works from
  the Dev side: `release-all-repos.yml`, `publish-public.yml` and
  `public-release.json` itself. All other workflows
  are published. To keep something else private, add it to `exclude_paths` /
  `exclude_globs` of its repository (`defaults` apply to all). A path excluded
  later is removed from the public repository by the next publish.
- `cntSafety/<Name>Dev` becomes `cntSafety/<Name>` in every text file. This is
  what turns the `.gitmodules` URLs into public ones.
- every submodule pin becomes the commit published for that repository earlier
  in the same run. The Dev SHA does not exist in the public repository. The
  pins of `tools/nodejs_api` and `tools/python_api` on `dataset` follow the
  dataset released with them.
- a tag `<version>` and a GitHub release in the public repository, with the Dev
  release's notes (names rewritten).

A rerun is safe: unchanged content makes no commit, an existing tag with the
same content is reused, an existing tag with different content fails the run.
Tags are never moved and nothing is force-pushed.

The released commit of a component must already name its Dev submodule
repositories in its `.gitmodules` (`RiaGraphNodejsDev`, `RiaGraphPythonDev`
point at `RiaGraphDatasetDev`); an older tag does not, and publishing it fails
with an explanation rather than publishing a pointer at the wrong repository.

## Secrets

| Secret | Used for |
|---|---|
| `TOKEN_GH_RIAGRAPHACTIONS_READ` | checking out the private `RiaGraphActions` repository |
| `TOKEN_GH_RIAGRAPH_PUBLISH` | reading the Dev repositories and writing the public ones (contents, releases); starting `release-build.yml` in `cntSafety/RiaGraph` (Actions: read and write there); also the cross-repo reads in the component CIs |
| `GH_TOKEN_RIAGRAPH` | `release-all-repos.yml` dispatching and following `release.yml` in the component repositories; the submodule checkouts of `ci-workflow.yml` |

## Where the code is

The publisher (`publish_public.py`) and its tests live
in `cntSafety/RiaGraphActions` and are used as `./.riagraph-actions/<action>`
after checking it out with `TOKEN_GH_RIAGRAPHACTIONS_READ`, pinned to `v1`.
Download scripts and package metadata keep the public repository names on
purpose: they tell consumers where to download from. The Dev workflows pass
`RIAG_GITHUB_REPOSITORY` to point the scripts at the Dev repository.
