#ifndef WAVE_H
#define WAVE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


typedef struct {
    float  *data;
    size_t  size;
    int     channels;
} Samples;

typedef struct {
    char riff[4];
    uint32_t chunkSize;
    char wave[4];
    char fmt[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataSize;
} WAVHeader;

bool readWav(const char *filename, Samples *samples, float fs);

void writeWav(const char *filename, Samples *samples, float fs);

#endif //WAVE_H
