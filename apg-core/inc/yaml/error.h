#ifndef UNITCFG_ERROR_H
#define UNITCFG_ERROR_H

typedef enum {
    UC_OK = 0,
    UC_E_IO,
    UC_E_OOM,
    UC_E_LEX,
    UC_E_PARSE,
    UC_E_TYPE,
    UC_E_RANGE,
    UC_E_MISSING
} uc_status;

typedef struct {
    int line;
    int col;
} uc_loc;

typedef struct {
    uc_status status;
    uc_loc    loc;
    char      msg[128];
} uc_error;

void uc_error_set(uc_error *e, uc_status s, uc_loc loc, const char *fmt, ...);
const char *uc_status_str(uc_status s);

#endif