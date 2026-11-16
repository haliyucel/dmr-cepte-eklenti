// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * DMR Cepte — MD-380 vokoderi, AYRI 32 BIT YARDIMCI SUREC (32 ve 64 bit cihazlar).
 *
 * NEDEN
 * -----
 * Gercek DVSI AMBE+2, MD-380 firmware'inin icindeki hazir Cortex-M4
 * Thumb-2 makine kodudur; kaynagi yok, yeniden derlenemez. 64 bit bir surec
 * Thumb komutu calistiramaz. Bu yuzden YALNIZCA 64 bit calisan
 * telefonlarda firmware Unicorn oykunucusunde kosuyordu (eski app/jni/md380emu, git gecmisinde).
 * 64 bit bir telefonun gunlugunde oykunucu 3 cercevelik (60 ms'lik) bir patlamayi ortalama
 * 19-34 ms'de, en kotu 169 ms'de cozuyordu ve underrun'lar goruluyordu.
 * Ayrica yalnizca-GPLv2 Unicorn'un uygulamayla AYNI surece baglanmasi lisans
 * acisindan sorunluydu.
 *
 * Oysa 64 bit ARM cekirdegi (CONFIG_COMPAT) 32 bit bir SURECI calistirabilir;
 * yalnizca 64 bit Android'lerde eksik olan 32 bit kullanici kitapliklaridir
 * (/system/lib, 32 bit linker). 64 bit bir telefonda statik bagli 32 bit bir ikilinin ve
 * RWX Thumb kodunun calistigi olculdu. Bu program o yuzden hicbir paylasimli
 * kitapliga ihtiyac duymayan STATIK bir 32 bit yurutulebilirdir ve firmware'i
 * DOGRUDAN, native calistirir. Oykunucu yok. 2026-11-16'dan beri 32 bit
 * cihazlarda da (eskiden uygulama icinde JNI ile yapilan is) bu program calisir.
 *
 * Uygulama (Md380.kt) bunu ProcessBuilder ile baslatir ve stdin/stdout
 * uzerinden asagidaki kucuk ikili protokolle konusur. Ayri surec olmanin iki
 * kazanci daha var: firmware cokerse uygulama degil yalnizca bu surec olur,
 * ve yeniden baslatmak SRAM'i temiz dokumden yeniden yukler.
 *
 * AD NEDEN lib*.so: paket yoneticisi APK'dan yalnizca lib/<abi>/lib*.so
 * adli dosyalari nativeLibraryDir'e cikarir, ve Android 10+ uygulamanin
 * VERI dizininden exec'e izin vermez; nativeLibraryDir'den verir. Dosya
 * aslinda bir ELF yurutulebilirdir, kitaplik degil; hic dlopen edilmez.
 *
 * PROTOKOL (tum sayilar kucuk uclu)
 *   Cagri:  libmd380yardimci.so <fw.img> <ram.img>
 *   Acilis: "M38Y" + surum(1) + durum(1) + uzunluk(2) + ileti
 *           durum 0 = hazir (ileti: sinama ve hiz satiri). Degilse ileti
 *           hatayi anlatir ve surec cikar (cikis kodu = durum).
 *   'D' + 9 bayt AMBE    -> durum(1) [+ 320 bayt PCM, 160 ornek int16]
 *   'E' + 320 bayt PCM   -> durum(1) [+ 9 bayt AMBE]
 *   Durum 0 degilse (1 cokme, 2 sure asimi, 3 bilinmeyen istek) ardindan
 *   uzunluk(2) + ileti gelir ve surec CIKAR: firmware'in bellegi artik
 *   guvenilir degildir, temiz baslangic icin uygulama sureci yeniden baslatir.
 *   stdin kapanirsa (uygulama bitti ya da oldu) surec sessizce cikar.
 *
 * KAYNAK VE LISANS
 * FEC/tablolar ve md380_decode_fec/md380_encode_fec, md380_rt.c'deki
 * kopyalarin AYNISIDIR (nostar/md380_vocoder, GPLv2 veya sonrasi, Copyright
 * (C) 2020-2021 Doug McLain AD8DP; md380tools, Travis Goodspeed). Tablolar
 * ../md380/md380_tables.inc'ten degistirilmeden alinir; lisans metni orada.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* nostar/md380tools tablolari + WRITE_BIT/READ_BIT makrolari. */
#include "../md380/md380_tables.inc"

#define PROTOKOL_SURUMU 1

/* ── firmware bellek haritasi (md380_rt.c ile ayni) ─────────────────────── */
#define FW_ADDR    0x0800C000UL
#define FW_LEN     0xF2C00UL          /* cozulmus firmware tam bu boyda */
#define FW_MAP_LEN 0xF3000UL          /* sayfa sinirina yuvarlanmis */
#define SRAM_ADDR  0x20000000UL
#define SRAM_LEN   0x20000UL
#define TCRAM_ADDR 0x10000000UL
#define TCRAM_LEN  0x20000UL

/* ── firmware icindeki giris noktalari (symbols_d02.032) ────────────────── */
/* Adreslerin tek olmasi Thumb bitidir; BLX dogru kipe gecer.               */
#define A_DECODE_WAV    0x08051249UL
#define A_ENCODE_THING  0x08050d91UL
/* ── SRAM icindeki tamponlar ─────────────────────────────────────────────  */
#define A_AMBE_IN       0x20011c8eUL
#define A_AMBE_OUT0     0x20011aa8UL
#define A_AMBE_OUT1     0x20011b48UL
#define A_AMBE_MYSTERY  0x20011224UL
#define A_AMBE_EN_MYST  0x2000c730UL
#define A_WAV_IN0       0x2000de82UL
#define A_WAV_IN1       0x2000df22UL
#define A_AMBE_OUT      0x2000dfc6UL

/* ── sure sinirlari ──────────────────────────────────────────────────────
 * Acilis: ilk cagrilar firmware sayfalarini bellege ceker, ustune 50
 * cercevelik hiz olcumu var; 5 s bol. Tek cerceve native olarak milisaniye
 * mertebesinde (acilis satirindaki "cozme .. us" gercek degeri soyler);
 * 400 ms asildiysa firmware sonsuz donguye girmistir. */
#define ACILIS_SINIR_US 5000000L
#define CAGRI_SINIR_US  400000L

typedef int (*fn_decode_wav_t)(int16_t *wavbuf, int eighty, int16_t *bitbuf,
                               int a4, int16_t a5, int16_t a6, int a7);
typedef int (*fn_encode_thing_t)(int16_t *bitbuf, int a2, int16_t *wavbuf,
                                 int eighty, int a5, int16_t a6, int16_t a7, int a8);

static const fn_decode_wav_t   fw_decode_wav   = (fn_decode_wav_t) A_DECODE_WAV;
static const fn_encode_thing_t fw_encode_thing = (fn_encode_thing_t) A_ENCODE_THING;

static char g_hata[256] = "henuz baslatilmadi";

/* ── cokme ve donma yakalama ─────────────────────────────────────────────
 *
 * Ayri surecte bir cokme uygulamayi DUSURMEZ, ama nedenini soylemeden olen
 * bir surec teshisi zorlastirir. Bu yuzden firmware cagrilari md380_rt.c'deki
 * gibi sigsetjmp ile cerceveleniyor: SIGSEGV/SIGILL/SIGBUS yakalanir,
 * uygulamaya aciklama yazilir ve surec cikar. SIGALRM de ayni yoldan gelir:
 * her cagriya setitimer ile sure siniri konuyor, sonsuz donguye giren
 * firmware boylece kesilip "dondu" diye raporlaniyor (32 bit JNI surumu
 * donmayi yakalayamiyordu, bkz. Md380.kt "deneniyor" isareti).
 */
static sigjmp_buf g_jmp;
static volatile sig_atomic_t g_koruma = 0;

static void kaza(int sig) {
    if (g_koruma) siglongjmp(g_jmp, sig);
    /* Cagri bittikten hemen sonra gelen gec zil: yok say. */
    if (sig == SIGALRM) return;
    /* Korumasiz an: eski davranisa birak (surec olur, uygulama EOF gorur). */
    signal(sig, SIG_DFL);
    raise(sig);
}

static void korumaKur(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = kaza;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;
    sigaction(SIGSEGV, &sa, 0);
    sigaction(SIGILL,  &sa, 0);
    sigaction(SIGBUS,  &sa, 0);
    sigaction(SIGFPE,  &sa, 0);
    sigaction(SIGALRM, &sa, 0);
}

/* us = 0 zamanlayiciyi durdurur. */
static void sureKur(long us) {
    struct itimerval t;
    memset(&t, 0, sizeof(t));
    t.it_value.tv_sec  = us / 1000000L;
    t.it_value.tv_usec = us % 1000000L;
    setitimer(ITIMER_REAL, &t, 0);
}

typedef void (*is_t)(void *);

/* is(arg)'i koruma ve sure siniri altinda calistirir. 0 = tamam, degilse
 * isi kesen sinyalin numarasi. */
static int korumali(is_t is, void *arg, long sinirUs) {
    int sig = sigsetjmp(g_jmp, 1);
    if (sig != 0) {
        g_koruma = 0;
        sureKur(0);
        return sig;
    }
    g_koruma = 1;
    sureKur(sinirUs);
    is(arg);
    sureKur(0);
    g_koruma = 0;
    return 0;
}

/* ── bolge esleme ───────────────────────────────────────────────────────── */
/*
 * MAP_FIXED tehlikelidir: adres doluysa oradaki eslemeyi SESSIZCE ezer.
 * Once ipucu adresiyle MAP_FIXED'siz deniyoruz; cekirdek ayni adresi
 * verirse bolge bostu demektir. Statik ikili 0x10000 civarina yuklenir ve
 * brk/mmap bu adreslerden uzakta kalir, yani normalde hep bostur.
 */
static int bolgeEsle(unsigned long adres, size_t uzunluk, void **cikti) {
    void *p = mmap((void *) adres, uzunluk, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        snprintf(g_hata, sizeof(g_hata),
                 "mmap 0x%08lX (%lu bayt) basarisiz: %s",
                 adres, (unsigned long) uzunluk, strerror(errno));
        return -1;
    }
    if (p != (void *) adres) {
        munmap(p, uzunluk);
        snprintf(g_hata, sizeof(g_hata),
                 "0x%08lX adresi dolu (cekirdek %p verdi)", adres, p);
        return -2;
    }
    *cikti = p;
    return 0;
}

static int dosyaOku(const char *yol, void *hedef, size_t beklenen) {
    int fd = open(yol, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        snprintf(g_hata, sizeof(g_hata), "acilamadi: %s (%s)", yol, strerror(errno));
        return -1;
    }
    size_t n = 0;
    while (n < beklenen) {
        ssize_t r = read(fd, (uint8_t *) hedef + n, beklenen - n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        n += (size_t) r;
    }
    close(fd);
    if (n != beklenen) {
        snprintf(g_hata, sizeof(g_hata),
                 "%s boyu yanlis: %lu okundu, %lu bekleniyordu",
                 yol, (unsigned long) n, (unsigned long) beklenen);
        return -2;
    }
    return 0;
}

/* ── nostar'in vokoder islevleri (md380_rt.c ile ayni) ──────────────────── */

static void md380_decode49(const uint8_t *ambe49, int16_t *pcm) {
    int16_t *ambe    = (int16_t *) A_AMBE_IN;
    int16_t *outbuf0 = (int16_t *) A_AMBE_OUT0;
    int16_t *outbuf1 = (int16_t *) A_AMBE_OUT1;
    int ambei = 0;
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 8; j++)
            ambe[ambei++] = (ambe49[i] >> (7 - j)) & 1;
    ambe[ambei++] = ambe49[6] ? 1 : 0;

    fw_decode_wav(outbuf0, 80, ambe, 0, 0, 0, (int) A_AMBE_MYSTERY);
    fw_decode_wav(outbuf1, 80, ambe, 0, 0, 1, (int) A_AMBE_MYSTERY);
    memmove(pcm,       outbuf0, 160);
    memmove(&pcm[80],  outbuf1, 160);
}

static void md380_encode49(uint8_t *ambe49, const int16_t *pcm) {
    int16_t *inbuf0 = (int16_t *) A_WAV_IN0;
    int16_t *inbuf1 = (int16_t *) A_WAV_IN1;
    int16_t *ambe   = (int16_t *) A_AMBE_OUT;
    memset(ambe, 0, 50);
    for (int i = 0; i < 80; i++) inbuf0[i] = pcm[i];
    for (int i = 0; i < 80; i++) inbuf1[i] = pcm[i + 80];
    fw_encode_thing(ambe, 0, inbuf0, 0x50, 0x1840, 0,   0x2000, (int) A_AMBE_EN_MYST);
    fw_encode_thing(ambe, 0, inbuf1, 0x50, 0x1840, 0x1, 0x2000, (int) A_AMBE_EN_MYST);
    memset(ambe49, 0, 7);
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 8; ++j)
            ambe49[i] |= (ambe[(i * 8) + (7 - j)] << j);
    ambe49[6] = ambe[48] ? 0x80 : 0;
}

