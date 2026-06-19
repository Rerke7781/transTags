#!/usr/bin/env bash
set -euo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESKTOP_DIR="$HOME/.local/share/applications"
DESKTOP_FILE="$DESKTOP_DIR/transTags_linux.desktop"

mkdir -p "$DESKTOP_DIR"

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=transTags Linux
Comment=Window opacity, click-through, always-on-top and centering hotkeys
Exec=$APP_DIR/run.sh
Icon=preferences-system-windows
Terminal=false
Categories=Utility;
StartupNotify=false
EOF

chmod +x "$DESKTOP_FILE"
echo "Installed $DESKTOP_FILE"
