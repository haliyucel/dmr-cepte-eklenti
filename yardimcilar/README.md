# DMR Cepte yardımcı programları (kaynak kodu)

DMR Cepte uygulamasının APK'sında gelen iki küçük yardımcı programın tam kaynağı ve derleme betikleri.
Uygulama bunları **ayrı süreç** olarak başlatır ve stdin/stdout borusuyla konuşur; uygulamanın kendi
sürecine yüklenmezler. Lisans: **GPL-2.0-or-later** (deponun `LICENSE` dosyası).

| APK'daki dosya | Kaynak | Ne yapar |
|---|---|---|
| `lib/*/libmd380yardimci.so` | `md380yardimci/md380_yardimci.c` + `md380/md380_tables.inc` | Kullanıcının onayıyla cihaza indirilen MD-380 firmware'indeki ses kodlayıcıyı doğrudan işlemcide çalıştırır (statik 32 bit ARM yürütülebilir; firmware APK'da yoktur). FEC tabloları ve işlevleri nostar/md380_vocoder'dan (Copyright (C) 2020-2021 Doug McLain AD8DP; md380tools'a dayanır), GPL v2 veya sonrası. |
| `lib/*/libvokoderyardimci.so` | `vokoderyardimci/vokoder_yardimci.c` | Kullanıcının indirdiği ses çözücü eklentisini (bu deponun kökündeki `vocoder_plugin.android.*`) kendi sürecinde `dlopen` ile açar ve istek/yanıt protokolüyle sunar. Protokol dosyanın başında. |

Dosya adları `lib*.so` olsa da ikisi de yürütülebilir programdır: Android yalnız bu adla paketlenen
dosyaları uygulamanın çalıştırabildiği klasöre çıkarır.

**Derleme** (Android NDK 28.2.13676358; Windows, Linux ya da macOS):

    sh md380yardimci/derle_md380yardimci.sh
    sh vokoderyardimci/derle_vokoderyardimci.sh

Çıktılar `cikti/<abi>/` altına yazılır. Bu kaynaklar, uygulamanın 5.16.19 ve sonraki sürümlerindeki
ikilileri bit bit aynı üretir:

    9b98dc49e13164ab201faee744cb6222c489838c4b6ee033f2b2e5e582ca06a4  arm64-v8a/libmd380yardimci.so
    9b98dc49e13164ab201faee744cb6222c489838c4b6ee033f2b2e5e582ca06a4  armeabi-v7a/libmd380yardimci.so
    e8a263f35b93b64ad4106381b8148e6a8371c3c1657b6eaffdb3df1bdf6a9273  arm64-v8a/libvokoderyardimci.so
    93455a21d81960114083bb86c8d36e78c95743ca8106c5c1a2cc4a8a9a99c414  armeabi-v7a/libvokoderyardimci.so

APK'daki dosyayı karşılaştırmak için APK bir zip arşivi olarak açılabilir (`lib/<abi>/`).

---

# DMR Cepte helper programs (source code)

Complete source and build scripts of the two small helper programs shipped in the DMR Cepte APK. The app
starts them as **separate processes** and talks to them over stdin/stdout; they are never loaded into the
app's own process. License: **GPL-2.0-or-later** (`LICENSE` at the repository root).

- `libmd380yardimci.so`: runs the vocoder inside the MD-380 firmware that the user downloads with consent
  (static 32-bit ARM executable; the firmware is not in the APK). FEC tables and functions from
  nostar/md380_vocoder (Copyright (C) 2020-2021 Doug McLain AD8DP; based on md380tools), GPL v2 or later.
- `libvokoderyardimci.so`: opens the user-downloaded vocoder plugin (`vocoder_plugin.android.*` at the
  repository root) with `dlopen` in its own process and serves it over a request/response protocol.

Build with Android NDK 28.2.13676358 using the two scripts above; outputs go to `cikti/<abi>/` and match the
binaries in app version 5.16.19 and later bit for bit (SHA-256 above).
