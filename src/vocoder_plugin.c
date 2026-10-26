// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * DMR Cepte ses cozucu eklentisi (mbelib-neo ile).
 *
 * DroidStar'in eklenti arayuzunu birebir verir, uygulamanin kabugu (dmrvocoder.c) onu
 * dlopen ile acar:
 *   create() / destroy(p)
 *   VocoderPlugin::decode_2450x1150(short* pcm, unsigned char* ambe)  -> DMR / NXDN AMBE+2
 *   VocoderPlugin::encode_2450x1150(short* pcm, unsigned char* ambe)
 *   VocoderPlugin::decode_2400x1200(short* pcm, unsigned char* ambe)  -> D-STAR AMBE
 *   VocoderPlugin::encode_2400x1200(short* pcm, unsigned char* ambe)
 * Uye islevler C++ adlariyla (Itanium ABI) disa verilir; ilk arguman nesnedir (this).
 * Ek olarak (DroidStar'da yok): dmrcepte_imbe_decode_4400 / dmrcepte_imbe_encode_4400 (P25).
 *
 * 2450x1150 VERISI: havadaki 72 bitlik AMBE+2 cercevesi, 9 bayt, en yuksek bit once
 * (DMR ses patlamasindaki sirayla; karistirma ve hata duzeltme bitleri dahil).
 * 2400x1200 VERISI: D-STAR DV cercevesinin 9 ses bayti.
 * PCM: 160 ornek, 8 kHz, 16 bit.
 *
 * DMR karistirma tablolari DSD'den (include/dmr_const.h, ISC lisansi, Copyright (C) 2010
 * DSD Author), mbelib-neo tools/quality/mbe_quality_dvsi.c araciligiyla.
 * mbelib-neo: https://github.com/arancormonk/mbelib-neo (GPL-2.0-or-later; ISC ve PFFFT
 * bildirimleri korunur). Kodekler patentli olabilir; bkz. mbelib-neo legal/patents.md.
 */
#include <stdlib.h>
#include <string.h>

#include "mbelib-neo/mbelib.h"

#define DISA __attribute__((visibility("default")))

/* Karistirilmis bit 2i -> [satir_yuksek[i]][sutun_yuksek[i]], bit 2i+1 -> [satir_dusuk[i]][sutun_dusuk[i]]. */
static const unsigned char satir_yuksek[36] = {
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2,
};
static const unsigned char sutun_yuksek[36] = {
    23, 10, 22, 9, 21, 8,  20, 7, 19, 6, 18, 5, 17, 4, 16, 3, 15, 2,
    14, 1,  13, 0, 12, 10, 11, 9, 10, 8, 9,  7, 8,  6, 7,  5, 6,  4,
};
static const unsigned char satir_dusuk[36] = {
    0, 2, 0, 2, 0, 2, 0, 2, 0, 3, 0, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3, 1, 3,
};
static const unsigned char sutun_dusuk[36] = {
    5,  3, 4,  2, 3,  1, 2,  0, 1,  13, 0,  12, 22, 11, 21, 10, 20, 9,
    19, 8, 18, 7, 17, 6, 16, 5, 15, 4,  14, 3,  13, 2,  12, 1,  11, 0,
};

typedef struct {
    /* cozucu (DMR/NXDN ve D-STAR ayri: ikisi ayni nesnede karismasin) */
    mbe_parms c_cur, c_prev, c_prev_enh;
    mbe_parms d_cur, d_prev, d_prev_enh;
    /* kodlayicilar */
    mbe_ambe2450_encoder* k2450;
    mbe_parms k_cur, k_prev, k_tmp;
    mbe_ambe2400_encoder* k2400;
    mbe_parms kd_cur, kd_prev, kd_tmp;
    /* IMBE (P25 ag bicimi, 88 bit) */
    mbe_parms i_cur, i_prev, i_prev_enh;
    mbe_imbe4400_encoder* k4400;
    mbe_parms ki_cur, ki_prev, ki_tmp;
} Eklenti;

