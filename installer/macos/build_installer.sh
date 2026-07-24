#!/usr/bin/env bash
# Builds abcTrain-macOS-<version>.dmg from an already-built `cmake --build`
# tree: one pkgbuild component package per (plugin x format) = 12
# packages, combined via productbuild's distribution.xml into a single
# .pkg with a native component-selection UI, then wrapped in a DMG
# alongside a README link and a folder-opening helper script.
#
# Usage: build_installer.sh <build-dir> <output-dmg-path>
#   <build-dir>       the directory passed to `cmake -B` (contains
#                      EarTrainer_artefacts/, LearnerEQ_artefacts/, etc.)
#   <output-dmg-path> where to write the final .dmg
#
# See decisions/008-installers.md for the design (why System/User is a
# native choice here but a free-text custom path isn't, why AU/VST3
# default on and Standalone defaults off, etc).
set -euo pipefail

BUILD_DIR="${1:?usage: build_installer.sh <build-dir> <output-dmg-path>}"
OUTPUT_DMG="${2:?usage: build_installer.sh <build-dir> <output-dmg-path>}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"

VERSION="$(grep -oE 'VERSION [0-9]+\.[0-9]+\.[0-9]+' "$REPO_ROOT/CMakeLists.txt" | head -1 | awk '{print $2}')"
if [ -z "$VERSION" ]; then
    echo "Could not read a VERSION from CMakeLists.txt's project() call" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d)"
cleanup() {
    rm -rf "$WORK_DIR"
    rm -f "$SCRIPT_DIR/LICENSE.txt"
}
trap cleanup EXIT

PKG_DIR="$WORK_DIR/pkgs"
mkdir -p "$PKG_DIR"

# productbuild's <license> pane needs a file next to distribution.xml;
# generated here (not committed) so LICENSE never has to be kept in sync
# by hand in two places.
cp "$REPO_ROOT/LICENSE" "$SCRIPT_DIR/LICENSE.txt"

# key:TargetDirName:Product Name (must match CMakeLists.txt's target name
# and PRODUCT_NAME for that target exactly, including the space in
# "Learner EQ" etc.)
PLUGINS=(
    "eartrainer:EarTrainer:Ear Trainer"
    "learnereq:LearnerEQ:Learner EQ"
    "learnercomp:LearnerComp:Learner Comp"
    "learnerverb:LearnerVerb:Learner Verb"
)

build_component_pkg() {
    local identifier="$1" root="$2" install_location="$3" out="$4"
    pkgbuild \
        --root "$root" \
        --identifier "$identifier" \
        --version "$VERSION" \
        --install-location "$install_location" \
        "$out" >/dev/null
}

for entry in "${PLUGINS[@]}"; do
    IFS=":" read -r key target product <<< "$entry"
    artefacts="$BUILD_DIR/${target}_artefacts/Release"

    if [ ! -d "$artefacts" ]; then
        echo "Expected build output not found: $artefacts (did you build $target first?)" >&2
        exit 1
    fi

    vst3_root="$WORK_DIR/root_${key}_vst3"
    mkdir -p "$vst3_root"
    cp -R "$artefacts/VST3/${product}.vst3" "$vst3_root/"
    build_component_pkg "com.earsnap.abctrain.${key}.vst3" "$vst3_root" \
        "/Library/Audio/Plug-Ins/VST3" "$PKG_DIR/${key}_vst3.pkg"

    au_root="$WORK_DIR/root_${key}_au"
    mkdir -p "$au_root"
    cp -R "$artefacts/AU/${product}.component" "$au_root/"
    build_component_pkg "com.earsnap.abctrain.${key}.au" "$au_root" \
        "/Library/Audio/Plug-Ins/Components" "$PKG_DIR/${key}_au.pkg"

    app_root="$WORK_DIR/root_${key}_app"
    mkdir -p "$app_root"
    cp -R "$artefacts/Standalone/${product}.app" "$app_root/"
    build_component_pkg "com.earsnap.abctrain.${key}.app" "$app_root" \
        "/Applications/abcTrain" "$PKG_DIR/${key}_app.pkg"
done

PRODUCT_PKG="$WORK_DIR/abcTrain.pkg"
productbuild \
    --distribution "$SCRIPT_DIR/distribution.xml" \
    --package-path "$PKG_DIR" \
    --resources "$SCRIPT_DIR" \
    "$PRODUCT_PKG" >/dev/null

DMG_STAGING="$WORK_DIR/dmg"
mkdir -p "$DMG_STAGING"
cp "$PRODUCT_PKG" "$DMG_STAGING/abcTrain-${VERSION}.pkg"
cp "$SCRIPT_DIR/Open Plugins Folder.command" "$DMG_STAGING/"
chmod +x "$DMG_STAGING/Open Plugins Folder.command"

mkdir -p "$(dirname "$OUTPUT_DMG")"
hdiutil create -volname "abcTrain ${VERSION}" -srcfolder "$DMG_STAGING" -ov -format UDZO "$OUTPUT_DMG" >/dev/null

echo "Built $OUTPUT_DMG"
