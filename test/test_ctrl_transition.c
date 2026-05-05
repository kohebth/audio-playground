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

static ctrl_param_t *find_ctrl_param(ctrl_unit_t *ctrl, const char *name) {
    for (int i = 0; i < ctrl->n_params; i++) {
        if (strcmp(ctrl->params[i].name, name) == 0) return &ctrl->params[i];
    }
    return NULL;
}

int main(void) {
    const char *path = "units/marshall_plexi_head_amp.unit.yaml";
    runtime_context_t ctx = {.sample_rate = 48000, .chunk_length = TEST_CHUNK};
    runtime_unit_t *unit = runtime_unit_load(path, ctx);
    if (!unit) return fail("failed to load marshall amp");

    ctrl_unit_t ctrl;
    if (!ctrl_unit_init(&ctrl, unit, path)) return fail("failed to initialize ctrl");

    ctrl_param_t *presence = find_ctrl_param(&ctrl, "presence");
    if (!presence) return fail("missing presence ctrl param");
    if (presence->n_bindings < 1) return fail("presence did not bind to any runtime config");

    float initial = presence->current;
    if (!ctrl_unit_set_smoothing_ms(&ctrl, "presence", 60.0f)) return fail("failed to set smoothing");
    if (!ctrl_unit_set_target(&ctrl, "presence", 0.95f)) return fail("failed to set target");

    float previous = presence->current;
    for (int i = 0; i < 24; i++) {
        ctrl_unit_tick(&ctrl, TEST_CHUNK);
        if (!isfinite(presence->current)) return fail("presence became non-finite");
        if (presence->current + 1e-6f < previous) return fail("presence smoothing was not monotonic");
        if (presence->current > 0.95f + 1e-4f) return fail("presence smoothing overshot target");
        previous = presence->current;
    }

    if (presence->current <= initial) return fail("presence did not move toward target");
    if (fabsf(presence->current - 0.95f) > 0.05f) return fail("presence did not approach target closely enough");

    float in[TEST_CHUNK];
    float out[TEST_CHUNK];
    for (int i = 0; i < TEST_CHUNK; i++) {
        in[i] = 0.15f * sinf(2.0f * 3.14159265358979323846f * 220.0f * (float)i / 48000.0f);
        out[i] = 0.0f;
    }
    ctrl_unit_process(&ctrl, in, out);
    for (int i = 0; i < TEST_CHUNK; i++) {
        if (!isfinite(out[i])) return fail("ctrl processed audio produced non-finite output");
    }

    ctrl_unit_destroy(&ctrl);
    runtime_unit_destroy(unit);
    return 0;
}
