#!/usr/bin/env bash
# Builds abcTrain-Linux-<version>.tar.gz from an already-built
# `cmake --build` tree: <Plugin>/VST3/<Product>.vst3 and
# <Plugin>/Standalone/<Product> per plugin, plus install.sh/README.txt/
# LICENSE at the top level.
#
# Usage: package_tar.sh <build-dir> <output-tar-gz-path>
set -euo pipefail

BUILD_DIR="${1:?usage: package_tar.sh <build-dir> <output-tar-gz-path>}"
OUTPUT_TARBALL="${2:?usage: package_tar.sh <build-dir> <output-tar-gz-path>}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"

# Prefer the version CMake resolved from `git describe` (see the
# abctrain-version.txt block in CMakeLists.txt); fall back to parsing
# project() only when packaging outside a configured build tree.
if [ -f "$BUILD_DIR/abctrain-version.txt" ]; then
    VERSION="$(tr -d '[:space:]' < "$BUILD_DIR/abctrain-version.txt")"
else
    VERSION="$(grep -oE 'VERSION [0-9]+\.[0-9]+\.[0-9]+' "$REPO_ROOT/CMakeLists.txt" | head -1 | awk '{print $2}')"
fi
if [ -z "$VERSION" ]; then
    echo "Could not read a VERSION from CMakeLists.txt's project() call" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

STAGE_DIR="$WORK_DIR/abcTrain-${VERSION}"
mkdir -p "$STAGE_DIR"

# key:TargetDirName (folder key must match install.sh's PLUGINS array)
PLUGINS=(
    "EarTrainer:EarTrainer"
    "LearnerEQ:LearnerEQ"
    "LearnerComp:LearnerComp"
    "LearnerVerb:LearnerVerb"
)

for entry in "${PLUGINS[@]}"; do
    key="${entry%%:*}"
    target="${entry##*:}"
    artefacts="$BUILD_DIR/${target}_artefacts/Release"

    if [ ! -d "$artefacts" ]; then
        echo "Expected build output not found: $artefacts (did you build $target first?)" >&2
        exit 1
    fi

    mkdir -p "$STAGE_DIR/$key/VST3" "$STAGE_DIR/$key/Standalone"
    cp -r "$artefacts/VST3/." "$STAGE_DIR/$key/VST3/"
    cp -r "$artefacts/Standalone/." "$STAGE_DIR/$key/Standalone/"
done

cp "$SCRIPT_DIR/install.sh" "$STAGE_DIR/"
chmod +x "$STAGE_DIR/install.sh"
cp "$SCRIPT_DIR/README.txt" "$STAGE_DIR/"
cp "$REPO_ROOT/LICENSE" "$STAGE_DIR/"

mkdir -p "$(dirname "$OUTPUT_TARBALL")"
tar -czf "$OUTPUT_TARBALL" -C "$WORK_DIR" "abcTrain-${VERSION}"

echo "Built $OUTPUT_TARBALL"
