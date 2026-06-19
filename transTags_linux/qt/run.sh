#!/usr/bin/env bash
set -euo pipefail

if [[ "${XDG_SESSION_TYPE:-}" != "x11" ]]; then
  echo "Current session is '${XDG_SESSION_TYPE:-unknown}'."
  echo "For full-window control, log out and choose 'Ubuntu on Xorg'."
  echo "Continuing anyway; Wayland native windows may not be controllable."
fi

if [[ ! -x build/transTags_linux ]]; then
  ./build.sh
fi

exec ./build/transTags_linux
