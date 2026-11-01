# RiaGraph — inherited CI that needs infrastructure we do not have

**Status:** register, not a plan. One row per `RIAG-TODO(infra-…)` marker in the tree.

RiaGraph inherited a working CI setup from ladybug. Some of it depends on machines,
container images, registries and repositories that belong to the upstream project and
have no RiaGraph equivalent yet.

Decision **D11** of [`riagraph-fork-plan.md`](riagraph-fork-plan.md) governs what to do
about that: **mark, do not decide.** Identifiers inside these workflows are renamed and a
marker is added as the first line, but nothing is deleted, no trigger is disabled, and no
new infrastructure is stood up as part of the rename programme. A workflow listed here is
expected to fail until someone makes the decision the row describes.

Find every marker with:

```bash
git grep -n 'RIAG-TODO(infra'
```

## Register

| Topic | Where | What is missing | What would unblock it |
|---|---|---|---|
| `infra-musl-runner` | `.github/workflows/ci-musl-test-workflow.yml` | Nothing missing, but unverified. | The runner workspace path is derived from the repository name and is now `/__w/RiaGraph/RiaGraph`. The mechanical rename produced a lowercase `riagraph`, which would have been wrong; it is corrected. Confirm on a real runner. |

## Removed

The other rows were resolved by deleting the workflows instead of building the
infrastructure they needed:

- `infra-benchmark-runners`: `benchmark-workflow.yml`, `lsqb-benchmark-workflow.yml`
- `infra-builder-image`: `build-and-deploy.yml`, `java-workflow.yml`, `nodejs-workflow.yml`.
  The release build (`release-build.yml`) builds in `manylinux_2_28` instead of the GHCR image.
- `infra-extension-repo`: `deploy-extension.yml`, `purge-extension.yml`,
  `build-and-deploy-extension.yml`, `get-extensions-from-ghcr.yml`, `build-extensions.yml`.
  Extensions are published to GitHub Pages by `RiaGraphExtension`.
- `infra-api-docs`: `gen-docs.yml`, which pushed to an upstream repository.

`nightly-sanitizers.yml` went with them (the sanitizer runs live in the verification
repository), and so did the old release path: `precompiled-bin-workflow.yml`,
`python-wheel-workflow.yml`, `wasm-workflow.yml`, `nodejs-binaries.yml` and
`release-artifacts.yml`, all replaced by `release-build.yml` (see
[`release-process.md`](release-process.md)). Git history has all of them.

## Not in this register

`ci-workflow.yml`, `codeql.yml` and `zizmor.yml` carry no marker and are expected to work.
If one of them fails, that is a bug to fix, not an infrastructure decision.

## Observed consequence

The `infra-extension-repo` gap is not theoretical. It is the **only** remaining failure in
the core test suite. A full `make test` in `docker/build-env` gives:

```
99% tests passed, 3 tests failed out of 2743
        1939 - e2e_test_extension~extension.ForceInstallExtension (Failed)
        1940 - e2e_test_extension~extension.UninstallExtensionError (Failed)
        1941 - e2e_test_extension~extension.LoadNotInstalledExtension (Failed)
```

All three are `HTTP Returns: 404` against
`https://cntsafety.github.io/RiaGraphExtension/v0.20.0/<platform>/...`. They will keep
failing until GitHub Pages is enabled on `cntSafety/RiaGraphExtension` and
`publish-pages.yml` has published a build. The URL being exactly right is evidence the
rename landed; nothing here is a rename defect.

## Related open items

`O3` (public registry names) and `O5` (this register) in
[`riagraph-fork-plan.md`](riagraph-fork-plan.md) §16.
