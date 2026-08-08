#ifndef AUDIO_PLAYGROUND_APGCORE_VALUE_V2_H
#define AUDIO_PLAYGROUND_APGCORE_VALUE_V2_H

typedef enum {
    APG_V2_VALUE_LITERAL,
    APG_V2_VALUE_VARREF,
} apg_v2_value_kind_t;

typedef struct {
    apg_v2_value_kind_t kind;
    const char         *text;
} apg_v2_value_t;

#endif // AUDIO_PLAYGROUND_APGCORE_VALUE_V2_H
