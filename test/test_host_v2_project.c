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

static int load_resolved_project(const char *path, uc_arena *arena, apg_project_v2_resolved_t *project) {
    uc_error err = {0};
    if (uc_arena_init(arena, 2u * 1024u * 1024u) != 0)
        return fail("resolved project arena init failed");
    uc_status status = apg_project_v2_load_resolved_file(path, arena, project, &err);
    if (status != UC_OK) {
        fprintf(stderr, "failed to load resolved project %s: %s\n", path, err.msg);
        uc_arena_free(arena);
        return fail("resolved project load failed");
    }
    return 0;
}

static int test_project_host_live_controls(void) {
    apg_v2_host_project_t *host = NULL;
    uc_error               err  = {0};

    uc_status status = apg_v2_host_project_load_file(
        "test/fixtures/projects-v2/two-gain-chain.project.v2.yaml", TEST_FRAMES, 48000.0f, &host, &err
    );
    if (status != UC_OK) {
        fprintf(stderr, "failed to load project: %s\n", err.msg);
        return fail("project host load failed");
    }

    const float input[TEST_FRAMES]             = {0.25f, -0.5f, 1.0f};
    float       output[TEST_FRAMES]            = {0.0f, 0.0f, 0.0f};
    float       expected_default[TEST_FRAMES]  = {1.5f, -3.0f, 6.0f};
    float       expected_bypassed[TEST_FRAMES] = {0.75f, -1.5f, 3.0f};
    float       expected_muted[TEST_FRAMES]    = {0.0f, 0.0f, 0.0f};

    if (!apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES)) {
        apg_v2_host_project_destroy(host);
        return fail("project host process failed");
    }
    if (expect_samples_near(output, expected_default, "default two gain")) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    if (!apg_v2_host_project_set_bypass(host, "gain1", true)) {
        apg_v2_host_project_destroy(host);
        return fail("project host refused bypass");
    }
    if (!apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES)) {
        apg_v2_host_project_destroy(host);
        return fail("project host process with bypass failed");
    }
    if (expect_samples_near(output, expected_bypassed, "bypassed two gain")) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    if (!apg_v2_host_project_set_mute(host, true)) {
        apg_v2_host_project_destroy(host);
        return fail("project host refused mute");
    }
    if (!apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES)) {
        apg_v2_host_project_destroy(host);
        return fail("project host process with mute failed");
    }
    if (expect_samples_near(output, expected_muted, "muted two gain")) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    apg_v2_host_project_destroy(host);
    return 0;
}

static int test_project_host_prepare_failure_keeps_active(void) {
    apg_v2_host_project_t *host = NULL;
    uc_error               err  = {0};

    uc_status status = apg_v2_host_project_load_file(
        "test/fixtures/projects-v2/simple-gain-board.project.v2.yaml", TEST_FRAMES, 48000.0f, &host, &err
    );
    if (status != UC_OK)
        return fail("project host load failed");

    apg_project_v2_resolved_t   invalid = {0};
    apg_v2_host_project_swap_t *swap    = NULL;
    status                              = apg_v2_host_project_prepare_swap(host, &invalid, &swap, &err);
    if (status == UC_OK || swap) {
        apg_v2_host_project_swap_destroy(&swap);
        apg_v2_host_project_destroy(host);
        return fail("invalid swap prepare unexpectedly succeeded");
    }

    const float input[TEST_FRAMES]    = {0.25f, -0.5f, 1.0f};
    float       output[TEST_FRAMES]   = {0.0f, 0.0f, 0.0f};
    float       expected[TEST_FRAMES] = {0.5f, -1.0f, 2.0f};
    if (!apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES)) {
        apg_v2_host_project_destroy(host);
        return fail("project host process after failed prepare failed");
    }
    if (expect_samples_near(output, expected, "active after failed prepare")) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    apg_v2_host_project_destroy(host);
    return 0;
}

static int test_project_host_commit_swap_crossfades(void) {
    apg_v2_host_project_t *host = NULL;
    uc_error               err  = {0};

    uc_status status = apg_v2_host_project_load_file(
        "test/fixtures/projects-v2/simple-gain-board.project.v2.yaml", TEST_FRAMES, 48000.0f, &host, &err
    );
    if (status != UC_OK)
        return fail("project host load failed");

    uc_arena                  arena   = {0};
    apg_project_v2_resolved_t project = {0};
    if (load_resolved_project("test/fixtures/projects-v2/two-gain-chain.project.v2.yaml", &arena, &project)) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    apg_v2_host_project_swap_t *swap = NULL;
    status                           = apg_v2_host_project_prepare_swap(host, &project, &swap, &err);
    uc_arena_free(&arena);
    if (status != UC_OK || !swap) {
        apg_v2_host_project_destroy(host);
        return fail("project host prepare swap failed");
    }
    if (!apg_v2_host_project_commit_swap(host, &swap) || swap) {
        apg_v2_host_project_swap_destroy(&swap);
        apg_v2_host_project_destroy(host);
        return fail("project host commit swap failed");
    }

    const float input[TEST_FRAMES]         = {1.0f, 1.0f, 1.0f};
    float       output[TEST_FRAMES]        = {0.0f, 0.0f, 0.0f};
    float       expected_fade[TEST_FRAMES] = {10.0f / 3.0f, 14.0f / 3.0f, 6.0f};
    float       expected_new[TEST_FRAMES]  = {6.0f, 6.0f, 6.0f};

    if (!apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES)) {
        apg_v2_host_project_destroy(host);
        return fail("project host crossfade process failed");
    }
    if (expect_samples_near(output, expected_fade, "swap crossfade")) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    if (!apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES)) {
        apg_v2_host_project_destroy(host);
        return fail("project host post-crossfade process failed");
    }
    if (expect_samples_near(output, expected_new, "post-swap output")) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    apg_v2_host_project_destroy(host);
    return 0;
}

