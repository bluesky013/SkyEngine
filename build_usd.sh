#!/bin/bash
# OpenUSD minimal monolithic 构建（Release only），产出对齐 thirdparty 布局:
#   3rd/output/usd/{include, lib/Release}
# 用法: ./build_usd.sh <source_dir>
set -euo pipefail

ENJ=$HOME/code/SkyEngine
SRC=${1:-$ENJ/3rd/intermediate/OpenUSD}
BUILD=$SRC/build_usd
INSTALL=$BUILD/install
OUT=$ENJ/3rd/output/usd

export CMAKE_POLICY_VERSION_MINIMUM=3.5

cmake -S "$SRC" -B "$BUILD" -G Xcode \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$INSTALL" \
  -DCMAKE_PREFIX_PATH="/opt/homebrew;/opt/homebrew/opt/tbb" \
  -DPXR_ENABLE_PYTHON_SUPPORT=OFF \
  -DPXR_ENABLE_GL_SUPPORT=OFF \
  -DPXR_BUILD_IMAGING=OFF \
  -DPXR_BUILD_USDVIEW=OFF \
  -DPXR_BUILD_USD_TOOLS=OFF \
  -DPXR_BUILD_TESTS=OFF \
  -DPXR_BUILD_EXAMPLES=OFF \
  -DPXR_BUILD_TUTORIALS=OFF \
  -DPXR_BUILD_MONOLITHIC=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DPXR_ENABLE_MATERIALX_SUPPORT=OFF \
  -DPXR_ENABLE_OPENVDB_SUPPORT=OFF \
  -DPXR_ENABLE_PTEX_SUPPORT=OFF \
  -DPXR_ENABLE_HDF5_SUPPORT=OFF

cmake --build "$BUILD" --config Release -- -jobs 12
cmake --install "$BUILD" --config Release

# 拷贝到 thirdparty 布局
rm -rf "$OUT"
mkdir -p "$OUT/lib/Release"
cp -R "$INSTALL/include" "$OUT/include"
# monolithic 库的产出名按平台不同（libusd_ms.dylib）
find "$INSTALL/lib" -maxdepth 1 -name "libusd_ms*" -exec cp {} "$OUT/lib/Release/" \;
find "$INSTALL/lib" -maxdepth 1 -name "*.dylib" -exec cp {} "$OUT/lib/Release/" \;
# plugInfo.json 资源（ArDefaultResolver/SDF 格式插件解析需要）
cp -R "$INSTALL/lib/usd" "$OUT/lib/usd"
cp -R "$INSTALL/plugin" "$OUT/plugin"
echo "USD_BUILD_DONE"
ls "$OUT/lib/Release/" | head
