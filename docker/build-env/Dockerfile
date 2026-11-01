# RiaGraph build and test environment.
#
# A reproducible Linux toolchain for building and testing the core, used to run the
# Phase 9 verification in docs/riagraph-fork-plan.md from any host. It is deliberately
# close to .github/docker/ubuntu-22.04-gcc13/Dockerfile, the reference toolchain image,
# so that a result here means something about the other Linux builds. (The release build
# uses manylinux_2_28, see docs/build_tips.md.)
#
# Ubuntu 22.04 is the floor: it ships OpenSSL 3.0, which the core requires
# (find_package(OpenSSL 3 REQUIRED) in CMakeLists.txt).
#
#   docker build -t riagraph-build-env docker/build-env
#   docker run --rm -v "$PWD":/work -w /work riagraph-build-env make release
#
# See docker/build-env/README.md for the wrapper script and the verification targets.

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        gnupg \
        software-properties-common \
    && add-apt-repository ppa:ubuntu-toolchain-r/test -y \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        build-essential \
        ccache \
        cmake \
        g++-13 \
        gcc-13 \
        git \
        make \
        ninja-build \
        libssl-dev \
        openjdk-17-jdk \
        pkg-config \
        python3 \
        python3-pip \
        python3-venv \
        rsync \
        sudo \
        tar \
        unzip \
        xz-utils \
        zip \
        zstd \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 130 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 130 \
    && JAVA_HOME_PATH="$(find /usr/lib/jvm -maxdepth 1 -type d -name 'java-17-openjdk-*' | head -n 1)" \
    && ln -s "$JAVA_HOME_PATH" /usr/lib/jvm/java-17-openjdk \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

ENV JAVA_HOME=/usr/lib/jvm/java-17-openjdk
ENV PATH="${JAVA_HOME}/bin:${PATH}"

# The build shells out to `python3`; some scripts still say `python`.
RUN ln -sf /usr/bin/python3 /usr/local/bin/python

# pyarrow is a *test* dependency, not a build one: the dictionary_bug e2e imports parquet
# fixtures that are generated rather than committed (see
# scripts/generate-dictionary-bug-fixture.py, and .gitignore). Without it that test fails
# for want of its input, which is easy to mistake for a real regression.
RUN pip3 install --no-cache-dir pyarrow

# The repository is bind-mounted rather than copied, so the container always builds the
# working tree as it is on the host. git refuses to operate on a tree owned by another
# uid, which is exactly what a bind mount from a Windows host looks like.
RUN git config --system --add safe.directory '*'

# Keep the object cache on a volume so rebuilds across `docker run` invocations are not
# from scratch; a cold build of this tree is long.
ENV CCACHE_DIR=/ccache
VOLUME /ccache

WORKDIR /work
CMD ["bash"]
