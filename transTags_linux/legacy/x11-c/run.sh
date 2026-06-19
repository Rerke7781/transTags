#!/usr/bin/env bash
set -euo pipefail

if [[ "${XDG_SESSION_TYPE:-}" != "x11" ]]; then
  echo "Current session is '${XDG_SESSION_TYPE:-unknown}'."
  echo "transTags Linux needs an X11 session. Log out and choose 'Ubuntu on Xorg'."
  exit 1
fi

make
exec ./transTags_linux
