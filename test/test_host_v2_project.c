#include <apgcore/host/host_v2.h>

#include <math.h>
#include <stdio.h>

#define TEST_FRAMES 3u

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int expect_samples_near(const float *actual, const float *expected, const char *label) {
    for (size_t i = 0; i < TEST_FRAMES; i++) {
        float diff = fabsf(actual[i] - expected[i]);
        if (diff > 0.00001f) {
            fprintf(stderr, "unexpected %s sample %zu: got %f expected %f\n", label, i, actual[i], expected[i]);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    apg_v2_host_project_t *host = NULL;
    uc_error               err  = {0};

    uc_status status = apg_v2_host_project_load_file(
        "test/fixtures/projects-v2/simple-gain-board.project.v2.yaml", TEST_FRAMES, 48000.0f, &host, &err
    );
    if (status != UC_OK) {
        fprintf(stderr, "failed to load project: %s\n", err.msg);
        return fail("project host load failed");
    }

    const float input[TEST_FRAMES]        = {0.25f, -0.5f, 1.0f};
    float       output[TEST_FRAMES]       = {0.0f, 0.0f, 0.0f};
    float       gain_default[TEST_FRAMES] = {0.5f, -1.0f, 2.0f};
    float       gain_updated[TEST_FRAMES] = {0.5015625f, -1.003125f, 2.00625f};

    if (!apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES)) {
        fprintf(stderr, "project host process failed: %s\n", apg_v2_host_project_last_error(host));
        apg_v2_host_project_destroy(host);
        return fail("project host process failed");
    }
    if (expect_samples_near(output, gain_default, "default gain")) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    if (!apg_v2_host_project_set_param(host, "gain1.gain", 3.0f)) {
        apg_v2_host_project_destroy(host);
        return fail("project host refused namespaced param");
    }
    if (apg_v2_host_project_set_param(host, "gain", 3.0f)) {
        apg_v2_host_project_destroy(host);
        return fail("project host accepted ambiguous param name");
    }
    if (!apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES)) {
        fprintf(stderr, "project host process failed after param update: %s\n", apg_v2_host_project_last_error(host));
        apg_v2_host_project_destroy(host);
        return fail("project host process after update failed");
    }
    if (expect_samples_near(output, gain_updated, "updated gain")) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    apg_v2_host_project_destroy(host);
    return 0;
}
