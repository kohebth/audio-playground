#include <ctrl/ctrls.h>
#include <runtime.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_CHUNK 512

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int process_ctrl(ctrl_unit_t *ctrl, const float *input, float *output) {
    float in_copy[TEST_CHUNK];
    memcpy(in_copy, input, sizeof(in_copy));
    ctrl_unit_process(ctrl, in_copy, output);
    for (int i = 0; i < TEST_CHUNK; i++) {
        if (!isfinite(output[i])) return 1;
    }
    return 0;
}

int main(void) {
    const char *amp_path = "units/marshall_plexi_head_amp.unit.yaml";
    const char *cab_path = "units/marshall_4x12_greenback_cabinet.unit.yaml";
    runtime_context_t ctx = {.sample_rate = 48000, .chunk_length = TEST_CHUNK};

    runtime_unit_t *amp = runtime_unit_load(amp_path, ctx);
    runtime_unit_t *cab = runtime_unit_load(cab_path, ctx);
    if (!amp || !cab) return fail("failed to load amp/cab chain");

    ctrl_unit_t amp_ctrl;
    ctrl_unit_t cab_ctrl;
    if (!ctrl_unit_init(&amp_ctrl, amp, amp_path)) return fail("failed to init amp ctrl");
    if (!ctrl_unit_init(&cab_ctrl, cab, cab_path)) return fail("failed to init cab ctrl");

    ctrl_unit_set_smoothing_ms(&amp_ctrl, "input_gain", 80.0f);
    ctrl_unit_set_target(&amp_ctrl, "input_gain", 8.5f);
    ctrl_unit_set_smoothing_ms(&amp_ctrl, "presence", 50.0f);
    ctrl_unit_set_target(&amp_ctrl, "presence", 0.8f);

    float input[TEST_CHUNK];
    float amp_out[TEST_CHUNK];
    float cab_out[TEST_CHUNK];
    float previous_last = 0.0f;
    float peak = 0.0f;
    double sum_sq = 0.0;
    int sample_count = 0;

    for (int chunk = 0; chunk < 64; chunk++) {
        for (int i = 0; i < TEST_CHUNK; i++) {
            int n = chunk * TEST_CHUNK + i;
            float sine = sinf(2.0f * 3.14159265358979323846f * 110.0f * (float)n / 48000.0f);
            input[i] = 0.18f * sine;
        }

        if (process_ctrl(&amp_ctrl, input, amp_out)) return fail("amp produced non-finite output");
        if (process_ctrl(&cab_ctrl, amp_out, cab_out)) return fail("cab produced non-finite output");

        if (chunk > 0) {
            float boundary_delta = fabsf(cab_out[0] - previous_last);
            if (boundary_delta > 4.0f) return fail("chain produced a large chunk-boundary discontinuity");
        }
        previous_last = cab_out[TEST_CHUNK - 1];

        for (int i = 0; i < TEST_CHUNK; i++) {
            float a = fabsf(cab_out[i]);
            if (a > peak) peak = a;
            sum_sq += (double)cab_out[i] * (double)cab_out[i];
            sample_count++;
        }
    }

    double rms = sqrt(sum_sq / (double)sample_count);
    if (peak <= 1e-7f) return fail("chain output is silent");
    if (peak > 8.0f) return fail("chain output peak is excessive");
    if (rms <= 1e-8 || rms > 2.0) return fail("chain output RMS is out of range");

    ctrl_unit_destroy(&amp_ctrl);
    ctrl_unit_destroy(&cab_ctrl);
    runtime_unit_destroy(amp);
    runtime_unit_destroy(cab);
    return 0;
}
