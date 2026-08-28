#!/bin/bash
#
# Removes everything abcTrain installed, and nothing else.
#
# macOS has no uninstaller for a .pkg - the installer writes files and
# forgets about them - so this is the only way to undo it without hunting
# through four folders by hand. It ships on the disk image next to the
# installer, so it is there before anyone needs it rather than after.
#
# Two rules this script keeps:
#
#   - It shows the full list and asks, before it deletes anything.
#   - Your progress is a separate question, asked separately. Somebody
#     uninstalling to reinstall a newer build almost never means "and
#     throw away my levels", and the two are trivially easy to conflate.

set -u

BOLD=$'\033[1m'; DIM=$'\033[2m'; RED=$'\033[31m'; GREEN=$'\033[32m'; OFF=$'\033[0m'

PLUGINS=("ABC Ear Trainer" "ABC Learner EQ" "ABC Learner Comp" "ABC Learner Verb")

# Both roots: the installer offers "for all users" and "for me only", and
# somebody who has done both over time has copies in both places.
VST3_DIRS=("/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/VST3")
AU_DIRS=("/Library/Audio/Plug-Ins/Components" "$HOME/Library/Audio/Plug-Ins/Components")
APP_DIRS=("/Applications/abcTrain" "$HOME/Applications/abcTrain")

# Levels, achievements, per-exercise stats; then theme/language/modules;
# then the reference-audio library's own bookkeeping. Three files in two
# folders - see docs/wiki/Troubleshooting.md, which named only one of them
# for a long time.
SETTINGS_DIRS=("$HOME/Library/Application Support/EarTrainer"
               "$HOME/Library/Application Support/abcTrain")

# Written by ReferenceAudioLibrary::addBuiltInCategories, not by the
# installer - but it is ours, and leaving it behind is leaving litter.
CACHE_DIRS=("$TMPDIR/abcTrain")

found=()

collect() {
    local dir="$1" suffix="$2"
    [ -d "$dir" ] || return 0

    for name in "${PLUGINS[@]}"; do
        [ -e "$dir/$name$suffix" ] && found+=("$dir/$name$suffix")
    done
}

for d in "${VST3_DIRS[@]}"; do collect "$d" ".vst3"; done
for d in "${AU_DIRS[@]}"; do collect "$d" ".component"; done
for d in "${APP_DIRS[@]}"; do [ -d "$d" ] && found+=("$d"); done
for d in "${CACHE_DIRS[@]}"; do [ -d "$d" ] && found+=("$d"); done

echo
echo "${BOLD}Uninstall abcTrain${OFF}"
echo

if [ ${#found[@]} -eq 0 ]; then
    echo "Nothing to remove - no abcTrain plugins or apps found in the"
    echo "places the installer writes to."
    echo
    echo "${DIM}If your DAW still lists them, it is showing a cached scan."
    echo "Rescan its plugin folders and they will go.${OFF}"
    echo
    read -r -p "Press Return to close. " _
    exit 0
fi

echo "This will remove:"
echo
for f in "${found[@]}"; do echo "  ${RED}-${OFF} $f"; done
echo

needs_admin=0
for f in "${found[@]}"; do
    case "$f" in /Library/*|/Applications/*) needs_admin=1 ;; esac
done

[ "$needs_admin" -eq 1 ] && echo "${DIM}Some of these are outside your Home folder, so you will be asked${OFF}
${DIM}for your password.${OFF}
"

read -r -p "Remove these? [y/N] " answer
case "$answer" in
    [yY]|[yY][eE][sS]) ;;
    *) echo; echo "Nothing was removed."; echo; read -r -p "Press Return to close. " _; exit 0 ;;
esac

echo
for f in "${found[@]}"; do
    # Quoted, and never a bare variable: an empty one here would be a
    # catastrophically different command.
    if [ -n "$f" ] && [ -e "$f" ]; then
        case "$f" in
            /Library/*|/Applications/*) sudo rm -rf "$f" ;;
            *) rm -rf "$f" ;;
        esac
        echo "  ${GREEN}removed${OFF} $f"
    fi
done

# Package receipts, so a later install does not think it is an upgrade of
# something that is no longer there. Failing here is harmless.
if [ "$needs_admin" -eq 1 ]; then
    for pkg in $(pkgutil --pkgs 2>/dev/null | grep -i '^com\.earsnap\.abctrain\.' || true); do
        sudo pkgutil --forget "$pkg" >/dev/null 2>&1 && echo "  ${GREEN}forgot${OFF} receipt $pkg"
    done
fi

echo
echo "${BOLD}Plugins and applications removed.${OFF}"
echo

# --- the separate question --------------------------------------------
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
        *)
            echo
            echo "Progress kept."
            ;;
    esac
    echo
fi

echo "${DIM}Your DAW may still list the plugins until it rescans.${OFF}"
echo
read -r -p "Press Return to close. " _
