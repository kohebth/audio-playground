#include <runtime.h>
#include "atom/dsp_atoms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────

static float *rt_find_signal(runtime_unit_t *u, const char *name) {
    for (int i = 0; i < u->n_signals; i++) {
        if (strcmp(u->signals[i].name, name) == 0)
            return u->signals[i].buffer;
    }
    return NULL;
}

static float *rt_alloc_signal(runtime_unit_t *u, const char *name) {
    float *existing = rt_find_signal(u, name);
    if (existing) return existing;

    if (u->n_signals >= RT_MAX_SIGNALS) {
        fprintf(stderr, "runtime: too many signals (max %d)\n", RT_MAX_SIGNALS);
        return NULL;
    }

    int idx = u->n_signals++;
    u->signals[idx].name = strdup(name);
    u->signals[idx].buffer = &u->signal_pool[idx * u->ctx.chunk_length];
    memset(u->signals[idx].buffer, 0, u->ctx.chunk_length * sizeof(float));
    return u->signals[idx].buffer;
}

static float rt_resolve_value(runtime_unit_t *u, const char *expr) {
    // ${params.X}
    if (strncmp(expr, "${params.", 9) == 0) {
        char key[64];
        sscanf(expr + 9, "%63[^}]", key);
        for (int i = 0; i < u->n_params; i++)
            if (strcmp(u->params[i].name, key) == 0)
                return u->params[i].value;
    }
    // ${internal.X}
    if (strncmp(expr, "${internal.", 11) == 0) {
        char key[64];
        sscanf(expr + 11, "%63[^}]", key);
        for (int i = 0; i < u->n_internals; i++)
            if (strcmp(u->internals[i].name, key) == 0)
                return u->internals[i].value;
    }
    // ${context.sample_rate}
    if (strcmp(expr, "${context.sample_rate}") == 0) {
        return u->ctx.sample_rate;
    }
    // literal float
    return strtof(expr, NULL);
}

static float *rt_resolve_signal(runtime_unit_t *u, const char *expr) {
    // ${signals.X}
    if (strncmp(expr, "${signals.", 10) == 0) {
        char key[64];
        sscanf(expr + 10, "%63[^}]", key);
        return rt_find_signal(u, key);
    }
    // direct signal name
    return rt_find_signal(u, expr);
}

// ─────────────────────────────────────────────
// Minimal YAML parser — handles the UNIT.md format
// ─────────────────────────────────────────────

typedef enum {
    SEC_NONE, SEC_PARAMS, SEC_INTERNAL, SEC_SIGNALS, SEC_PIPELINE,
    SEC_STEP, SEC_STEP_OUT, SEC_STEP_IN, SEC_STEP_CONFIG, SEC_STEP_STATE
} yaml_section_t;

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' '))
        s[--len] = '\0';
    return s;
}

// Extract key and value from "  key: value" or "  key: { ... }"
static int parse_kv(const char *line, char *key, char *val) {
    key[0] = val[0] = '\0';
    const char *colon = strchr(line, ':');
    if (!colon) return 0;

    int klen = (int)(colon - line);
    if (klen <= 0 || klen > 63) return 0;
    strncpy(key, line, klen);
    key[klen] = '\0';

    // trim key
    char *k = key;
    while (*k == ' ' || *k == '-' || *k == '\t') k++;
    memmove(key, k, strlen(k) + 1);

    // value after colon
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t') v++;
    strncpy(val, v, 255);
    val[255] = '\0';

    // remove trailing whitespace
    int vlen = strlen(val);
    while (vlen > 0 && (val[vlen-1] == '\n' || val[vlen-1] == '\r' || val[vlen-1] == ' '))
        val[--vlen] = '\0';

    return 1;
}

static int indent_level(const char *line) {
    int n = 0;
    while (line[n] == ' ') n++;
    return n;
}

// Parse "{ min: 0.0, max: 80.0, default: 40.0, unit: dB }" → extract default
static float parse_param_default(const char *braces) {
    const char *def = strstr(braces, "default:");
    if (!def) return 0.0f;
    return strtof(def + 8, NULL);
}

