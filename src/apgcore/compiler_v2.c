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

static uc_status mark_input_port_signals(const apg_unit_v2_t *unit, int *signal_available, uc_error *err) {
    for (size_t i = 0; i < unit->input_ports_len; i++) {
        if (!port_is_audio(&unit->input_ports[i]))
            continue;
        int signal_index = find_signal_index(unit, unit->input_ports[i].name);
        if (signal_index < 0)
            return set_error(err, UC_E_MISSING, "input audio port is missing matching graph signal");
        signal_available[signal_index] = 1;
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

static uc_status
validate_output_port_signals_produced(const apg_unit_v2_t *unit, const int *signal_available, uc_error *err) {
    for (size_t i = 0; i < unit->output_ports_len; i++) {
        if (!port_is_audio(&unit->output_ports[i]))
            continue;
        int signal_index = find_signal_index(unit, unit->output_ports[i].name);
        if (signal_index < 0)
            return set_error(err, UC_E_MISSING, "output audio port is missing matching graph signal");
        if (!signal_available[signal_index])
            return set_error(err, UC_E_MISSING, "output audio port signal is not produced by the graph");
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
    static const char *const               generation_dc_out[]    = {"signal"};
    static const char *const               generation_dc_config[] = {"value"};
    static const char *const               pair_in[]              = {"signal_a", "signal_b"};
    static const char *const               mono_in[]              = {"signal"};
    static const char *const               mono_out[]             = {"signal"};
    static const char *const               clip_hard_config[]     = {"threshold"};
    static const char *const               clip_soft_config[]     = {"threshold", "curve"};
    static const char *const               mix_wet_dry_in[]       = {"dry", "wet"};
    static const char *const               mix_wet_dry_config[]   = {"mix"};
    static const apg_atom_binding_schema_t schemas[]              = {
        {      "generation_dc",    APG_BIND_SECTION_OUT,    generation_dc_out,
         sizeof(generation_dc_out) / sizeof(generation_dc_out[0])                                                                },
        {      "generation_dc", APG_BIND_SECTION_CONFIG, generation_dc_config,
         sizeof(generation_dc_config) / sizeof(generation_dc_config[0])                                                          },
        { "amplitude_multiply",     APG_BIND_SECTION_IN,              pair_in,               sizeof(pair_in) / sizeof(pair_in[0])},
        { "amplitude_multiply",    APG_BIND_SECTION_OUT,             mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {      "amplitude_add",     APG_BIND_SECTION_IN,              pair_in,               sizeof(pair_in) / sizeof(pair_in[0])},
        {      "amplitude_add",    APG_BIND_SECTION_OUT,             mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        { "amplitude_subtract",     APG_BIND_SECTION_IN,              pair_in,               sizeof(pair_in) / sizeof(pair_in[0])},
        { "amplitude_subtract",    APG_BIND_SECTION_OUT,             mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {"amplitude_clip_hard",     APG_BIND_SECTION_IN,              mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        {"amplitude_clip_hard",    APG_BIND_SECTION_OUT,             mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {"amplitude_clip_hard", APG_BIND_SECTION_CONFIG,     clip_hard_config,
         sizeof(clip_hard_config) / sizeof(clip_hard_config[0])                                                                  },
        {"amplitude_clip_soft",     APG_BIND_SECTION_IN,              mono_in,               sizeof(mono_in) / sizeof(mono_in[0])},
        {"amplitude_clip_soft",    APG_BIND_SECTION_OUT,             mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {"amplitude_clip_soft", APG_BIND_SECTION_CONFIG,     clip_soft_config,
         sizeof(clip_soft_config) / sizeof(clip_soft_config[0])                                                                  },
        {        "mix_wet_dry",     APG_BIND_SECTION_IN,       mix_wet_dry_in, sizeof(mix_wet_dry_in) / sizeof(mix_wet_dry_in[0])},
        {        "mix_wet_dry",    APG_BIND_SECTION_OUT,             mono_out,             sizeof(mono_out) / sizeof(mono_out[0])},
        {        "mix_wet_dry", APG_BIND_SECTION_CONFIG,   mix_wet_dry_config,
         sizeof(mix_wet_dry_config) / sizeof(mix_wet_dry_config[0])                                                              },
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

static uc_status validate_binding_key(const char *atom, apg_bind_section_t section, const char *key, uc_error *err) {
    if (atom_binding_key_allowed(atom, section, key))
        return UC_OK;

    char msg[128];
    snprintf(msg, sizeof(msg), "atom '%s' does not accept binding key '%s'", atom ? atom : "", key ? key : "");
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
        if (!binding_key_present(bindings, bindings_len, schema->keys[i])) {
            char msg[160];
            snprintf(
                msg, sizeof(msg), "atom '%s' is missing required binding key '%s'", atom ? atom : "", schema->keys[i]
            );
            return set_error(err, UC_E_MISSING, msg);
        }
    }
    return UC_OK;
}

static uc_status compile_signal_bindings(
    const apg_unit_v2_t         *unit,
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
    uc_status status = validate_required_binding_keys(atom, section, bindings, bindings_len, err);
    if (status != UC_OK)
        return status;
    if (bindings_len == 0)
        return UC_OK;

    apg_v2_compiled_binding_t *compiled = uc_arena_alloc(arena, bindings_len * sizeof(*compiled), sizeof(void *));
    if (!compiled)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < bindings_len; i++) {
        status = validate_binding_key(atom, section, bindings[i].key, err);
        if (status != UC_OK)
            return status;
        if (bindings[i].value.kind != UC_VAL_LITERAL)
            return set_error(err, UC_E_TYPE, "signal binding must be a literal signal name");
        int signal_index = find_signal_index(unit, bindings[i].value.text);
        if (signal_index < 0) {
            char msg[128];
            snprintf(
                msg, sizeof(msg), "unknown signal binding '%s'", bindings[i].value.text ? bindings[i].value.text : ""
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
    const char                  *atom,
    const apg_unit_v2_binding_t *bindings,
    size_t                       bindings_len,
    uc_arena                    *arena,
    apg_v2_compiled_binding_t  **out_bindings,
    size_t                      *out_len,
    uc_error                    *err
) {
    *out_bindings    = NULL;
    *out_len         = 0;
    uc_status status = validate_required_binding_keys(atom, APG_BIND_SECTION_CONFIG, bindings, bindings_len, err);
    if (status != UC_OK)
        return status;
    if (bindings_len == 0)
        return UC_OK;

    apg_v2_compiled_binding_t *compiled = uc_arena_alloc(arena, bindings_len * sizeof(*compiled), sizeof(void *));
    if (!compiled)
        return set_error(err, UC_E_OOM, "arena OOM");

    for (size_t i = 0; i < bindings_len; i++) {
        status = validate_binding_key(atom, APG_BIND_SECTION_CONFIG, bindings[i].key, err);
        if (status != UC_OK)
            return status;

        compiled[i].key     = bindings[i].key;
        compiled[i].literal = NULL;
        compiled[i].index   = 0;

        if (bindings[i].value.kind == UC_VAL_VARREF) {
            const char *param_name  = param_ref_name(bindings[i].value.text);
            int         param_index = find_param_index(unit, param_name);
            if (param_index < 0) {
                char msg[128];
                snprintf(
                    msg, sizeof(msg), "unknown config parameter '%s'",
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
    int *node_scheduled              = uc_arena_alloc(arena, unit->nodes_len * sizeof(*node_scheduled), sizeof(int));
    int *signal_available = uc_arena_alloc(arena, unit->signals_len * sizeof(*signal_available), sizeof(int));
    if (((!nodes || !schedule || !node_scheduled) && unit->nodes_len > 0) ||
        (!signal_available && unit->signals_len > 0))
        return set_error(err, UC_E_OOM, "arena OOM");
    for (size_t i = 0; i < unit->nodes_len; i++)
        node_scheduled[i] = 0;
    for (size_t i = 0; i < unit->signals_len; i++)
        signal_available[i] = 0;

    uc_status status = mark_input_port_signals(unit, signal_available, err);
    if (status != UC_OK)
        return status;

    atom_registry_init();
    for (size_t i = 0; i < unit->nodes_len; i++) {
        const apg_unit_v2_node_t    *src  = &unit->nodes[i];
        const atom_registry_entry_t *atom = atom_registry_find(src->atom);
        if (!atom)
            return set_error(err, UC_E_MISSING, "unknown atom during compile");

        nodes[i].id   = src->id;
        nodes[i].atom = atom;

        status = compile_signal_bindings(
            unit, src->atom, APG_BIND_SECTION_IN, src->in, src->in_len, arena, &nodes[i].in, &nodes[i].in_len, err
        );
        if (status != UC_OK)
            return status;
        status = compile_signal_bindings(
            unit, src->atom, APG_BIND_SECTION_OUT, src->out, src->out_len, arena, &nodes[i].out, &nodes[i].out_len, err
        );
        if (status != UC_OK)
            return status;
        status = compile_config_bindings(
            unit, src->atom, src->config, src->config_len, arena, &nodes[i].config, &nodes[i].config_len, err
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

    out->unit         = unit;
    out->nodes        = nodes;
    out->nodes_len    = unit->nodes_len;
    out->schedule     = schedule;
    out->schedule_len = unit->nodes_len;
    return UC_OK;
}
