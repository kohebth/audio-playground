#include <apgcore/metadata/atom_catalog.h>

#include <atom/atom_capability.h>
#include <atom_registry.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char                      *atom;
    const apg_atom_contract_field_t *inputs;
    size_t                           inputs_len;
    const apg_atom_contract_field_t *outputs;
    size_t                           outputs_len;
    const apg_atom_contract_field_t *config;
    size_t                           config_len;
} apg_catalog_contract_t;

#define FIELD_COUNT(fields) (sizeof(fields) / sizeof((fields)[0]))
#define FIELD(name, type) \
    { name, type, true }
#define FIELD_OPT(name, type) \
    { name, type, false }

static const apg_atom_contract_field_t field_signal[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
};
static const apg_atom_contract_field_t field_pair[] = {
    FIELD("signal_a", APG_ATOM_FIELD_SIGNAL),
    FIELD("signal_b", APG_ATOM_FIELD_SIGNAL),
};
static const apg_atom_contract_field_t field_generation_dc_config[] = {
    FIELD("value", APG_ATOM_FIELD_SCALAR),
};
static const apg_atom_contract_field_t field_generation_lfo_config[] = {
    FIELD("frequency", APG_ATOM_FIELD_FLOAT),
    FIELD("waveform", APG_ATOM_FIELD_INT),
    FIELD("phase_offset", APG_ATOM_FIELD_FLOAT),
    FIELD_OPT("sample_rate", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_gate[] = {
    FIELD("gate", APG_ATOM_FIELD_SIGNAL),
};
static const apg_atom_contract_field_t field_threshold_config[] = {
    FIELD("threshold", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_clip_hard_config[] = {
    FIELD("threshold", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_clip_soft_config[] = {
    FIELD("threshold", APG_ATOM_FIELD_FLOAT),
    FIELD("curve", APG_ATOM_FIELD_INT),
};
static const apg_atom_contract_field_t field_delay_line_config[] = {
    FIELD("length", APG_ATOM_FIELD_INT),
};
static const apg_atom_contract_field_t field_delay_fractional_config[] = {
    FIELD("delay_samples", APG_ATOM_FIELD_FLOAT),
    FIELD("interpolation", APG_ATOM_FIELD_INT),
};
static const apg_atom_contract_field_t field_delay_tap_input[] = {
    FIELD("buffer", APG_ATOM_FIELD_BUFFER),
    FIELD("tap_position", APG_ATOM_FIELD_SCALAR),
};
static const apg_atom_contract_field_t field_delay_tap_config[] = {
    FIELD("coefficient", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_filter_biquad_coefficients_config[] = {
    FIELD("b0", APG_ATOM_FIELD_FLOAT),
    FIELD("b1", APG_ATOM_FIELD_FLOAT),
    FIELD("b2", APG_ATOM_FIELD_FLOAT),
    FIELD("a1", APG_ATOM_FIELD_FLOAT),
    FIELD("a2", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_filter_biquad_input[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD_OPT("cutoff", APG_ATOM_FIELD_SIGNAL_OPTIONAL),
};
static const apg_atom_contract_field_t field_filter_biquad_config[] = {
    FIELD("cutoff", APG_ATOM_FIELD_FLOAT),
    FIELD("q", APG_ATOM_FIELD_FLOAT),
    FIELD("mode", APG_ATOM_FIELD_INT),
    FIELD_OPT("sample_rate", APG_ATOM_FIELD_FLOAT),
    FIELD("smoothing_ms", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_filter_delay_config[] = {
    FIELD("delay_samples", APG_ATOM_FIELD_INT),
    FIELD("coefficient", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_filter_comb_fb_input[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD_OPT("delay", APG_ATOM_FIELD_SIGNAL_OPTIONAL),
};
static const apg_atom_contract_field_t field_filter_dc_block_config[] = {
    FIELD("coefficient", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_signal_modulator[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD("modulator", APG_ATOM_FIELD_SIGNAL),
};
static const apg_atom_contract_field_t field_scrub_input[] = {
    FIELD("buffer", APG_ATOM_FIELD_BUFFER),
    FIELD("position", APG_ATOM_FIELD_SIGNAL),
};
static const apg_atom_contract_field_t field_depth_config[] = {
    FIELD("depth", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_scrub_config[] = {
    FIELD("buffer_size", APG_ATOM_FIELD_INT),
};
static const apg_atom_contract_field_t field_wet_dry_input[] = {
    FIELD("dry", APG_ATOM_FIELD_SIGNAL),
    FIELD("wet", APG_ATOM_FIELD_SIGNAL),
};
static const apg_atom_contract_field_t field_wet_dry_config[] = {
    FIELD("mix", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_mix_matrix_io[] = {
    FIELD("signals", APG_ATOM_FIELD_SIGNAL_ARRAY),
};
static const apg_atom_contract_field_t field_mix_matrix_config[] = {
    FIELD("coefficients", APG_ATOM_FIELD_FLOAT_MATRIX),
};
static const apg_atom_contract_field_t field_crossfade_config[] = {
    FIELD("t", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_stereo_input[] = {
    FIELD("left", APG_ATOM_FIELD_SIGNAL),
    FIELD("right", APG_ATOM_FIELD_SIGNAL),
};
static const apg_atom_contract_field_t field_stereo_output[] = {
    FIELD("left", APG_ATOM_FIELD_SIGNAL),
    FIELD("right", APG_ATOM_FIELD_SIGNAL),
};
static const apg_atom_contract_field_t field_ms_input[] = {
    FIELD("mid", APG_ATOM_FIELD_SIGNAL),
    FIELD("side", APG_ATOM_FIELD_SIGNAL),
};
static const apg_atom_contract_field_t field_ms_output[] = {
    FIELD("mid", APG_ATOM_FIELD_SIGNAL),
    FIELD("side", APG_ATOM_FIELD_SIGNAL),
};
static const apg_atom_contract_field_t field_pan_config[] = {
    FIELD("position", APG_ATOM_FIELD_FLOAT),
};

static const apg_catalog_contract_t catalog_contracts[] = {
    {"generation_dc", NULL, 0u, field_signal, FIELD_COUNT(field_signal), field_generation_dc_config,
     FIELD_COUNT(field_generation_dc_config)},
    {"generation_lfo", NULL, 0u, field_signal, FIELD_COUNT(field_signal), field_generation_lfo_config,
     FIELD_COUNT(field_generation_lfo_config)},
    {"amplitude_multiply", field_pair, FIELD_COUNT(field_pair), field_signal, FIELD_COUNT(field_signal), NULL, 0u},
    {"amplitude_add", field_pair, FIELD_COUNT(field_pair), field_signal, FIELD_COUNT(field_signal), NULL, 0u},
    {"amplitude_subtract", field_pair, FIELD_COUNT(field_pair), field_signal, FIELD_COUNT(field_signal), NULL, 0u},
    {"amplitude_clip_hard", field_signal, FIELD_COUNT(field_signal), field_signal, FIELD_COUNT(field_signal),
     field_clip_hard_config, FIELD_COUNT(field_clip_hard_config)},
    {"amplitude_clip_soft", field_signal, FIELD_COUNT(field_signal), field_signal, FIELD_COUNT(field_signal),
     field_clip_soft_config, FIELD_COUNT(field_clip_soft_config)},
    {"delay_unit", field_signal, FIELD_COUNT(field_signal), field_signal, FIELD_COUNT(field_signal), NULL, 0u},
    {"delay_line", field_signal, FIELD_COUNT(field_signal), field_signal, FIELD_COUNT(field_signal),
     field_delay_line_config, FIELD_COUNT(field_delay_line_config)},
    {"delay_fractional", field_signal, FIELD_COUNT(field_signal), field_signal, FIELD_COUNT(field_signal),
     field_delay_fractional_config, FIELD_COUNT(field_delay_fractional_config)},
    {"delay_tap_feedback", field_delay_tap_input, FIELD_COUNT(field_delay_tap_input), field_signal,
     FIELD_COUNT(field_signal), field_delay_tap_config, FIELD_COUNT(field_delay_tap_config)},
    {"delay_tap_feedforward", field_delay_tap_input, FIELD_COUNT(field_delay_tap_input), field_signal,
     FIELD_COUNT(field_signal), field_delay_tap_config, FIELD_COUNT(field_delay_tap_config)},
    {"filter_biquad_coefficients", field_signal, FIELD_COUNT(field_signal), field_signal, FIELD_COUNT(field_signal),
     field_filter_biquad_coefficients_config, FIELD_COUNT(field_filter_biquad_coefficients_config)},
    {"filter_biquad", field_filter_biquad_input, FIELD_COUNT(field_filter_biquad_input), field_signal,
     FIELD_COUNT(field_signal), field_filter_biquad_config, FIELD_COUNT(field_filter_biquad_config)},
    {"filter_allpass", field_signal, FIELD_COUNT(field_signal), field_signal, FIELD_COUNT(field_signal),
     field_filter_delay_config, FIELD_COUNT(field_filter_delay_config)},
    {"filter_comb_ff", field_signal, FIELD_COUNT(field_signal), field_signal, FIELD_COUNT(field_signal),
     field_filter_delay_config, FIELD_COUNT(field_filter_delay_config)},
    {"filter_comb_fb", field_filter_comb_fb_input, FIELD_COUNT(field_filter_comb_fb_input), field_signal,
     FIELD_COUNT(field_signal), field_filter_delay_config, FIELD_COUNT(field_filter_delay_config)},
    {"filter_dc_block", field_signal, FIELD_COUNT(field_signal), field_signal, FIELD_COUNT(field_signal),
     field_filter_dc_block_config, FIELD_COUNT(field_filter_dc_block_config)},
    {"detect_threshold", field_signal, FIELD_COUNT(field_signal), field_gate, FIELD_COUNT(field_gate),
     field_threshold_config, FIELD_COUNT(field_threshold_config)},
    {"modulation_amplitude", field_signal_modulator, FIELD_COUNT(field_signal_modulator), field_signal,
     FIELD_COUNT(field_signal), field_depth_config, FIELD_COUNT(field_depth_config)},
    {"modulation_frequency", field_signal_modulator, FIELD_COUNT(field_signal_modulator), field_signal,
     FIELD_COUNT(field_signal), field_depth_config, FIELD_COUNT(field_depth_config)},
    {"modulation_phase", field_signal_modulator, FIELD_COUNT(field_signal_modulator), field_signal,
     FIELD_COUNT(field_signal), field_depth_config, FIELD_COUNT(field_depth_config)},
    {"modulation_ring", field_signal_modulator, FIELD_COUNT(field_signal_modulator), field_signal,
     FIELD_COUNT(field_signal), NULL, 0u},
    {"modulation_scrub", field_scrub_input, FIELD_COUNT(field_scrub_input), field_signal, FIELD_COUNT(field_signal),
     field_scrub_config, FIELD_COUNT(field_scrub_config)},
    {"mix_crossfade", field_pair, FIELD_COUNT(field_pair), field_signal, FIELD_COUNT(field_signal),
     field_crossfade_config, FIELD_COUNT(field_crossfade_config)},
    {"mix_wet_dry", field_wet_dry_input, FIELD_COUNT(field_wet_dry_input), field_signal, FIELD_COUNT(field_signal),
     field_wet_dry_config, FIELD_COUNT(field_wet_dry_config)},
    {"mix_matrix", field_mix_matrix_io, FIELD_COUNT(field_mix_matrix_io), field_mix_matrix_io,
     FIELD_COUNT(field_mix_matrix_io), field_mix_matrix_config, FIELD_COUNT(field_mix_matrix_config)},
    {"mix_pan_stereo", field_signal, FIELD_COUNT(field_signal), field_stereo_output,
     FIELD_COUNT(field_stereo_output), field_pan_config, FIELD_COUNT(field_pan_config)},
    {"mix_encode_ms", field_stereo_input, FIELD_COUNT(field_stereo_input), field_ms_output,
     FIELD_COUNT(field_ms_output), NULL, 0u},
    {"mix_decode_ms", field_ms_input, FIELD_COUNT(field_ms_input), field_stereo_output,
     FIELD_COUNT(field_stereo_output), NULL, 0u},
};

static const apg_catalog_contract_t *find_contract(const char *atom_name) {
    for (size_t i = 0u; i < FIELD_COUNT(catalog_contracts); ++i) {
        if (strcmp(catalog_contracts[i].atom, atom_name) == 0)
            return &catalog_contracts[i];
    }
    return NULL;
}

static const apg_atom_contract_field_t *
contract_fields(const apg_catalog_contract_t *contract, apg_atom_contract_section_t section, size_t *out_len) {
    if (out_len)
        *out_len = 0u;
    if (!contract)
        return NULL;

    switch (section) {
    case APG_ATOM_CONTRACT_IN:
        if (out_len)
            *out_len = contract->inputs_len;
        return contract->inputs;
    case APG_ATOM_CONTRACT_OUT:
        if (out_len)
            *out_len = contract->outputs_len;
        return contract->outputs;
    case APG_ATOM_CONTRACT_CONFIG:
        if (out_len)
            *out_len = contract->config_len;
        return contract->config;
    case APG_ATOM_CONTRACT_STATE:
        return NULL;
    }
    return NULL;
}

static const char *contract_field_type_name(apg_atom_contract_field_type_t type) {
    switch (type) {
    case APG_ATOM_FIELD_SIGNAL:
        return "signal";
    case APG_ATOM_FIELD_SIGNAL_OPTIONAL:
        return "signal_optional";
    case APG_ATOM_FIELD_SIGNAL_ARRAY:
        return "signal_array";
    case APG_ATOM_FIELD_SCALAR:
        return "scalar";
    case APG_ATOM_FIELD_FLOAT:
        return "float";
    case APG_ATOM_FIELD_INT:
        return "int";
    case APG_ATOM_FIELD_BUFFER:
        return "buffer";
    case APG_ATOM_FIELD_FLOAT_MATRIX:
        return "float_matrix";
    case APG_ATOM_FIELD_UNKNOWN:
        break;
    }
    return "unknown";
}

static apg_atom_contract_field_type_t registry_field_type(atom_field_type_t type) {
    switch (type) {
    case FIELD_FLOAT:
        return APG_ATOM_FIELD_FLOAT;
    case FIELD_INT:
        return APG_ATOM_FIELD_INT;
    case FIELD_SIGNAL:
        return APG_ATOM_FIELD_SIGNAL;
    case FIELD_BUFFER:
        return APG_ATOM_FIELD_BUFFER;
    case FIELD_FLOAT_PTR:
        return APG_ATOM_FIELD_SIGNAL;
    case FIELD_FLOAT_PP:
        return APG_ATOM_FIELD_SIGNAL_ARRAY;
    }
    return APG_ATOM_FIELD_UNKNOWN;
}

static const char *field_type_name(atom_field_type_t type) {
    switch (type) {
    case FIELD_FLOAT:
        return "float";
    case FIELD_INT:
        return "int";
    case FIELD_SIGNAL:
        return "signal";
    case FIELD_BUFFER:
        return "buffer";
    case FIELD_FLOAT_PTR:
        return "float_ptr";
    case FIELD_FLOAT_PP:
        return "float_pp";
    }
    return "unknown";
}

bool apg_atom_profile_known(const char *profile) {
    return profile && (strcmp(profile, "desktop_full") == 0 || strcmp(profile, "wasm_realtime") == 0 ||
                       strcmp(profile, "m7_static") == 0 || strcmp(profile, "offline_render") == 0);
}

bool apg_atom_known(const char *name) {
    if (!name)
        return false;
    atom_registry_init();
    return atom_registry_find(name) != NULL;
}

bool apg_atom_profile_supported(const char *name, const char *profile) {
    if (!name || !apg_atom_profile_known(profile))
        return false;

    atom_registry_init();
    const atom_registry_entry_t *entry = atom_registry_find(name);
    if (!entry)
        return false;

    if (strcmp(profile, "desktop_full") == 0 || strcmp(profile, "offline_render") == 0)
        return true;
    if (strcmp(profile, "wasm_realtime") == 0)
        return (entry->flags & APG_ATOM_WASM_SAFE) != 0u;
    if (strcmp(profile, "m7_static") == 0)
        return (entry->flags & APG_ATOM_M7_SAFE) != 0u;
    return false;
}

size_t apg_atom_contract_field_count(const char *atom, apg_atom_contract_section_t section) {
    if (!atom)
        return 0u;
    if (section == APG_ATOM_CONTRACT_STATE) {
        atom_registry_init();
        const atom_registry_entry_t *entry = atom_registry_find(atom);
        return entry && entry->n_state_fields > 0 ? (size_t)entry->n_state_fields : 0u;
    }

    size_t fields_len = 0u;
    contract_fields(find_contract(atom), section, &fields_len);
    return fields_len;
}

bool apg_atom_contract_field(
    const char *atom, apg_atom_contract_section_t section, size_t index, apg_atom_contract_field_t *out
) {
    if (!atom || !out)
        return false;
    memset(out, 0, sizeof(*out));

    if (section == APG_ATOM_CONTRACT_STATE) {
        atom_registry_init();
        const atom_registry_entry_t *entry = atom_registry_find(atom);
        if (!entry || index >= (size_t)entry->n_state_fields)
            return false;
        out->name     = entry->state_fields[index].name;
        out->type     = registry_field_type(entry->state_fields[index].type);
        out->required = true;
        return true;
    }

    size_t                           fields_len = 0u;
    const apg_atom_contract_field_t *fields     = contract_fields(find_contract(atom), section, &fields_len);
    if (!fields || index >= fields_len)
        return false;
    *out = fields[index];
    return true;
}

bool apg_atom_contract_find_field(
    const char *atom, apg_atom_contract_section_t section, const char *key, apg_atom_contract_field_t *out
) {
    if (!key)
        return false;
    const size_t fields_len = apg_atom_contract_field_count(atom, section);
    for (size_t i = 0u; i < fields_len; ++i) {
        apg_atom_contract_field_t field;
        if (apg_atom_contract_field(atom, section, i, &field) && field.name && strcmp(field.name, key) == 0) {
            if (out)
                *out = field;
            return true;
        }
    }
    return false;
}

bool apg_atom_contract_field_required(const char *atom, apg_atom_contract_section_t section, const char *key) {
    apg_atom_contract_field_t field;
    return apg_atom_contract_find_field(atom, section, key, &field) && field.required;
}

apg_atom_contract_field_type_t
apg_atom_contract_field_type(const char *atom, apg_atom_contract_section_t section, const char *key) {
    apg_atom_contract_field_t field;
    if (!apg_atom_contract_find_field(atom, section, key, &field))
        return APG_ATOM_FIELD_UNKNOWN;
    return field.type;
}

static void write_json_string(FILE *out, const char *text) {
    fputc('"', out);
    if (text) {
        for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
            if (*p == '"' || *p == '\\')
                fputc('\\', out);
            if (*p >= 0x20)
                fputc(*p, out);
        }
    }
    fputc('"', out);
}

static void write_catalog_fields(FILE *out, const apg_atom_contract_field_t *fields, size_t fields_len) {
    fputc('[', out);
    for (size_t i = 0u; i < fields_len; ++i) {
        if (i > 0u)
            fputc(',', out);
        fputs("{\"name\":", out);
        write_json_string(out, fields[i].name);
        fputs(",\"type\":", out);
        write_json_string(out, contract_field_type_name(fields[i].type));
        fputc('}', out);
    }
    fputc(']', out);
}

static void write_registry_fields(FILE *out, const atom_field_desc_t *fields, int fields_len) {
    fputc('[', out);
    for (int i = 0; i < fields_len; ++i) {
        if (i > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        write_json_string(out, fields[i].name);
        fputs(",\"type\":", out);
        write_json_string(out, field_type_name(fields[i].type));
        if (fields[i].type == FIELD_BUFFER)
            fprintf(out, ",\"buffer_samples\":%zu", fields[i].buffer_samples);
        fputc('}', out);
    }
    fputc(']', out);
}

void apg_atom_catalog_write_json(FILE *out) {
    if (!out)
        return;

    atom_registry_init();
    fputs("{\"schema\":\"apg.atom_catalog.v2\",\"atoms\":[", out);
    const int count = atom_registry_count();
    for (int i = 0; i < count; ++i) {
        const atom_registry_entry_t  *entry    = atom_registry_get(i);
        const apg_catalog_contract_t *contract = entry ? find_contract(entry->name) : NULL;
        if (!entry)
            continue;
        if (i > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        write_json_string(out, entry->name);
        fputs(",\"category\":", out);
        write_json_string(out, entry->category);
        fprintf(
            out, ",\"sizes\":{\"out\":%zu,\"in\":%zu,\"config\":%zu,\"state\":%zu}", entry->out_size,
            entry->in_size, entry->config_size, entry->state_size
        );
        fputs(",\"stateful\":", out);
        fputs(entry->state_size > 0u ? "true" : "false", out);
        fputs(",\"profiles\":{\"desktop_full\":", out);
        fputs(apg_atom_profile_supported(entry->name, "desktop_full") ? "true" : "false", out);
        fputs(",\"wasm_realtime\":", out);
        fputs(apg_atom_profile_supported(entry->name, "wasm_realtime") ? "true" : "false", out);
        fputs(",\"m7_static\":", out);
        fputs(apg_atom_profile_supported(entry->name, "m7_static") ? "true" : "false", out);
        fputs(",\"offline_render\":", out);
        fputs(apg_atom_profile_supported(entry->name, "offline_render") ? "true" : "false", out);
        fputc('}', out);
        fputs(",\"inputs\":", out);
        write_catalog_fields(out, contract ? contract->inputs : NULL, contract ? contract->inputs_len : 0u);
        fputs(",\"outputs\":", out);
        write_catalog_fields(out, contract ? contract->outputs : NULL, contract ? contract->outputs_len : 0u);
        fputs(",\"config\":", out);
        if (contract && contract->config_len > 0u)
            write_catalog_fields(out, contract->config, contract->config_len);
        else
            write_registry_fields(out, entry->config_fields, entry->n_config_fields);
        fputs(",\"state\":", out);
        write_registry_fields(out, entry->state_fields, entry->n_state_fields);
        fputc('}', out);
    }
    fputs("]}\n", out);
}
