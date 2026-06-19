#!/usr/bin/env bash
set -euo pipefail

sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  pkg-config \
  qt6-base-dev \
  libqt6sql6-sqlite \
  libx11-dev \
  libxext-dev \
  libxfixes-dev
