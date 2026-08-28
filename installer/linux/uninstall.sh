#!/bin/bash
#
# Removes everything install.sh put down, and nothing else.
#
# Same two rules as the macOS uninstaller: show the whole list and ask
# before deleting anything, and treat "remove the software" and "throw
# away my progress" as two separate questions - somebody uninstalling in
# order to reinstall a newer build almost never means the second one.

set -u

BOLD=$'\033[1m'; DIM=$'\033[2m'; RED=$'\033[31m'; GREEN=$'\033[32m'; OFF=$'\033[0m'

PLUGINS=("ABC Ear Trainer" "ABC Learner EQ" "ABC Learner Comp" "ABC Learner Verb")

# The three places install.sh offers, plus the standalone folder.
VST3_DIRS=("$HOME/.vst3" "/usr/lib/vst3" "/usr/local/lib/vst3")
APP_DIRS=("$HOME/.local/share/abcTrain")

# Progress, then theme/language/modules. XDG first, with the plain ~/.config
# fallback JUCE uses when XDG_CONFIG_HOME is unset.
CONFIG_ROOT="${XDG_CONFIG_HOME:-$HOME/.config}"
SETTINGS_DIRS=("$CONFIG_ROOT/EarTrainer" "$CONFIG_ROOT/abcTrain")
CACHE_DIRS=("/tmp/abcTrain")

echo
echo "${BOLD}Uninstall abcTrain${OFF}"
echo

# A custom VST3 path was offered at install time and is not written down
# anywhere, so ask rather than guess. Guessing wrong here means either
# leaving plugins behind or deleting somebody else's folder.
read -r -p "Did you install the VST3s to a custom folder? Leave blank if not: " custom
[ -n "$custom" ] && VST3_DIRS+=("$custom")

found=()

for dir in "${VST3_DIRS[@]}"; do
    [ -d "$dir" ] || continue
    for name in "${PLUGINS[@]}"; do
        [ -e "$dir/$name.vst3" ] && found+=("$dir/$name.vst3")
    done
done

for dir in "${APP_DIRS[@]}" "${CACHE_DIRS[@]}"; do
    [ -d "$dir" ] && found+=("$dir")
done

if [ ${#found[@]} -eq 0 ]; then
    echo
    echo "Nothing to remove - no abcTrain plugins or apps found in the"
    echo "places install.sh writes to."
    echo
    echo "${DIM}If your DAW still lists them, it is showing a cached scan.${OFF}"
    exit 0
fi

echo
echo "This will remove:"
echo
for f in "${found[@]}"; do echo "  ${RED}-${OFF} $f"; done
echo

needs_sudo=0
for f in "${found[@]}"; do
    case "$f" in "$HOME"/*) ;; *) needs_sudo=1 ;; esac
done

[ "$needs_sudo" -eq 1 ] && echo "${DIM}Some of these are outside your home folder, so sudo will be used.${OFF}
"

read -r -p "Remove these? [y/N] " answer
case "$answer" in
    [yY]|[yY][eE][sS]) ;;
    *) echo; echo "Nothing was removed."; exit 0 ;;
esac

echo
for f in "${found[@]}"; do
    # Never a bare variable, and never without checking it still exists:
    # an empty one here would be a very different command.
    [ -n "$f" ] && [ -e "$f" ] || continue

    case "$f" in
        "$HOME"/*|/tmp/*) rm -rf "$f" ;;
        *) sudo rm -rf "$f" ;;
    esac
    echo "  ${GREEN}removed${OFF} $f"
done

echo
echo "${BOLD}Plugins and applications removed.${OFF}"
echo

remaining=()
for d in "${SETTINGS_DIRS[@]}"; do [ -d "$d" ] && remaining+=("$d"); done

if [ ${#remaining[@]} -gt 0 ]; then
    echo "Your progress is still here - levels, achievements, per-exercise"
    echo "records, and your theme and language:"
    echo
    for d in "${remaining[@]}"; do echo "  $d"; done
    echo
    echo "${DIM}Keep it if you might reinstall. It is a few kilobytes.${OFF}"
    echo
    read -r -p "Delete your progress too? [y/N] " answer

    case "$answer" in
        [yY]|[yY][eE][sS])
            for d in "${remaining[@]}"; do
                [ -n "$d" ] && [ -d "$d" ] && rm -rf "$d" && echo "  ${GREEN}removed${OFF} $d"
            done
            echo
            echo "Progress deleted."
            ;;
        *) echo; echo "Progress kept." ;;
    esac
    echo
fi

echo "${DIM}Your DAW may still list the plugins until it rescans.${OFF}"
