#!/bin/sh
# Starts gbtg from its own folder, so OriginalFiles beside it (or a relative GameDir) resolves there.
# Settings, saves, the log and the shader cache live in ~/.local/share/gbtg.
here=$(dirname "$(readlink -f "$0")")
cd "$here" || exit 1
exec ./gbtg "$@"
