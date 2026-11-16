# DMR Cepte ses çözücü eklentisi

> **Güncel kaynak `mbelib-neo-2.6.11` dalında:** [mbelib-neo 2.6.11 eklentisi ve uygulamadaki yardımcı programların
> (`libmd380yardimci`, `libvokoderyardimci`) kaynakları](../../tree/mbelib-neo-2.6.11). DMR Cepte o daldaki commit'leri
> kullanır (5.16.19: `d85d650`; 5.16.20 ve sonrası: `a72e65b`, x86_64 PC öykünücüleri için yükleyici dahil). Bu daldaki (`main`) dosyalar mbelib-neo 2.6.5'tir ve eski uygulama
> sürümleri içindir; yeni sürüm 64 bit cihazda da denenince buraya alınacak.

DMR Cepte uygulamasının, cihazda ayrıca indirdiği ses çözücü (vokoder) eklentisi. Uygulamanın APK'sının
içinde gelmez; kullanıcı onay verince bu depodan indirilir.

**İçinde:** [mbelib-neo](https://github.com/arancormonk/mbelib-neo) 2.6.5 (resmî yayının kaynak arşivi
`mbelib-neo-2.6.5-source.tar.gz`, SHA-256 `3df5ee1c7126a150357deca9d652efaec36237e312fc097cc00ef0327b758b87`) ve onu
DroidStar eklenti arayüzüne bağlayan `src/vocoder_plugin.c`.

| Fonksiyon | Kullanım |
|---|---|
| `create` / `destroy` | eklenti nesnesi |
| `VocoderPlugin::decode_2450x1150` / `encode_2450x1150` | DMR / NXDN / YSF (AMBE+2, havadaki 72 bitlik çerçeve, 9 bayt) |
| `VocoderPlugin::decode_2400x1200` / `encode_2400x1200` | D-STAR (9 baytlık DV verisi) |
| `dmrcepte_imbe_decode_4400` / `dmrcepte_imbe_encode_4400` | P25 (IMBE, 88 bit, 11 bayt) |

PCM: 160 örnek, 8 kHz, 16 bit.

**Dosyalar:** `vocoder_plugin.android.arm64` (64 bit ARM), `vocoder_plugin.android.arm` (32 bit ARM),
`vocoder_plugin.android.x86_64`; özetler `SHA256SUMS` içinde.

**Derleme:** `ANDROID_NDK=… CMAKE=… NINJA=… bash derle.sh` — mbelib-neo ve PFFFT kaynakları
`third_party/` altında, ağ gerekmez.

**Lisans:** GPL-2.0-or-later (`LICENSE`). mbelib-neo'nun ISC ve PFFFT bildirimleri `LICENSES/` ve
`third_party/` altında korunur. DMR karıştırma tabloları DSD'den (ISC).

**Patent:** AMBE/AMBE+2 kodekleri DVSI patentleri kapsamında olabilir; mbelib-neo üçüncü taraf patent
temizliği almadığını belirtir (`third_party/mbelib-neo/legal/patents.md`). Kullanım ve dağıtımın
ülkenizdeki durumunu değerlendirmek kullanıcıya aittir.

---

# DMR Cepte vocoder plugin (English)

> **Current source is on the `mbelib-neo-2.6.11` branch:** [the mbelib-neo 2.6.11 plugin and the source of the app's
> helper programs](../../tree/mbelib-neo-2.6.11) (`libmd380yardimci`, `libvokoderyardimci`). DMR Cepte uses commits on
> that branch (5.16.19: `d85d650`; 5.16.20 and later: `a72e65b`, including the loader for x86_64 PC emulators). Files on `main` are mbelib-neo 2.6.5 for older app versions.

The vocoder plugin that the DMR Cepte app downloads separately on the device (not bundled in the APK).
It wraps mbelib-neo 2.6.5 (official release source archive, SHA-256 above) behind the DroidStar plugin interface (see the table above)
and adds IMBE 4400 for P25. Licensed GPL-2.0-or-later; full corresponding source of mbelib-neo and PFFFT
is in `third_party/`. Codecs may be covered by patents; see `third_party/mbelib-neo/legal/patents.md`.