static runtime_unit_t *parse_yaml(const char *path, runtime_context_t ctx) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "runtime: cannot open %s\n", path);
        return NULL;
    }

    runtime_unit_t *u = calloc(1, sizeof(runtime_unit_t));
    u->ctx = ctx;
    if (u->ctx.chunk_length == 0) u->ctx.chunk_length = RT_CHUNK_LENGTH;

    // Pre-allocate signal pool
    u->signal_pool_count = RT_MAX_SIGNALS;
    u->signal_pool = calloc(u->signal_pool_count * u->ctx.chunk_length, sizeof(float));

    // Pre-allocate state pool for delay lines (64K floats = 256KB)
    u->state_pool = calloc(65536, sizeof(float));
    u->state_pool_used = 0;

    // Reserve "input" and "output" signals
    rt_alloc_signal(u, "input");
    rt_alloc_signal(u, "output");

    yaml_section_t section = SEC_NONE;
    rt_step_t *cur_step = NULL;

    // Temporary storage for step signal/config/state mappings
    // We'll resolve these after parsing
    typedef struct { char field[64]; char expr[256]; } mapping_t;
    mapping_t step_out[8], step_in[8], step_cfg[16], step_state[8];
    int n_step_out, n_step_in, n_step_cfg, n_step_state;
    char step_fn[64];
    char step_id[64];

    // For deferred resolution
    typedef struct {
        int step_idx;
        char fn[64];
        char id[64];
        mapping_t out[8];  int n_out;
        mapping_t in[8];   int n_in;
        mapping_t cfg[16]; int n_cfg;
        mapping_t st[8];   int n_st;
    } deferred_step_t;
    deferred_step_t deferred[RT_MAX_STEPS];
    int n_deferred = 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *t = trim(line);
        if (t[0] == '#' || t[0] == '\0' || strcmp(t, "---") == 0) continue;

        int indent = indent_level(line);
        char key[64], val[256];

        // Top-level sections
        if (indent == 0 && parse_kv(line, key, val)) {
            if (strcmp(key, "name") == 0)           { strncpy(u->name, val, 63); section = SEC_NONE; }
            else if (strcmp(key, "params") == 0)     section = SEC_PARAMS;
            else if (strcmp(key, "internal") == 0)   section = SEC_INTERNAL;
            else if (strcmp(key, "signals") == 0)    section = SEC_SIGNALS;
            else if (strcmp(key, "pipeline") == 0)   section = SEC_PIPELINE;
            else                                     section = SEC_NONE;
            continue;
        }

        // Params section
        if (section == SEC_PARAMS && indent >= 2 && parse_kv(t, key, val)) {
            if (u->n_params < RT_MAX_PARAMS) {
                u->params[u->n_params].name = strdup(key);
                u->params[u->n_params].value = parse_param_default(val);
                u->n_params++;
            }
            continue;
        }

        // Internal section
        if (section == SEC_INTERNAL && indent >= 2 && parse_kv(t, key, val)) {
            if (u->n_internals < RT_MAX_INTERNALS) {
                u->internals[u->n_internals].name = strdup(key);
                // Resolve simple expressions or literals
                u->internals[u->n_internals].value = rt_resolve_value(u, val);
                u->n_internals++;
            }
            continue;
        }

        // Signals section (list of "- name: X")
        if (section == SEC_SIGNALS && indent >= 2) {
            if (parse_kv(t, key, val) && strcmp(key, "name") == 0) {
                rt_alloc_signal(u, val);
            }
            continue;
        }

        // Pipeline section
        if (section == SEC_PIPELINE || section == SEC_STEP ||
            section == SEC_STEP_OUT || section == SEC_STEP_IN ||
            section == SEC_STEP_CONFIG || section == SEC_STEP_STATE) {

            // New step starts with "- id:"
            if (t[0] == '-' && strstr(t, "id:")) {
                // Save previous step if any
                if (cur_step && n_deferred > 0) {
                    // already saved
                }
                // Start new deferred step
                deferred_step_t *d = &deferred[n_deferred];
                memset(d, 0, sizeof(*d));
                d->step_idx = n_deferred;

                parse_kv(t, key, val);
                strncpy(d->id, val, 63);

                n_step_out = n_step_in = n_step_cfg = n_step_state = 0;
                step_fn[0] = '\0';
                strncpy(step_id, val, 63);
                section = SEC_STEP;
                n_deferred++;
                continue;
            }

            if (section == SEC_STEP && parse_kv(t, key, val)) {
                deferred_step_t *d = &deferred[n_deferred - 1];
                if (strcmp(key, "fn") == 0)        strncpy(d->fn, val, 63);
                else if (strcmp(key, "out") == 0)  section = SEC_STEP_OUT;
                else if (strcmp(key, "in") == 0)   section = SEC_STEP_IN;
                else if (strcmp(key, "config") == 0) {
                    if (strcmp(val, "~") == 0) section = SEC_STEP;
                    else section = SEC_STEP_CONFIG;
                }
                else if (strcmp(key, "state") == 0) {
                    if (strcmp(val, "~") == 0) section = SEC_STEP;
                    else section = SEC_STEP_STATE;
                }
                continue;
            }

            if (section == SEC_STEP_OUT && indent >= 6 && parse_kv(t, key, val)) {
                deferred_step_t *d = &deferred[n_deferred - 1];
                if (d->n_out < 8) {
                    strncpy(d->out[d->n_out].field, key, 63);
                    strncpy(d->out[d->n_out].expr, val, 255);
                    d->n_out++;
                }
                continue;
            }

            if (section == SEC_STEP_IN && indent >= 6 && parse_kv(t, key, val)) {
                deferred_step_t *d = &deferred[n_deferred - 1];
                if (d->n_in < 8) {
                    strncpy(d->in[d->n_in].field, key, 63);
                    strncpy(d->in[d->n_in].expr, val, 255);
                    d->n_in++;
                }
                continue;
            }

            if (section == SEC_STEP_CONFIG && indent >= 6 && parse_kv(t, key, val)) {
                if (strcmp(val, "~") == 0) { section = SEC_STEP; continue; }
                deferred_step_t *d = &deferred[n_deferred - 1];
                if (d->n_cfg < 16) {
                    strncpy(d->cfg[d->n_cfg].field, key, 63);
                    strncpy(d->cfg[d->n_cfg].expr, val, 255);
                    d->n_cfg++;
                }
                continue;
            }

            if (section == SEC_STEP_STATE && indent >= 6 && parse_kv(t, key, val)) {
                deferred_step_t *d = &deferred[n_deferred - 1];
                if (d->n_st < 8) {
                    strncpy(d->st[d->n_st].field, key, 63);
                    strncpy(d->st[d->n_st].expr, val, 255);
                    d->n_st++;
                }
                continue;
            }

            // Back to step level on lower indent
            if (indent <= 4 && section > SEC_STEP && parse_kv(t, key, val)) {
                section = SEC_STEP;
                // re-process this line
                deferred_step_t *d = &deferred[n_deferred - 1];
                if (strcmp(key, "fn") == 0)          strncpy(d->fn, val, 63);
                else if (strcmp(key, "out") == 0)    section = SEC_STEP_OUT;
                else if (strcmp(key, "in") == 0)     section = SEC_STEP_IN;
                else if (strcmp(key, "config") == 0) section = SEC_STEP_CONFIG;
                else if (strcmp(key, "state") == 0) {
                    if (strcmp(val, "~") == 0) section = SEC_STEP;
                    else section = SEC_STEP_STATE;
                }
                continue;
            }
        }
    }
    fclose(f);

    // ─────────────────────────────────────────────
    // Resolve deferred steps
    // ─────────────────────────────────────────────

    atom_registry_init();

    for (int i = 0; i < n_deferred && u->n_steps < RT_MAX_STEPS; i++) {
        deferred_step_t *d = &deferred[i];
        rt_step_t *step = &u->steps[u->n_steps];
        memset(step, 0, sizeof(*step));

        // Resolve atom
        const atom_registry_entry_t *atom = atom_registry_find(d->fn);
        if (!atom) {
            fprintf(stderr, "runtime: unknown atom '%s' in step '%s'\n", d->fn, d->id);
            continue;
        }
        step->id = strdup(d->id);
        step->atom = atom;

        // Allocate structs
        step->out    = calloc(1, atom->out_size > 0 ? atom->out_size : 1);
        step->in     = calloc(1, atom->in_size > 0 ? atom->in_size : 1);
        step->config = calloc(1, atom->config_size > 0 ? atom->config_size : 1);
        step->state  = calloc(1, atom->state_size > 0 ? atom->state_size : 1);

        // Wire output signals — resolve ${signals.X} or bare names
        for (int j = 0; j < d->n_out; j++) {
            float *sig = rt_resolve_signal(u, d->out[j].expr);
            if (!sig) sig = rt_find_signal(u, d->out[j].expr);
            if (!sig) sig = rt_alloc_signal(u, d->out[j].expr);
            if (sig) {
                float **field = (float **)((char *)step->out + j * sizeof(float *));
                *field = sig;
            }
        }

        // Wire input signals
        for (int j = 0; j < d->n_in; j++) {
            float *sig = rt_resolve_signal(u, d->in[j].expr);
            if (!sig) {
                // might be a value reference — allocate a temp signal and fill it
                sig = rt_alloc_signal(u, d->in[j].expr);
                if (sig) {
                    float val = rt_resolve_value(u, d->in[j].expr);
                    for (int k = 0; k < u->ctx.chunk_length; k++)
                        sig[k] = val;
                }
            }
            if (sig) {
                float **field = (float **)((char *)step->in + j * sizeof(float *));
                *field = sig;
            }
        }

        // Set config values — write floats/ints sequentially into config struct
        {
            size_t offset = 0;
            for (int j = 0; j < d->n_cfg; j++) {
                float val = rt_resolve_value(u, d->cfg[j].expr);
                // Check if field name suggests int type (heuristic)
                if (strstr(d->cfg[j].field, "waveform") ||
                    strstr(d->cfg[j].field, "interpolation") ||
                    strstr(d->cfg[j].field, "window_type") ||
                    strstr(d->cfg[j].field, "mode") ||
                    strstr(d->cfg[j].field, "color") ||
                    strstr(d->cfg[j].field, "factor") ||
                    strstr(d->cfg[j].field, "kernel_size") ||
                    strstr(d->cfg[j].field, "table_size") ||
                    strstr(d->cfg[j].field, "block_size") ||
                    strstr(d->cfg[j].field, "hop_size") ||
                    strstr(d->cfg[j].field, "num_") ||
                    strstr(d->cfg[j].field, "order") ||
                    strstr(d->cfg[j].field, "delay_samples") ||
                    strstr(d->cfg[j].field, "from_format") ||
                    strstr(d->cfg[j].field, "to_format") ||
                    strstr(d->cfg[j].field, "length") ||
                    strstr(d->cfg[j].field, "window_size") ||
                    strstr(d->cfg[j].field, "max_lag") ||
                    strstr(d->cfg[j].field, "buffer_size") ||
                    strstr(d->cfg[j].field, "num_taps") ||
                    strstr(d->cfg[j].field, "curve")) {
                    // align to int boundary
                    size_t align = offset % sizeof(int);
                    if (align) offset += sizeof(int) - align;
                    *(int *)((char *)step->config + offset) = (int)val;
                    offset += sizeof(int);
                } else {
                    // align to float boundary
                    size_t align = offset % sizeof(float);
                    if (align) offset += sizeof(float) - align;
                    *(float *)((char *)step->config + offset) = val;
                    offset += sizeof(float);
                }
            }
        }

        // Initialize state values
        if (strcmp(d->fn, "modulation_phase") == 0) {
            modulation_phase_state_t *st = (modulation_phase_state_t *)step->state;
            st->buffer = &u->state_pool[u->state_pool_used];
            u->state_pool_used += 4096;
            st->write_pos = 0;
        } else if (strcmp(d->fn, "filter_biquad") == 0) {
            filter_biquad_state_t *st = (filter_biquad_state_t *)step->state;
            st->z1 = 0;
            st->z2 = 0;
        } else {
            size_t offset = 0;
            for (int j = 0; j < d->n_st; j++) {
                float val = strtof(d->st[j].expr, NULL);
                *(float *)((char *)step->state + offset) = val;
                offset += sizeof(float);
            }
        }

        u->n_steps++;
    }

    printf("runtime: loaded '%s' with %d steps, %d signals, %d params\n",
           u->name, u->n_steps, u->n_signals, u->n_params);

    return u;
}

