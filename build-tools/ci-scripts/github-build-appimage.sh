#!/bin/bash
#
# SPDX-License-Identifier: CC0-1.0
#
# Builds a Krita AppImage the same way the `linux-build-qt5` GitLab job does
# (see build-tools/ci-scripts/linux.yml), but outside of KDE's infrastructure.
#
# Expected to be run from the root of Krita's source tree inside the
# invent-registry.kde.org/sysadmin/ci-images/krita-appimage-2204 image.
# Prebuilt dependencies are downloaded from KDE's public package registry.
#
# The resulting AppImage ends up in ./_packaging

set -e
set -x

DEPS_REPO_BRANCH=${DEPS_REPO_BRANCH:-master}
DEPS_BRANCH=${DEPS_BRANCH:-master}

export KDECI_BUILD_TYPE=${KDECI_BUILD_TYPE:-Release}
export KDECI_CACHE_PATH=${KDECI_CACHE_PATH:-$PWD/_ci-cache/packages}
export KDECI_CC_CACHE=${KDECI_CC_CACHE:-$PWD/_ci-cache/ccache}
export KDECI_GITLAB_SERVER=https://invent.kde.org/
export KDECI_PACKAGE_PROJECT=teams/ci-artifacts/krita-appimage
export KDECI_REPO_METADATA_PATH=$PWD/krita-deps-management/repo-metadata
export KDECI_COMPRESS_PACKAGES_ON_DOWNLOAD=False
export KDECI_ONLY_BUILD=True
export KDECI_EXTRA_CMAKE_ARGS=${KDECI_EXTRA_CMAKE_ARGS:--DHIDE_SAFE_ASSERTS=OFF -DBUILD_TESTING=OFF}

git config --global --add safe.directory '*'

if [ ! -d krita-deps-management ]; then
    git clone https://invent.kde.org/packaging/krita-deps-management.git -b "$DEPS_REPO_BRANCH" --depth=1
    git clone https://invent.kde.org/packaging/krita-ci-utilities.git krita-deps-management/ci-utilities --depth=1
fi

python3 -m venv venv --upgrade-deps
. venv/bin/activate
pip install -r krita-deps-management/requirements.txt
# Reactivate venv to force it to use the pip version of meson
. venv/bin/activate
# make sure venv doesn't take precedence over normal PATH search
unset VIRTUAL_ENV
export PATH="$HOME/tools/bin:$PATH"

python3 -u krita-deps-management/tools/replace-branch-in-seed-file.py krita-deps-management/latest/krita-deps.yml -p -o branch-corrected-deps.yml -d "$DEPS_BRANCH"
python3 -u krita-deps-management/tools/generate-deps-file.py -s branch-corrected-deps.yml -o .kde-ci.yml
python3 -u krita-deps-management/ci-utilities/run-ci-build.py --project krita --branch "$DEPS_BRANCH" --platform Linux/Qt5/Shared --skip-publishing
python3 -u build-tools/ci-scripts/build-appimage.py
