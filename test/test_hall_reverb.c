#include <wave.h>
#include <ctrl/ctrls.h>
#include <runtime.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK 512

static float *process_unit_buf(const char *unit_path,
                               const float *in, size_t n_frames, int sr) {
    runtime_context_t ctx = {.sample_rate = (float)sr, .chunk_length = CHUNK};
    runtime_unit_t *unit = runtime_unit_load(unit_path, ctx);
    if (!unit) return NULL;

    ctrl_unit_t ctrl;
    if (!ctrl_unit_init(&ctrl, unit, unit_path)) {
        runtime_unit_destroy(unit);
        return NULL;
    }

    float *out = calloc(n_frames, sizeof(float));
    float buf[CHUNK], obuf[CHUNK];
    size_t pos = 0;
    while (pos < n_frames) {
        size_t len = n_frames - pos;
        if (len > CHUNK) len = CHUNK;
        memcpy(buf, in + pos, len * sizeof(float));
        if (len < CHUNK) memset(buf + len, 0, (CHUNK - len) * sizeof(float));
        ctrl_unit_process(&ctrl, buf, obuf);
        memcpy(out + pos, obuf, len * sizeof(float));
        pos += CHUNK;
    }

    ctrl_unit_destroy(&ctrl);
    runtime_unit_destroy(unit);
    return out;
}

static int process_unit(const char *unit_path, const char *out_path,
                        float *mono, size_t n_frames, int sr) {
    runtime_context_t ctx = {.sample_rate = (float)sr, .chunk_length = CHUNK};
    runtime_unit_t *unit = runtime_unit_load(unit_path, ctx);
    if (!unit) return -1;

    ctrl_unit_t ctrl;
    if (!ctrl_unit_init(&ctrl, unit, unit_path)) {
        runtime_unit_destroy(unit);
        return -1;
    }

    Samples out = {0};
    out.size = n_frames;
    out.data = calloc(n_frames, sizeof(float));

    float buf[CHUNK], obuf[CHUNK];
    size_t pos = 0;
    int chunk = 0;
    while (pos < n_frames) {
        size_t len = n_frames - pos;
        if (len > CHUNK) len = CHUNK;
        memcpy(buf, mono + pos, len * sizeof(float));
        if (len < CHUNK) memset(buf + len, 0, (CHUNK - len) * sizeof(float));
        ctrl_unit_process(&ctrl, buf, obuf);
        memcpy(out.data + pos, obuf, len * sizeof(float));
        pos += CHUNK;

        if (chunk == 0) {
            float peak = 0;
            for (int k = 0; k < CHUNK; k++) {
                float a = fabsf(obuf[k]);
                if (a > peak) peak = a;
            }
            printf("  first chunk peak=%.6f\n", peak);
        }
        chunk++;
    }

    writeWav(out_path, &out, (float)sr);
    printf("  -> %s\n", out_path);

    free(out.data);
    ctrl_unit_destroy(&ctrl);
    runtime_unit_destroy(unit);
    return 0;
}

int main(void) {
    Samples in = {0};
    if (!readWav("samples/clean-strum-and-arpeggio.wav", &in, 0)) {
        fprintf(stderr, "FAIL: read wav\n");
        return 1;
    }
    int sr = 44100;
    size_t n_frames = in.size / in.channels;
    printf("input: %zu samples, %d ch, %zu frames, %d Hz\n",
           in.size, in.channels, n_frames, sr);

    float *mono = malloc(n_frames * sizeof(float));
    for (size_t i = 0; i < n_frames; i++) {
        float sum = 0.0f;
        for (int c = 0; c < in.channels; c++)
            sum += in.data[i * in.channels + c];
        mono[i] = sum / in.channels;
    }

    // printf("\n--- hall_reverb ---\n");
    // process_unit("units/hall_reverb.unit.yaml",
    //              "analysis/clean-strum-and-arpeggio-hall-reverb.wav",
    //              mono, n_frames, sr);

    // printf("\n--- overdrive ---\n");
    // process_unit("units/overdrive.unit.yaml",
    //              "analysis/clean-strum-and-arpeggio-overdrive.wav",
    //              mono, n_frames, sr);

    // printf("\n--- church_reverb ---\n");
    // process_unit("units/church_reverb.unit.yaml",
    //              "analysis/clean-strum-and-arpeggio-church-reverb.wav",
    //              mono, n_frames, sr);

    // printf("\n--- analog_delay ---\n");
    // process_unit("units/analog_delay.unit.yaml",
    //              "analysis/clean-strum-and-arpeggio-analog-delay.wav",
    //              mono, n_frames, sr);

    printf("\n--- marshall_plexi -> marshall_4x12 ---\n");
    float *plexi = process_unit_buf("units/marshall_plexi_head_amp.unit.yaml",
                                     mono, n_frames, sr);
    if (plexi) {
        process_unit("units/marshall_4x12_greenback_cabinet.unit.yaml",
                     "analysis/clean-strum-and-arpeggio-marshall-chain.wav",
                     plexi, n_frames, sr);
        free(plexi);
    }

    printf("\n--- plexi -> cab -> analog_delay -> hall_reverb ---\n");
    {
        float *a = process_unit_buf("units/marshall_plexi_head_amp.unit.yaml", mono, n_frames, sr);
        if (!a) goto cleanup;
        float *b = process_unit_buf("units/marshall_4x12_greenback_cabinet.unit.yaml", a, n_frames, sr);
        free(a); a = NULL;
        if (!b) goto cleanup;
        float *c = process_unit_buf("units/analog_delay.unit.yaml", b, n_frames, sr);
        free(b); b = NULL;
        if (!c) goto cleanup;
        process_unit("units/hall_reverb.unit.yaml",
                     "analysis/clean-strum-and-arpeggio-full-chain.wav",
                     c, n_frames, sr);
        free(c);
    }

cleanup:
    free(mono);
    free(in.data);
    return 0;
}