/* 72 bitlik (9 bayt) DMR cercevesi -> 49 bit -> 160 ornek PCM. */
static void md380_decode_fec(const uint8_t *ambe, int16_t *pcm) {
    unsigned int a = 0U, b = 0U, c = 0U;
    unsigned int MASK = 0x800000U;
    uint8_t ambe49[9] = {0};
    for (unsigned int i = 0U; i < 24U; i++, MASK >>= 1)
        if (READ_BIT(ambe, A_TABLE[i])) a |= MASK;
    MASK = 0x400000U;
    for (unsigned int i = 0U; i < 23U; i++, MASK >>= 1)
        if (READ_BIT(ambe, B_TABLE[i])) b |= MASK;
    MASK = 0x1000000U;
    for (unsigned int i = 0U; i < 25U; i++, MASK >>= 1)
        if (READ_BIT(ambe, C_TABLE[i])) c |= MASK;
    a >>= 12;
    b ^= (PRNG_TABLE[a] >> 1);
    b >>= 11;
    MASK = 0x000800U;
    for (unsigned int i = 0U; i < 12U; i++, MASK >>= 1) {
        WRITE_BIT(ambe49, (i + 0U),  a & MASK);
        WRITE_BIT(ambe49, (i + 12U), b & MASK);
    }
    MASK = 0x1000000U;
    for (unsigned int i = 0U; i < 25U; i++, MASK >>= 1)
        WRITE_BIT(ambe49, (i + 24U), c & MASK);
    md380_decode49(ambe49, pcm);
}

