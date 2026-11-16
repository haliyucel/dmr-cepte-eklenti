#!/bin/sh
# arm64-v8a/libmd380yardimci.so'yu (MD-380 firmware'ini native calistiran
# STATIK 32 bit ARM yurutulebilir) md380_yardimci.c'den bastan uretir.
# Cikti: app/src/main/jniLibs/{arm64-v8a,armeabi-v7a}/libmd380yardimci.so (ayni dosya)
#
# Gerekenler: Android SDK icinde NDK 28.2.13676358. Baska hicbir sey
# indirilmez (Unicorn yok).
#
# NEDEN arm64-v8a KLASORUNDE 32 BIT BIR IKILI: dosya yalnizca 64 bit
# kurulumlarda kullaniliyordu; 2026-11-16'dan beri armeabi-v7a'ya da kopyalanir (bkz. sonu)
# yapar). Paket yoneticisi yalnizca cihazin sectigi ABI klasorunu cikarir;
# yalnizca 64 bit calisan telefonlar lib/armeabi-v7a'ya hic bakmaz. Dosya
# hic dlopen edilmez, yalnizca exec edilir; ELF sinifi bu yuzden onemsiz.
#
# NEDEN STATIK: yalnizca 64 bit Android'lerde 32 bit bionic ve 32 bit linker
# (/system/bin/linker, /system/lib) YOKTUR; dinamik bagli bir 32 bit ikili
# daha baslamadan "No such file or directory" ile duser. Statik ikili
# cekirdegin COMPAT katmanindan baska bir sey istemez. -static PIE'yi de
# kapatir (ET_EXEC, 0x10000 civarina yuklenir): firmware'in sabit adresleri
# (0x0800C000, 0x10000000, 0x20000000) boylece bos kalir.
#
# -z max-page-size=16384: Google Play ve Android 15+ arm64-v8a altindaki her
# lib*.so icin LOAD bolumlerinin 16 KB hizali olmasini denetliyor; 32 bit
# oldugunu bilmeden bu dosyayi da tarar.
#
# -ffile-prefix-map: ikiliye yerel mutlak yollar girmesin (yeniden
# uretilebilirlik, kullanici adi sizmasin).
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
mkdir -p "$JL/arm64-v8a" "$JL/armeabi-v7a"
CIKTI="$JL/arm64-v8a/libmd380yardimci.so"
JM="$(cygpath -m "$J" 2>/dev/null || echo "$J")"

"$TB/clang$X" --target=armv7a-linux-androideabi21 -static -O2 -Wall \
  -ffunction-sections -fdata-sections \
  -ffile-prefix-map="$JM/=md380yardimci/" -ffile-prefix-map="$J/=md380yardimci/" \
  "$J/md380_yardimci.c" -lm \
  -Wl,--gc-sections -Wl,-z,max-page-size=16384 \
  -o "$CIKTI"
"$TB/llvm-strip$X" --strip-all "$CIKTI"

# DOGRULAMA: 32 bit ARM, yurutulebilir (EXEC), yorumlayici (INTERP) YOK,
# bolumler firmware'in adreslerinin altinda.
"$TB/llvm-readelf$X" -h "$CIKTI" | grep -E "Class|Type|Machine"
if "$TB/llvm-readelf$X" -l "$CIKTI" | grep -q INTERP; then
  echo "HATA: ikili dinamik bagli (INTERP var)"; exit 1
fi
"$TB/llvm-readelf$X" -l "$CIKTI" | grep LOAD
ls -la "$CIKTI"
# 32 BIT KURULUM DA AYNI YARDIMCIYI KULLANIR (2026-11-16, lisans incelemesi Y1/b):
# eskiden telsizde (armeabi-v7a) ayni is uygulama icinde JNI ile (libmd380vocoder.so)
# yapiliyordu. Statik 32 bit ikili 32 bit cekirdekte de dogrudan calisir.
IKINCI="$JL/armeabi-v7a/libmd380yardimci.so"
cp "$CIKTI" "$IKINCI"
ls -la "$IKINCI"
