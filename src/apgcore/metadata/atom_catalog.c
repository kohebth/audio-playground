#include <apgcore/metadata/atom_catalog.h>

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
    FIELD("b0", APG_ATOM_FIELD_FLOAT), FIELD("b1", APG_ATOM_FIELD_FLOAT), FIELD("b2", APG_ATOM_FIELD_FLOAT),
    FIELD("a1", APG_ATOM_FIELD_FLOAT), FIELD("a2", APG_ATOM_FIELD_FLOAT),
};
static const apg_atom_contract_field_t field_filter_biquad_input[] = {
    FIELD("signal", APG_ATOM_FIELD_SIGNAL),
    FIELD_OPT("cutoff", APG_ATOM_FIELD_SIGNAL_OPTIONAL),
};
static const apg_atom_contract_field_t field_filter_biquad_config[] = {
    FIELD("cutoff", APG_ATOM_FIELD_FLOAT),       FIELD("q", APG_ATOM_FIELD_FLOAT),
    FIELD("mode", APG_ATOM_FIELD_INT),           FIELD_OPT("sample_rate", APG_ATOM_FIELD_FLOAT),
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
    {             "generation_dc",                       NULL,                                       0,        field_signal,FIELD_COUNT(field_signal),field_generation_dc_config,
     FIELD_COUNT(field_generation_dc_config)                                                                                                                                                                                                    },
    {            "generation_lfo",                       NULL,                                       0,        field_signal,        FIELD_COUNT(field_signal), field_generation_lfo_config,
     FIELD_COUNT(field_generation_lfo_config)                                                                                                                                                                                                   },
    {        "amplitude_multiply",                 field_pair,                 FIELD_COUNT(field_pair),        field_signal,        FIELD_COUNT(field_signal),                        NULL,                                                    0},
    {             "amplitude_add",                 field_pair,                 FIELD_COUNT(field_pair),        field_signal,        FIELD_COUNT(field_signal),                        NULL,                                                    0},
    {        "amplitude_subtract",                 field_pair,                 FIELD_COUNT(field_pair),        field_signal,        FIELD_COUNT(field_signal),                        NULL,                                                    0},
    {       "amplitude_clip_hard",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_clip_hard_config,                  FIELD_COUNT(field_clip_hard_config)                                                                                                                                                               },
    {       "amplitude_clip_soft",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_clip_soft_config,                  FIELD_COUNT(field_clip_soft_config)                                                                                                                                                               },
    {                "delay_unit",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),                        NULL,                                                    0},
    {                "delay_line",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_delay_line_config,                 FIELD_COUNT(field_delay_line_config)                                                                                                                                                              },
    {          "delay_fractional",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_delay_fractional_config,           FIELD_COUNT(field_delay_fractional_config)                                                                                                                                                        },
    {        "delay_tap_feedback",      field_delay_tap_input,      FIELD_COUNT(field_delay_tap_input),        field_signal,
     FIELD_COUNT(field_signal),      field_delay_tap_config,                  FIELD_COUNT(field_delay_tap_config)                                                                                                                               },
    {     "delay_tap_feedforward",      field_delay_tap_input,      FIELD_COUNT(field_delay_tap_input),        field_signal,
     FIELD_COUNT(field_signal),      field_delay_tap_config,                  FIELD_COUNT(field_delay_tap_config)                                                                                                                               },
    {"filter_biquad_coefficients",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_filter_biquad_coefficients_config, FIELD_COUNT(field_filter_biquad_coefficients_config)                                                                                                                                              },
    {             "filter_biquad",  field_filter_biquad_input,  FIELD_COUNT(field_filter_biquad_input),        field_signal,
     FIELD_COUNT(field_signal),  field_filter_biquad_config,              FIELD_COUNT(field_filter_biquad_config)                                                                                                                               },
    {            "filter_allpass",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_filter_delay_config,               FIELD_COUNT(field_filter_delay_config)                                                                                                                                                            },
    {            "filter_comb_ff",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_filter_delay_config,               FIELD_COUNT(field_filter_delay_config)                                                                                                                                                            },
    {            "filter_comb_fb", field_filter_comb_fb_input, FIELD_COUNT(field_filter_comb_fb_input),        field_signal,
     FIELD_COUNT(field_signal),   field_filter_delay_config,               FIELD_COUNT(field_filter_delay_config)                                                                                                                               },
    {           "filter_dc_block",               field_signal,               FIELD_COUNT(field_signal),        field_signal,        FIELD_COUNT(field_signal),
     field_filter_dc_block_config,            FIELD_COUNT(field_filter_dc_block_config)                                                                                                                                                         },
    {          "detect_threshold",               field_signal,               FIELD_COUNT(field_signal),          field_gate,          FIELD_COUNT(field_gate),
     field_threshold_config,                  FIELD_COUNT(field_threshold_config)                                                                                                                                                               },
    {      "modulation_amplitude",     field_signal_modulator,     FIELD_COUNT(field_signal_modulator),        field_signal,
     FIELD_COUNT(field_signal),          field_depth_config,                      FIELD_COUNT(field_depth_config)                                                                                                                               },
    {      "modulation_frequency",     field_signal_modulator,     FIELD_COUNT(field_signal_modulator),        field_signal,
     FIELD_COUNT(field_signal),          field_depth_config,                      FIELD_COUNT(field_depth_config)                                                                                                                               },
    {          "modulation_phase",     field_signal_modulator,     FIELD_COUNT(field_signal_modulator),        field_signal,
     FIELD_COUNT(field_signal),          field_depth_config,                      FIELD_COUNT(field_depth_config)                                                                                                                               },
    {           "modulation_ring",     field_signal_modulator,     FIELD_COUNT(field_signal_modulator),        field_signal,
     FIELD_COUNT(field_signal),                        NULL,                                                    0                                                                                                                               },
    {          "modulation_scrub",          field_scrub_input,          FIELD_COUNT(field_scrub_input),        field_signal,        FIELD_COUNT(field_signal),
     field_scrub_config,                      FIELD_COUNT(field_scrub_config)                                                                                                                                                                   },
    {             "mix_crossfade",                 field_pair,                 FIELD_COUNT(field_pair),        field_signal,        FIELD_COUNT(field_signal),
     field_crossfade_config,                  FIELD_COUNT(field_crossfade_config)                                                                                                                                                               },
    {               "mix_wet_dry",        field_wet_dry_input,        FIELD_COUNT(field_wet_dry_input),        field_signal,        FIELD_COUNT(field_signal),
     field_wet_dry_config,                    FIELD_COUNT(field_wet_dry_config)                                                                                                                                                                 },
    {                "mix_matrix",        field_mix_matrix_io,        FIELD_COUNT(field_mix_matrix_io), field_mix_matrix_io,
     FIELD_COUNT(field_mix_matrix_io),     field_mix_matrix_config,                 FIELD_COUNT(field_mix_matrix_config)                                                                                                                        },
    {            "mix_pan_stereo",               field_signal,               FIELD_COUNT(field_signal), field_stereo_output, FIELD_COUNT(field_stereo_output),
     field_pan_config,                        FIELD_COUNT(field_pan_config)                                                                                                                                                                     },
    {             "mix_encode_ms",         field_stereo_input,         FIELD_COUNT(field_stereo_input),     field_ms_output,
     FIELD_COUNT(field_ms_output),                        NULL,                                                    0                                                                                                                            },
    {             "mix_decode_ms",             field_ms_input,             FIELD_COUNT(field_ms_input), field_stereo_output,
     FIELD_COUNT(field_stereo_output),                        NULL,                                                    0                                                                                                                        },
};

static const apg_catalog_contract_t *find_contract(const char *atom_name) {
    for (size_t i = 0; i < FIELD_COUNT(catalog_contracts); i++) {
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
