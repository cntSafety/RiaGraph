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
2. `kind: prerelease` stops there. Everything stays in the Dev repositories.
3. `kind: release` continues with **Publish to public repositories**
   (`publish-public.yml`): the released tag is published to the nine public
   repositories (below). The public RiaGraph release is created as a draft.
4. Run **Release** (`release-artifacts.yml`) with the tag and the run id of the
   `precompiled-bin` build. It attaches the binaries to the RiaGraphDev release
   and mirrors them into the public RiaGraph release, which it then publishes.
   For a prerelease there is no public release and the mirror step does nothing.

`dry_run` on step 1 creates nothing and publishes nothing. To rehearse the
public side, run **Publish to public repositories** by hand with `ref: main`,
any version (`v0.0.1-test`) and `dry_run` ticked: it reads everything, builds
and checks the public trees, and pushes nothing.

## What publishing does

Per repository, in the order of `.github/public-release.json` (a repository
after every repository it has a submodule on, RiaGraph last):

- one commit on the public `main` containing the released Dev tree. The public
  history is one commit per release; the commit message is `Release <version>`
  and carries `[skip ci]`.
- the tree is complete except for what `public-release.json` excludes. At the
  moment that is the release machinery of RiaGraphDev, which only works from
  the Dev side: `release-all-repos.yml`, `release-artifacts.yml`,
  `publish-public.yml` and `public-release.json` itself. All other workflows
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
| `TOKEN_GH_RIAGRAPH_PUBLISH` | reading the Dev repositories and writing the public ones (contents, releases); also the cross-repo reads in the component CIs |
| `GH_TOKEN_RIAGRAPH` | `release-all-repos.yml` dispatching and following `release.yml` in the component repositories; the submodule checkouts of `ci-workflow.yml` |

## Where the code is

The publisher (`publish_public.py`), its tests and `mirror-release-assets` live
in `cntSafety/RiaGraphActions` and are used as `./.riagraph-actions/<action>`
after checking it out with `TOKEN_GH_RIAGRAPHACTIONS_READ`, pinned to `v1`.
Download scripts and package metadata keep the public repository names on
purpose: they tell consumers where to download from. The Dev workflows pass
`RIAG_GITHUB_REPOSITORY` to point the scripts at the Dev repository.
