#include <apgcore/compiler_v2.h>

#include <stdio.h>
#include <string.h>

typedef enum {
    APG_BIND_SECTION_IN,
    APG_BIND_SECTION_OUT,
    APG_BIND_SECTION_CONFIG,
} apg_bind_section_t;

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static const char *bind_section_name(apg_bind_section_t section) {
    switch (section) {
    case APG_BIND_SECTION_IN:
        return "in";
    case APG_BIND_SECTION_OUT:
        return "out";
    case APG_BIND_SECTION_CONFIG:
        return "config";
    }
    return "binding";
}

static const char *param_ref_name(const char *text) {
    if (!text)
        return NULL;
    return strncmp(text, "params.", 7) == 0 ? text + 7 : NULL;
}

static int find_signal_index(const apg_unit_v2_t *unit, const char *name) {
    if (!unit || !name)
        return -1;
    for (size_t i = 0; i < unit->signals_len; i++) {
        if (unit->signals[i] && strcmp(unit->signals[i], name) == 0)
            return (int)i;
    }
    return -1;
}

static int find_param_index(const apg_unit_v2_t *unit, const char *name) {
    if (!unit || !name)
        return -1;
    for (size_t i = 0; i < unit->params_len; i++) {
        if (unit->params[i].name && strcmp(unit->params[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

static int port_is_audio(const apg_unit_v2_port_t *port) {
    return port && port->type && strcmp(port->type, "audio") == 0;
}

static size_t port_signal_count(const apg_unit_v2_port_t *port) {
    return port && port->signals_len > 0u ? port->signals_len : 1u;
}

static const char *port_signal_name(const apg_unit_v2_port_t *port, size_t channel_index) {
    if (!port)
        return NULL;
    if (port->signals_len > 0u)
        return channel_index < port->signals_len ? port->signals[channel_index] : NULL;
    return channel_index == 0u ? port->name : NULL;
}

static uc_status mark_input_port_signals(const apg_unit_v2_t *unit, int *signal_available, uc_error *err) {
    for (size_t i = 0; i < unit->input_ports_len; i++) {
        if (!port_is_audio(&unit->input_ports[i]))
            continue;
        for (size_t ch = 0; ch < port_signal_count(&unit->input_ports[i]); ch++) {
            int signal_index = find_signal_index(unit, port_signal_name(&unit->input_ports[i], ch));
            if (signal_index < 0)
                return set_error(err, UC_E_MISSING, "input audio port is missing graph signal mapping");
            signal_available[signal_index] = 1;
        }
    }
    return UC_OK;
}

static int node_inputs_available(const apg_v2_compiled_node_t *node, const int *signal_available) {
    for (size_t i = 0; i < node->in_len; i++) {
        size_t signal_index = node->in[i].index;
        if (node->in[i].kind == APG_BIND_SIGNAL && !signal_available[signal_index])
            return 0;
    }
    return 1;
}

static void mark_node_outputs_available(const apg_v2_compiled_node_t *node, int *signal_available) {
    for (size_t i = 0; i < node->out_len; i++) {
        if (node->out[i].kind == APG_BIND_SIGNAL)
            signal_available[node->out[i].index] = 1;
    }
}

static uc_status record_node_output_producers(
    const apg_v2_compiled_node_t *node, uint32_t node_index, uint32_t *signal_producers, uc_error *err
) {
    for (size_t i = 0; i < node->out_len; i++) {
        if (node->out[i].kind != APG_BIND_SIGNAL)
            continue;
        size_t signal_index = node->out[i].index;
        if (signal_producers[signal_index] != UINT32_MAX) {
            char msg[160];
            snprintf(
                msg, sizeof(msg), "node '%s' out binding '%s' writes signal with multiple producers",
                node->id ? node->id : "", node->out[i].key ? node->out[i].key : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }
        signal_producers[signal_index] = node_index;
    }
    return UC_OK;
}

static uc_status
validate_output_port_signals_produced(const apg_unit_v2_t *unit, const int *signal_available, uc_error *err) {
    for (size_t i = 0; i < unit->output_ports_len; i++) {
        if (!port_is_audio(&unit->output_ports[i]))
            continue;
        for (size_t ch = 0; ch < port_signal_count(&unit->output_ports[i]); ch++) {
            int signal_index = find_signal_index(unit, port_signal_name(&unit->output_ports[i], ch));
            if (signal_index < 0)
                return set_error(err, UC_E_MISSING, "output audio port is missing graph signal mapping");
            if (!signal_available[signal_index])
                return set_error(err, UC_E_MISSING, "output audio port signal is not produced by the graph");
        }
    }
    return UC_OK;
}

static uc_status build_schedule(
    const apg_v2_compiled_node_t *nodes,
    size_t                        nodes_len,
    uint32_t                     *schedule,
    int                          *node_scheduled,
    int                          *signal_available,
    uc_error                     *err
) {
    size_t scheduled_len = 0;
    while (scheduled_len < nodes_len) {
        int progressed = 0;
        for (size_t i = 0; i < nodes_len; i++) {
            if (node_scheduled[i] || !node_inputs_available(&nodes[i], signal_available))
                continue;
            schedule[scheduled_len++] = (uint32_t)i;
            node_scheduled[i]         = 1;
            mark_node_outputs_available(&nodes[i], signal_available);
            progressed = 1;
        }
        if (!progressed)
            return set_error(err, UC_E_MISSING, "graph contains unresolved dependency or direct cycle");
    }
    return UC_OK;
}

static int key_in_list(const char *key, const char *const *keys, size_t keys_len) {
    for (size_t i = 0; i < keys_len; i++) {
        if (strcmp(key, keys[i]) == 0)
            return 1;
    }
    return 0;
}

typedef struct {
    const char        *atom;
    apg_bind_section_t section;
    const char *const *keys;
    size_t             keys_len;
} apg_atom_binding_schema_t;

static const apg_atom_binding_schema_t *find_binding_schema(const char *atom, apg_bind_section_t section) {
    static const char *const               generation_dc_out[]       = {"signal"};
    static const char *const               generation_dc_config[]    = {"value"};
    static const char *const               pair_in[]                 = {"signal_a", "signal_b"};
    static const char *const               mono_in[]                 = {"signal"};
    static const char *const               mono_out[]                = {"signal"};
    static const char *const               clip_hard_config[]        = {"threshold"};
    static const char *const               clip_soft_config[]        = {"threshold", "curve"};
    static const char *const               delay_line_config[]       = {"length"};
    static const char *const               delay_fractional_config[] = {"delay_samples", "interpolation"};
    static const char *const               filter_biquad_config[]    = {"b0", "b1", "b2", "a1", "a2"};
    static const char *const               filter_delay_config[]     = {"delay_samples", "coefficient"};
    static const char *const               filter_comb_fb_in[]       = {"signal", "delay"};
    static const char *const               filter_dc_block_config[]  = {"coefficient"};
    static const char *const               signal_modulator_in[]     = {"signal", "modulator"};
    static const char *const               scrub_in[]                = {"buffer", "position"};
    static const char *const               depth_config[]            = {"depth"};
    static const char *const               scrub_config[]            = {"buffer_size"};
    static const char *const               mix_wet_dry_in[]          = {"dry", "wet"};
    static const char *const               mix_wet_dry_config[]      = {"mix"};
    static const char *const               crossfade_config[]        = {"t"};
    static const char *const               stereo_in[]               = {"left", "right"};
    static const char *const               stereo_out[]              = {"left", "right"};
    static const char *const               ms_in[]                   = {"mid", "side"};
    static const char *const               ms_out[]                  = {"mid", "side"};
    static const char *const               pan_config[]              = {"position"};
    static const apg_atom_binding_schema_t schemas[]                 = {
        {       "generation_dc",    APG_BIND_SECTION_OUT,       generation_dc_out,
         sizeof(generation_dc_out) / sizeof(generation_dc_out[0])                                                                    },
        {       "generation_dc", APG_BIND_SECTION_CONFIG,    generation_dc_config,
         sizeof(generation_dc_config) / sizeof(generation_dc_config[0])                                                              },
        {  "amplitude_multiply",     APG_BIND_SECTION_IN,                 pair_in,               sizeof(pair_in) / sizeof(pair_in[0])},
        {  "amplitude_multiply",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {       "amplitude_add",     APG_BIND_SECTION_IN,                 pair_in,               sizeof(pair_in) / sizeof(pair_in[0])},
        {       "amplitude_add",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {  "amplitude_subtract",     APG_BIND_SECTION_IN,                 pair_in,               sizeof(pair_in) / sizeof(pair_in[0])},
        {  "amplitude_subtract",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        { "amplitude_clip_hard",     APG_BIND_SECTION_IN,                 mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        { "amplitude_clip_hard",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        { "amplitude_clip_hard", APG_BIND_SECTION_CONFIG,        clip_hard_config,
         sizeof(clip_hard_config) / sizeof(clip_hard_config[0])                                                                      },
        { "amplitude_clip_soft",     APG_BIND_SECTION_IN,                 mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        { "amplitude_clip_soft",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        { "amplitude_clip_soft", APG_BIND_SECTION_CONFIG,        clip_soft_config,
         sizeof(clip_soft_config) / sizeof(clip_soft_config[0])                                                                      },
        {          "delay_unit",     APG_BIND_SECTION_IN,                 mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        {          "delay_unit",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {          "delay_line",     APG_BIND_SECTION_IN,                 mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        {          "delay_line",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {          "delay_line", APG_BIND_SECTION_CONFIG,       delay_line_config,
         sizeof(delay_line_config) / sizeof(delay_line_config[0])                                                                    },
        {    "delay_fractional",     APG_BIND_SECTION_IN,                 mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        {    "delay_fractional",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {    "delay_fractional", APG_BIND_SECTION_CONFIG, delay_fractional_config,
         sizeof(delay_fractional_config) / sizeof(delay_fractional_config[0])                                                        },
        {       "filter_biquad",     APG_BIND_SECTION_IN,                 mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        {       "filter_biquad",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {       "filter_biquad", APG_BIND_SECTION_CONFIG,    filter_biquad_config,
         sizeof(filter_biquad_config) / sizeof(filter_biquad_config[0])                                                              },
        {      "filter_allpass",     APG_BIND_SECTION_IN,                 mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        {      "filter_allpass",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {      "filter_allpass", APG_BIND_SECTION_CONFIG,     filter_delay_config,
         sizeof(filter_delay_config) / sizeof(filter_delay_config[0])                                                                },
        {      "filter_comb_ff",     APG_BIND_SECTION_IN,                 mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        {      "filter_comb_ff",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {      "filter_comb_ff", APG_BIND_SECTION_CONFIG,     filter_delay_config,
         sizeof(filter_delay_config) / sizeof(filter_delay_config[0])                                                                },
        {      "filter_comb_fb",     APG_BIND_SECTION_IN,       filter_comb_fb_in,
         sizeof(filter_comb_fb_in) / sizeof(filter_comb_fb_in[0])                                                                    },
        {      "filter_comb_fb",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {      "filter_comb_fb", APG_BIND_SECTION_CONFIG,     filter_delay_config,
         sizeof(filter_delay_config) / sizeof(filter_delay_config[0])                                                                },
        {     "filter_dc_block",     APG_BIND_SECTION_IN,                 mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        {     "filter_dc_block",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {     "filter_dc_block", APG_BIND_SECTION_CONFIG,  filter_dc_block_config,
         sizeof(filter_dc_block_config) / sizeof(filter_dc_block_config[0])                                                          },
        {"modulation_amplitude",     APG_BIND_SECTION_IN,     signal_modulator_in,
         sizeof(signal_modulator_in) / sizeof(signal_modulator_in[0])                                                                },
        {"modulation_amplitude",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {"modulation_amplitude", APG_BIND_SECTION_CONFIG,            depth_config,     sizeof(depth_config) / sizeof(depth_config[0])},
        {     "modulation_ring",     APG_BIND_SECTION_IN,     signal_modulator_in,
         sizeof(signal_modulator_in) / sizeof(signal_modulator_in[0])                                                                },
        {     "modulation_ring",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {"modulation_frequency",     APG_BIND_SECTION_IN,     signal_modulator_in,
         sizeof(signal_modulator_in) / sizeof(signal_modulator_in[0])                                                                },
        {"modulation_frequency",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {"modulation_frequency", APG_BIND_SECTION_CONFIG,            depth_config,     sizeof(depth_config) / sizeof(depth_config[0])},
        {    "modulation_phase",     APG_BIND_SECTION_IN,     signal_modulator_in,
         sizeof(signal_modulator_in) / sizeof(signal_modulator_in[0])                                                                },
        {    "modulation_phase",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {    "modulation_phase", APG_BIND_SECTION_CONFIG,            depth_config,     sizeof(depth_config) / sizeof(depth_config[0])},
        {    "modulation_scrub",     APG_BIND_SECTION_IN,                scrub_in,             sizeof(scrub_in) / sizeof(scrub_in[0])},
        {    "modulation_scrub",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {    "modulation_scrub", APG_BIND_SECTION_CONFIG,            scrub_config,     sizeof(scrub_config) / sizeof(scrub_config[0])},
        {       "mix_crossfade",     APG_BIND_SECTION_IN,                 pair_in,               sizeof(pair_in) / sizeof(pair_in[0])},
        {       "mix_crossfade",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {       "mix_crossfade", APG_BIND_SECTION_CONFIG,        crossfade_config,
         sizeof(crossfade_config) / sizeof(crossfade_config[0])                                                                      },
        {      "mix_pan_stereo",     APG_BIND_SECTION_IN,                 mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        {      "mix_pan_stereo",    APG_BIND_SECTION_OUT,              stereo_out,         sizeof(stereo_out) / sizeof(stereo_out[0])},
        {      "mix_pan_stereo", APG_BIND_SECTION_CONFIG,              pan_config,         sizeof(pan_config) / sizeof(pan_config[0])},
        {       "mix_encode_ms",     APG_BIND_SECTION_IN,               stereo_in,           sizeof(stereo_in) / sizeof(stereo_in[0])},
        {       "mix_encode_ms",    APG_BIND_SECTION_OUT,                  ms_out,                 sizeof(ms_out) / sizeof(ms_out[0])},
        {       "mix_decode_ms",     APG_BIND_SECTION_IN,                   ms_in,                   sizeof(ms_in) / sizeof(ms_in[0])},
        {       "mix_decode_ms",    APG_BIND_SECTION_OUT,              stereo_out,         sizeof(stereo_out) / sizeof(stereo_out[0])},
        {         "mix_wet_dry",     APG_BIND_SECTION_IN,          mix_wet_dry_in, sizeof(mix_wet_dry_in) / sizeof(mix_wet_dry_in[0])},
        {         "mix_wet_dry",    APG_BIND_SECTION_OUT,                mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {         "mix_wet_dry", APG_BIND_SECTION_CONFIG,      mix_wet_dry_config,
         sizeof(mix_wet_dry_config) / sizeof(mix_wet_dry_config[0])                                                                  },
    };

    if (!atom)
        return NULL;
    for (size_t i = 0; i < sizeof(schemas) / sizeof(schemas[0]); i++) {
        if (schemas[i].section == section && strcmp(schemas[i].atom, atom) == 0)
            return &schemas[i];
    }
    return NULL;
}

static int atom_has_schema(const char *atom) {
    return find_binding_schema(atom, APG_BIND_SECTION_IN) || find_binding_schema(atom, APG_BIND_SECTION_OUT) ||
           find_binding_schema(atom, APG_BIND_SECTION_CONFIG);
}

static int atom_binding_key_allowed(const char *atom, apg_bind_section_t section, const char *key) {
    const apg_atom_binding_schema_t *schema = find_binding_schema(atom, section);
    if (schema)
        return key && key_in_list(key, schema->keys, schema->keys_len);
    return !atom_has_schema(atom);
}

static int atom_binding_key_optional(const char *atom, apg_bind_section_t section, const char *key) {
    return atom && key && section == APG_BIND_SECTION_IN && strcmp(atom, "filter_comb_fb") == 0 &&
           strcmp(key, "delay") == 0;
}

static uc_status validate_binding_key(
    const char *node_id, const char *atom, apg_bind_section_t section, const char *key, uc_error *err
) {
    if (atom_binding_key_allowed(atom, section, key))
        return UC_OK;

    char msg[192];
    snprintf(
        msg, sizeof(msg), "node '%s' atom '%s' %s binding key '%s' is not accepted", node_id ? node_id : "",
        atom ? atom : "", bind_section_name(section), key ? key : ""
    );
    return set_error(err, UC_E_MISSING, msg);
}

static int binding_key_present(const apg_unit_v2_binding_t *bindings, size_t bindings_len, const char *key) {
    for (size_t i = 0; i < bindings_len; i++) {
        if (bindings[i].key && strcmp(bindings[i].key, key) == 0)
            return 1;
    }
    return 0;
}

static uc_status validate_required_binding_keys(
    const char                  *node_id,
    const char                  *atom,
    apg_bind_section_t           section,
    const apg_unit_v2_binding_t *bindings,
    size_t                       bindings_len,
    uc_error                    *err
) {
    const apg_atom_binding_schema_t *schema = find_binding_schema(atom, section);
    if (!schema)
        return UC_OK;

    for (size_t i = 0; i < schema->keys_len; i++) {
        if (atom_binding_key_optional(atom, section, schema->keys[i]))
            continue;
        if (!binding_key_present(bindings, bindings_len, schema->keys[i])) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' atom '%s' is missing required %s binding key '%s'", node_id ? node_id : "",
                atom ? atom : "", bind_section_name(section), schema->keys[i]
            );
            return set_error(err, UC_E_MISSING, msg);
        }
    }
    return UC_OK;
}

static uc_status compile_signal_bindings(
    const apg_unit_v2_t         *unit,
    const char                  *node_id,
    const char                  *atom,
    apg_bind_section_t           section,
    const apg_unit_v2_binding_t *bindings,
    size_t                       bindings_len,
    uc_arena                    *arena,
    apg_v2_compiled_binding_t  **out_bindings,
    size_t                      *out_len,
    uc_error                    *err
) {
    *out_bindings    = NULL;
    *out_len         = 0;
    uc_status status = validate_required_binding_keys(node_id, atom, section, bindings, bindings_len, err);
    if (status != UC_OK)
        return status;
    if (bindings_len == 0)
        return UC_OK;

    apg_v2_compiled_binding_t *compiled = uc_arena_alloc(arena, bindings_len * sizeof(*compiled), sizeof(void *));
    if (!compiled)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < bindings_len; i++) {
        status = validate_binding_key(node_id, atom, section, bindings[i].key, err);
        if (status != UC_OK)
            return status;
        if (bindings[i].value.kind != UC_VAL_LITERAL) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' %s binding key '%s' must be a literal signal name", node_id ? node_id : "",
                bind_section_name(section), bindings[i].key ? bindings[i].key : ""
            );
            return set_error(err, UC_E_TYPE, msg);
        }
        int signal_index = find_signal_index(unit, bindings[i].value.text);
        if (signal_index < 0) {
            char msg[192];
            snprintf(
                msg, sizeof(msg), "node '%s' %s binding key '%s' references unknown signal '%s'",
                node_id ? node_id : "", bind_section_name(section), bindings[i].key ? bindings[i].key : "",
                bindings[i].value.text ? bindings[i].value.text : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }

        compiled[i].key     = bindings[i].key;
        compiled[i].kind    = APG_BIND_SIGNAL;
        compiled[i].index   = (size_t)signal_index;
        compiled[i].literal = NULL;
    }

    *out_bindings = compiled;
    *out_len      = bindings_len;
    return UC_OK;
}

static uc_status compile_config_bindings(
    const apg_unit_v2_t         *unit,
    const char                  *node_id,
    const char                  *atom,
    const apg_unit_v2_binding_t *bindings,
    size_t                       bindings_len,
    uc_arena                    *arena,
    apg_v2_compiled_binding_t  **out_bindings,
    size_t                      *out_len,
    uc_error                    *err
) {
    *out_bindings = NULL;
    *out_len      = 0;
    uc_status status =
        validate_required_binding_keys(node_id, atom, APG_BIND_SECTION_CONFIG, bindings, bindings_len, err);
    if (status != UC_OK)
        return status;
    if (bindings_len == 0)
        return UC_OK;

    apg_v2_compiled_binding_t *compiled = uc_arena_alloc(arena, bindings_len * sizeof(*compiled), sizeof(void *));
    if (!compiled)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < bindings_len; i++) {
        status = validate_binding_key(node_id, atom, APG_BIND_SECTION_CONFIG, bindings[i].key, err);
        if (status != UC_OK)
            return status;

        compiled[i].key     = bindings[i].key;
        compiled[i].literal = NULL;
        compiled[i].index   = 0;

        if (bindings[i].value.kind == UC_VAL_VARREF) {
            const char *param_name  = param_ref_name(bindings[i].value.text);
            int         param_index = find_param_index(unit, param_name);
            if (param_index < 0) {
                char msg[192];
                snprintf(
                    msg, sizeof(msg), "node '%s' config binding key '%s' references unknown parameter '%s'",
                    node_id ? node_id : "", bindings[i].key ? bindings[i].key : "",
                    bindings[i].value.text ? bindings[i].value.text : ""
                );
                return set_error(err, UC_E_MISSING, msg);
            }
            compiled[i].kind  = APG_BIND_PARAM;
            compiled[i].index = (size_t)param_index;
        } else {
            compiled[i].kind    = APG_BIND_LITERAL;
            compiled[i].literal = bindings[i].value.text;
        }
    }

    *out_bindings = compiled;
    *out_len      = bindings_len;
    return UC_OK;
}

uc_status apg_v2_compile_unit(const apg_unit_v2_t *unit, uc_arena *arena, apg_v2_compiled_unit_t *out, uc_error *err) {
    if (!unit || !arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    apg_v2_compiled_node_t *nodes    = uc_arena_alloc(arena, unit->nodes_len * sizeof(*nodes), sizeof(void *));
    uint32_t               *schedule = uc_arena_alloc(arena, unit->nodes_len * sizeof(*schedule), sizeof(uint32_t));
    int      *node_scheduled         = uc_arena_alloc(arena, unit->nodes_len * sizeof(*node_scheduled), sizeof(int));
    int      *signal_available = uc_arena_alloc(arena, unit->signals_len * sizeof(*signal_available), sizeof(int));
    uint32_t *signal_producers = uc_arena_alloc(arena, unit->signals_len * sizeof(*signal_producers), sizeof(uint32_t));
    if (((!nodes || !schedule || !node_scheduled) && unit->nodes_len > 0) ||
        ((!signal_available || !signal_producers) && unit->signals_len > 0))
        return set_error(err, UC_E_OOM, "arena OOM");
    for (size_t i = 0; i < unit->nodes_len; i++)
        node_scheduled[i] = 0;
    for (size_t i = 0; i < unit->signals_len; i++) {
        signal_available[i] = 0;
        signal_producers[i] = UINT32_MAX;
    }

    uc_status status = mark_input_port_signals(unit, signal_available, err);
    if (status != UC_OK)
        return status;

    atom_registry_init();
    for (size_t i = 0; i < unit->nodes_len; i++) {
        const apg_unit_v2_node_t    *src  = &unit->nodes[i];
        const atom_registry_entry_t *atom = atom_registry_find(src->atom);
        if (!atom) {
            char msg[160];
            snprintf(
                msg, sizeof(msg), "node '%s' references unknown atom '%s'", src->id ? src->id : "",
                src->atom ? src->atom : ""
            );
            return set_error(err, UC_E_MISSING, msg);
        }

        nodes[i].id   = src->id;
        nodes[i].atom = atom;

        status = compile_signal_bindings(
            unit, src->id, src->atom, APG_BIND_SECTION_IN, src->in, src->in_len, arena, &nodes[i].in, &nodes[i].in_len,
            err
        );
        if (status != UC_OK)
            return status;
        status = compile_signal_bindings(
            unit, src->id, src->atom, APG_BIND_SECTION_OUT, src->out, src->out_len, arena, &nodes[i].out,
            &nodes[i].out_len, err
        );
        if (status != UC_OK)
            return status;
        status = record_node_output_producers(&nodes[i], (uint32_t)i, signal_producers, err);
        if (status != UC_OK)
            return status;
        status = compile_config_bindings(
            unit, src->id, src->atom, src->config, src->config_len, arena, &nodes[i].config, &nodes[i].config_len, err
        );
        if (status != UC_OK)
            return status;
    }

    status = build_schedule(nodes, unit->nodes_len, schedule, node_scheduled, signal_available, err);
    if (status != UC_OK)
        return status;

    status = validate_output_port_signals_produced(unit, signal_available, err);
    if (status != UC_OK)
        return status;

    out->unit                 = unit;
    out->nodes                = nodes;
    out->nodes_len            = unit->nodes_len;
    out->schedule             = schedule;
    out->schedule_len         = unit->nodes_len;
    out->signal_producers     = signal_producers;
    out->signal_producers_len = unit->signals_len;
    return UC_OK;
}
