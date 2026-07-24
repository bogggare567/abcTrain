#!/bin/bash
# Double-click after installing to jump straight to where the plugins
# landed. The stock macOS Installer.app has no "open folder when done"
# button of its own (unlike a Windows Inno Setup installer), so this is
# the equivalent shipped alongside the .pkg on the DMG instead - see
# decisions/008-installers.md.
open "/Library/Audio/Plug-Ins/VST3" "/Library/Audio/Plug-Ins/Components" "/Applications/abcTrain" 2>/dev/null
open "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components" 2>/dev/null
exit 0
