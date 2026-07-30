#!/usr/bin/env bash
# Interactive installer for the abcTrain Linux release. Run this from
# inside the extracted abcTrain-<version>/ directory - it looks for
# sibling <Plugin>/VST3 and <Plugin>/Standalone folders next to itself
# (see package_tar.sh for how that layout is built).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# name:folder-key (folder-key must match package_tar.sh's PLUGINS array)
PLUGINS=(
    "ABC Ear Trainer:EarTrainer"
    "ABC Learner EQ:LearnerEQ"
    "ABC Learner Comp:LearnerComp"
    "ABC Learner Verb:LearnerVerb"
)

ask_yes_no() {
    local prompt="$1" default="$2" hint answer
    hint="y/N"; [ "$default" = "y" ] && hint="Y/n"
    while true; do
        read -r -p "$prompt [$hint]: " answer || answer=""
        answer="${answer:-$default}"
        case "$answer" in
            [Yy]*) echo "y"; return ;;
            [Nn]*) echo "n"; return ;;
            *) echo "Please answer y or n." >&2 ;;
        esac
    done
}

echo "abcTrain installer"
echo "==================="
echo "This copies the plugins you choose into place - nothing is installed"
echo "without you confirming it below."
echo

SELECTED_PLUGINS=()
for entry in "${PLUGINS[@]}"; do
    name="${entry%%:*}"
    key="${entry##*:}"
    if [ -d "$SCRIPT_DIR/$key" ]; then
        if [ "$(ask_yes_no "Install $name?" "y")" = "y" ]; then
            SELECTED_PLUGINS+=("$key")
        fi
    fi
done

if [ "${#SELECTED_PLUGINS[@]}" -eq 0 ]; then
    echo "Nothing selected - exiting without changing anything."
    exit 0
fi

echo
echo "Where should VST3 plugins go?"
echo "  1) This user only: \$HOME/.vst3 (default, no sudo needed)"
echo "  2) All users of this machine: /usr/lib/vst3 (needs sudo)"
echo "  3) A custom path you type in"
read -r -p "Choose [1-3, default 1]: " vst3_choice || vst3_choice=""
NEED_SUDO_VST3=0
case "${vst3_choice:-1}" in
    2) VST3_DEST="/usr/lib/vst3"; NEED_SUDO_VST3=1 ;;
    3) read -r -p "Custom VST3 path: " VST3_DEST ;;
    *) VST3_DEST="$HOME/.vst3" ;;
esac

echo
STANDALONE_CHOICE="$(ask_yes_no "Also install the Standalone app(s)?" "n")"
STANDALONE_DEST="$HOME/.local/share/abcTrain"
if [ "$STANDALONE_CHOICE" = "y" ]; then
    read -r -p "Standalone install path [default: $STANDALONE_DEST]: " custom_dest || custom_dest=""
    STANDALONE_DEST="${custom_dest:-$STANDALONE_DEST}"
fi

copy_maybe_sudo() {
    local src="$1" dest_dir="$2" need_sudo="$3"
    if [ "$need_sudo" = "1" ]; then
        sudo mkdir -p "$dest_dir"
        sudo cp -r "$src" "$dest_dir/"
    else
        mkdir -p "$dest_dir"
        cp -r "$src" "$dest_dir/"
    fi
}

for key in "${SELECTED_PLUGINS[@]}"; do
    if [ -d "$SCRIPT_DIR/$key/VST3" ]; then
        shopt -s nullglob
        for vst3 in "$SCRIPT_DIR/$key/VST3/"*.vst3; do
            copy_maybe_sudo "$vst3" "$VST3_DEST" "$NEED_SUDO_VST3"
            echo "Installed $(basename "$vst3") -> $VST3_DEST"
        done
        shopt -u nullglob
    fi

    if [ "$STANDALONE_CHOICE" = "y" ] && [ -d "$SCRIPT_DIR/$key/Standalone" ]; then
        mkdir -p "$STANDALONE_DEST"
        cp -r "$SCRIPT_DIR/$key/Standalone/." "$STANDALONE_DEST/"
        echo "Installed Standalone build(s) -> $STANDALONE_DEST"
    fi
done

echo
echo "Done."
echo "VST3 plugins: $VST3_DEST"
[ "$STANDALONE_CHOICE" = "y" ] && echo "Standalone apps: $STANDALONE_DEST"
exit 0
