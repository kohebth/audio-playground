#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/measure/measure_v2.h>
#include <apgcore/runtime/runtime_v2.h>

#include "test_runtime_v2_harness.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHAIN_TEST_CHUNK 512u
#define CHAIN_TEST_GAIN  6.0f

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static bool read_u16_le(FILE *file, uint16_t *out) {
    unsigned char bytes[2];
    if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes))
        return false;
    *out = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u);
    return true;
}

static bool read_u32_le(FILE *file, uint32_t *out) {
    unsigned char bytes[4];
    if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes))
        return false;
    *out = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
    return true;
}

static bool read_tag(FILE *file, char out[4]) { return fread(out, 1, 4, file) == 4; }

static float read_pcm_sample(FILE *file, uint16_t bits_per_sample) {
    if (bits_per_sample == 8u) {
        int byte = fgetc(file);
        return (float)(byte - 128) / 128.0f;
    }

    if (bits_per_sample == 16u) {
        unsigned char bytes[2];
        if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes))
            return NAN;
        return (float)(int16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u)) / 32768.0f;
    }

    if (bits_per_sample == 24u) {
        unsigned char bytes[3];
        if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes))
            return NAN;
        int32_t sample = (int32_t)bytes[0] | ((int32_t)bytes[1] << 8) | ((int32_t)bytes[2] << 16);
        if (sample & 0x800000)
            sample |= ~0xFFFFFF;
        return (float)sample / 8388608.0f;
    }

    if (bits_per_sample == 32u) {
        unsigned char bytes[4];
        if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes))
            return NAN;
        int32_t sample = (int32_t)((uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) |
                                   ((uint32_t)bytes[3] << 24u));
        return (float)sample / 2147483648.0f;
    }

    return NAN;
}

static bool
parse_wav_mono_float(const char *path, float **out_samples, size_t *out_samples_len, uint32_t *out_sample_rate) {
    FILE    *file = fopen(path, "rb");
    char     tag[4];
    uint32_t chunk_size;
    uint32_t fmt_chunk_size;
    uint16_t format          = 0;
    uint16_t channels        = 0;
    uint32_t sample_rate     = 0;
    uint16_t bits_per_sample = 0;
    bool     got_fmt         = false;
    float   *samples         = NULL;
    size_t   sample_count    = 0;

    if (!file)
        return false;

    if (!read_tag(file, tag) || strncmp(tag, "RIFF", 4) != 0) {
        fclose(file);
        return false;
    }
    if (!read_u32_le(file, &chunk_size)) {
        fclose(file);
        return false;
    }
    (void)chunk_size;
    if (!read_tag(file, tag) || strncmp(tag, "WAVE", 4) != 0) {
        fclose(file);
        return false;
    }

    while (read_tag(file, tag)) {
        if (!read_u32_le(file, &fmt_chunk_size))
            break;

        if (strncmp(tag, "fmt ", 4) == 0) {
            uint32_t byte_rate;
            uint16_t block_align;
            if (fmt_chunk_size < 16u || !read_u16_le(file, &format) || !read_u16_le(file, &channels) ||
                !read_u32_le(file, &sample_rate) || !read_u32_le(file, &byte_rate) ||
                !read_u16_le(file, &block_align) || !read_u16_le(file, &bits_per_sample)) {
                fclose(file);
                return false;
            }
            if (fmt_chunk_size > 16u)
                fseek(file, (long)(fmt_chunk_size - 16u), SEEK_CUR);
            (void)byte_rate;
            (void)block_align;
            got_fmt = true;
        } else if (strncmp(tag, "data", 4) == 0) {
            if (!got_fmt || format != 1u || bits_per_sample == 0u || channels == 0u || sample_rate == 0u) {
                fclose(file);
                return false;
            }

            uint32_t bytes_per_sample = bits_per_sample / 8u;
            uint32_t bytes_per_frame  = channels * bytes_per_sample;
            if (bytes_per_sample == 0u || bits_per_sample % 8u != 0u || bytes_per_frame == 0u || fmt_chunk_size == 0u ||
                (fmt_chunk_size % bytes_per_frame) != 0u) {
                fclose(file);
                return false;
            }

            sample_count = fmt_chunk_size / bytes_per_frame;
            samples      = (float *)malloc(sample_count * sizeof(*samples));
            if (!samples) {
                fclose(file);
                return false;
            }

            for (size_t frame = 0; frame < sample_count; frame++) {
                double mixed = 0.0;
                for (uint16_t ch = 0; ch < channels; ch++) {
                    float sample = read_pcm_sample(file, bits_per_sample);
                    if (isnan(sample)) {
                        free(samples);
                        fclose(file);
                        return false;
                    }
                    mixed += (double)sample;
                }
                samples[frame] = (float)(mixed / (double)channels);
            }
            break;
        } else {
            if (fseek(file, (long)fmt_chunk_size, SEEK_CUR) != 0) {
                fclose(file);
                return false;
            }
        }
    }

    fclose(file);
    if (!samples) {
        return false;
    }
    if (out_samples_len)
        *out_samples_len = sample_count;
    if (out_sample_rate)
        *out_sample_rate = sample_rate;
    *out_samples = samples;
    return true;
}