static int bit_al(const unsigned char* b, int i) { return (b[i >> 3] >> (7 - (i & 7))) & 1; }

static void bit_koy(unsigned char* b, int i, int v) {
    if (v) b[i >> 3] |= (unsigned char)(1u << (7 - (i & 7)));
}

/* 72 bit (9 bayt) -> 4x24 duzlem. */
static void dmr_ac(const unsigned char* ambe, char fr[4][24]) {
    memset(fr, 0, 4 * 24);
    for (int i = 0; i < 36; ++i) {
        fr[satir_yuksek[i]][sutun_yuksek[i]] = (char)bit_al(ambe, 2 * i);
        fr[satir_dusuk[i]][sutun_dusuk[i]] = (char)bit_al(ambe, 2 * i + 1);
    }
}

/* 4x24 duzlem -> 72 bit (9 bayt); dmr_ac'in tersi. */
static void dmr_kapa(const char fr[4][24], unsigned char* ambe) {
    memset(ambe, 0, 9);
    for (int i = 0; i < 36; ++i) {
        bit_koy(ambe, 2 * i, fr[satir_yuksek[i]][sutun_yuksek[i]] & 1);
        bit_koy(ambe, 2 * i + 1, fr[satir_dusuk[i]][sutun_dusuk[i]] & 1);
    }
}

DISA void* create(void) {
    Eklenti* e = (Eklenti*)calloc(1, sizeof(Eklenti));
    if (!e) return NULL;
    mbe_initMbeParms(&e->c_cur, &e->c_prev, &e->c_prev_enh);
    mbe_initMbeParms(&e->d_cur, &e->d_prev, &e->d_prev_enh);
    mbe_initMbeParms(&e->k_cur, &e->k_prev, &e->k_tmp);
    mbe_initMbeParms(&e->kd_cur, &e->kd_prev, &e->kd_tmp);
    mbe_initMbeParms(&e->i_cur, &e->i_prev, &e->i_prev_enh);
    mbe_initMbeParms(&e->ki_cur, &e->ki_prev, &e->ki_tmp);
    e->k2450 = mbe_ambe2450EncoderAlloc();
    e->k2400 = mbe_ambe2400EncoderAlloc();
    e->k4400 = mbe_imbe4400EncoderAlloc();
    if (!e->k2450 || !e->k2400 || !e->k4400) {
        if (e->k2450) mbe_ambe2450EncoderFree(e->k2450);
        if (e->k2400) mbe_ambe2400EncoderFree(e->k2400);
        if (e->k4400) mbe_imbe4400EncoderFree(e->k4400);
        free(e);
        return NULL;
    }
    return e;
}

DISA void destroy(void* p) {
    Eklenti* e = (Eklenti*)p;
    if (!e) return;
    mbe_ambe2450EncoderFree(e->k2450);
    mbe_ambe2400EncoderFree(e->k2400);
    mbe_imbe4400EncoderFree(e->k4400);
    free(e);
}

/* VocoderPlugin::decode_2450x1150(short*, unsigned char*) */
DISA void _ZN13VocoderPlugin16decode_2450x1150EPsPh(void* p, short* pcm, unsigned char* ambe) {
    Eklenti* e = (Eklenti*)p;
    char fr[4][24];
    char d[49];
    mbe_process_result r;
    memset(pcm, 0, 160 * sizeof(short));
    if (!e) return;
    dmr_ac(ambe, fr);
    mbe_initProcessResult(&r);
    (void)mbe_processAmbe3600x2450Frame(pcm, &r, (const char (*)[24])fr, d, &e->c_cur, &e->c_prev, &e->c_prev_enh);
}

