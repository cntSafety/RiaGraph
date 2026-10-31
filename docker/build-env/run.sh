#!/usr/bin/env bash
# Run a build or test command for RiaGraph inside the Docker build environment.
#
#   docker/build-env/run.sh make release
#   docker/build-env/run.sh make test
#   docker/build-env/run.sh bash            # interactive shell
#
# Why the source is copied into a volume rather than bind-mounted:
#
# A C++ build of this tree writes tens of thousands of object files. On Docker Desktop
# for Windows and macOS a bind mount crosses a filesystem bridge, and that write traffic
# is what makes a containerised build several times slower than a native one. The source
# is bind-mounted read-only and synced into a named volume on the container's own
# filesystem, so both the reads and the object writes are local.
#
# The consequence is that build output does NOT appear in the host tree. That is
# deliberate: this environment verifies the working tree without writing to it.
#
#   --sync    re-sync the host tree into the volume before running (default on first use)
#   --fresh   delete the volume first, forcing a clean tree and a cold build
#   --shell   run the command through `bash -lc` instead of exec'ing it directly
#
# Environment:
#   RIAG_IMAGE      image tag           (default riagraph-build-env)
#   RIAG_VOLUME     source volume       (default riagraph-work)
#   RIAG_CCACHE     ccache volume       (default riagraph-ccache)
#   RIAG_JOBS       parallel build jobs (default: container nproc)
#   RIAG_CPUS       --cpus for the container   (default: unset, i.e. all)
#   RIAG_MEMORY     --memory for the container (default: unset, i.e. all)
#
# On resource limits: an unrestricted build takes every core in the Docker VM. That is
# what you want when this is the only thing running, and actively harmful when it is
# not -- concurrent unrestricted builds can exhaust the VM and take the daemon down
# with them, losing every running container. If anything else is using Docker, set
# RIAG_CPUS and RIAG_MEMORY, and set RIAG_JOBS to match RIAG_CPUS.

set -euo pipefail

IMAGE="${RIAG_IMAGE:-riagraph-build-env}"
VOLUME="${RIAG_VOLUME:-riagraph-work}"
CCACHE="${RIAG_CCACHE:-riagraph-ccache}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Docker on Windows needs a native path, and MSYS must be told not to mangle it.
export MSYS_NO_PATHCONV=1
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOST_PATH="$(cygpath -w "${REPO_ROOT}")" ;;
    *)                    HOST_PATH="${REPO_ROOT}" ;;
esac

SYNC=0
FRESH=0
SHELL_MODE=0
while [ $# -gt 0 ]; do
    case "$1" in
        --sync)  SYNC=1; shift ;;
        --fresh) FRESH=1; SYNC=1; shift ;;
        --shell) SHELL_MODE=1; shift ;;
        --) shift; break ;;
        *) break ;;
    esac
done

if [ "$#" -eq 0 ]; then
    set -- bash
fi

if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
    echo "==> building image ${IMAGE}"
    docker build -t "${IMAGE}" "${SCRIPT_DIR}"
fi

if [ "${FRESH}" -eq 1 ]; then
    echo "==> removing volume ${VOLUME}"
    docker volume rm -f "${VOLUME}" >/dev/null 2>&1 || true
fi

if ! docker volume inspect "${VOLUME}" >/dev/null 2>&1; then
    SYNC=1
fi

if [ "${SYNC}" -eq 1 ]; then
    echo "==> syncing ${REPO_ROOT} into volume ${VOLUME}"
    # rsync rather than tar, for --delete: a source file removed on the host must be
    # removed from the volume too, or it lingers and quietly keeps a stale build
    # working. That is precisely the failure this environment exists to rule out.
    #
    # Excluded from the sync:
    #   /.git            not needed to build; it is the largest thing in the tree
    #   /build           container-owned build output -- protected from --delete so
    #                    that a re-sync stays an incremental rebuild rather than a
    #                    cold one. It is the one directory the host does not own.
    #   tools/*/dataset  the top-level dataset/ submodule, checked out twice more
    docker run --rm \
        -v "${HOST_PATH}:/src:ro" \
        -v "${VOLUME}:/work" \
        "${IMAGE}" \
        bash -lc '
            set -eu
            rsync -a --delete \
                --exclude=/.git \
                --exclude=/build \
                --exclude=/tools/python_api/dataset \
                --exclude=/tools/nodejs_api/dataset \
                /src/ /work/
            echo "synced $(find /work -type f -not -path "/work/build/*" | wc -l) source files"
        '
fi

JOBS="${RIAG_JOBS:-}"

# Opt-in resource limits. Unset means "use the whole Docker VM", which is right when
# this is the only thing running and dangerous when it is not.
LIMITS=()
[ -n "${RIAG_CPUS:-}" ]   && LIMITS+=(--cpus "${RIAG_CPUS}")
[ -n "${RIAG_MEMORY:-}" ] && LIMITS+=(--memory "${RIAG_MEMORY}")

echo "==> ${IMAGE}: $*"
if [ "${SHELL_MODE}" -eq 1 ]; then
    exec docker run --rm -it \
        -v "${VOLUME}:/work" -v "${CCACHE}:/ccache" -w /work \
        ${LIMITS[@]+"${LIMITS[@]}"} \
        ${JOBS:+-e CMAKE_BUILD_PARALLEL_LEVEL="${JOBS}"} \
        "${IMAGE}" bash -lc "$*"
else
    exec docker run --rm -i \
        -v "${VOLUME}:/work" -v "${CCACHE}:/ccache" -w /work \
        ${LIMITS[@]+"${LIMITS[@]}"} \
        ${JOBS:+-e CMAKE_BUILD_PARALLEL_LEVEL="${JOBS}"} \
        "${IMAGE}" "$@"
fi
