#include <apgcore/compiler/compiler_v2.h>
#include <apgcore/measure/atom_cost.h>
#include <apgcore/measure/measure_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/validator/unit_v2.h>
#include <atom/dsp_types.h>

#include "test_runtime_v2_harness.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int
load_compile_runtime(uc_arena *arena, apg_unit_v2_t *unit, apg_v2_compiled_unit_t *plan, apg_v2_runtime_t *runtime) {
    uc_error  err    = {0};
    uc_status status = apg_unit_v2_load_file("test/fixtures/units-v2/simple_gain.unit.v2.yaml", arena, unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "load error: %s\n", err.msg);
        return fail("failed to load fixture");
    }
    status = apg_v2_compile_unit(unit, arena, plan, &err);
    if (status != UC_OK) {
        fprintf(stderr, "compile error: %s\n", err.msg);
        return fail("failed to compile fixture");
    }
    status = test_apg_v2_runtime_init_registry(plan, 8u, 48000.0f, arena, runtime, &err);
    if (status != UC_OK) {
        fprintf(stderr, "runtime error: %s\n", err.msg);
        return fail("failed to initialize runtime");
    }
    return 0;
}

static int test_measure_snapshot_and_meters(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_v2_compiled_unit_t plan;
    apg_unit_v2_t          unit;
    apg_v2_runtime_t       runtime;
    if (load_compile_runtime(&arena, &unit, &plan, &runtime)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_measure_runtime_snapshot_t snapshot;
    if (!apg_v2_measure_runtime_snapshot(&runtime, &snapshot))
        return fail("measure snapshot failed");
    if (snapshot.frame_capacity != 8u || snapshot.sample_rate != 48000.0f || snapshot.signals_len != 3u ||
        snapshot.params_len != 1u || snapshot.nodes_len != 2u || snapshot.input_meters_len != 1u ||
        snapshot.output_meters_len != 1u || snapshot.has_processed)
        return fail("unexpected measure snapshot before process");

    apg_v2_meter_snapshot_t meter;
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) || meter.valid)
        return fail("unexpected initial output meter");

    float input[4]  = {0.25f, -0.5f, 0.75f, -1.0f};
    float output[4] = {0};
    if (!test_runtime_process_mono_ports(&runtime, "input", input, "output", output, 4u))
        return fail("runtime process failed");

    if (!apg_v2_measure_runtime_snapshot(&runtime, &snapshot) || !snapshot.has_processed)
        return fail("measure snapshot did not report processed state");
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) || !meter.valid || meter.frames != 4u ||
        fabsf(meter.rms - 0.6846532f) > 0.00001f)
        return fail("unexpected output meter after process");
    if (apg_v2_measure_get_output_meter(&runtime, "missing", 0u, &meter))
        return fail("measure accepted missing meter");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_measure_last_error(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_v2_compiled_unit_t plan;
    apg_unit_v2_t          unit;
    apg_v2_runtime_t       runtime;
    if (load_compile_runtime(&arena, &unit, &plan, &runtime)) {
        uc_arena_free(&arena);
        return 1;
    }

    float input[4]  = {0};
    float output[4] = {0};
    if (test_runtime_process_mono_ports(&runtime, "missing", input, "output", output, 4u))
        return fail("runtime accepted missing input port");
    const char *error = apg_v2_measure_last_error(&runtime);
    if (!error || !strstr(error, "input audio port"))
        return fail("measure did not expose runtime error");

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int test_measure_snapshot_is_non_mutating(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_v2_compiled_unit_t plan;
    apg_unit_v2_t          unit;
    apg_v2_runtime_t       runtime;
    if (load_compile_runtime(&arena, &unit, &plan, &runtime)) {
        uc_arena_free(&arena);
        return 1;
    }

    apg_v2_measure_runtime_snapshot_t snapshot_before;
    if (!apg_v2_measure_runtime_snapshot(&runtime, &snapshot_before)) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("initial snapshot failed");
    }

    float input[4]  = {0.25f, -0.5f, 0.75f, -1.0f};
    float output[4] = {0};
    if (test_runtime_process_mono_ports(&runtime, "missing", input, "output", output, 4u)) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("runtime accepted missing input port");
    }

    const char *runtime_error = apg_v2_measure_last_error(&runtime);
    if (!runtime_error || !strstr(runtime_error, "input audio port")) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("runtime error message not set");
    }

    apg_v2_measure_runtime_snapshot_t snapshot_after_error;
    if (!apg_v2_measure_runtime_snapshot(&runtime, &snapshot_after_error)) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("snapshot failed after runtime error");
    }

    apg_v2_meter_snapshot_t meter;
    if (!apg_v2_measure_get_output_meter(&runtime, "output", 0u, &meter) || meter.valid) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("measure should report invalid output meter");
    }

    const char *metric = apg_v2_measure_last_error(&runtime);
    if (!metric || strcmp(metric, runtime_error) != 0) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("measure path changed runtime error text");
    }

    if (snapshot_before.frame_capacity != snapshot_after_error.frame_capacity ||
        snapshot_before.sample_rate != snapshot_after_error.sample_rate ||
        snapshot_before.signals_len != snapshot_after_error.signals_len ||
        snapshot_before.params_len != snapshot_after_error.params_len ||
        snapshot_before.nodes_len != snapshot_after_error.nodes_len ||
        snapshot_before.input_meters_len != snapshot_after_error.input_meters_len ||
        snapshot_before.output_meters_len != snapshot_after_error.output_meters_len ||
        snapshot_before.project_muted != snapshot_after_error.project_muted) {
        apg_v2_runtime_destroy(&runtime);
        uc_arena_free(&arena);
        return fail("snapshot metadata changed across non-mutating reads");
    }

    apg_v2_runtime_destroy(&runtime);
    uc_arena_free(&arena);
    return 0;
}

