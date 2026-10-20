#!/usr/bin/env bash
# Build VectorFM for the Korg NTS-1 mkII without Docker.
# Downloads the ARM GCC 10.3 toolchain (xPack) and the logue SDK on first run,
# then builds dist/VectorFM.nts1mkiiunit.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
DEPS="$ROOT/.deps"
SDK_DIR="$DEPS/logue-sdk"
SDK_REPO="https://github.com/korginc/logue-sdk"
SDK_COMMIT="e78b16c6068b1c30fc072f2aebb8da5eef6c005c"

mkdir -p "$DEPS"

# Preferred: Arm's official GCC 10.3-2021.10, exactly what Korg's SDK/Docker uses.
# Fallback: xPack's build of the same GCC 10.3 (for hosts that can't reach developer.arm.com).
ARM_VER="10.3-2021.10"
ARM_BASE="https://developer.arm.com/-/media/Files/downloads/gnu-rm/$ARM_VER"
XPACK_VER="10.3.1-2.3"
XPACK_BASE="https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v$XPACK_VER"

case "$(uname -s)-$(uname -m)" in
  Linux-x86_64)  ARM_PKG="gcc-arm-none-eabi-$ARM_VER-x86_64-linux.tar.bz2";  XPACK_PLAT="linux-x64" ;;
  Linux-aarch64) ARM_PKG="gcc-arm-none-eabi-$ARM_VER-aarch64-linux.tar.bz2"; XPACK_PLAT="linux-arm64" ;;
  Darwin-*)      ARM_PKG="gcc-arm-none-eabi-$ARM_VER-mac.tar.bz2";            XPACK_PLAT="darwin-$( [ "$(uname -m)" = arm64 ] && echo arm64 || echo x64 )" ;;
  *) echo "Unsupported platform: $(uname -s)-$(uname -m)"; exit 1 ;;
esac

ARM_DIR="$DEPS/gcc-arm-none-eabi-$ARM_VER"
XPACK_DIR="$DEPS/xpack-arm-none-eabi-gcc-$XPACK_VER"

if [ ! -x "$ARM_DIR/bin/arm-none-eabi-gcc" ] && [ "${VFM_TOOLCHAIN:-}" != "xpack" ]; then
  echo "==> Downloading Arm GNU toolchain $ARM_VER"
  curl -fsSL "$ARM_BASE/$ARM_PKG" | tar xj -C "$DEPS" || echo "    (Arm download unavailable)"
fi

if [ -x "$ARM_DIR/bin/arm-none-eabi-gcc" ] && [ "${VFM_TOOLCHAIN:-}" != "xpack" ]; then
  GCC_DIR="$ARM_DIR"
elif [ "${VFM_TOOLCHAIN:-}" = "arm" ]; then
  echo "Arm toolchain required (VFM_TOOLCHAIN=arm) but not available"; exit 1
else
  if [ ! -x "$XPACK_DIR/bin/arm-none-eabi-gcc" ]; then
    echo "==> Downloading xPack ARM GCC $XPACK_VER (fallback)"
    curl -fsSL "$XPACK_BASE/xpack-arm-none-eabi-gcc-$XPACK_VER-$XPACK_PLAT.tar.gz" | tar xz -C "$DEPS"
  fi
  GCC_DIR="$XPACK_DIR"
fi
echo "==> Toolchain: $("$GCC_DIR/bin/arm-none-eabi-gcc" --version | head -1)"

if [ ! -d "$SDK_DIR/platform/nts-1_mkii" ]; then
  echo "==> Fetching logue SDK"
  git clone -q "$SDK_REPO" "$SDK_DIR"
  git -C "$SDK_DIR" checkout -q "$SDK_COMMIT"
  git -C "$SDK_DIR" submodule update -q --init --depth 1 platform/ext/CMSIS
fi

PLAT="$SDK_DIR/platform/nts-1_mkii"

mkdir -p "$ROOT/dist"

echo "==> Building VectorFM"
make -C "$ROOT/vectorfm" ${1:-install} \
  GCC_BIN_PATH="$GCC_DIR/bin" \
  COMMON_INC_PATH="$PLAT/common" \
  COMMON_SRC_PATH="$PLAT/common" \
  EXTDIR="$SDK_DIR/platform/ext" \
  LDDIR="$PLAT/ld" \
  INSTALLDIR="$ROOT/dist"

if [ "${1:-install}" = "install" ]; then
  echo "==> Done: dist/VectorFM.nts1mkiiunit"
fi
