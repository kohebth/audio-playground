#include <apgcore/atom_catalog.h>

#include <atom_registry.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *type;
} apg_catalog_field_t;

typedef struct {
    const char                *atom;
    const apg_catalog_field_t *inputs;
    size_t                     inputs_len;
    const apg_catalog_field_t *outputs;
    size_t                     outputs_len;
    const apg_catalog_field_t *config;
    size_t                     config_len;
} apg_catalog_contract_t;

#define FIELD_COUNT(fields) (sizeof(fields) / sizeof((fields)[0]))

static const apg_catalog_field_t field_signal[] = {
    {"signal", "signal"}
};
static const apg_catalog_field_t field_pair[] = {
    {"signal_a", "signal"},
    {"signal_b", "signal"}
};
static const apg_catalog_field_t field_generation_dc_config[] = {
    {"value", "scalar"}
};
static const apg_catalog_field_t field_generation_lfo_config[] = {
    {   "frequency", "float"},
    {    "waveform",   "int"},
    {"phase_offset", "float"},
    { "sample_rate", "float"}
};
static const apg_catalog_field_t field_gate[] = {
    {"gate", "signal"}
};
static const apg_catalog_field_t field_threshold_config[] = {
    {"threshold", "float"}
};
static const apg_catalog_field_t field_clip_hard_config[] = {
    {"threshold", "float"}
};
static const apg_catalog_field_t field_clip_soft_config[] = {
    {"threshold", "float"},
    {    "curve",   "int"}
};
static const apg_catalog_field_t field_delay_line_config[] = {
    {"length", "int"}
};
static const apg_catalog_field_t field_delay_fractional_config[] = {
    {"delay_samples", "float"},
    {"interpolation",   "int"}
};
static const apg_catalog_field_t field_delay_tap_input[] = {
    {      "buffer", "buffer"},
    {"tap_position", "scalar"}
};
static const apg_catalog_field_t field_delay_tap_config[] = {
    {"coefficient", "float"}
};
static const apg_catalog_field_t field_filter_biquad_config[] = {
    {"b0", "float"},
    {"b1", "float"},
    {"b2", "float"},
    {"a1", "float"},
    {"a2", "float"}
};
static const apg_catalog_field_t field_filter_delay_config[] = {
    {"delay_samples",   "int"},
    {  "coefficient", "float"}
};
static const apg_catalog_field_t field_filter_comb_fb_input[] = {
    {"signal",          "signal"},
    { "delay", "signal_optional"}
};
static const apg_catalog_field_t field_filter_dc_block_config[] = {
    {"coefficient", "float"}
};
static const apg_catalog_field_t field_signal_modulator[] = {
    {   "signal", "signal"},
    {"modulator", "signal"}
};
static const apg_catalog_field_t field_scrub_input[] = {
    {  "buffer", "buffer"},
    {"position", "signal"}
};
static const apg_catalog_field_t field_depth_config[] = {
    {"depth", "float"}
};
static const apg_catalog_field_t field_scrub_config[] = {
    {"buffer_size", "int"}
};
static const apg_catalog_field_t field_wet_dry_input[] = {
    {"dry", "signal"},
    {"wet", "signal"}
};
static const apg_catalog_field_t field_wet_dry_config[] = {
    {"mix", "float"}
};
static const apg_catalog_field_t field_mix_matrix_io[] = {
    {"signals", "signal_array"}
};
static const apg_catalog_field_t field_mix_matrix_config[] = {
    {"coefficients", "float_matrix"}
};
static const apg_catalog_field_t field_crossfade_config[] = {
    {"t", "float"}
};
static const apg_catalog_field_t field_stereo_input[] = {
    { "left", "signal"},
    {"right", "signal"}
};
static const apg_catalog_field_t field_stereo_output[] = {
    { "left", "signal"},
    {"right", "signal"}
};
static const apg_catalog_field_t field_ms_input[] = {
    { "mid", "signal"},
    {"side", "signal"}
};
static const apg_catalog_field_t field_ms_output[] = {
    { "mid", "signal"},
    {"side", "signal"}
};
static const apg_catalog_field_t field_pan_config[] = {
    {"position", "float"}
};

