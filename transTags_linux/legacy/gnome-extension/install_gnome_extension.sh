#!/usr/bin/env bash
set -euo pipefail

UUID="transtags@sim.local"
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST_DIR="$HOME/.local/share/gnome-shell/extensions/$UUID"

mkdir -p "$DEST_DIR"
rm -rf "$DEST_DIR"/*
cp "$SRC_DIR/metadata.json" "$SRC_DIR/extension.js" "$DEST_DIR/"
mkdir -p "$DEST_DIR/schemas"
cp "$SRC_DIR"/schemas/*.xml "$DEST_DIR/schemas/"
glib-compile-schemas "$DEST_DIR/schemas"

export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/$(id -u)/bus}"

python3 - "$UUID" <<'PY'
import ast
import subprocess
import sys

uuid = sys.argv[1]
raw = subprocess.check_output(
    ["gsettings", "get", "org.gnome.shell", "enabled-extensions"],
    text=True,
).strip()

enabled = ast.literal_eval(raw.replace("@as ", "")) if raw else []
if uuid not in enabled:
    enabled.append(uuid)
    subprocess.check_call([
        "gsettings",
        "set",
        "org.gnome.shell",
        "enabled-extensions",
        repr(enabled),
    ])
PY

gnome-extensions enable "$UUID" || {
  echo "Extension files installed and added to org.gnome.shell enabled-extensions."
  echo "The running GNOME Shell session has not scanned it yet."
  echo "Log out and log back in once, then it will load automatically."
  exit 1
}

gnome-extensions info "$UUID"
