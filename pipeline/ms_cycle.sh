#!/bin/sh
# Kept for muscle memory: the build is pipeline/ms_cycle.py (MS-082), which runs on macOS, Windows and Linux.
exec python3 "$(dirname "$0")/ms_cycle.py" "$@"