// ─────────────────────────────────────────────
// Pipeline execution
// ─────────────────────────────────────────────

void runtime_unit_process(runtime_unit_t *unit, float *in, float *out) {
    if (!unit || !in || !out) return;

    // Copy input audio into the "input" signal
    float *input_sig = rt_find_signal(unit, "input");
    if (input_sig)
        memcpy(input_sig, in, unit->ctx.chunk_length * sizeof(float));

    // Execute each pipeline step
    atom_call_t call;
    for (int i = 0; i < unit->n_steps; i++) {
        rt_step_t *step = &unit->steps[i];
        call.out    = step->out;
        call.in     = step->in;
        call.config = step->config;
        call.state  = step->state;
        step->atom->thunk(&call);
    }

    // Copy "output" signal to output buffer
    float *output_sig = rt_find_signal(unit, "output");
    if (output_sig)
        memcpy(out, output_sig, unit->ctx.chunk_length * sizeof(float));
}

// ─────────────────────────────────────────────
// Parameter update
// ─────────────────────────────────────────────

bool runtime_unit_set_param(runtime_unit_t *unit, const char *name, float value) {
    for (int i = 0; i < unit->n_params; i++) {
        if (strcmp(unit->params[i].name, name) == 0) {
            unit->params[i].value = value;
            unit->is_changed = true;
            return true;
        }
    }
    return false;
}

// ─────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────

runtime_unit_t *runtime_unit_load(const char *yaml_path, runtime_context_t ctx) {
    return parse_yaml(yaml_path, ctx);
}

void runtime_unit_destroy(runtime_unit_t *unit) {
    if (!unit) return;

    for (int i = 0; i < unit->n_signals; i++)
        free((void *)unit->signals[i].name);

    for (int i = 0; i < unit->n_params; i++)
        free((void *)unit->params[i].name);

    for (int i = 0; i < unit->n_internals; i++)
        free((void *)unit->internals[i].name);

    for (int i = 0; i < unit->n_steps; i++) {
        free((void *)unit->steps[i].id);
        free(unit->steps[i].out);
        free(unit->steps[i].in);
        free(unit->steps[i].config);
        free(unit->steps[i].state);
    }

    free(unit->signal_pool);
    free(unit->state_pool);
    free(unit);
}
