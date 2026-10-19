#!/bin/bash
#
# SPDX-License-Identifier: CC0-1.0
#
# Local development build of the krita-spectral branch inside KDE's
# krita-appimage-2204 build image, using rootless podman (or docker).
# Prebuilt dependencies come from KDE's public package registry, exactly
# like in build-tools/ci-scripts/github-build-appimage.sh.
#
# Usage (from anywhere inside the source tree):
#
#   build-tools/spectral/container-build.sh deps        # download deps (once, and after rebasing)
#   build-tools/spectral/container-build.sh configure   # configure _build
#   build-tools/spectral/container-build.sh build [target...]
#   build-tools/spectral/container-build.sh install     # build everything and install into _install (needed by tests)
#   build-tools/spectral/container-build.sh test <ctest regex>
#   build-tools/spectral/container-build.sh appimage    # install + package into _packaging/
#   build-tools/spectral/container-build.sh shell       # interactive shell in the container
#
# Everything is written to ignored folders of the source tree:
#   _install (deps), _build, _staging, _packaging, _ci-cache, venv, krita-deps-management

set -e

IMAGE=${KRITA_BUILD_IMAGE:-invent-registry.kde.org/sysadmin/ci-images/krita-appimage-2204}
ENGINE=${CONTAINER_ENGINE:-podman}
JOBS=${JOBS:-$(( $(nproc) > 2 ? $(nproc) - 2 : 1 ))}
SOURCE_DIR=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)

if [ -z "$INSIDE_KRITA_CONTAINER" ]; then
    mkdir -p "$SOURCE_DIR/_ci-cache/ccache"

    ENGINE_ARGS=(--rm -i -v "$SOURCE_DIR:/krita:z" -w /krita -e INSIDE_KRITA_CONTAINER=1 -e JOBS="$JOBS")
    if [ "$ENGINE" = podman ]; then
        ENGINE_ARGS+=(--userns=keep-id)
    else
        ENGINE_ARGS+=(--user "$(id -u):$(id -g)")
    fi
    if [ -t 0 ]; then
        ENGINE_ARGS+=(-t)
    fi

    exec "$ENGINE" run "${ENGINE_ARGS[@]}" "$IMAGE" bash /krita/build-tools/spectral/container-build.sh "$@"
fi

# ---- everything below runs inside the container ----

cd /krita
git config --global --add safe.directory '*'
export PATH="$HOME/tools/bin:$PATH"
export CCACHE_DIR=/krita/_ci-cache/ccache
export CCACHE_MAXSIZE=20G

ENV_FILE=/krita/_build-env/env

activate_env() {
    if [ ! -f "$ENV_FILE" ]; then
        echo "Dependencies are not set up yet, run '$0 deps' first" >&2
        exit 1
    fi
    # shellcheck disable=SC1090
    . "$ENV_FILE"
}

case "$1" in
deps)
    if [ ! -d krita-deps-management ]; then
        git clone https://invent.kde.org/packaging/krita-deps-management.git --depth=1
        git clone https://invent.kde.org/packaging/krita-ci-utilities.git krita-deps-management/ci-utilities --depth=1
    else
        git -C krita-deps-management pull --ff-only
        git -C krita-deps-management/ci-utilities pull --ff-only
    fi

    python3 -m venv venv --upgrade-deps
    . venv/bin/activate
    pip install -r krita-deps-management/requirements.txt
    . venv/bin/activate
    unset VIRTUAL_ENV

    export KDECI_CACHE_PATH=/krita/_ci-cache/packages
    export KDECI_GITLAB_SERVER=https://invent.kde.org/
    export KDECI_PACKAGE_PROJECT=teams/ci-artifacts/krita-appimage
    export KDECI_REPO_METADATA_PATH=/krita/krita-deps-management/repo-metadata
    export KDECI_COMPRESS_PACKAGES_ON_DOWNLOAD=False

    # the dependency list is generated into .kde-ci.yml, which is tracked by git,
    # so put the original back afterwards
    cp .kde-ci.yml _kde-ci.yml.orig
    trap 'mv -f /krita/_kde-ci.yml.orig /krita/.kde-ci.yml' EXIT

    python3 -u krita-deps-management/tools/replace-branch-in-seed-file.py krita-deps-management/latest/krita-deps.yml -p -o branch-corrected-deps.yml -d master
    python3 -u krita-deps-management/tools/generate-deps-file.py -s branch-corrected-deps.yml -o .kde-ci.yml
    mkdir -p _build-env
    python3 -u krita-deps-management/ci-utilities/run-ci-build.py --project krita --branch master --platform Linux/Qt5/Shared --only-env -e "$ENV_FILE"
    ;;

configure)
    activate_env
    cmake -S /krita -B /krita/_build -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/krita/_install \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DBUILD_TESTING=ON \
        -DHIDE_SAFE_ASSERTS=OFF \
        -DKRITA_ENABLE_PCH=OFF \
        -DFOUNDATION_BUILD=ON
    ;;

build)
    activate_env
    shift
    if [ $# -gt 0 ]; then
        cmake --build /krita/_build --parallel "$JOBS" --target "$@"
    else
        cmake --build /krita/_build --parallel "$JOBS"
    fi
    ;;

install)
    activate_env
    cmake --build /krita/_build --parallel "$JOBS"
    cmake --install /krita/_build
    ;;

test)
    activate_env
    shift
    export QT_QPA_PLATFORM=offscreen
    ctest --test-dir /krita/_build --output-on-failure -R "$@"
    ;;

appimage)
    activate_env
    cmake --build /krita/_build --parallel "$JOBS"
    rm -rf /krita/_staging
    DESTDIR=/krita/_staging cmake --install /krita/_build > /dev/null
    # the packaging scripts need the python modules from the deps step
    . /krita/venv/bin/activate
    unset VIRTUAL_ENV
    python3 -u build-tools/ci-scripts/build-appimage.py
    ls -l /krita/_packaging/*.AppImage
    ;;

shell)
    activate_env
    exec bash -i
    ;;

*)
    sed -n '/^# Usage/,/^$/p' "$0"
    exit 1
    ;;
esac