/* VocoderPlugin::encode_2450x1150(short*, unsigned char*) */
DISA void _ZN13VocoderPlugin16encode_2450x1150EPsPh(void* p, short* pcm, unsigned char* ambe) {
    Eklenti* e = (Eklenti*)p;
    char d[49];
    char fr[4][24];
    memset(ambe, 0, 9);
    if (!e) return;
    if (mbe_encodeAmbe2450ParmsShort(e->k2450, pcm, d, &e->k_cur, &e->k_prev) < 0) return;
    mbe_moveMbeParms(&e->k_cur, &e->k_prev);
    if (mbe_encodeAmbe3600x2450Frame(d, fr) < 0) return;
    dmr_kapa((const char (*)[24])fr, ambe);
}

/* VocoderPlugin::decode_2400x1200(short*, unsigned char*) */
DISA void _ZN13VocoderPlugin16decode_2400x1200EPsPh(void* p, short* pcm, unsigned char* ambe) {
    Eklenti* e = (Eklenti*)p;
    char fr[4][24];
    char d[49];
    mbe_process_result r;
    memset(pcm, 0, 160 * sizeof(short));
    if (!e) return;
    if (mbe_decodeDStarDVData(ambe, fr) < 0) return;
    mbe_initProcessResult(&r);
    (void)mbe_processAmbe3600x2400Frame(pcm, &r, (const char (*)[24])fr, d, &e->d_cur, &e->d_prev, &e->d_prev_enh);
}

/* VocoderPlugin::encode_2400x1200(short*, unsigned char*) */
DISA void _ZN13VocoderPlugin16encode_2400x1200EPsPh(void* p, short* pcm, unsigned char* ambe) {
    Eklenti* e = (Eklenti*)p;
    char d[49];
    char fr[4][24];
    memset(ambe, 0, 9);
    if (!e) return;
    if (mbe_encodeAmbe2400ParmsShort(e->k2400, pcm, d, &e->kd_cur, &e->kd_prev) < 0) return;
    mbe_moveMbeParms(&e->kd_cur, &e->kd_prev);
    if (mbe_encodeAmbe3600x2400Frame(d, fr) < 0) return;
    (void)mbe_encodeDStarDVData((const char (*)[24])fr, ambe);
}

/*
 * IMBE 4400 (P25 ag bicimi): 11 bayt = 88 bit, en yuksek bit once, sira u0..u3 (12'ser),
 * u4..u6 (11'er), u7 (7) — MMDVMHost P25Audio::decode ile mbelib imbe_d[88] ayni sirada.
 * Bunlar DroidStar arayuzunde yok (uygulamanin kabugu istege bagli arar).
 */
DISA void dmrcepte_imbe_decode_4400(void* p, short* pcm, const unsigned char* imbe11) {
    Eklenti* e = (Eklenti*)p;
    char d[88];
    mbe_process_result r;
    memset(pcm, 0, 160 * sizeof(short));
    if (!e) return;
    for (int i = 0; i < 88; ++i) d[i] = (char)bit_al(imbe11, i);
    mbe_initProcessResult(&r);
    (void)mbe_processImbe4400Data(pcm, &r, d, &e->i_cur, &e->i_prev, &e->i_prev_enh);
}

DISA void dmrcepte_imbe_encode_4400(void* p, short* pcm, unsigned char* imbe11) {
    Eklenti* e = (Eklenti*)p;
    char d[88];
    memset(imbe11, 0, 11);
    if (!e) return;
    if (mbe_encodeImbe4400ParmsShort(e->k4400, pcm, d, &e->ki_cur, &e->ki_prev) < 0) return;
    mbe_moveMbeParms(&e->ki_cur, &e->ki_prev);
    for (int i = 0; i < 88; ++i) bit_koy(imbe11, i, d[i] & 1);
}

/* Teshis: eklentinin kimligi (uygulama istege bagli okur). */
DISA const char* eklenti_kimligi(void) { return "DMR Cepte eklentisi, mbelib-neo"; }
DISA const char* eklenti_mbelib_surumu(void) { return mbe_versionString(); }
