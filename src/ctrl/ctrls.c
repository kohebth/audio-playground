#include <ctrl/ctrls.h>

#include <arena.h>
#include <atom_registry.h>
#include <error.h>
#include <loader.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unit.h>

#define CTRL_DEFAULT_SMOOTHING_MS 35.0f

static ctrl_param_t *ctrl_find_param(ctrl_unit_t *ctrl, const char *name) {
    if (!ctrl || !name) return NULL;
    for (int i = 0; i < ctrl->n_params; i++) {
        if (strcmp(ctrl->params[i].name, name) == 0) return &ctrl->params[i];
    }
    return NULL;
}

static int ctrl_find_runtime_step(runtime_unit_t *unit, const char *id) {
    if (!unit || !id) return -1;
    for (int i = 0; i < unit->n_steps; i++) {
        if (unit->steps[i].id && strcmp(unit->steps[i].id, id) == 0) return i;
    }
    return -1;
}

static const atom_field_desc_t *ctrl_find_config_field(const atom_registry_entry_t *atom, const char *field_name) {
    if (!atom || !field_name) return NULL;
    for (int i = 0; i < atom->n_config_fields; i++) {
        if (strcmp(atom->config_fields[i].name, field_name) == 0) return &atom->config_fields[i];
    }
    return NULL;
}

static bool ctrl_parse_param_ref(const char *expr, char *out_name, size_t out_size) {
    if (!expr || !out_name || out_size == 0) return false;

    const char *start = expr;
    while (*start == ' ' || *start == '"' || *start == '\'') start++;
    const char *end = NULL;
    if (strncmp(start, "${params.", 9) == 0) {
        start += 9;
        end = strchr(start, '}');
    } else if (strncmp(start, "params.", 7) == 0) {
        start += 7;
        end = start + strlen(start);
    } else {
        return false;
    }

    if (!end || end == start) return false;

    size_t len = (size_t)(end - start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out_name, start, len);
    out_name[len] = '\0';
    return true;
}

static float ctrl_clamp_param(const ctrl_param_t *param, float value) {
    if (!param) return value;
    if (param->min < param->max) {
        if (value < param->min) return param->min;
        if (value > param->max) return param->max;
    }
    return value;
}

static void ctrl_apply_param(ctrl_unit_t *ctrl, ctrl_param_t *param) {
    if (!ctrl || !ctrl->unit || !param) return;

    runtime_unit_set_param(ctrl->unit, param->name, param->current);

    for (int i = 0; i < param->n_bindings; i++) {
        ctrl_config_binding_t *binding = &param->bindings[i];
        if (!binding->field_addr) continue;

        if (binding->field_type == FIELD_INT) {
            *(int *)binding->field_addr = (int)lroundf(param->current);
        } else if (binding->field_type == FIELD_FLOAT) {
            *(float *)binding->field_addr = param->current;
        }
    }
}

static bool ctrl_add_binding(ctrl_param_t *param, runtime_unit_t *unit, int step_index, const char *field_name) {
    if (!param || !unit || step_index < 0 || step_index >= unit->n_steps || !field_name) return false;
    if (param->n_bindings >= CTRL_MAX_BINDINGS) return false;

    rt_step_t *step = &unit->steps[step_index];
    const atom_field_desc_t *field = ctrl_find_config_field(step->atom, field_name);
    if (!field || !step->config) return false;
    if (field->type != FIELD_FLOAT && field->type != FIELD_INT) return false;

    ctrl_config_binding_t *binding = &param->bindings[param->n_bindings++];
    memset(binding, 0, sizeof(*binding));
    binding->step_index = step_index;
    binding->field_type = field->type;
    binding->field_addr = (char *)step->config + field->offset;
    strncpy(binding->field_name, field_name, sizeof(binding->field_name) - 1);
    return true;
}

static void ctrl_discover_bindings(ctrl_unit_t *ctrl, const uc_unit *uc) {
    if (!ctrl || !ctrl->unit || !uc) return;

    for (size_t i = 0; i < uc->pipeline_len; i++) {
        const uc_stage *stage = &uc->pipeline[i];
        int step_index = ctrl_find_runtime_step(ctrl->unit, stage->id);
        if (step_index < 0) continue;

        for (size_t j = 0; j < stage->config_len; j++) {
            char param_name[64];
            if (!ctrl_parse_param_ref(stage->config[j].value.text, param_name, sizeof(param_name))) continue;

            ctrl_param_t *param = ctrl_find_param(ctrl, param_name);
            if (!param) continue;

            ctrl_add_binding(param, ctrl->unit, step_index, stage->config[j].key);
        }
    }
}