static const apg_catalog_contract_t catalog_contracts[] = {
    {        "generation_dc",                       NULL,                                       0,        field_signal,FIELD_COUNT(field_signal),field_generation_dc_config,
     FIELD_COUNT(field_generation_dc_config)                                                                                                                                                                                     },
    {       "generation_lfo",                       NULL,                                       0,        field_signal,        FIELD_COUNT(field_signal), field_generation_lfo_config,
     FIELD_COUNT(field_generation_lfo_config)                                                                                                                                                                                    },
    {   "amplitude_multiply",                 field_pair,                 FIELD_COUNT(field_pair),        field_signal,        FIELD_COUNT(field_signal),                        NULL,                                          0},
    {        "amplitude_add",                 field_pair,                 FIELD_COUNT(field_pair),        field_signal,        FIELD_COUNT(field_signal),                        NULL,                                          0},
    {   "amplitude_subtract",                 field_pair,                 FIELD_COUNT(field_pair),        field_signal,        FIELD_COUNT(field_signal),                        NULL,                                          0},
    {  "amplitude_clip_hard",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_clip_hard_config,        FIELD_COUNT(field_clip_hard_config)                                                                                                                                                          },
    {  "amplitude_clip_soft",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_clip_soft_config,        FIELD_COUNT(field_clip_soft_config)                                                                                                                                                          },
    {           "delay_unit",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),                        NULL,                                          0},
    {           "delay_line",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_delay_line_config,       FIELD_COUNT(field_delay_line_config)                                                                                                                                                         },
    {     "delay_fractional",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_delay_fractional_config, FIELD_COUNT(field_delay_fractional_config)                                                                                                                                                   },
    {   "delay_tap_feedback",      field_delay_tap_input,      FIELD_COUNT(field_delay_tap_input),        field_signal,
     FIELD_COUNT(field_signal),      field_delay_tap_config,        FIELD_COUNT(field_delay_tap_config)                                                                                                                          },
    {"delay_tap_feedforward",      field_delay_tap_input,      FIELD_COUNT(field_delay_tap_input),        field_signal,
     FIELD_COUNT(field_signal),      field_delay_tap_config,        FIELD_COUNT(field_delay_tap_config)                                                                                                                          },
    {        "filter_biquad",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_filter_biquad_config,    FIELD_COUNT(field_filter_biquad_config)                                                                                                                                                      },
    {       "filter_allpass",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_filter_delay_config,     FIELD_COUNT(field_filter_delay_config)                                                                                                                                                       },
    {       "filter_comb_ff",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_filter_delay_config,     FIELD_COUNT(field_filter_delay_config)                                                                                                                                                       },
    {       "filter_comb_fb", field_filter_comb_fb_input, FIELD_COUNT(field_filter_comb_fb_input),        field_signal,
     FIELD_COUNT(field_signal),   field_filter_delay_config,     FIELD_COUNT(field_filter_delay_config)                                                                                                                          },
    {      "filter_dc_block",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_filter_dc_block_config,  FIELD_COUNT(field_filter_dc_block_config)                                                                                                                                                    },
    {     "detect_threshold",               field_signal,               FIELD_COUNT(field_signal),          field_gate,          FIELD_COUNT(field_gate),
     field_threshold_config,        FIELD_COUNT(field_threshold_config)                                                                                                                                                          },
    { "modulation_amplitude",     field_signal_modulator,     FIELD_COUNT(field_signal_modulator),        field_signal,
     FIELD_COUNT(field_signal),          field_depth_config,            FIELD_COUNT(field_depth_config)                                                                                                                          },
    { "modulation_frequency",     field_signal_modulator,     FIELD_COUNT(field_signal_modulator),        field_signal,
     FIELD_COUNT(field_signal),          field_depth_config,            FIELD_COUNT(field_depth_config)                                                                                                                          },
    {     "modulation_phase",     field_signal_modulator,     FIELD_COUNT(field_signal_modulator),        field_signal,
     FIELD_COUNT(field_signal),          field_depth_config,            FIELD_COUNT(field_depth_config)                                                                                                                          },
    {      "modulation_ring",     field_signal_modulator,     FIELD_COUNT(field_signal_modulator),        field_signal,
     FIELD_COUNT(field_signal),                        NULL,                                          0                                                                                                                          },
    {     "modulation_scrub",          field_scrub_input,          FIELD_COUNT(field_scrub_input),        field_signal,        FIELD_COUNT(field_signal),
     field_scrub_config,            FIELD_COUNT(field_scrub_config)                                                                                                                                                              },
    {        "mix_crossfade",                 field_pair,                 FIELD_COUNT(field_pair),        field_signal,        FIELD_COUNT(field_signal),
     field_crossfade_config,        FIELD_COUNT(field_crossfade_config)                                                                                                                                                          },
    {          "mix_wet_dry",        field_wet_dry_input,        FIELD_COUNT(field_wet_dry_input),        field_signal,        FIELD_COUNT(field_signal),
     field_wet_dry_config,          FIELD_COUNT(field_wet_dry_config)                                                                                                                                                            },
    {           "mix_matrix",        field_mix_matrix_io,        FIELD_COUNT(field_mix_matrix_io), field_mix_matrix_io,
     FIELD_COUNT(field_mix_matrix_io),     field_mix_matrix_config,       FIELD_COUNT(field_mix_matrix_config)                                                                                                                   },
    {       "mix_pan_stereo",               field_signal,               FIELD_COUNT(field_signal), field_stereo_output, FIELD_COUNT(field_stereo_output),
     field_pan_config,              FIELD_COUNT(field_pan_config)                                                                                                                                                                },
    {        "mix_encode_ms",         field_stereo_input,         FIELD_COUNT(field_stereo_input),     field_ms_output,
     FIELD_COUNT(field_ms_output),                        NULL,                                          0                                                                                                                       },
    {        "mix_decode_ms",             field_ms_input,             FIELD_COUNT(field_ms_input), field_stereo_output,
     FIELD_COUNT(field_stereo_output),                        NULL,                                          0                                                                                                                   },
};

