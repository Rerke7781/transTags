#!/usr/bin/env bash
set -euo pipefail

sudo apt update
sudo apt install -y \
  build-essential \
  pkg-config \
  libx11-dev \
  libxext-dev \
  libxfixes-dev
