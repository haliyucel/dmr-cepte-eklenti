#!/usr/bin/env bash
# mbelib-neo'yu yeni RESMI YAYINA tasir (DMR Cepte ses cozucu eklentisi).
#
#   bash tools/mbelib_guncelle.sh                  denetle: yeni yayin var mi (cikis 0 ayni, 10 yeni var)
#   bash tools/mbelib_guncelle.sh indir [SURUM]    indir + dogrula + farki inceleme icin ayir; DURUR
#   bash tools/mbelib_guncelle.sh uygula SURUM     third_party'yi degistir, README'yi guncelle, derle
#
# NEDEN ASAMALI: CMake yapilandirmasi kaynagin CMake kodunu CALISTIRIR. "indir" yalniz indirir,
# dogrular ve CMakeLists.txt/cmake/legal/LICENSES/include farkini dosyaya yazar; bu fark
# okunmadan "uygula" calistirilmamali. Her asama beklenmeyen her durumda DURUR.
#
# Sonra (elle): telsizde tools/deneme ile eski/yeni karsilastirmasi (dstar, p25, dmr; cikis_rms ve
# cozme suresi), commit, ve uygulamada arac/eklenti_pin.sh (DMRCepte-kaynak) ile sabitleme.
set -euo pipefail
KOK="$(cd "$(dirname "$0")/.." && pwd)"
DEPO="arancormonk/mbelib-neo"
UA="Mozilla/5.0 (compatible; build-check)"     # genel istemci adi: proje/kisi bilgisi gonderilmez
CALISMA="${TMPDIR:-/tmp}/mbelib-guncelle"

simdiki() { sed -n 's/^project(mbelib-neo VERSION \([0-9.]*\) .*/\1/p' "$KOK/third_party/mbelib-neo/CMakeLists.txt"; }
son_yayin() {
  curl -fsSL -m 20 -A "$UA" "https://api.github.com/repos/$DEPO/releases/latest" |
    sed -n 's/.*"tag_name": *"v\{0,1\}\([0-9][0-9.]*\)".*/\1/p' | head -1
}
dur() { echo "DURDU: $*" >&2; exit 1; }

KOMUT="${1:-denetle}"
case "$KOMUT" in
denetle)
  S="$(simdiki)"; Y="$(son_yayin)"
  [ -n "$S" ] || dur "simdiki surum okunamadi"
  [ -n "$Y" ] || dur "son yayin okunamadi (ag?)"
  if [ "$S" = "$Y" ]; then echo "guncel: mbelib-neo $S"; exit 0; fi
  echo "YENI YAYIN: mbelib-neo $Y (eklentide $S) -> bash tools/mbelib_guncelle.sh indir $Y"
  exit 10 ;;

