#include <runtime.h>
#include <math.h>
#include <stdio.h>

#include "atom/dsp_atoms.h"
#include "loader.h"

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
    if (strncmp(expr, "params.", 7) == 0) {
        const char *key = expr + 7;
        for (int i = 0; i < u->n_params; i++)
            if (strcmp(u->params[i].name, key) == 0)
                return u->params[i].value;
    }
    if (strncmp(expr, "internal.", 9) == 0) {
        const char *key = expr + 9;
        for (int i = 0; i < u->n_internals; i++)
            if (strcmp(u->internals[i].name, key) == 0)
                return u->internals[i].value;
    }
    if (strcmp(expr, "context.sample_rate") == 0) {
        return u->ctx.sample_rate;
    }
    return strtof(expr, NULL);
}

static float *rt_resolve_signal(runtime_unit_t *u, const char *expr) {
    if (strncmp(expr, "signals.", 8) == 0) {
        const char *key = expr + 8;
        return rt_find_signal(u, key);
    }
    return rt_find_signal(u, expr);
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

    // Copy "output" signal to output buffer with NaN detection
    float *output_sig = rt_find_signal(unit, "output");
    if (output_sig) {
        for (int i = 0; i < unit->ctx.chunk_length; i++) {
            if (isnan(output_sig[i])) {
                static int nan_warn = 0;
                if (nan_warn++ % 100 == 0) fprintf(stderr, "runtime: WARNING: NaN detected in output!\n");
                output_sig[i] = 0.0f; 
            }
        }
        memcpy(out, output_sig, unit->ctx.chunk_length * sizeof(float));
        
    }
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
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0) {
        fprintf(stderr, "runtime: arena init failed\n");
        return NULL;
    }

    uc_unit uc;
    uc_error err = {0};
    uc_status s = uc_load_file(yaml_path, &arena, &uc, &err);
    if (s != UC_OK) {
        fprintf(stderr, "runtime: [%d:%d] %s\n", err.loc.line, err.loc.col, err.msg);
        uc_arena_free(&arena);
        return NULL;
    }

    runtime_unit_t *u = calloc(1, sizeof(runtime_unit_t));
    if (!u) {
        uc_arena_free(&arena);
        return NULL;
    }


    u->ctx = ctx;
    if (u->ctx.chunk_length == 0) u->ctx.chunk_length = RT_CHUNK_LENGTH;

    if (uc.name) strncpy(u->name, uc.name, 63);
    if (uc.version) {} // version ignored for now

    u->signal_pool_count = RT_MAX_SIGNALS;
    u->signal_pool = calloc(u->signal_pool_count * u->ctx.chunk_length, sizeof(float));
    u->state_pool = calloc(1000000, sizeof(float)); // 4MB state pool
    u->state_pool_used = 0;

    rt_alloc_signal(u, "input");
    rt_alloc_signal(u, "output");

    atom_registry_init();

    for (size_t i = 0; i < uc.params_len && u->n_params < RT_MAX_PARAMS; i++) {
        u->params[u->n_params].name = strdup(uc.params[i].name);
        u->params[u->n_params].value = (float)uc.params[i].def;
        u->n_params++;
    }

    for (size_t i = 0; i < uc.internal_len && u->n_internals < RT_MAX_INTERNALS; i++) {
        u->internals[u->n_internals].name = strdup(uc.internal[i].key);
        u->internals[u->n_internals].value = (float)uc.internal[i].value;
        u->n_internals++;
    }

    for (size_t i = 0; i < uc.signals_len; i++) {
        rt_alloc_signal(u, uc.signals[i]);
    }

    for (size_t i = 0; i < uc.pipeline_len && u->n_steps < RT_MAX_STEPS; i++) {
        uc_stage *st = &uc.pipeline[i];
        rt_step_t *step = &u->steps[u->n_steps];
        memset(step, 0, sizeof(*step));

        const atom_registry_entry_t *atom = atom_registry_find(st->fn);
        if (!atom) {
            fprintf(stderr, "runtime: unknown atom '%s' in step '%s'\n", st->fn, st->id ? st->id : "?");
            continue;
        }

        step->id = strdup(st->id);
        step->atom = atom;
        step->out = calloc(1, atom->out_size > 0 ? atom->out_size : 1);
        step->in = calloc(1, atom->in_size > 0 ? atom->in_size : 1);
        step->config = calloc(1, atom->config_size > 0 ? atom->config_size : 1);
        step->state = calloc(1, atom->state_size > 0 ? atom->state_size : 1);

        // Auto-allocate buffers in state
        for (int j = 0; j < atom->n_state_fields; j++) {
            if (atom->state_fields[j].type == FIELD_BUFFER) {
                float **ptr = (float **)((char *)step->state + atom->state_fields[j].offset);
                *ptr = &u->state_pool[u->state_pool_used];
                u->state_pool_used += 65536; // Default buffer size: 64k samples
            }
        }

        for (size_t j = 0; j < st->out_len; j++) {
            const char *expr = st->out[j].value.text;
            float *sig = rt_resolve_signal(u, expr);
            if (!sig) sig = rt_find_signal(u, expr);
            if (!sig) sig = rt_alloc_signal(u, expr);
            if (sig) {
                float **field = (float **)((char *)step->out + j * sizeof(float *));
                *field = sig;
            }
        }

        for (size_t j = 0; j < st->in_len; j++) {
            const char *expr = st->in[j].value.text;
            float *sig = rt_resolve_signal(u, expr);
            if (!sig && st->in[j].value.kind == UC_VAL_LITERAL) {
                sig = rt_alloc_signal(u, st->in[j].key);
                if (sig) {
                    float val = rt_resolve_value(u, expr);
                    for (int k = 0; k < u->ctx.chunk_length; k++) sig[k] = val;
                }
            }
            if (sig) {
                float **field = (float **)((char *)step->in + j * sizeof(float *));
                *field = sig;
            }
        }

        if (atom->config_fields) {
            for (size_t j = 0; j < st->config_len; j++) {
                float val = rt_resolve_value(u, st->config[j].value.text);
                const char *key = st->config[j].key;
                for (int k = 0; k < atom->n_config_fields; k++) {
                    if (strcmp(atom->config_fields[k].name, key) == 0) {
                        void *addr = (char *)step->config + atom->config_fields[k].offset;
                        if (atom->config_fields[k].type == FIELD_INT) *(int *)addr = (int)val;
                        else *(float *)addr = val;
                        break;
                    }
                }
            }
        } else {
            size_t offset = 0;
            for (size_t j = 0; j < st->config_len; j++) {
                float val = rt_resolve_value(u, st->config[j].value.text);
                if (strstr(st->config[j].key, "waveform") ||
                    strstr(st->config[j].key, "interpolation") ||
                    strstr(st->config[j].key, "mode") ||
                    strstr(st->config[j].key, "num_") ||
                    strstr(st->config[j].key, "order") ||
                    strstr(st->config[j].key, "length") ||
                    strstr(st->config[j].key, "size")) {
                    size_t align = offset % sizeof(int);
                    if (align) offset += sizeof(int) - align;
                    *(int *)((char *)step->config + offset) = (int)val;
                    offset += sizeof(int);
                } else {
                    size_t align = offset % sizeof(float);
                    if (align) offset += sizeof(float) - align;
                    *(float *)((char *)step->config + offset) = val;
                    offset += sizeof(float);
                }
            }
        }

        if (atom->state_fields) {
            for (size_t j = 0; j < st->state_len; j++) {
                float val = (float)st->state[j].value;
                const char *key = st->state[j].key;
                for (int k = 0; k < atom->n_state_fields; k++) {
                    if (strcmp(atom->state_fields[k].name, key) == 0) {
                        void *addr = (char *)step->state + atom->state_fields[k].offset;
                        if (atom->state_fields[k].type == FIELD_INT) *(int *)addr = (int)val;
                        else if (atom->state_fields[k].type == FIELD_FLOAT) *(float *)addr = val;
                        break;
                    }
                }
            }
        } else {
            for (size_t j = 0; j < st->state_len; j++) {
                float val = (float)st->state[j].value;
                *(float *)((char *)step->state + j * sizeof(float)) = val;
            }
        }

        u->n_steps++;
    }

    uc_arena_free(&arena);

    printf("runtime: loaded '%s' with %d steps, %d signals, %d params\n",
           u->name, u->n_steps, u->n_signals, u->n_params);

    return u;
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
        free((void *)unit->steps[i].out);
        free((void *)unit->steps[i].in);
        free((void *)unit->steps[i].config);
        free((void *)unit->steps[i].state);
    }

    free(unit->signal_pool);
    free(unit->state_pool);
    free(unit);
}
