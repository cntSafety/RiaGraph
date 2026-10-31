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
| `infra-benchmark-runners` | `.github/workflows/benchmark-workflow.yml`, `.github/workflows/lsqb-benchmark-workflow.yml` | Dedicated benchmark machines with a consistent specification. `lsqb` additionally installs a wheel from PyPI that RiaGraph does not publish. | Self-hosted runners with a stable hardware profile, plus a decision on registry publication (open item O3). Benchmark numbers from shared GitHub runners are noise. |
| `infra-builder-image` | `.github/workflows/build-and-deploy.yml`, `.github/workflows/java-workflow.yml`, `.github/workflows/nodejs-workflow.yml` | `ghcr.io/cntsafety/ubuntu-22.04-gcc13`. The reference is renamed into our namespace, but the image has never been built or pushed. | Build `.github/docker/ubuntu-22.04-gcc13/Dockerfile` and publish it to GHCR under `cntSafety`. This is the cheapest row here and unblocks three workflows. |
| `infra-extension-repo` | `.github/workflows/deploy-extension.yml`, `.github/workflows/purge-extension.yml`, `.github/workflows/build-and-deploy-extension.yml`, `.github/workflows/get-extensions-from-ghcr.yml` | `ghcr.io/cntsafety/extension-repo` and the CDN-fronted deploy target behind it. | **Superseded by Phase 6.** Extensions are now published to GitHub Pages by `cntSafety/RiaGraphExtension/.github/workflows/publish-pages.yml`, which is what `OFFICIAL_EXTENSION_REPO` resolves against. These four workflows describe the mechanism they replaced. Reconcile — most likely by removing them — rather than reviving them. |
| `infra-api-docs` | `.github/workflows/gen-docs.yml` | A RiaGraph equivalent of `LadybugDB/api-docs`, and the `DOC_PUSH_TOKEN` secret. | Decide where generated API documentation should live. The checkout still names the **upstream** repository: that reference is deliberately left alone because renaming it would invent a repository name, and the workflow cannot run without the secret. Do not point it at an upstream repository we can push to. |
| `infra-musl-runner` | `.github/workflows/ci-musl-test-workflow.yml` | Nothing missing, but unverified. | The runner workspace path is derived from the repository name and is now `/__w/RiaGraph/RiaGraph`. The mechanical rename produced a lowercase `riagraph`, which would have been wrong; it is corrected. Confirm on a real runner. |

## Not in this register

These inherited workflows carry no marker and are expected to work once the rename lands.
If one of them fails, that is a bug to fix, not an infrastructure decision:

`ci-workflow.yml`, `codeql.yml`, `nightly-sanitizers.yml`, `zizmor.yml`,
`wasm-workflow.yml`, `python-wheel-workflow.yml`, `precompiled-bin-workflow.yml`,
`release-artifacts.yml`, `build-extensions.yml`.

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
