#ifndef AUDIO_PLAYGROUND_CTRLS_H
#define AUDIO_PLAYGROUND_CTRLS_H

#include <runtime.h>
#include <stdbool.h>

#define CTRL_MAX_PARAMS   64
#define CTRL_MAX_BINDINGS 256

#if defined(APG_ENABLE_V1_DEPRECATED_WARNINGS)
#if defined(__GNUC__) || defined(__clang__)
#define APG_CTRL_V1_DEPRECATED __attribute__((deprecated("APG v1 ctrl adapter is legacy; use APGCore v2 controls")))
#else
#define APG_CTRL_V1_DEPRECATED
#endif
#else
#define APG_CTRL_V1_DEPRECATED
#endif

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

// Legacy v1 control adapter. Keep only while v1 runtime/unit tests depend on it.
APG_CTRL_V1_DEPRECATED bool ctrl_unit_init(ctrl_unit_t *ctrl, runtime_unit_t *unit, const char *unit_yaml_path);
APG_CTRL_V1_DEPRECATED void ctrl_unit_destroy(ctrl_unit_t *ctrl);

APG_CTRL_V1_DEPRECATED bool ctrl_unit_set_target(ctrl_unit_t *ctrl, const char *name, float value);
APG_CTRL_V1_DEPRECATED bool ctrl_unit_set_smoothing_ms(ctrl_unit_t *ctrl, const char *name, float smoothing_ms);
APG_CTRL_V1_DEPRECATED bool ctrl_unit_set_all_smoothing_ms(ctrl_unit_t *ctrl, float smoothing_ms);

APG_CTRL_V1_DEPRECATED void ctrl_unit_tick(ctrl_unit_t *ctrl, int n_samples);
APG_CTRL_V1_DEPRECATED bool ctrl_unit_process_frames(ctrl_unit_t *ctrl, float *in, float *out, uint32_t frames);
APG_CTRL_V1_DEPRECATED void ctrl_unit_process(ctrl_unit_t *ctrl, float *in, float *out);

#endif