static void md380_encode_fec(uint8_t *ambe, const int16_t *pcm) {
    unsigned int aOrig = 0U, bOrig = 0U, cOrig = 0U;
    unsigned int MASK = 0x000800U;
    uint8_t ambe49[9] = {0};
    md380_encode49(ambe49, pcm);
    for (unsigned int i = 0U; i < 12U; i++, MASK >>= 1) {
        if (READ_BIT(ambe49, (i + 0U)))  aOrig |= MASK;
        if (READ_BIT(ambe49, (i + 12U))) bOrig |= MASK;
    }
    MASK = 0x1000000U;
    for (unsigned int i = 0U; i < 25U; i++, MASK >>= 1)
        if (READ_BIT(ambe49, (i + 24U))) cOrig |= MASK;
    unsigned int a = ENCODING_TABLE_24128[aOrig];
    unsigned int p = PRNG_TABLE[aOrig] >> 1;
    unsigned int b = ENCODING_TABLE_23127[bOrig] >> 1;
    b ^= p;
    memset(ambe, 0, 9);
    MASK = 0x800000U;
    for (unsigned int i = 0U; i < 24U; i++, MASK >>= 1)
        WRITE_BIT(ambe, A_TABLE[i], a & MASK);
    MASK = 0x400000U;
    for (unsigned int i = 0U; i < 23U; i++, MASK >>= 1)
        WRITE_BIT(ambe, B_TABLE[i], b & MASK);
    MASK = 0x1000000U;
    for (unsigned int i = 0U; i < 25U; i++, MASK >>= 1)
        WRITE_BIT(ambe, C_TABLE[i], cOrig & MASK);
}

