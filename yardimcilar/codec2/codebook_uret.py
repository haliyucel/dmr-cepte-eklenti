# -*- coding: utf-8 -*-
"""
Codec 2'nin generate_codebook.c araci, Python ile.

Codec 2 derlenirken kod tablolari (codebook*.c) src/codebook/*.txt dosyalarindan
URETILIR. Ust akis bunu kendi derledigi bir C programiyla yapiyor; bu bilgisayarda
Android disi bir C derleyicisi yok, bu yuzden ayni cikti burada uretilir.

Girdi bicimi (generate_codebook.c ile ayni): iki tamsayi (k, m), ardindan k*m sayi.
'#' satir sonuna kadar yorumdur. Sayilar strtod gibi okunur.
Cikti: "%g" bicimli diziler + const struct lsp_codebook <ad>[] (ust akisla ayni metin).

Kullanim: python codebook_uret.py <dizi_adi> <cikti.c> <girdi1.txt> [girdi2.txt ...]
"""
import math
import re
import struct
import sys

SAYI = re.compile(r'[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?')


def sayilar(yol):
    """Dosyadaki sayilar, yorumlar atlanarak (generate_codebook.c get_float ile ayni)."""
    with open(yol, 'r', encoding='ascii', errors='strict') as f:
        for satir in f:
            satir = satir.split('#', 1)[0]
            for m in SAYI.finditer(satir):
                yield float(m.group(0))


def yukle(yol):
    it = sayilar(yol)
    k = int(next(it))
    m = int(next(it))
    cb = [next(it) for _ in range(k * m)]
    return k, m, cb


def g(x):
    # C araci sayiyi float'a (32 bit) okuyup printf("%g") ile yaziyor: once 32 bite
    # yuvarlanir, sonra 6 anlamli hane. (64 bitle yazinca son hane yer yer farkli cikiyordu.)
    x = struct.unpack('<f', struct.pack('<f', x))[0]
    return '%g' % x


def main():
    if len(sys.argv) < 4:
        sys.stderr.write(__doc__)
        sys.exit(1)
    ad, cikti, girdiler = sys.argv[1], sys.argv[2], sys.argv[3:]
    kitaplar = [yukle(p) for p in girdiler]
    o = []
    o.append('/* THIS IS A GENERATED FILE. Edit generate_codebook.c and its input */\n\n')
    o.append('/*\n * This intermediary file and the files that used to create it are under \n')
    o.append(' * The LGPL. See the file COPYING.\n */\n\n')
    o.append('#include "defines.h"\n\n')
    for i, (yol, (k, m, cb)) in enumerate(zip(girdiler, kitaplar)):
        o.append('  /* %s */\n' % yol)
        o.append('#ifdef __EMBEDDED__\nstatic const float codes%d[] = {\n#else\nstatic float codes%d[] = {\n#endif\n' % (i, i))
        sinir = k * m
        for j in range(sinir):
            o.append('  ' + g(cb[j]))
            if j < sinir - 1:
                o.append(',')
            if (j + 1) % k == 0:
                o.append('\n')
        o.append('};\n')
    o.append('\nconst struct lsp_codebook %s[] = {\n' % ad)
    for i, (yol, (k, m, cb)) in enumerate(zip(girdiler, kitaplar)):
        o.append('  /* %s */\n' % yol)
        # C'deki roundf (yarimi sifirdan uzaga) ile ayni; Python round() yarimi cifte yuvarlar.
        log2m = int(math.floor(math.log(m) / math.log(2) + 0.5))
        o.append('  {\n    %d,\n    %d,\n    %d,\n    codes%d\n  }' % (k, log2m, m, i))
        o.append(',\n')
    o.append('  { 0, 0, 0, 0 }\n};\n')
    with open(cikti, 'w', encoding='ascii', newline='\n') as f:
        f.write(''.join(o))


if __name__ == '__main__':
    main()
