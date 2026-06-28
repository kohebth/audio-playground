#ifndef AUDIO_PLAYGROUND_CTRLS_H
#define AUDIO_PLAYGROUND_CTRLS_H

#include <runtime.h>
#include <stdbool.h>

#define CTRL_MAX_PARAMS   64
#define CTRL_MAX_BINDINGS 256

typedef struct {
    int   step_index;
    char  field_name[64];
    void *field_addr;
    int   field_type;
} ctrl_config_binding_t;

typedef struct {
    char  name[64];
    float current;
    float target;
    float min;
    float max;
    float smoothing_ms;

    ctrl_config_binding_t bindings[CTRL_MAX_BINDINGS];
    int                   n_bindings;
} ctrl_param_t;

typedef struct {
    runtime_unit_t *unit;
    float           sample_rate;
    int             chunk_length;
    ctrl_param_t    params[CTRL_MAX_PARAMS];
    int             n_params;
} ctrl_unit_t;

bool ctrl_unit_init(ctrl_unit_t *ctrl, runtime_unit_t *unit, const char *unit_yaml_path);
void ctrl_unit_destroy(ctrl_unit_t *ctrl);

bool ctrl_unit_set_target(ctrl_unit_t *ctrl, const char *name, float value);
bool ctrl_unit_set_smoothing_ms(ctrl_unit_t *ctrl, const char *name, float smoothing_ms);
bool ctrl_unit_set_all_smoothing_ms(ctrl_unit_t *ctrl, float smoothing_ms);

void ctrl_unit_tick(ctrl_unit_t *ctrl, int n_samples);
bool ctrl_unit_process_frames(ctrl_unit_t *ctrl, float *in, float *out, uint32_t frames);
void ctrl_unit_process(ctrl_unit_t *ctrl, float *in, float *out);

#endif
