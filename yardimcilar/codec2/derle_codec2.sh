#!/usr/bin/env bash
# DMR Cepte — Codec 2'yi (M17 ses kodegi) Android icin derler: libcodec2jni.so.
#
# Kaynak: github.com/drowe67/codec2 (LGPL 2.1), SABIT commit (asagida). Yalniz M17'nin
# kullandigi 3200 ve 1600 kipleri acik (CODEC2_MODE_EN_DEFAULT=0); diger kiplerin
# kodu derleyici tarafindan atilir. Kod tablolari codebook_uret.py ile uretilir
# (bu bilgisayarda Android disi C derleyicisi yok).
#
# Cikti: uygulama kaynaginda app/src/main/jniLibs/{arm64-v8a,armeabi-v7a}/libcodec2jni.so;
#        eklenti deposundaki kopyada (yardimcilar/codec2) yardimcilar/cikti/<abi>/.
# Kullanim: bash derle_codec2.sh [codec2_kaynak_dizini]
#   Dizin verilmezse: yaninda codec2-kaynak/ varsa (eklenti deposu) o kullanilir,
#   yoksa sabit commit GitHub'dan alinir. Ikisi de ayni ikiliyi uretir (2026-11-17 dogrulandi).
# Yollarda bosluk olabilir: dosya ve bayrak listeleri dizidir.
set -e

COMMIT=310777b1c6f1af0bc7c72f5b32f80f6fd9136962
BU="$(cd "$(dirname "$0")" && pwd)"
PROJE="$(cd "$BU/../.." && pwd)"
SDK="${ANDROID_SDK:-${LOCALAPPDATA:-$HOME}/Android/Sdk}"
NDK="${NDK:-${ANDROID_NDK:-$SDK/ndk/28.2.13676358}}"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) HOST=windows-x86_64; X=.exe ;;
  Darwin) HOST=darwin-x86_64; X= ;;
  *) HOST=linux-x86_64; X= ;;
esac
CLANG="$NDK/toolchains/llvm/prebuilt/$HOST/bin/clang$X"
[ -f "$CLANG" ] || { echo "NDK clang yok: $CLANG"; exit 1; }
PY=python; command -v python >/dev/null 2>&1 || PY=python3

KAYNAK="${1:-}"
if [ -z "$KAYNAK" ] && [ -d "$BU/codec2-kaynak/src" ]; then
  KAYNAK="$BU/codec2-kaynak"
  [ "$(tr -d '\r\n' < "$KAYNAK/KAYNAK_COMMIT")" = "$COMMIT" ] || { echo "codec2-kaynak baska commit'ten"; exit 1; }
  echo "codec2 ${COMMIT:0:7} (codec2-kaynak/)"
else
  if [ -z "$KAYNAK" ]; then
    KAYNAK="$(mktemp -d)/codec2"
    git clone --quiet https://github.com/drowe67/codec2.git "$KAYNAK"
  fi
  ( cd "$KAYNAK" && git checkout --quiet "$COMMIT" && echo "codec2 $(git rev-parse --short HEAD)" )
fi
SRC="$KAYNAK/src"
CB="$SRC/codebook"

URET="$(mktemp -d)"
# CMake'in cmake/version.h.in'den urettigi baslik (proje 1.2.0).
mkdir -p "$URET/codec2"
cat > "$URET/codec2/version.h" <<'SON'
#ifndef CODEC2_HAVE_VERSION
#define CODEC2_HAVE_VERSION
#define CODEC2_VERSION_MAJOR 1
#define CODEC2_VERSION_MINOR 2
#define CODEC2_VERSION_PATCH 0
#define CODEC2_VERSION "1.2.0"
#endif
SON
py() { "$PY" -I "$BU/codebook_uret.py" "$@"; }
LSP=(); DLSP=()
for i in 1 2 3 4 5 6 7 8 9 10; do LSP+=("$CB/lsp$i.txt"); DLSP+=("$CB/dlsp$i.txt"); done
py lsp_cb   "$URET/codebook.c"   "${LSP[@]}"
py lsp_cbd  "$URET/codebookd.c"  "${DLSP[@]}"
py lsp_cbjmv "$URET/codebookjmv.c" "$CB/lspjmv1.txt" "$CB/lspjmv2.txt" "$CB/lspjmv3.txt"
py ge_cb    "$URET/codebookge.c" "$CB/gecb.txt"
py newamp1vq_cb "$URET/codebooknewamp1.c" "$CB/train_120_1.txt" "$CB/train_120_2.txt"
py newamp1_energy_cb "$URET/codebooknewamp1_energy.c" "$CB/newamp1_energy_q.txt"
py newamp2vq_cb "$URET/codebooknewamp2.c" "$CB/codes_450.txt"
py newamp2_energy_cb "$URET/codebooknewamp2_energy.c" "$CB/newamp2_energy_q.txt"

# Kodlayici/cozucu cekirdegi (modem, FreeDV vb. YOK).
KAYNAKLAR=()
for f in codec2.c codec2_fft.c kiss_fft.c kiss_fftr.c lpc.c nlp.c postfilter.c sine.c interp.c \
         lsp.c mbest.c newamp1.c phase.c quantise.c pack.c; do
  KAYNAKLAR+=("$SRC/$f")
done
for f in codebook codebookd codebookjmv codebookge codebooknewamp1 codebooknewamp1_energy codebooknewamp2 codebooknewamp2_energy; do
  KAYNAKLAR+=("$URET/$f.c")
done

BAYRAK=(-O2 -fPIC -shared -std=gnu11 -w -DNDEBUG -fvisibility=hidden -ffunction-sections -fdata-sections -Wl,--gc-sections
 -DCODEC2_MODE_EN_DEFAULT=0 -DCODEC2_MODE_3200_EN=1 -DCODEC2_MODE_1600_EN=1
 "-I$SRC" "-I$URET" -Wl,--no-undefined -Wl,--build-id=sha1 -Wl,-z,max-page-size=16384 -s)

JL="$PROJE/app/src/main/jniLibs"
[ -d "$JL" ] || JL="$BU/../cikti"
for hedef in "arm64-v8a aarch64-linux-android21" "armeabi-v7a armv7a-linux-androideabi21"; do
  set -- $hedef
  ABI=$1; TRIPLE=$2
  mkdir -p "$JL/$ABI"
  CIKTI="$JL/$ABI/libcodec2jni.so"
  "$CLANG" --target=$TRIPLE "${BAYRAK[@]}" -o "$CIKTI" "$BU/codec2_jni.c" "${KAYNAKLAR[@]}" -lm -llog
  echo "$ABI: $(wc -c < "$CIKTI") bayt  $(sha256sum "$CIKTI" | cut -c1-16)"
done
