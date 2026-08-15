#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
workspace="$(cd "$script_dir/.." && pwd)"
source_dir="${STREAMBOX_FFMPEG_SOURCE:-/d/ffmpeg-8.1.2/ffmpeg-8.1.2}"
build_dir="$workspace/third_party/ffmpeg-build"
prefix="$workspace/third_party/ffmpeg-sdk"
qt_mingw="${STREAMBOX_QT_MINGW:-/d/Qt/Tools/mingw1310_64/bin}"

export PATH="$qt_mingw:/usr/bin:$PATH"
export PKG_CONFIG_PATH=""
export PKG_CONFIG_LIBDIR=""

mkdir -p "$build_dir" "$prefix"
cd "$build_dir"

if [[ ! -f config.h ]]; then
    "$source_dir/configure" \
        --prefix="$prefix" \
        --target-os=mingw32 \
        --arch=x86_64 \
        --cc=gcc \
        --cxx=g++ \
        --ar=ar \
        --ranlib=ranlib \
        --strip=strip \
        --nm=nm \
        --enable-shared \
        --disable-static \
        --disable-programs \
        --disable-doc \
        --disable-debug \
        --disable-avdevice \
        --enable-network \
        --enable-schannel \
        --enable-protocol=file,http,https,tcp,tls,udp,crypto,data,httpproxy \
        --extra-cflags="-O2" \
        --extra-ldflags="-static-libgcc"
fi

make -j"$(nproc)"
make install

echo "FFmpeg SDK installed to $prefix"