static int load_runtime(uc_arena *arena, apg_v2_runtime_t *runtime) {
    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    uc_error                  err = {0};

    uc_status status = apg_project_v2_load_resolved_file(
        "test/fixtures/projects-v2/two-gain-chain.project.v2.yaml", arena, &project, &err
    );
    if (status != UC_OK) {
        fprintf(stderr, "project load error: %s\n", err.msg);
        return fail("failed to load two-gain-chain fixture");
    }

    status = apg_project_v2_compile(&project, arena, &compiled, &err);
    if (status != UC_OK) {
        fprintf(stderr, "project compile error: %s\n", err.msg);
        return fail("failed to compile two-gain-chain fixture");
    }

    status = test_apg_v2_runtime_init_registry(&compiled.plan, CHAIN_TEST_CHUNK, 48000.0f, arena, runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime init error: %s\n", err.msg);
        return fail("failed to initialize v2 runtime");
    }
    return 0;
}

int main(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024u * 1024u) != 0)
        return fail("arena init failed");

    float   *samples     = NULL;
    size_t   frame_count = 0u;
    uint32_t sample_rate = 0u;
    if (!parse_wav_mono_float(
            "test/fixtures/samples/344211_giomilko_c-major-9-bossa-nova-guitar.wav", &samples, &frame_count,
            &sample_rate
        )) {
        uc_arena_free(&arena);
        return fail("failed to load guitar sample fixture");
    }

    if (sample_rate != 48000u) {
        free(samples);
        uc_arena_free(&arena);
        fprintf(stderr, "sample rate mismatch: %u\n", (unsigned)sample_rate);
        return fail("unexpected sample rate on guitar fixture");
    }

    apg_v2_runtime_t runtime;
    if (load_runtime(&arena, &runtime)) {
        free(samples);
        uc_arena_free(&arena);
        return 1;
    }

    float output[CHAIN_TEST_CHUNK];
    for (size_t offset = 0u; offset < frame_count;) {
        uint32_t frames =
            (uint32_t)((frame_count - offset < CHAIN_TEST_CHUNK) ? (frame_count - offset) : CHAIN_TEST_CHUNK);
        if (!test_runtime_process_mono_ports(&runtime, "input", &samples[offset], "output", output, frames)) {
            fprintf(stderr, "runtime error: %s\n", apg_v2_measure_last_error(&runtime));
            free(samples);
            apg_v2_runtime_destroy(&runtime);
            uc_arena_free(&arena);
            return fail("runtime failed processing guitar chain fixture");
        }

        for (uint32_t i = 0u; i < frames; i++) {
            float expected = samples[offset + i] * CHAIN_TEST_GAIN;
            if (!isfinite(output[i])) {
                free(samples);
                apg_v2_runtime_destroy(&runtime);
                uc_arena_free(&arena);
                return fail("runtime produced non-finite output");
            }
            if (fabsf(output[i] - expected) > 1e-4f) {
                fprintf(
                    stderr, "sample mismatch at frame %zu: expected %.7f got %.7f\n", offset + i, (double)expected,
                    (double)output[i]
                );
                free(samples);
                apg_v2_runtime_destroy(&runtime);
                uc_arena_free(&arena);
                return fail("guitar chain output did not match expected gain");
            }
        }
        offset += frames;
    }

    free(samples);
    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}