bool ctrl_unit_init(ctrl_unit_t *ctrl, runtime_unit_t *unit, const char *unit_yaml_path) {
    if (!ctrl || !unit || !unit_yaml_path) return false;
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->unit = unit;
    ctrl->sample_rate = unit->ctx.sample_rate > 0.0f ? unit->ctx.sample_rate : 48000.0f;
    ctrl->chunk_length = unit->ctx.chunk_length > 0 ? unit->ctx.chunk_length : RT_CHUNK_LENGTH;

    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0) return false;

    uc_unit uc;
    uc_error err = {0};
    uc_status status = uc_load_file(unit_yaml_path, &arena, &uc, &err);
    if (status != UC_OK) {
        fprintf(stderr, "ctrls: [%d:%d] %s\n", err.loc.line, err.loc.col, err.msg);
        uc_arena_free(&arena);
        return false;
    }

    for (size_t i = 0; i < uc.params_len && ctrl->n_params < CTRL_MAX_PARAMS; i++) {
        ctrl_param_t *param = &ctrl->params[ctrl->n_params++];
        memset(param, 0, sizeof(*param));
        strncpy(param->name, uc.params[i].name, sizeof(param->name) - 1);
        param->current = (float)uc.params[i].def;
        param->target = param->current;
        param->min = (float)uc.params[i].min;
        param->max = (float)uc.params[i].max;
        param->smoothing_ms = CTRL_DEFAULT_SMOOTHING_MS;
    }

    ctrl_discover_bindings(ctrl, &uc);

    for (int i = 0; i < ctrl->n_params; i++) {
        ctrl_apply_param(ctrl, &ctrl->params[i]);
    }

    uc_arena_free(&arena);
    return true;
}

void ctrl_unit_destroy(ctrl_unit_t *ctrl) {
    if (!ctrl) return;
    memset(ctrl, 0, sizeof(*ctrl));
}

bool ctrl_unit_set_target(ctrl_unit_t *ctrl, const char *name, float value) {
    ctrl_param_t *param = ctrl_find_param(ctrl, name);
    if (!param) return false;
    param->target = ctrl_clamp_param(param, value);
    return true;
}

bool ctrl_unit_set_smoothing_ms(ctrl_unit_t *ctrl, const char *name, float smoothing_ms) {
    ctrl_param_t *param = ctrl_find_param(ctrl, name);
    if (!param) return false;
    if (smoothing_ms < 0.0f) smoothing_ms = 0.0f;
    param->smoothing_ms = smoothing_ms;
    return true;
}

bool ctrl_unit_set_all_smoothing_ms(ctrl_unit_t *ctrl, float smoothing_ms) {
    if (!ctrl) return false;
    if (smoothing_ms < 0.0f) smoothing_ms = 0.0f;
    for (int i = 0; i < ctrl->n_params; i++) {
        ctrl->params[i].smoothing_ms = smoothing_ms;
    }
    return true;
}

void ctrl_unit_tick(ctrl_unit_t *ctrl, int n_samples) {
    if (!ctrl || !ctrl->unit) return;
    if (n_samples <= 0) n_samples = ctrl->chunk_length;

    for (int i = 0; i < ctrl->n_params; i++) {
        ctrl_param_t *param = &ctrl->params[i];
        float target = ctrl_clamp_param(param, param->target);

        if (param->smoothing_ms <= 0.0f || ctrl->sample_rate <= 0.0f) {
            param->current = target;
        } else {
            float tau_samples = (param->smoothing_ms * 0.001f) * ctrl->sample_rate;
            float coeff = expf(-(float)n_samples / (tau_samples + 1.0f));
            param->current = target + (param->current - target) * coeff;
        }

        ctrl_apply_param(ctrl, param);
    }
}

void ctrl_unit_process(ctrl_unit_t *ctrl, float *in, float *out) {
    if (!ctrl || !ctrl->unit) return;
    ctrl_unit_tick(ctrl, ctrl->chunk_length);
    runtime_unit_process(ctrl->unit, in, out);
}
