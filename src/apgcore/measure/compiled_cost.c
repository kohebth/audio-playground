#include <apgcore/measure/atom_cost.h>

#include <limits.h>
#include <string.h>

static uint64_t sat_add_u64(uint64_t a, uint64_t b) {
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}

bool apg_compiled_unit_estimate_cost(
    const apg_v2_compiled_unit_t *compiled,
    const apg_process_info_t *process_info,
    apg_graph_cost_result_t *out
) {
    if (!compiled || !out)
        return false;
    if (compiled->schedule_len > 0u && !compiled->schedule)
        return false;
    if (compiled->nodes_len > 0u && !compiled->nodes)
        return false;

    memset(out, 0, sizeof(*out));

    const size_t count = compiled->schedule_len > 0u ? compiled->schedule_len : compiled->nodes_len;
    for (size_t position = 0u; position < count; ++position) {
        const size_t node_index = compiled->schedule_len > 0u ? compiled->schedule[position] : position;
        if (node_index >= compiled->nodes_len)
            return false;

        const apg_v2_compiled_node_t *node = &compiled->nodes[node_index];
        const atom_registry_entry_t *entry = node->atom ? node->atom : atom_registry_find(node->atom_name);
        if (!entry)
            return false;

        apg_atom_cost_result_t atom_cost;
        const apg_spectral_info_t *spectral = node->has_spectral_info ? &node->spectral_info : NULL;
        if (!apg_atom_estimate_cost(entry, NULL, process_info, spectral, &atom_cost))
            return false;

        out->cpu_acu = sat_add_u64(out->cpu_acu, atom_cost.cpu_acu);
        out->persistent_bytes = sat_add_u64(out->persistent_bytes, atom_cost.persistent_bytes);
        if (atom_cost.scratch_bytes > out->scratch_bytes)
            out->scratch_bytes = atom_cost.scratch_bytes;
        if (UINT32_MAX - out->latency_frames < atom_cost.latency_frames)
            out->latency_frames = UINT32_MAX;
        else
            out->latency_frames += atom_cost.latency_frames;
        out->atom_count++;
    }

    out->cost_class = apg_cost_classify(out->cpu_acu);
    return true;
}
