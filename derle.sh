#!/usr/bin/env bash
# DMR Cepte ses cozucu eklentisi — derleme betigi (Android NDK + CMake).
#   ANDROID_NDK=/yol/ndk/28.x  CMAKE=/yol/cmake  NINJA=/yol/ninja  bash derle.sh
# Cikti: vocoder_plugin.android.{arm64,arm,x86_64} ve SHA256SUMS (bu klasorde).
# mbelib-neo kaynagi third_party/mbelib-neo altinda (resmi yayin arsivinden birebir);
# PFFFT, mbelib-neo'nun kendi icinde sabitledigi arsivden (third_party/mbelib-neo/
# third_party/pffft-<commit>.tar.gz) mbelib-neo'nun SHA-256 denetimiyle acilir. Ag gerekmez.
#
# DURDURAN DENETIMLER (sessizce yanlis ikili uretilmesin):
#  - CMake "Manually-specified variables were not used" derse (secenek adi degismis);
#  - disa acik sembol kumesi tools/semboller.txt'den farkliysa (yukleyici dlsym ile arar);
#  - ikilideki mbelib-neo surum dizgisi CMakeLists.txt'deki surumle ayni degilse.
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
BIN="$NDK/toolchains/llvm/prebuilt/$HOST/bin"
CC="$BIN/clang"; [ -x "$CC" ] || CC="$CC.exe"
NM="$BIN/llvm-nm"; [ -x "$NM" ] || NM="$NM.exe"
SURUM="$(sed -n 's/^project(mbelib-neo VERSION \([0-9.]*\) .*/\1/p' "$KOK/third_party/mbelib-neo/CMakeLists.txt")"
[ -n "$SURUM" ] || { echo "HATA: mbelib-neo surumu CMakeLists.txt'den okunamadi"; exit 1; }
echo "mbelib-neo $SURUM"
GEC="${TMPDIR:-/tmp}/dmrcepte-eklenti-derle"
# Windows'ta yol 260 karakteri asamaz; FetchContent'in PFFFT alt derlemesi ~150 karakter ekler.
if [ "$HOST" = windows-x86_64 ] && [ "${#GEC}" -gt 90 ]; then
  echo "HATA: gecici klasor yolu cok uzun (${#GEC} karakter); kisa bir TMPDIR verin (orn. /tmp)"; exit 1
fi
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
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON 2>&1 | tee "$GEC/$ABI.yapilandirma.txt"
  if grep -q "Manually-specified variables were not used" "$GEC/$ABI.yapilandirma.txt"; then
    echo "HATA ($ABI): CMake bir secenegi kullanmadi (adi degismis olabilir); yukaridaki uyariya bak"; exit 1
  fi
  "$CMAKE" --build "$B" --target mbe-static
  # -s: NDK arac zinciri -g ekliyor; hata ayiklama bilgisi ve derleyen makinenin
  # yollari ikiliye girmesin. Disa acik (dinamik) semboller kalir.
  CIKTI="$KOK/vocoder_plugin.android.$AD"
  "$CC" --target="$HEDEF" -shared -fPIC -O2 -std=c99 -fvisibility=hidden -Wall -s \
    -I "$KOK/third_party/mbelib-neo/include" -I "$B/include" \
    -o "$CIKTI" "$KOK/src/vocoder_plugin.c" "$B/libmbe-neo.a" -lm \
    -Wl,--exclude-libs,ALL -Wl,--gc-sections -Wl,-z,max-page-size=16384 -Wl,--no-undefined -Wl,--build-id
  if ! diff <("$NM" -D --defined-only "$CIKTI" | awk '{print $NF}' | sort) <(tr -d '\r' < "$KOK/tools/semboller.txt" | sort); then
    echo "HATA ($ABI): disa acik semboller tools/semboller.txt ile ayni degil"; exit 1
  fi
  if ! LC_ALL=C grep -a -q -F "$SURUM" "$CIKTI"; then
    echo "HATA ($ABI): ikilide '$SURUM' surum dizgisi yok"; exit 1
  fi
done
(cd "$KOK" && sha256sum vocoder_plugin.android.* > SHA256SUMS)
echo "TAMAM (mbelib-neo $SURUM)"; cat "$KOK/SHA256SUMS"