static const apg_catalog_contract_t *find_contract(const char *atom_name) {
    for (size_t i = 0; i < FIELD_COUNT(catalog_contracts); i++) {
        if (strcmp(catalog_contracts[i].atom, atom_name) == 0)
            return &catalog_contracts[i];
    }
    return NULL;
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

bool apg_atom_profile_supported(const char *name, const char *profile) {
    if (!name || !profile)
        return false;
    if (strcmp(profile, "desktop_full") == 0 || strcmp(profile, "offline_render") == 0)
        return true;
    if (strncmp(name, "freq_", 5) == 0)
        return false;
    if (strcmp(profile, "m7_static") == 0 && strncmp(name, "src_", 4) == 0)
        return false;
    return true;
}

static void write_json_string(FILE *out, const char *text) {
    fputc('"', out);
    if (text) {
        for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
            if (*p == '"' || *p == '\\')
                fputc('\\', out);
            if (*p >= 0x20)
                fputc(*p, out);
        }
    }
    fputc('"', out);
}

static void write_category(FILE *out, const char *name) {
    const char *sep = name ? strchr(name, '_') : NULL;
    fputc('"', out);
    if (name && sep && sep > name)
        fprintf(out, "%.*s", (int)(sep - name), name);
    else if (name)
        fputs(name, out);
    fputc('"', out);
}

static void write_catalog_fields(FILE *out, const apg_catalog_field_t *fields, size_t fields_len) {
    fputc('[', out);
    for (size_t i = 0; i < fields_len; i++) {
        if (i > 0u)
            fputc(',', out);
        fputs("{\"name\":", out);
        write_json_string(out, fields[i].name);
        fputs(",\"type\":", out);
        write_json_string(out, fields[i].type);
        fputc('}', out);
    }
    fputc(']', out);
}

static void write_registry_fields(FILE *out, const atom_field_desc_t *fields, int fields_len) {
    fputc('[', out);
    for (int i = 0; i < fields_len; i++) {
        if (i > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        write_json_string(out, fields[i].name);
        fputs(",\"type\":", out);
        write_json_string(out, field_type_name(fields[i].type));
        if (fields[i].type == FIELD_BUFFER) {
            fprintf(out, ",\"buffer_samples\":%zu", fields[i].buffer_samples);
        }
        fputc('}', out);
    }
    fputc(']', out);
}

void apg_atom_catalog_write_json(FILE *out) {
    if (!out)
        return;
    atom_registry_init();
    fputs("{\"schema\":\"apg.atom_catalog.v1\",\"atoms\":[", out);
    int count = atom_registry_count();
    for (int i = 0; i < count; i++) {
        const atom_registry_entry_t  *entry    = atom_registry_get(i);
        const apg_catalog_contract_t *contract = entry ? find_contract(entry->name) : NULL;
        if (!entry)
            continue;
        if (i > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        write_json_string(out, entry->name);
        fputs(",\"category\":", out);
        write_category(out, entry->name);
        fprintf(
            out, ",\"sizes\":{\"out\":%zu,\"in\":%zu,\"config\":%zu,\"state\":%zu}", entry->out_size, entry->in_size,
            entry->config_size, entry->state_size
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
