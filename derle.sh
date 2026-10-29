#!/usr/bin/env bash
# DMR Cepte ses cozucu eklentisi — derleme betigi (Android NDK + CMake).
#   ANDROID_NDK=/yol/ndk/28.x  CMAKE=/yol/cmake  NINJA=/yol/ninja  bash derle.sh
# Cikti: vocoder_plugin.android.{arm64,arm,x86_64} ve SHA256SUMS (bu klasorde).
# mbelib-neo ve PFFFT kaynaklari third_party/ altindadir; ag gerekmez.
set -euo pipefail
KOK="$(cd "$(dirname "$0")" && pwd)"
NDK="${ANDROID_NDK:?ANDROID_NDK tanimli degil}"
CMAKE="${CMAKE:-cmake}"
NINJA="${NINJA:-ninja}"
TC="$NDK/build/cmake/android.toolchain.cmake"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) HOST=windows-x86_64 ;;
  Darwin) HOST=darwin-x86_64 ;;
  *) HOST=linux-x86_64 ;;
esac
CC="$NDK/toolchains/llvm/prebuilt/$HOST/bin/clang"
[ -x "$CC" ] || CC="$CC.exe"
GEC="${TMPDIR:-/tmp}/dmrcepte-eklenti-derle"
rm -rf "$GEC"; mkdir -p "$GEC"
for ABI in arm64-v8a armeabi-v7a x86_64; do
  case "$ABI" in
    arm64-v8a)   AD=arm64;  HEDEF=aarch64-linux-android21 ;;
    armeabi-v7a) AD=arm;    HEDEF=armv7a-linux-androideabi21 ;;
    x86_64)      AD=x86_64; HEDEF=x86_64-linux-android21 ;;
  esac
  B="$GEC/$ABI"
  "$CMAKE" -S "$KOK/third_party/mbelib-neo" -B "$B" -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" \
    -DCMAKE_TOOLCHAIN_FILE="$TC" -DANDROID_ABI="$ABI" -DANDROID_PLATFORM=android-21 \
    -DCMAKE_BUILD_TYPE=Release -DMBELIB_BUILD_TESTS=OFF -DMBELIB_BUILD_EXAMPLES=OFF \
    -DMBELIB_ENABLE_NATIVE=OFF -DMBELIB_WARNINGS_AS_ERRORS=OFF \
    -DFETCHCONTENT_SOURCE_DIR_MBELIB_PFFFT="$KOK/third_party/pffft" -DFETCHCONTENT_FULLY_DISCONNECTED=ON
  "$CMAKE" --build "$B" --target mbe-static
  # -s: NDK arac zinciri -g ekliyor; hata ayiklama bilgisi ve derleyen makinenin
  # yollari ikiliye girmesin. Disa acik (dinamik) semboller kalir.
  "$CC" --target="$HEDEF" -shared -fPIC -O2 -std=c99 -fvisibility=hidden -Wall -s \
    -I "$KOK/third_party/mbelib-neo/include" -I "$B/include" \
    -o "$KOK/vocoder_plugin.android.$AD" "$KOK/src/vocoder_plugin.c" "$B/libmbe-neo.a" -lm \
    -Wl,--exclude-libs,ALL -Wl,--gc-sections -Wl,-z,max-page-size=16384 -Wl,--no-undefined -Wl,--build-id
done
(cd "$KOK" && sha256sum vocoder_plugin.android.* > SHA256SUMS)
echo "TAMAM"; cat "$KOK/SHA256SUMS"
