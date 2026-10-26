// Eklenti denemesi: dmrvocoder.c gibi dlopen ile acar, PCM'i kodlayip yeniden cozer.
// Kullanim: deneme <eklenti.so> <dmr|dstar|p25> <girdi.raw s16le 8k> <cikti.raw> <cerceveler.bin>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef void* (*f_create)(void);
typedef void (*f_destroy)(void*);
typedef void (*f_kod)(void*, short*, unsigned char*);

int main(int argc, char** argv) {
    if (argc < 6) { fprintf(stderr, "kullanim: deneme eklenti kip girdi cikti cerceve\n"); return 2; }
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
    printf("mbelib-neo: %s kip=%s\n", kim ? kim() : "?", kip);
    void* d = cr();
    void* e = cr();
    FILE* gi = fopen(argv[3], "rb");
    FILE* ci = fopen(argv[4], "wb");
    FILE* am = fopen(argv[5], "wb");
    if (!gi || !ci || !am) { fprintf(stderr, "dosya acilamadi\n"); return 1; }
    short in[160], out[160];
    unsigned char a[16];
    long n = 0, bos = 0;
    double ein = 0, eout = 0;
    while (fread(in, sizeof(short), 160, gi) == 160) {
        memset(a, 0, sizeof(a));
        enc(e, in, a);
        fwrite(a, 1, (size_t)bayt, am);
        dec(d, out, a);
        fwrite(out, sizeof(short), 160, ci);
        int sifir = 1;
        for (int i = 0; i < 160; ++i) {
            ein += (double)in[i] * in[i];
            eout += (double)out[i] * out[i];
            if (out[i]) sifir = 0;
        }
        bos += sifir;
        ++n;
    }
    printf("cerceve=%ld tamamen_sessiz=%ld giris_rms=%.0f cikis_rms=%.0f\n", n, bos,
           n ? sqrt(ein / (n * 160.0)) : 0, n ? sqrt(eout / (n * 160.0)) : 0);
    de(d); de(e);
    fclose(gi); fclose(ci); fclose(am);
    return 0;
}
