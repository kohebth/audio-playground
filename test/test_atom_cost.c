#include <apgcore/measure/atom_cost.h>

#include <atom/dsp_types.h>

#include <stdio.h>
#include <string.h>

static int fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static int estimate(
    const char *name,
    const void *config,
    uint32_t frames,
    const apg_spectral_info_t *spectral,
    apg_atom_cost_result_t *out
) {
    const atom_registry_entry_t *entry = atom_registry_find(name);
    if (!entry)
        return fail("atom is missing from registry");
    apg_process_info_t info = {.sample_rate = 48000.0f, .frames = frames, .output_frames = frames, .channels = 1u};
    if (!apg_atom_estimate_cost(entry, config, &info, spectral, out))
        return fail("atom cost estimate failed");
    return 0;
}

int main(void) {
    atom_registry_init();

    apg_atom_cost_result_t add_64;
    apg_atom_cost_result_t add_128;
    if (estimate("amplitude_add", NULL, 64u, NULL, &add_64) ||
        estimate("amplitude_add", NULL, 128u, NULL, &add_128))
        return 1;
    if (add_128.cpu_acu <= add_64.cpu_acu)
        return fail("per-frame cost is not monotonic");
    if (add_128.cost_class != apg_cost_classify(add_128.cpu_acu))
        return fail("cost classification is inconsistent");
    if (strcmp(apg_cost_class_name(APG_COST_TRIVIAL), "trivial") != 0 ||
        strcmp(apg_cost_class_name(APG_COST_EXTREME), "extreme") != 0)
        return fail("cost class names are invalid");

    float kernel[128] = {0};
    filter_fir_params_t fir_small = {.kernel = kernel, .kernel_size = 16};
    filter_fir_params_t fir_large = {.kernel = kernel, .kernel_size = 128};
    apg_atom_cost_result_t fir_16;
    apg_atom_cost_result_t fir_128;
    if (estimate("filter_fir", &fir_small, 128u, NULL, &fir_16) ||
        estimate("filter_fir", &fir_large, 128u, NULL, &fir_128))
        return 1;
    if (fir_128.cpu_acu <= fir_16.cpu_acu)
        return fail("FIR cost does not increase with taps");
    if (fir_128.latency_frames != 63u)
        return fail("FIR latency estimate is invalid");
    if (fir_128.persistent_bytes == 0u)
        return fail("FIR persistent memory is missing");

    interpolation_lagrange_params_t lagrange_2 = {.order = 2};
    interpolation_lagrange_params_t lagrange_8 = {.order = 8};
    apg_atom_cost_result_t lag_2;
    apg_atom_cost_result_t lag_8;
    if (estimate("interpolation_lagrange", &lagrange_2, 128u, NULL, &lag_2) ||
        estimate("interpolation_lagrange", &lagrange_8, 128u, NULL, &lag_8))
        return 1;
    if (lag_8.cpu_acu <= lag_2.cpu_acu)
        return fail("Lagrange cost does not increase with order");

    apg_spectral_info_t fft_256 = {.fft_size = 256u, .bin_count = 129u, .hop_size = 256u};
    apg_spectral_info_t fft_2048 = {.fft_size = 2048u, .bin_count = 1025u, .hop_size = 2048u};
    apg_atom_cost_result_t fft_small;
    apg_atom_cost_result_t fft_large;
    if (estimate("freq_fft", NULL, 128u, &fft_256, &fft_small) ||
        estimate("freq_fft", NULL, 128u, &fft_2048, &fft_large))
        return 1;
    if (fft_large.cpu_acu <= fft_small.cpu_acu || fft_large.scratch_bytes <= fft_small.scratch_bytes)
        return fail("FFT estimate does not scale with transform size");

    apg_spectral_info_t overlap = {.fft_size = 1024u, .bin_count = 513u, .hop_size = 256u};
    apg_atom_cost_result_t overlap_cost;
    if (estimate("freq_overlap_add", NULL, 128u, &overlap, &overlap_cost))
        return 1;
    if (overlap_cost.latency_frames != 768u)
        return fail("spectral overlap latency is invalid");

    delay_line_params_t delay = {.length = 96};
    const atom_registry_entry_t *entries[] = {
        atom_registry_find("amplitude_add"),
        atom_registry_find("filter_fir"),
        atom_registry_find("delay_line"),
    };
    const void *configs[] = {NULL, &fir_small, &delay};
    apg_process_info_t info = {.sample_rate = 48000.0f, .frames = 128u, .output_frames = 128u, .channels = 1u};
    apg_graph_cost_result_t graph;
    if (!apg_graph_estimate_cost(entries, configs, NULL, 3u, &info, &graph))
        return fail("graph estimate failed");
    if (graph.atom_count != 3u)
        return fail("graph atom count is invalid");
    if (graph.cpu_acu <= fir_16.cpu_acu)
        return fail("graph CPU cost was not aggregated");
    if (graph.latency_frames != fir_16.latency_frames + 96u)
        return fail("graph conservative latency is invalid");
    if (graph.persistent_bytes == 0u)
        return fail("graph persistent memory is missing");

    if (apg_atom_estimate_cost(NULL, NULL, &info, NULL, &add_64))
        return fail("missing atom estimate unexpectedly succeeded");

    return 0;
}
