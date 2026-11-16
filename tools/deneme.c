// Eklenti denemesi: yukleyici gibi dlopen ile acar; PCM'i kodlayip yeniden cozer
// ya da hazir cerceveleri yalniz cozer. Cerceve basina sureleri de olcer.
//   deneme <eklenti.so> <dmr|dstar|p25> <girdi.raw s16le 8k> <cikti.raw> <cerceveler.bin>
//       kodla + coz (eski bicim); kodlanan cerceveler cerceveler.bin'e yazilir
//   deneme <eklenti.so> <dmr|dstar|p25> coz <cerceveler.bin> <cikti.raw>
//       yalniz coz: ayni cerceveleri iki eklenti surumuyle cozup karsilastirmak icin
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef void* (*f_create)(void);
typedef void (*f_destroy)(void*);
typedef void (*f_kod)(void*, short*, unsigned char*);

static double simdi_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

typedef struct { double top, enc; long n; } sure;
static void sure_ekle(sure* s, double us) { s->top += us; if (us > s->enc) s->enc = us; s->n++; }

int main(int argc, char** argv) {
    int yalniz_coz = argc >= 6 && strcmp(argv[3], "coz") == 0;
    if (argc < 6) {
        fprintf(stderr, "kullanim: deneme eklenti kip girdi cikti cerceve | deneme eklenti kip coz cerceve cikti\n");
        return 2;
    }
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    const char* kip = argv[2];
    const char* dn; const char* en; int bayt;
    if (strcmp(kip, "dmr") == 0) { dn = "_ZN13VocoderPlugin16decode_2450x1150EPsPh"; en = "_ZN13VocoderPlugin16encode_2450x1150EPsPh"; bayt = 9; }
    else if (strcmp(kip, "dstar") == 0) { dn = "_ZN13VocoderPlugin16decode_2400x1200EPsPh"; en = "_ZN13VocoderPlugin16encode_2400x1200EPsPh"; bayt = 9; }
    else if (strcmp(kip, "p25") == 0) { dn = "dmrcepte_imbe_decode_4400"; en = "dmrcepte_imbe_encode_4400"; bayt = 11; }
    else { fprintf(stderr, "bilinmeyen kip\n"); return 2; }
    f_create cr = (f_create)dlsym(lib, "create");
    f_destroy de = (f_destroy)dlsym(lib, "destroy");
    f_kod dec = (f_kod)dlsym(lib, dn);
    f_kod enc = (f_kod)dlsym(lib, en);
    const char* (*kim)(void) = (const char* (*)(void))dlsym(lib, "eklenti_mbelib_surumu");
    if (!cr || !de || !dec || !enc) { fprintf(stderr, "sembol eksik\n"); return 1; }
    printf("mbelib-neo: %s kip=%s %s\n", kim ? kim() : "?", kip, yalniz_coz ? "coz" : "kodla+coz");
    void* d = cr();
    void* e = cr();
    FILE* gi = fopen(yalniz_coz ? argv[4] : argv[3], "rb");
    FILE* ci = fopen(yalniz_coz ? argv[5] : argv[4], "wb");
    FILE* am = yalniz_coz ? NULL : fopen(argv[5], "wb");
    if (!gi || !ci || (!yalniz_coz && !am)) { fprintf(stderr, "dosya acilamadi\n"); return 1; }
    short in[160], out[160];
    unsigned char a[16];
    long n = 0, bos = 0;
    double ein = 0, eout = 0;
    sure sk = {0, 0, 0}, sc = {0, 0, 0};
    for (;;) {
        memset(a, 0, sizeof(a));
        if (yalniz_coz) {
            if (fread(a, 1, (size_t)bayt, gi) != (size_t)bayt) break;
        } else {
            if (fread(in, sizeof(short), 160, gi) != 160) break;
            double t0 = simdi_us();
            enc(e, in, a);
            sure_ekle(&sk, simdi_us() - t0);
            fwrite(a, 1, (size_t)bayt, am);
            for (int i = 0; i < 160; ++i) ein += (double)in[i] * in[i];
        }
        double t1 = simdi_us();
        dec(d, out, a);
        sure_ekle(&sc, simdi_us() - t1);
        fwrite(out, sizeof(short), 160, ci);
        int sifir = 1;
        for (int i = 0; i < 160; ++i) {
            eout += (double)out[i] * out[i];
            if (out[i]) sifir = 0;
        }
        bos += sifir;
        ++n;
    }
    printf("cerceve=%ld tamamen_sessiz=%ld giris_rms=%.0f cikis_rms=%.0f\n", n, bos,
           (n && !yalniz_coz) ? sqrt(ein / (n * 160.0)) : 0, n ? sqrt(eout / (n * 160.0)) : 0);
    if (sk.n) printf("kodlama: ort %.0f us, en uzun %.0f us\n", sk.top / sk.n, sk.enc);
    if (sc.n) printf("cozme:   ort %.0f us, en uzun %.0f us\n", sc.top / sc.n, sc.enc);
    de(d); de(e);
    fclose(gi); fclose(ci); if (am) fclose(am);
    return 0;
}
