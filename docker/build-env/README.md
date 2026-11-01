# RiaGraph build and test environment

A reproducible Linux toolchain in a container, for building and testing the core from any
host. It exists so that the Phase 9 verification in
[`docs/riagraph-fork-plan.md`](../../docs/riagraph-fork-plan.md) can be run without a
working native toolchain — on Windows in particular, where the core's
`find_package(OpenSSL 3 REQUIRED)` is the usual blocker.

The image tracks [`.github/docker/ubuntu-22.04-gcc13/Dockerfile`](../../.github/docker/ubuntu-22.04-gcc13/Dockerfile),
the reference toolchain image. The release build uses `manylinux_2_28` instead (see `docs/build_tips.md`). Ubuntu 22.04 is the
floor: it is the oldest Ubuntu that ships OpenSSL 3.

| Component | Version |
| --- | --- |
| Base | `ubuntu:22.04` (OpenSSL 3.0.2) |
| Compiler | GCC 13 (`ubuntu-toolchain-r/test`) |
| Build | CMake 3.22, Ninja 1.10, Make 4.3, ccache |
| Also | Python 3.10, OpenJDK 17 |

## Use

```bash
docker/build-env/run.sh make release      # build the core, shared library and shell
docker/build-env/run.sh make test         # build with tests and run ctest
docker/build-env/run.sh --shell bash      # interactive shell in the environment
docker/build-env/run.sh --fresh make release   # discard the volume, cold build
```

The first invocation builds the image and syncs the tree; both are cached afterwards.

| Flag | Effect |
| --- | --- |
| `--sync` | Re-sync the host tree into the volume before running. Do this after editing files on the host. |
| `--fresh` | Delete the source volume first: clean tree, cold build. |
| `--shell` | Run the argument through `bash -lc` rather than exec'ing it. |

| Variable | Default | Meaning |
| --- | --- | --- |
| `RIAG_IMAGE` | `riagraph-build-env` | image tag |
| `RIAG_VOLUME` | `riagraph-work` | source volume |
| `RIAG_CCACHE` | `riagraph-ccache` | compiler cache volume |
| `RIAG_JOBS` | container `nproc` | parallel build jobs |
| `RIAG_CPUS` | unset (all) | `--cpus` for the container |
| `RIAG_MEMORY` | unset (all) | `--memory` for the container |

### Sharing the machine

By default the build takes every core in the Docker VM. That is what you want when it is
the only thing running, and harmful when it is not: concurrent unrestricted C++ builds can
exhaust the VM and take the Docker daemon down with them, killing every running container
— we lost a near-complete test run that way. If anything else is using Docker, budget
explicitly and keep `RIAG_JOBS` in line with `RIAG_CPUS`:

```bash
RIAG_CPUS=6 RIAG_MEMORY=8g RIAG_JOBS=6 docker/build-env/run.sh make test
```

## Why the source is copied instead of bind-mounted

A build of this tree writes tens of thousands of object files. On Docker Desktop for
Windows and macOS a bind mount crosses a filesystem bridge, and that write traffic is what
makes a containerised build several times slower than a native one. So `run.sh`
bind-mounts the host tree **read-only** and syncs it into a named volume on the
container's own filesystem, where both the source reads and the object writes are local.

Two consequences worth knowing:

- **Build output does not appear in the host tree.** That is deliberate — this
  environment verifies the working tree without writing to it.
- **Host edits are not live.** Re-run with `--sync` after changing files. The sync is
  `rsync -a --delete`, so a file removed on the host is removed from the volume too and
  cannot linger to keep a stale build working. `build/` is excluded from the delete: it is
  container-owned, and protecting it is what keeps a re-sync an incremental rebuild rather
  than a cold one.

The sync skips `.git` (not needed to build, and the largest thing in the tree) and the
`dataset` submodule where it is checked out again under `tools/python_api` and
`tools/nodejs_api`. The top-level `dataset/` is included, because the end-to-end tests
read from it.

`ccache` persists in its own volume, so a rebuild after `--sync` is incremental even
though each `run.sh` invocation is a fresh container.

## Scope

This environment covers the **core**: `make release`, `make test`, `make shell-test`.

It does not build the language bindings. Each binding has its own toolchain (`pip`/`uv`,
`npm`, Gradle, `cargo`, Emscripten) and its own repository and CI; running them here would
duplicate that without reproducing it. Verify bindings in their own repositories.
