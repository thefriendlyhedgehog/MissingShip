#!/bin/bash
# MissingShip — The Missing Link on the Ship of Harkinian engine.
#
# Launches MissingShip.app so macOS sees a real bundle identity
# (com.missingship.MissingShip) — per-app audio tools like QuietDown can only target a
# bundled app, not a bare executable. SHIP_HOME is also baked into the app's Info.plist,
# so double-clicking the .app works too.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$HERE/MissingShip.app"
export SHIP_HOME="$HERE/home"

# Double-clicking the app takes SHIP_HOME from Info.plist (LSEnvironment). Keep it pointing at THIS checkout's home,
# so a clone on another machine works by double-click after one scripted launch (MS-009). Info.plist is not part of
# the ad-hoc signature, so editing it is safe; lsregister refreshes Launch Services' cached copy.
PLIST="$APP/Contents/Info.plist"
if [ "$(plutil -extract LSEnvironment.SHIP_HOME raw "$PLIST" 2>/dev/null)" != "$SHIP_HOME" ]; then
  plutil -replace LSEnvironment.SHIP_HOME -string "$SHIP_HOME" "$PLIST"
  /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$APP" 2>/dev/null || true
fi

mkdir -p "$SHIP_HOME/mods"
[ -f "$SHIP_HOME/shipofharkinian.json" ] || cp "$HERE/profile/shipofharkinian.json" "$SHIP_HOME/"
cp "$HERE/mod/MissingLink_Scenes.o2r" "$SHIP_HOME/mods/"

if [ ! -f "$SHIP_HOME/oot.o2r" ]; then
  echo "MissingShip: $SHIP_HOME/oot.o2r is missing."
  echo "Copy the oot.o2r that Ship of Harkinian generated from your own OoT NTSC 1.0 ROM into:"
  echo "  $SHIP_HOME/"
  exit 1
fi

# one instance only
# -x matches the exact process NAME; -f would also match any shell whose command
# line merely contains this path (including the one invoking this script).
pkill -x MissingShip 2>/dev/null || true

cd "$APP/Contents/MacOS"
exec ./MissingShip "$@"
