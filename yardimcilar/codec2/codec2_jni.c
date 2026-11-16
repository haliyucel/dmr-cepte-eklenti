/*
 * DMR Cepte — Codec 2 JNI koprusu (libcodec2jni.so).
 *
 * M17'nin ses kodegi Codec 2 (LGPL 2.1, github.com/drowe67/codec2). Bu dosya yalniz
 * Kotlin'deki com.dmrcepte.app.Codec2 nesnesinin yerel yontemlerini Codec 2'nin
 * C arayuzune baglar. Statik baglama (ad eslesmesi); R8 kurali proguard-rules.pro'da.
 *
 * Kipler: 3200 (M17 yalniz ses: 20 ms = 160 ornek -> 8 bayt) ve
 *         1600 (M17 ses+veri: 40 ms = 320 ornek -> 8 bayt).
 */
#include <jni.h>
#include <stdint.h>
#include "codec2.h"

static struct CODEC2 *tut(jlong h) { return (struct CODEC2 *)(intptr_t)h; }

JNIEXPORT jlong JNICALL Java_com_dmrcepte_app_Codec2_nativeAc(JNIEnv *e, jclass c, jint kip) {
    (void)e; (void)c;
    int m;
    if (kip == 3200) m = CODEC2_MODE_3200;
    else if (kip == 1600) m = CODEC2_MODE_1600;
    else return 0;
    return (jlong)(intptr_t)codec2_create(m);
}

JNIEXPORT void JNICALL Java_com_dmrcepte_app_Codec2_nativeKapat(JNIEnv *e, jclass c, jlong h) {
    (void)e; (void)c;
    if (h != 0) codec2_destroy(tut(h));
}

JNIEXPORT jint JNICALL Java_com_dmrcepte_app_Codec2_nativeOrnek(JNIEnv *e, jclass c, jlong h) {
    (void)e; (void)c;
    return h == 0 ? -1 : codec2_samples_per_frame(tut(h));
}

JNIEXPORT jint JNICALL Java_com_dmrcepte_app_Codec2_nativeBayt(JNIEnv *e, jclass c, jlong h) {
    (void)e; (void)c;
    return h == 0 ? -1 : codec2_bytes_per_frame(tut(h));
}

/** pcm (ornek sayisi kadar) -> bit (bayt sayisi kadar). Donus: yazilan bayt, hata < 0. */
JNIEXPORT jint JNICALL Java_com_dmrcepte_app_Codec2_nativeKodla(JNIEnv *e, jclass c, jlong h,
        jshortArray pcm, jbyteArray bit) {
    (void)c;
    struct CODEC2 *k = tut(h);
    if (k == NULL || pcm == NULL || bit == NULL) return -1;
    int n = codec2_samples_per_frame(k);
    int nb = codec2_bytes_per_frame(k);
    if ((*e)->GetArrayLength(e, pcm) < n || (*e)->GetArrayLength(e, bit) < nb) return -2;
    jshort *p = (*e)->GetShortArrayElements(e, pcm, NULL);
    jbyte *b = (*e)->GetByteArrayElements(e, bit, NULL);
    if (p == NULL || b == NULL) {
        if (p) (*e)->ReleaseShortArrayElements(e, pcm, p, JNI_ABORT);
        if (b) (*e)->ReleaseByteArrayElements(e, bit, b, JNI_ABORT);
        return -3;
    }
    codec2_encode(k, (unsigned char *)b, (short *)p);
    (*e)->ReleaseShortArrayElements(e, pcm, p, JNI_ABORT);
    (*e)->ReleaseByteArrayElements(e, bit, b, 0);
    return nb;
}

/** bit (bayt sayisi kadar) -> pcm (ornek sayisi kadar). Donus: yazilan ornek, hata < 0. */
JNIEXPORT jint JNICALL Java_com_dmrcepte_app_Codec2_nativeCoz(JNIEnv *e, jclass c, jlong h,
        jbyteArray bit, jshortArray pcm) {
    (void)c;
    struct CODEC2 *k = tut(h);
    if (k == NULL || pcm == NULL || bit == NULL) return -1;
    int n = codec2_samples_per_frame(k);
    int nb = codec2_bytes_per_frame(k);
    if ((*e)->GetArrayLength(e, pcm) < n || (*e)->GetArrayLength(e, bit) < nb) return -2;
    jbyte *b = (*e)->GetByteArrayElements(e, bit, NULL);
    jshort *p = (*e)->GetShortArrayElements(e, pcm, NULL);
    if (p == NULL || b == NULL) {
        if (p) (*e)->ReleaseShortArrayElements(e, pcm, p, JNI_ABORT);
        if (b) (*e)->ReleaseByteArrayElements(e, bit, b, JNI_ABORT);
        return -3;
    }
    codec2_decode(k, (short *)p, (const unsigned char *)b);
    (*e)->ReleaseByteArrayElements(e, bit, b, JNI_ABORT);
    (*e)->ReleaseShortArrayElements(e, pcm, p, 0);
    return n;
}
