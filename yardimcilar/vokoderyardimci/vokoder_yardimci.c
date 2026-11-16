// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * DMR Cepte vokoder eklentisi yukleyicisi (ayri surec).
 *
 * Eklentiyi (vocoder_plugin.android.<abi>, GPL-2.0-or-later, kaynagi eklenti
 * deposunda) dlopen ile acar ve stdin/stdout uzerinden basit bir istek-yanit
 * protokoluyle sunar. Uygulama bu programi AYRI SUREC olarak calistirir;
 * eklenti uygulamanin kendi surecine hic yuklenmez.
 *
 * Acilis : "EKLY" + surum(1) + durum(1) + uzunluk(2, kucuk uclu) + ileti.
 *          durum 0 ise ileti "caps=<n>;kimlik=<metin>".
 * Istek  : op(1) + girdi (op'a gore sabit uzunluk).
 * Yanit  : durum(1); 0 ise cikti (op'a gore sabit uzunluk), degilse
 *          uzunluk(2) + ileti ve program cikar.
 *   'D' 2450x1150 (yoksa 2400) cozme : 9 bayt   -> 320 bayt (160 ornek, kucuk uclu)
 *   'E' 2450x1150 (yoksa 2400) kodla : 320 bayt -> 9 bayt
 *   'd' 2400x1200 cozme              : 9 bayt   -> 320 bayt
 *   'e' 2400x1200 kodla              : 320 bayt -> 9 bayt
 *   'I' IMBE 4400 cozme              : 11 bayt  -> 320 bayt
 *   'i' IMBE 4400 kodla              : 320 bayt -> 11 bayt
 * stdin kapaninca program temiz cikar.
 *
 * Vokoder IC DURUM tasir: cozme ve kodlama icin AYRI nesneler kullanilir.
 */
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROTOKOL 1

static void *(*fn_create)(void);
static void  (*fn_destroy)(void *);
static void  (*fn_dec2450)(void *, short *, unsigned char *);
static void  (*fn_enc2450)(void *, short *, unsigned char *);
static void  (*fn_dec2400)(void *, short *, unsigned char *);
static void  (*fn_enc2400)(void *, short *, unsigned char *);
static void  (*fn_imbeDec)(void *, short *, const unsigned char *);
static void  (*fn_imbeEnc)(void *, short *, unsigned char *);
static const char *(*fn_kimlik)(void);
static const char *(*fn_surum)(void);

static int yaz_tam(const void *b, size_t n)
{
    const unsigned char *p = (const unsigned char *)b;
    while (n > 0) {
        ssize_t r = write(1, p, n);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        p += r; n -= (size_t)r;
    }
    return 0;
}

static int oku_tam(void *b, size_t n)
{
    unsigned char *p = (unsigned char *)b;
    while (n > 0) {
        ssize_t r = read(0, p, n);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) return -1;
        p += r; n -= (size_t)r;
    }
    return 0;
}

static void ileti_yaz(const char *m)
{
    size_t n = strlen(m);
    if (n > 1000) n = 1000;
    unsigned char u[2] = { (unsigned char)(n & 0xFF), (unsigned char)(n >> 8) };
    yaz_tam(u, 2);
    yaz_tam(m, n);
}

static void acilis(int durum, const char *m)
{
    unsigned char b[6] = { 'E', 'K', 'L', 'Y', PROTOKOL, (unsigned char)durum };
    yaz_tam(b, 6);
    ileti_yaz(m);
}

static void hata_cik(const char *m)
{
    unsigned char d = 1;
    yaz_tam(&d, 1);
    ileti_yaz(m);
    _exit(1);
}

static int pcm_oku(short *s)
{
    unsigned char b[320];
    if (oku_tam(b, sizeof b)) return -1;
    for (int i = 0; i < 160; i++) s[i] = (short)(b[2 * i] | (b[2 * i + 1] << 8));
    return 0;
}

static void pcm_yaz(const short *s)
{
    unsigned char b[320];
    for (int i = 0; i < 160; i++) {
        b[2 * i] = (unsigned char)(s[i] & 0xFF);
        b[2 * i + 1] = (unsigned char)((s[i] >> 8) & 0xFF);
    }
    yaz_tam(b, sizeof b);
}

int main(int argc, char **argv)
{
    char m[700];
    if (argc < 2) { acilis(2, "kullanim: libvokoderyardimci.so <eklenti>"); return 2; }
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        const char *e = dlerror();
        snprintf(m, sizeof m, "dlopen: %s", e ? e : "?");
        acilis(3, m);
        return 3;
    }
    fn_create  = (void *(*)(void)) dlsym(lib, "create");
    fn_destroy = (void (*)(void *)) dlsym(lib, "destroy");
    fn_dec2450 = (void (*)(void *, short *, unsigned char *)) dlsym(lib, "_ZN13VocoderPlugin16decode_2450x1150EPsPh");
    fn_enc2450 = (void (*)(void *, short *, unsigned char *)) dlsym(lib, "_ZN13VocoderPlugin16encode_2450x1150EPsPh");
    fn_dec2400 = (void (*)(void *, short *, unsigned char *)) dlsym(lib, "_ZN13VocoderPlugin16decode_2400x1200EPsPh");
    fn_enc2400 = (void (*)(void *, short *, unsigned char *)) dlsym(lib, "_ZN13VocoderPlugin16encode_2400x1200EPsPh");
    fn_imbeDec = (void (*)(void *, short *, const unsigned char *)) dlsym(lib, "dmrcepte_imbe_decode_4400");
    fn_imbeEnc = (void (*)(void *, short *, unsigned char *)) dlsym(lib, "dmrcepte_imbe_encode_4400");
    fn_kimlik  = (const char *(*)(void)) dlsym(lib, "eklenti_kimligi");
    fn_surum   = (const char *(*)(void)) dlsym(lib, "eklenti_mbelib_surumu");
    if (!fn_create || !fn_destroy) { acilis(4, "create/destroy yok"); return 4; }
    if (!fn_dec2450 && !fn_dec2400) { acilis(5, "cozme islevi yok"); return 5; }
    void *dec = fn_create();
    void *enc = fn_create();
    if (!dec || !enc) { acilis(6, "create bos dondu"); return 6; }

    int caps = 0;
    if (fn_dec2450) caps |= 1;
    if (fn_enc2450) caps |= 2;
    if (fn_dec2400) caps |= 4;
    if (fn_enc2400) caps |= 8;
    if (fn_imbeDec) caps |= 16;
    if (fn_imbeEnc) caps |= 32;
    /* Kimlik eski JNI kabugundaki bicimle ayni: "<kimlik> · <surum>" (U+00B7, UTF-8);
       kimlik islevi yoksa bos (DroidStar eklentisi). */
    const char *k = fn_kimlik ? fn_kimlik() : 0;
    const char *s = fn_surum ? fn_surum() : "";
    if (k) snprintf(m, sizeof m, "caps=%d;kimlik=%s \xC2\xB7 %s", caps, k, s ? s : "");
    else   snprintf(m, sizeof m, "caps=%d;kimlik=", caps);
    acilis(0, m);

    const unsigned char ok = 0;
    for (;;) {
        unsigned char op;
        unsigned char a[16];
        short pcm[160];
        if (oku_tam(&op, 1)) break;
        memset(a, 0, sizeof a);
        memset(pcm, 0, sizeof pcm);
        switch (op) {
        case 'D':
            if (oku_tam(a, 9)) goto son;
            if (fn_dec2450) fn_dec2450(dec, pcm, a); else fn_dec2400(dec, pcm, a);
            yaz_tam(&ok, 1); pcm_yaz(pcm);
            break;
        case 'E':
            if (pcm_oku(pcm)) goto son;
            if (fn_enc2450) fn_enc2450(enc, pcm, a);
            else if (fn_enc2400) fn_enc2400(enc, pcm, a);
            else hata_cik("kodlama islevi yok");
            yaz_tam(&ok, 1); yaz_tam(a, 9);
            break;
        case 'd':
            if (oku_tam(a, 9)) goto son;
            if (!fn_dec2400) hata_cik("2400x1200 cozme yok");
            fn_dec2400(dec, pcm, a);
            yaz_tam(&ok, 1); pcm_yaz(pcm);
            break;
        case 'e':
            if (pcm_oku(pcm)) goto son;
            if (!fn_enc2400) hata_cik("2400x1200 kodlama yok");
            fn_enc2400(enc, pcm, a);
            yaz_tam(&ok, 1); yaz_tam(a, 9);
            break;
        case 'I':
            if (oku_tam(a, 11)) goto son;
            if (!fn_imbeDec) hata_cik("IMBE cozme yok");
            fn_imbeDec(dec, pcm, a);
            yaz_tam(&ok, 1); pcm_yaz(pcm);
            break;
        case 'i':
            if (pcm_oku(pcm)) goto son;
            if (!fn_imbeEnc) hata_cik("IMBE kodlama yok");
            fn_imbeEnc(enc, pcm, a);
            yaz_tam(&ok, 1); yaz_tam(a, 11);
            break;
        default:
            hata_cik("bilinmeyen istek");
        }
    }
son:
    fn_destroy(dec);
    fn_destroy(enc);
    dlclose(lib);
    return 0;
}