static int estimate_cost(
    const char *name,
    const void *config,
    uint32_t frames,
    const apg_spectral_info_t *spectral,
    apg_atom_cost_result_t *out
) {
    const atom_registry_entry_t *entry = atom_registry_find(name);
    if (!entry)
        return fail("cost model atom is missing from registry");
    apg_process_info_t info = {.sample_rate = 48000.0f, .frames = frames, .output_frames = frames, .channels = 1u};
    if (!apg_atom_estimate_cost(entry, config, &info, spectral, out))
        return fail("atom cost estimate failed");
    return 0;
}

static int test_atom_cost_model(void) {
    atom_registry_init();

    apg_atom_cost_result_t add_64;
    apg_atom_cost_result_t add_128;
    if (estimate_cost("amplitude_add", NULL, 64u, NULL, &add_64) ||
        estimate_cost("amplitude_add", NULL, 128u, NULL, &add_128))
        return 1;
    if (add_128.cpu_acu <= add_64.cpu_acu)
        return fail("cost model is not monotonic by frame count");
    if (add_128.cost_class != apg_cost_classify(add_128.cpu_acu))
        return fail("cost class is inconsistent");
    if (strcmp(apg_cost_class_name(APG_COST_TRIVIAL), "trivial") != 0 ||
        strcmp(apg_cost_class_name(APG_COST_EXTREME), "extreme") != 0)
        return fail("cost class names are invalid");

    float kernel[128] = {0};
    filter_fir_params_t fir_16_params  = {.kernel = kernel, .kernel_size = 16};
    filter_fir_params_t fir_128_params = {.kernel = kernel, .kernel_size = 128};
    apg_atom_cost_result_t fir_16;
    apg_atom_cost_result_t fir_128;
    if (estimate_cost("filter_fir", &fir_16_params, 128u, NULL, &fir_16) ||
        estimate_cost("filter_fir", &fir_128_params, 128u, NULL, &fir_128))
        return 1;
    if (fir_128.cpu_acu <= fir_16.cpu_acu)
        return fail("FIR cost does not increase with tap count");
    if (fir_128.latency_frames != 63u || fir_128.persistent_bytes == 0u)
        return fail("FIR latency or memory estimate is invalid");

    interpolation_lagrange_params_t lagrange_2_params = {.order = 2};
    interpolation_lagrange_params_t lagrange_8_params = {.order = 8};
    apg_atom_cost_result_t lagrange_2;
    apg_atom_cost_result_t lagrange_8;
    if (estimate_cost("interpolation_lagrange", &lagrange_2_params, 128u, NULL, &lagrange_2) ||
        estimate_cost("interpolation_lagrange", &lagrange_8_params, 128u, NULL, &lagrange_8))
        return 1;
    if (lagrange_8.cpu_acu <= lagrange_2.cpu_acu)
        return fail("Lagrange cost does not increase with order");

    apg_spectral_info_t fft_256  = {.fft_size = 256u, .bin_count = 129u, .hop_size = 256u};
    apg_spectral_info_t fft_2048 = {.fft_size = 2048u, .bin_count = 1025u, .hop_size = 2048u};
    apg_atom_cost_result_t fft_small;
    apg_atom_cost_result_t fft_large;
    if (estimate_cost("freq_fft", NULL, 128u, &fft_256, &fft_small) ||
        estimate_cost("freq_fft", NULL, 128u, &fft_2048, &fft_large))
        return 1;
    if (fft_large.cpu_acu <= fft_small.cpu_acu || fft_large.scratch_bytes <= fft_small.scratch_bytes)
        return fail("FFT cost does not scale with transform size");

    apg_spectral_info_t overlap = {.fft_size = 1024u, .bin_count = 513u, .hop_size = 256u};
    apg_atom_cost_result_t overlap_cost;
    if (estimate_cost("freq_overlap_add", NULL, 128u, &overlap, &overlap_cost))
        return 1;
    if (overlap_cost.latency_frames != 768u)
        return fail("overlap latency estimate is invalid");

    delay_line_params_t delay_params = {.length = 96};
    const atom_registry_entry_t *entries[] = {
        atom_registry_find("amplitude_add"),
        atom_registry_find("filter_fir"),
        atom_registry_find("delay_line"),
    };
    const void *configs[] = {NULL, &fir_16_params, &delay_params};
    apg_process_info_t info = {.sample_rate = 48000.0f, .frames = 128u, .output_frames = 128u, .channels = 1u};
    apg_graph_cost_result_t graph;
    if (!apg_graph_estimate_cost(entries, configs, NULL, 3u, &info, &graph))
        return fail("graph cost estimate failed");
    if (graph.atom_count != 3u || graph.cpu_acu <= fir_16.cpu_acu || graph.persistent_bytes == 0u)
        return fail("graph cost aggregation is invalid");
    if (graph.latency_frames != fir_16.latency_frames + 96u)
        return fail("graph conservative latency is invalid");

    return 0;
}

int main(void) {
    if (test_measure_snapshot_and_meters())
        return 1;
    if (test_measure_last_error())
        return 1;
    if (test_measure_snapshot_is_non_mutating())
        return 1;
    return test_atom_cost_model();
}
