#!/usr/bin/env bash
# Build and run the desktop DSP tests (needs build.sh to have fetched the SDK once).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
[ -d .deps/logue-sdk ] || ./build.sh nts1
c++ -O2 -std=c++17 -Wall -Wextra -I core -I .deps/logue-sdk/platform/nts-1_mkii/common \
  test/render.cc -o test/render
./test/render
