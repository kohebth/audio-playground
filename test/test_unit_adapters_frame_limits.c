#include <unit/chorus.h>
#include <unit/sustainer.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_FRAMES 1024u
#define SENTINEL   12345.0f

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static void fill_input(float *input, uint32_t frames) {
    for (uint32_t i = 0; i <= MAX_FRAMES; i++)
        input[i] = SENTINEL;
    for (uint32_t i = 0; i < frames; i++)
        input[i] = ((float)(int)(i % 31u) - 15.0f) / 64.0f;
}

static void fill_output(float *output) {
    for (uint32_t i = 0; i <= MAX_FRAMES; i++)
        output[i] = SENTINEL;
}

static int assert_output_sane(const float *output, uint32_t frames) {
    for (uint32_t i = 0; i < frames; i++) {
        if (!isfinite(output[i]))
            return fail("unit adapter produced non-finite output");
    }
    if (output[frames] != SENTINEL)
        return fail("unit adapter wrote past requested frame count");
    return 0;
}

static int test_chorus(void) {
    float input[MAX_FRAMES + 1u];
    float output[MAX_FRAMES + 1u];
    float mod_buffer[4096] = {0};

    ChorusState state = {0};
    state.mod_state.buffer = mod_buffer;
    ChorusParams params = {.rate = 0.4f, .depth = 12.0f, .sample_rate = 48000u};
    const uint32_t frames[] = {64u, 128u, 512u, 1024u};

    for (uint32_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        fill_input(input, frames[i]);
        fill_output(output);
        chorus_out_t out = {output};
        chorus_in_t in = {input};
        if (!chorus_process_frames(out, in, params, &state, frames[i]))
            return fail("chorus_process_frames rejected valid frame count");
        if (assert_output_sane(output, frames[i]))
            return 1;
    }

    fill_input(input, 64u);
    fill_output(output);
    chorus_out_t out = {output};
    chorus_in_t in = {input};
    if (chorus_process_frames(out, in, params, &state, 0u))
        return fail("chorus_process_frames accepted zero frames");
    if (output[0] != SENTINEL)
        return fail("rejected chorus call modified output");
    return 0;
}

static int test_sustainer(void) {
    float input[MAX_FRAMES + 1u];
    float output[MAX_FRAMES + 1u];

    SustainerState state = {0};
    SustainerParams params = {
        .gain = 12.0f,
        .threshold = -36.0f,
        .attack = 0.005f,
        .release = 0.080f,
        .sample_rate = 48000u,
    };
    const uint32_t frames[] = {64u, 128u, 512u, 1024u};

    for (uint32_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        fill_input(input, frames[i]);
        fill_output(output);
        sustainer_out_t out = {output};
        sustainer_in_t in = {input};
        if (!sustainer_process_frames(out, in, params, &state, frames[i]))
            return fail("sustainer_process_frames rejected valid frame count");
        if (assert_output_sane(output, frames[i]))
            return 1;
    }

    fill_input(input, 64u);
    fill_output(output);
    sustainer_out_t out = {output};
    sustainer_in_t in = {input};
    if (sustainer_process_frames(out, in, params, &state, 0u))
        return fail("sustainer_process_frames accepted zero frames");
    if (output[0] != SENTINEL)
        return fail("rejected sustainer call modified output");
    return 0;
}

int main(void) {
    if (test_chorus())
        return 1;
    if (test_sustainer())
        return 1;
    return 0;
}
