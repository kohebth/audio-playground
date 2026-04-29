#ifndef UNITCFG_UNIT_H
#define UNITCFG_UNIT_H

#include <stddef.h>

typedef enum { UC_VAL_LITERAL, UC_VAL_VARREF } uc_value_kind;

typedef struct {
    uc_value_kind kind;
    const char   *text;
} uc_value;

typedef struct {
    const char *key;
    uc_value    value;
} uc_kv;

typedef struct {
    const char *key;
    double      value;
} uc_kv_d;

typedef struct {
    const char *name;
    double      min;
    double      max;
    double      def;
    const char *unit;
} uc_param;

typedef struct {
    const char *id;
    const char *fn;

    uc_kv      *in;     size_t in_len;
    uc_kv      *out;    size_t out_len;
    uc_kv      *config; size_t config_len;
    uc_kv_d    *state;  size_t state_len;
} uc_stage;

typedef struct {
    int sample_rate;
    int channel;
} uc_system;

typedef struct {
    const char  *name;
    const char  *version;
    uc_system    system;

    uc_param    *params;   size_t params_len;
    uc_kv_d     *internal; size_t internal_len;

    const char **signals;  size_t signals_len;

    uc_stage    *pipeline; size_t pipeline_len;
} uc_unit;

#endif