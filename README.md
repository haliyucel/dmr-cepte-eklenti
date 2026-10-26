# DMR Cepte ses çözücü eklentisi

DMR Cepte uygulamasının, cihazda ayrıca indirdiği ses çözücü (vokoder) eklentisi. Uygulamanın APK'sının
içinde gelmez; kullanıcı onay verince bu depodan indirilir.

**İçinde:** [mbelib-neo](https://github.com/arancormonk/mbelib-neo) 2.6.2 (commit `c37d2c3`) ve onu
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

The vocoder plugin that the DMR Cepte app downloads separately on the device (not bundled in the APK).
It wraps mbelib-neo 2.6.2 (commit `c37d2c3`) behind the DroidStar plugin interface (see the table above)
and adds IMBE 4400 for P25. Licensed GPL-2.0-or-later; full corresponding source of mbelib-neo and PFFFT
is in `third_party/`. Codecs may be covered by patents; see `third_party/mbelib-neo/legal/patents.md`.