static int test_project_host_swap_preserves_bypass(void) {
    apg_v2_host_project_t *host = NULL;
    uc_error               err  = {0};

    uc_status status = apg_v2_host_project_load_file(
        "test/fixtures/projects-v2/two-gain-chain.project.v2.yaml", TEST_FRAMES, 48000.0f, &host, &err
    );
    if (status != UC_OK)
        return fail("project host load failed");
    if (!apg_v2_host_project_set_bypass(host, "gain1", true)) {
        apg_v2_host_project_destroy(host);
        return fail("project host refused bypass before swap");
    }

    uc_arena                  arena   = {0};
    apg_project_v2_resolved_t project = {0};
    if (load_resolved_project("test/fixtures/projects-v2/two-gain-chain.project.v2.yaml", &arena, &project)) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    apg_v2_host_project_swap_t *swap = NULL;
    status                           = apg_v2_host_project_prepare_swap(host, &project, &swap, &err);
    uc_arena_free(&arena);
    if (status != UC_OK || !swap) {
        apg_v2_host_project_destroy(host);
        return fail("project host prepare same-project swap failed");
    }
    if (!apg_v2_host_project_commit_swap(host, &swap)) {
        apg_v2_host_project_swap_destroy(&swap);
        apg_v2_host_project_destroy(host);
        return fail("project host commit same-project swap failed");
    }

    const float input[TEST_FRAMES]    = {1.0f, 1.0f, 1.0f};
    float       output[TEST_FRAMES]   = {0.0f, 0.0f, 0.0f};
    float       expected[TEST_FRAMES] = {3.0f, 3.0f, 3.0f};
    if (!apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES) ||
        !apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES)) {
        apg_v2_host_project_destroy(host);
        return fail("project host process after bypass-preserving swap failed");
    }
    if (expect_samples_near(output, expected, "preserved bypass after swap")) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    apg_v2_host_project_destroy(host);
    return 0;
}

static int test_project_host_swap_preserves_param(void) {
    apg_v2_host_project_t *host = NULL;
    uc_error               err  = {0};

    uc_status status = apg_v2_host_project_load_file(
        "test/fixtures/projects-v2/simple-gain-board.project.v2.yaml", TEST_FRAMES, 48000.0f, &host, &err
    );
    if (status != UC_OK)
        return fail("project host load failed");
    if (!apg_v2_host_project_set_param(host, "gain1.gain", 3.0f)) {
        apg_v2_host_project_destroy(host);
        return fail("project host refused param before swap");
    }

    uc_arena                  arena   = {0};
    apg_project_v2_resolved_t project = {0};
    if (load_resolved_project("test/fixtures/projects-v2/simple-gain-board.project.v2.yaml", &arena, &project)) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    apg_v2_host_project_swap_t *swap = NULL;
    status                           = apg_v2_host_project_prepare_swap(host, &project, &swap, &err);
    uc_arena_free(&arena);
    if (status != UC_OK || !swap) {
        apg_v2_host_project_destroy(host);
        return fail("project host prepare param-preserving swap failed");
    }
    if (!apg_v2_host_project_commit_swap(host, &swap)) {
        apg_v2_host_project_swap_destroy(&swap);
        apg_v2_host_project_destroy(host);
        return fail("project host commit param-preserving swap failed");
    }

    const float input[TEST_FRAMES]    = {1.0f, 1.0f, 1.0f};
    float       output[TEST_FRAMES]   = {0.0f, 0.0f, 0.0f};
    float       expected[TEST_FRAMES] = {3.0f, 3.0f, 3.0f};
    for (size_t i = 0; i < 170u; i++) {
        if (!apg_v2_host_project_process_mono_ports(host, "input", input, "output", output, TEST_FRAMES)) {
            apg_v2_host_project_destroy(host);
            return fail("project host process after param-preserving swap failed");
        }
    }
    if (expect_samples_near(output, expected, "preserved param after swap")) {
        apg_v2_host_project_destroy(host);
        return 1;
    }

    apg_v2_host_project_destroy(host);
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
    if (test_project_host_live_controls())
        return 1;
    if (test_project_host_prepare_failure_keeps_active())
        return 1;
    if (test_project_host_commit_swap_crossfades())
        return 1;
    if (test_project_host_swap_preserves_bypass())
        return 1;
    return test_project_host_swap_preserves_param();
}