/* ── korumali isler ─────────────────────────────────────────────────────── */

struct coz_is { const uint8_t *ambe; int16_t *pcm; };
static void cozIs(void *a) {
    struct coz_is *c = (struct coz_is *) a;
    md380_decode_fec(c->ambe, c->pcm);
}

struct kod_is { const int16_t *pcm; uint8_t *ambe; };
static void kodIs(void *a) {
    struct kod_is *k = (struct kod_is *) a;
    md380_encode_fec(k->ambe, k->pcm);
}

static int64_t simdiUs(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t) t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

/*
 * KENDINI SINAMA + HIZ OLCUMU. "Esledim" demek "calisiyor" demek DEGIL:
 * firmware yanlis SRAM durumuyla sessizce SIFIR uretebilir (md380_rt.c).
 * 1 kHz'lik bir ton kodlanip geri cozuluyor ve cikisin ses tasidigi
 * olculuyor. Ardindan 25'er cerceveyle hiz: bir cerceve 20 ms'lik ses.
 */
struct sinama { int tepe; int cozUs; int kodUs; };
static void sinamaIs(void *a) {
    struct sinama *s = (struct sinama *) a;
    int16_t ton[160], cozulen[160];
    uint8_t cerceve[9];
    for (int i = 0; i < 160; i++)
        ton[i] = (int16_t) (8000.0 * sin(2.0 * 3.14159265358979 * 1000.0 * i / 8000.0));
    md380_encode_fec(cerceve, ton);
    md380_decode_fec(cerceve, cozulen);
    int tepe = 0;
    for (int i = 0; i < 160; i++) {
        int v = cozulen[i] < 0 ? -cozulen[i] : cozulen[i];
        if (v > tepe) tepe = v;
    }
    s->tepe = tepe;
    int64_t t0 = simdiUs();
    for (int i = 0; i < 25; i++) md380_decode_fec(cerceve, cozulen);
    s->cozUs = (int) ((simdiUs() - t0) / 25);
    t0 = simdiUs();
    for (int i = 0; i < 25; i++) md380_encode_fec(cerceve, ton);
    s->kodUs = (int) ((simdiUs() - t0) / 25);
}

