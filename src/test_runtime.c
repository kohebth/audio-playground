#include <apgcore/host_v2.h>

#include <math.h>
#include <stdio.h>

#define TEST_FRAMES 512u

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static void fill_input(float *input, int chunk) {
    for (uint32_t i = 0; i < TEST_FRAMES; i++) {
        uint32_t n = (uint32_t)chunk * TEST_FRAMES + i;
        float    t = (float)n / 48000.0f;
        input[i]   = 0.2f * sinf(2.0f * 3.14159265358979323846f * 220.0f * t);
    }
}

int main(void) {
    apg_v2_host_unit_t host;
    uc_error           err = {0};
    uc_status status = apg_v2_host_load_file("units-v2/simple_gain.unit.v2.yaml", TEST_FRAMES, 48000.0f, &host, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        return fail("failed to load v2 runtime smoke unit");
    }

    if (!apg_v2_host_set_param(&host, "gain", 1.5f)) {
        apg_v2_host_destroy(&host);
        return fail("failed to set v2 runtime smoke gain");
    }

    float  input[TEST_FRAMES];
    float  output[TEST_FRAMES];
    double sum_sq = 0.0;
    float  peak   = 0.0f;
    for (int chunk = 0; chunk < 8; chunk++) {
        fill_input(input, chunk);
        if (!apg_v2_host_process_mono_ports(&host, "input", input, "output", output, TEST_FRAMES)) {
            fprintf(stderr, "runtime error: %s\n", apg_v2_runtime_last_error(&host.runtime));
            apg_v2_host_destroy(&host);
            return fail("v2 runtime smoke processing failed");
        }

        for (uint32_t i = 0; i < TEST_FRAMES; i++) {
            if (!isfinite(output[i])) {
                apg_v2_host_destroy(&host);
                return fail("v2 runtime smoke produced non-finite output");
            }
            float a = fabsf(output[i]);
            if (a > peak)
                peak = a;
            sum_sq += (double)output[i] * (double)output[i];
        }
    }

    double rms = sqrt(sum_sq / (double)(8u * TEST_FRAMES));
    printf("v2 runtime smoke: peak=%.6f rms=%.6f\n", peak, rms);

    apg_v2_host_destroy(&host);
    return (peak > 1e-6f && peak < 0.4f && rms > 1e-7 && rms < 0.3) ? 0 : fail("v2 runtime smoke output out of range");
}