indir)
  Y="${2:-$(son_yayin)}"
  [[ "$Y" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || dur "gecersiz surum: '$Y'"
  D="$CALISMA/$Y"
  rm -rf "$D"; mkdir -p "$D/ac"              # indirilen veri ayri, bos klasorde
  AR="mbelib-neo-$Y-source.tar.gz"
  U="https://github.com/$DEPO/releases/download/v$Y"
  curl -fsSL -m 300 -A "$UA" -o "$D/$AR" "$U/$AR" || dur "arsiv indirilemedi"
  curl -fsSL -m 60 -A "$UA" -o "$D/SHA256SUMS.txt" "$U/SHA256SUMS.txt" || dur "SHA256SUMS.txt indirilemedi"
  BEK="$(awk -v f="$AR" '{ g=$2; sub(/^\*/, "", g); if (g == f) print $1 }' "$D/SHA256SUMS.txt")"
  [[ "$BEK" =~ ^[0-9a-f]{64}$ ]] || dur "SHA256SUMS.txt'de $AR yok"
  GER="$(sha256sum "$D/$AR" | cut -d' ' -f1)"
  [ "$BEK" = "$GER" ] || dur "arsiv ozeti uyusmuyor: beklenen $BEK, gelen $GER"
  echo "arsiv dogrulandi: $AR $GER"
  tar -xzf "$D/$AR" -C "$D/ac"
  K="$D/ac/mbelib-neo-$Y-source"
  [ -d "$K" ] || dur "arsivde mbelib-neo-$Y-source klasoru yok"
  [ "$(sed -n 's/^project(mbelib-neo VERSION \([0-9.]*\) .*/\1/p' "$K/CMakeLists.txt")" = "$Y" ] ||
    dur "arsivin CMakeLists.txt surumu $Y degil"
  # PFFFT: mbelib-neo arsivin icinde sabitliyor; derle.sh baska yerden almiyor.
  PS="$(sed -n 's/^PFFFT_SHA=//p' "$K/tools/ci-dependency-pins.env")"
  PA="$(sed -n 's/^PFFFT_ARCHIVE_SHA256=//p' "$K/tools/ci-dependency-pins.env")"
  [ -f "$K/third_party/pffft-$PS.tar.gz" ] || dur "PFFFT arsivi yok: third_party/pffft-$PS.tar.gz"
  [ "$(sha256sum "$K/third_party/pffft-$PS.tar.gz" | cut -d' ' -f1)" = "$PA" ] || dur "PFFFT arsiv ozeti sabitle uyusmuyor"
  echo "PFFFT $PS dogrulandi"
  E="$KOK/third_party/mbelib-neo"
  {
    for a in CMakeLists.txt cmake legal LICENSES include; do diff -ruN "$E/$a" "$K/$a" || true; done
  } > "$D/inceleme.diff"
  echo "--- degisen satir sayisi ($(simdiki) -> $Y) ---"
  for a in src include CMakeLists.txt cmake legal LICENSES; do
    printf "  %-15s %s\n" "$a" "$(diff -ruN "$E/$a" "$K/$a" | grep -c '^[-+][^-+]' || true)"
  done
  echo "INCELE: $D/inceleme.diff  (CMake kodu yapilandirmada calisir; API, lisans ve patent notlari)"
  echo "sonra: bash tools/mbelib_guncelle.sh uygula $Y" ;;

uygula)
  Y="${2:-}"
  [[ "$Y" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || dur "surum verin: uygula X.Y.Z"
  K="$CALISMA/$Y/ac/mbelib-neo-$Y-source"
  [ -d "$K" ] || dur "once: bash tools/mbelib_guncelle.sh indir $Y"
  [ -z "$(git -C "$KOK" status --porcelain)" ] || dur "eklenti deposunda commit'lenmemis degisiklik var"
  S="$(simdiki)"
  [ "$S" != "$Y" ] || dur "eklenti zaten mbelib-neo $Y"
  ESKI_OZET="$(sed -n 's/.*SHA-256 `\([0-9a-f]\{64\}\)`.*/\1/p' "$KOK/README.md" | head -1)"
  YENI_OZET="$(sha256sum "$CALISMA/$Y/mbelib-neo-$Y-source.tar.gz" | cut -d' ' -f1)"
  [ -n "$ESKI_OZET" ] || dur "README'de arsiv ozeti bulunamadi"
  rm -rf "$KOK/third_party/mbelib-neo"
  cp -a "$K" "$KOK/third_party/mbelib-neo"
  diff -rq "$K" "$KOK/third_party/mbelib-neo" >/dev/null || dur "kopya arsivle birebir degil"
  # Kok LICENSES/ upstream'deki ayni adli dosyalarin kopyasi.
  for f in "$KOK"/LICENSES/*; do
    n="$(basename "$f")"
    [ -f "$K/LICENSES/$n" ] && cp "$K/LICENSES/$n" "$f"
  done
  # README: surum ve arsiv ozeti (yalniz tam surum dizgisi; 2.6.1 / 2.6.11 karismasin).
  SE="${S//./\\.}"
  sed -i -E "s/(^|[^0-9.])$SE([^0-9]|$)/\1$Y\2/g; s/(^|[^0-9.])$SE([^0-9]|$)/\1$Y\2/g; s/$ESKI_OZET/$YENI_OZET/g" "$KOK/README.md"
  if grep -n -E "(^|[^0-9.])$SE([^0-9]|$)" "$KOK/README.md"; then dur "README'de eski surum ($S) kaldi"; fi
  grep -q "$YENI_OZET" "$KOK/README.md" || dur "README'ye yeni ozet yazilamadi"
  # Derleme: Windows'ta SDK yollari varsayilan.
  if [ -z "${ANDROID_NDK:-}" ] && [ -n "${LOCALAPPDATA:-}" ]; then
    export ANDROID_NDK="$LOCALAPPDATA/Android/Sdk/ndk/28.2.13676358"
    export CMAKE="${CMAKE:-$LOCALAPPDATA/Android/Sdk/cmake/3.22.1/bin/cmake.exe}"
    export NINJA="${NINJA:-$LOCALAPPDATA/Android/Sdk/cmake/3.22.1/bin/ninja.exe}"
  fi
  bash "$KOK/derle.sh"
  echo
  echo "TAMAM: mbelib-neo $S -> $Y derlendi. Simdi:"
  echo "  1) telsizde tools/deneme ile eski/yeni (dstar, p25, dmr): cikis_rms ve cozme suresi"
  echo "  2) git add -A && git commit; once ayri dala gonder, 64 bit cihazda da denenince main'e al"
  echo "  3) DMRCepte-kaynak: bash arac/eklenti_pin.sh  (commit + ozetleri VocoderKur.kt'ye yazar)" ;;

*) dur "bilinmeyen komut: $KOMUT (denetle | indir [SURUM] | uygula SURUM)" ;;
esac