/* Basarili ise 0 ve ileti = hazir satiri; degilse md380_rt.c ile ayni
 * negatif kod ve g_hata. Hata durumunda esleme geri alinmaz: surec cikiyor. */
static int ac(const char *fwYol, const char *ramYol, char *ileti, size_t boy) {
    void *fw = 0, *sram = 0, *tcram = 0;
    if (bolgeEsle(FW_ADDR, FW_MAP_LEN, &fw))      return -1;
    if (bolgeEsle(SRAM_ADDR, SRAM_LEN, &sram))    return -2;
    if (bolgeEsle(TCRAM_ADDR, TCRAM_LEN, &tcram)) return -3;
    memset(tcram, 0, TCRAM_LEN);
    if (dosyaOku(fwYol, fw, FW_LEN))              return -4;
    if (dosyaOku(ramYol, sram, SRAM_LEN))         return -5;
    /* Firmware calistirilabilir olmali. Reddedilirse SELinux execmem. */
    if (mprotect(fw, FW_MAP_LEN, PROT_READ | PROT_EXEC)) {
        snprintf(g_hata, sizeof(g_hata),
                 "firmware R+X yapilamadi (SELinux execmem?): %s", strerror(errno));
        return -6;
    }
    /* Firmware VERI olarak yazildi, simdi KOD olarak okunacak. ARM'da komut
     * ve veri onbellekleri kendiliginden tutarli degil; cekirdek bunu
     * mprotect'te genelde halleder ama ona guvenmek yerine acikca. */
    __builtin___clear_cache((char *) fw, (char *) fw + FW_MAP_LEN);

    korumaKur();
    struct sinama s = { 0, 0, 0 };
    int sig = korumali(sinamaIs, &s, ACILIS_SINIR_US);
    if (sig == SIGALRM) {
        snprintf(g_hata, sizeof(g_hata), "firmware sinamasi %ld ms icinde bitmedi (dondu)",
                 ACILIS_SINIR_US / 1000);
        return -7;
    }
    if (sig != 0) {
        snprintf(g_hata, sizeof(g_hata), "firmware sinamada coktu (sinyal %d)", sig);
        return -7;
    }
    if (s.tepe < 64) {
        snprintf(g_hata, sizeof(g_hata),
                 "esleme oldu ama firmware sessiz cikti (tepe=%d)", s.tepe);
        return -8;
    }
    snprintf(ileti, boy,
             "md380 (yardimci) hazir, sinama tepesi=%d, cozme %d us, kodlama %d us / 20 ms cerceve",
             s.tepe, s.cozUs, s.kodUs);
    return 0;
}

/* ── boru G/C ─────────────────────────────────────────────────────────── */

