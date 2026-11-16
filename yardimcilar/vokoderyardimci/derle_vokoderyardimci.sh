#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# lib/<abi>/libvokoderyardimci.so'yu (eklenti yukleyicisi, AYRI SUREC) vokoder_yardimci.c'den uretir.
# Cikti: app/src/main/jniLibs/{arm64-v8a,armeabi-v7a}/libvokoderyardimci.so
#
# NEDEN AYRI SUREC (2026-11-16, lisans incelemesi Y1, karar b): vokoder eklentisi
# (mbelib-neo, GPL-2.0-or-later) eskiden uygulamanin surecine dlopen ile yukleniyordu.
# Artik bu kucuk yukleyici (GPL-2.0-or-later, kaynagi eklenti deposunda) eklentiyi
# kendi surecinde acar; uygulama onunla stdin/stdout borusuyla konusur.
#
# NEDEN lib*.so ADI: paket yoneticisi yalnizca lib*.so dosyalarini nativeLibraryDir'e
# cikarir; Android 10+ yurutulebilir dosyayi yalnizca oradan calistirmaya izin verir.
#
# DINAMIK + PIE: eklentiyi dlopen ile acmak icin bionic'in dinamik baglayicisi gerekir.
# Gerekenler: Android SDK icinde NDK 28.2.13676358.
set -e
J="$(cd "$(dirname "$0")" && pwd)"
SDK="${ANDROID_SDK:-${LOCALAPPDATA:-$HOME}/Android/Sdk}"
NDK="${ANDROID_NDK:-$SDK/ndk/28.2.13676358}"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) HOST=windows-x86_64; X=.exe ;;
  Darwin) HOST=darwin-x86_64; X= ;;
  *) HOST=linux-x86_64; X= ;;
esac
TB="$NDK/toolchains/llvm/prebuilt/$HOST/bin"
# Cikti: uygulama kaynaginda app/src/main/jniLibs/<abi>/; eklenti deposundaki
# kopyada (yardimcilar/) yardimcilar/cikti/<abi>/. Derleme bayraklari ikisinde ayni.
JL="$J/../../src/main/jniLibs"
[ -d "$JL" ] || JL="$J/../cikti"
for ABI in arm64-v8a armeabi-v7a; do
  case "$ABI" in
    arm64-v8a)   HEDEF=aarch64-linux-android21 ;;
    armeabi-v7a) HEDEF=armv7a-linux-androideabi21 ;;
  esac
  mkdir -p "$JL/$ABI"
  CIKTI="$JL/$ABI/libvokoderyardimci.so"
  "$TB/clang$X" --target=$HEDEF -O2 -fPIE -pie -Wall \
    -ffile-prefix-map="$J"=. \
    -Wl,--gc-sections -Wl,-z,max-page-size=16384 \
    -o "$CIKTI" "$J/vokoder_yardimci.c" -ldl
  "$TB/llvm-strip$X" --strip-all "$CIKTI"
  "$TB/llvm-readelf$X" -h "$CIKTI" | grep -E "Class|Type|Machine"
  ls -la "$CIKTI"
done
