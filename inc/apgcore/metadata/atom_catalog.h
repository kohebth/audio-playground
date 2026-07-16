#ifndef AUDIO_PLAYGROUND_APGCORE_ATOM_CATALOG_H
#define AUDIO_PLAYGROUND_APGCORE_ATOM_CATALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    APG_ATOM_CONTRACT_IN,
    APG_ATOM_CONTRACT_OUT,
    APG_ATOM_CONTRACT_CONFIG,
    APG_ATOM_CONTRACT_STATE,
} apg_atom_contract_section_t;

typedef enum {
    APG_ATOM_FIELD_UNKNOWN,
    APG_ATOM_FIELD_SIGNAL,
    APG_ATOM_FIELD_SIGNAL_OPTIONAL,
    APG_ATOM_FIELD_SIGNAL_ARRAY,
    APG_ATOM_FIELD_SCALAR,
    APG_ATOM_FIELD_FLOAT,
    APG_ATOM_FIELD_INT,
    APG_ATOM_FIELD_BUFFER,
    APG_ATOM_FIELD_FLOAT_MATRIX,
} apg_atom_contract_field_type_t;

typedef enum {
    APG_ATOM_VISIBILITY_UNKNOWN,
    APG_ATOM_VISIBILITY_PUBLIC,
    APG_ATOM_VISIBILITY_ADVANCED,
    APG_ATOM_VISIBILITY_INTERNAL,
} apg_atom_visibility_t;

typedef struct {
    const char                    *name;
    apg_atom_contract_field_type_t type;
    bool                           required;
    const char                    *parameter_type;
    const char                    *default_json;
    bool                           has_min;
    double                         min_value;
    bool                           has_max;
    double                         max_value;
    const char                    *unit;
    const char                    *scale;
    bool                           realtime;
    bool                           has_smoothing_ms;
    double                         smoothing_ms;
    bool                           structural;
    const char *const             *options;
    const int                     *option_values;
    size_t                         options_len;
} apg_atom_contract_field_t;

bool                  apg_atom_profile_known(const char *profile);
bool                  apg_atom_known(const char *name);
apg_atom_visibility_t apg_atom_visibility(const char *name);
bool                  apg_atom_profile_supported(const char *name, const char *profile);
size_t                apg_atom_contract_field_count(const char *atom, apg_atom_contract_section_t section);
bool                  apg_atom_contract_field(
                     const char *atom, apg_atom_contract_section_t section, size_t index, apg_atom_contract_field_t *out
                 );
bool apg_atom_contract_find_field(
    const char *atom, apg_atom_contract_section_t section, const char *key, apg_atom_contract_field_t *out
);
bool apg_atom_contract_field_required(const char *atom, apg_atom_contract_section_t section, const char *key);
apg_atom_contract_field_type_t
apg_atom_contract_field_type(const char *atom, apg_atom_contract_section_t section, const char *key);

/*
 * Write a UI-facing JSON catalog for the registered atom table.
 * The output is deterministic and intended for tooling/tests; callers own the FILE stream.
 */
void apg_atom_catalog_write_json(FILE *out);

#endif // AUDIO_PLAYGROUND_APGCORE_ATOM_CATALOG_H