static int hepsiniOku(void *p, size_t n) {
    uint8_t *b = (uint8_t *) p;
    while (n > 0) {
        ssize_t r = read(0, b, n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return -1;
        b += r; n -= (size_t) r;
    }
    return 0;
}

static int hepsiniYaz(const void *p, size_t n) {
    const uint8_t *b = (const uint8_t *) p;
    while (n > 0) {
        ssize_t r = write(1, b, n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return -1;
        b += r; n -= (size_t) r;
    }
    return 0;
}

/* bas + uzunluk(2) + ileti, TEK yazmada (uygulama parca parca okumasin). */
static void iletiliYaz(const uint8_t *bas, size_t basBoy, const char *ileti) {
    uint8_t b[8 + 2 + 512];
    size_t n = strlen(ileti);
    if (n > 512) n = 512;
    memcpy(b, bas, basBoy);
    b[basBoy]     = (uint8_t) (n & 0xFF);
    b[basBoy + 1] = (uint8_t) (n >> 8);
    memcpy(b + basBoy + 2, ileti, n);
    hepsiniYaz(b, basBoy + 2 + n);
}

static void acilisYaz(int durum, const char *ileti) {
    const uint8_t bas[6] = { 'M', '3', '8', 'Y', PROTOKOL_SURUMU, (uint8_t) durum };
    iletiliYaz(bas, sizeof(bas), ileti);
    /* Hata stderr'e de: boru okunamasa bile uygulama son satiri gorur. */
    if (durum != 0) dprintf(2, "md380yardimci: %s\n", ileti);
}

static int hataIleCik(int durum, const char *ileti) {
    const uint8_t bas[1] = { (uint8_t) durum };
    iletiliYaz(bas, sizeof(bas), ileti);
    dprintf(2, "md380yardimci: %s\n", ileti);
    return durum;
}

static int cagriHatasi(const char *ne, int sig) {
    char m[128];
    if (sig == SIGALRM) {
        snprintf(m, sizeof(m), "%s %ld ms icinde bitmedi (firmware dondu)",
                 ne, CAGRI_SINIR_US / 1000);
        return hataIleCik(2, m);
    }
    snprintf(m, sizeof(m), "%s sirasinda coktu (sinyal %d)", ne, sig);
    return hataIleCik(1, m);
}

int main(int argc, char **argv) {
    /* Uygulama gittiyse (boru kapali) write EPIPE donsun; SIGPIPE sureci
     * sessizce oldurmesin, dongu stdin'in kapandigini gorup duzgun ciksin. */
    signal(SIGPIPE, SIG_IGN);
    /* SES YOLUNDAYIZ: ag ipligi her cerceve icin bu sureci bekliyor.
     * THREAD_PRIORITY_AUDIO (-16) ile ayni; Android uygulama sureclerine
     * RLIMIT_NICE bunu izin veriyor. Olmazsa varsayilan oncelikte kalir. */
    setpriority(PRIO_PROCESS, 0, -16);

    if (argc < 3) {
        acilisYaz(9, "kullanim: libmd380yardimci.so <fw.img> <ram.img>");
        return 9;
    }
    char ileti[256];
    int rc = ac(argv[1], argv[2], ileti, sizeof(ileti));
    if (rc != 0) {
        char tam[300];
        snprintf(tam, sizeof(tam), "(%d) %s", rc, g_hata);
        acilisYaz(-rc, tam);
        return -rc;
    }
    acilisYaz(0, ileti);

    static uint8_t istek[320];
    static uint8_t yanit[1 + 320];
    for (;;) {
        uint8_t op;
        if (hepsiniOku(&op, 1)) return 0;          /* stdin kapandi: uygulama bitti */
        if (op == 'D') {
            int16_t pcm[160];
            if (hepsiniOku(istek, 9)) return 0;
            memset(pcm, 0, sizeof(pcm));
            struct coz_is c = { istek, pcm };
            int sig = korumali(cozIs, &c, CAGRI_SINIR_US);
            if (sig != 0) return cagriHatasi("cozme", sig);
            yanit[0] = 0;
            memcpy(yanit + 1, pcm, 320);           /* ARM kucuk uclu: int16 zaten LE */
            if (hepsiniYaz(yanit, 1 + 320)) return 0;
        } else if (op == 'E') {
            int16_t pcm[160];
            uint8_t ambe[9];
            if (hepsiniOku(istek, 320)) return 0;
            memcpy(pcm, istek, 320);
            memset(ambe, 0, sizeof(ambe));
            struct kod_is k = { pcm, ambe };
            int sig = korumali(kodIs, &k, CAGRI_SINIR_US);
            if (sig != 0) return cagriHatasi("kodlama", sig);
            yanit[0] = 0;
            memcpy(yanit + 1, ambe, 9);
            if (hepsiniYaz(yanit, 1 + 9)) return 0;
        } else {
            char m[64];
            snprintf(m, sizeof(m), "bilinmeyen istek 0x%02X", op);
            return hataIleCik(3, m);
        }
    }
}
