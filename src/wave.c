#include "wave.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool readWav(const char *filename, Samples *samples, float fs) {
    (void)fs;
    FILE *f = fopen(filename, "rb");
    if (!f) return false;

    WAVHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f); return false;
    }
    if (hdr.audioFormat != 1 || hdr.bitsPerSample != 16) {
        fclose(f); return false;
    }

    samples->channels = hdr.numChannels;
    samples->size = hdr.dataSize / (hdr.bitsPerSample / 8);
    samples->data = malloc(samples->size * sizeof(float));
    if (!samples->data) { fclose(f); return false; }

    for (size_t i = 0; i < samples->size; i++) {
        int16_t v;
        if (fread(&v, sizeof(v), 1, f) != 1) v = 0;
        samples->data[i] = v / 32768.0f;
    }
    fclose(f);
    return true;
}

void writeWav(const char *filename, Samples *samples, float fs) {
    WAVHeader hdr = {
        {'R', 'I', 'F', 'F'}, 0,
        {'W', 'A', 'V', 'E'},
        {'f', 'm', 't', ' '}, 16, 1, 1,
        (uint32_t)fs, 0, 0, 16,
        {'d', 'a', 't', 'a'}, 0
    };
    hdr.blockAlign = hdr.numChannels * hdr.bitsPerSample / 8;
    hdr.byteRate = hdr.sampleRate * hdr.blockAlign;
    hdr.dataSize = (uint32_t)samples->size * hdr.blockAlign;
    hdr.chunkSize = 36 + hdr.dataSize;

    FILE *f = fopen(filename, "wb");
    if (!f) return;
    fwrite(&hdr, sizeof(hdr), 1, f);
    for (size_t i = 0; i < samples->size; i++) {
        int16_t v = (int16_t)(fminf(fmaxf(samples->data[i], -1.0f), 1.0f) * 32767.0f);
        fwrite(&v, sizeof(v), 1, f);
    }
    fclose(f);
}
