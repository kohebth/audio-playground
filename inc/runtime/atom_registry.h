#ifndef ATOM_REGISTRY_H
#define ATOM_REGISTRY_H

#include <stddef.h>
#include <stdbool.h>

// ─────────────────────────────────────────────
// Uniform calling convention for all atoms
// ─────────────────────────────────────────────

typedef struct {
    void *out;      // pointer to allocated out struct (passed by value via thunk)
    void *in;       // pointer to allocated in struct  (passed by value via thunk)
    void *config;   // pointer to allocated params struct
    void *state;    // pointer to allocated state struct
} atom_call_t;

typedef void (*atom_thunk_fn)(atom_call_t *call);

// ─────────────────────────────────────────────
// Field descriptor — describes one struct member
// ─────────────────────────────────────────────

typedef enum {
    FIELD_FLOAT,        // float scalar
    FIELD_INT,          // int scalar
    FIELD_SIGNAL,       // float* (pointer to signal buffer)
    FIELD_BUFFER,       // float* (pointer to separately allocated buffer)
    FIELD_FLOAT_PTR,    // float* (pointer, e.g., transfer table)
    FIELD_FLOAT_PP,     // float** (pointer to pointer array)
} atom_field_type_t;

typedef struct {
    const char       *name;     // field name (matches YAML key)
    atom_field_type_t type;     // field type
    size_t            offset;   // offsetof within the struct
} atom_field_desc_t;

// ─────────────────────────────────────────────
// Registry entry — one per atom function
// ─────────────────────────────────────────────

typedef struct {
    const char              *name;          // atom name, e.g. "detect_envelope"
    atom_thunk_fn            thunk;         // generic wrapper function

    const atom_field_desc_t *out_fields;
    int                      n_out;
    size_t                   out_size;      // sizeof(out struct)

    const atom_field_desc_t *in_fields;
    int                      n_in;
    size_t                   in_size;       // sizeof(in struct), 0 if void*

    const atom_field_desc_t *config_fields;
    int                      n_config;
    size_t                   config_size;   // sizeof(params struct), 0 if void*

    const atom_field_desc_t *state_fields;
    int                      n_state;
    size_t                   state_size;    // sizeof(state struct), 0 if void*
} atom_registry_entry_t;

// ─────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────

// Initialize the global atom registry (call once at startup)
void atom_registry_init(void);

// Find an atom by name. Returns NULL if not found.
const atom_registry_entry_t *atom_registry_find(const char *name);

// Get the total number of registered atoms
int atom_registry_count(void);

// Get atom entry by index (for iteration)
const atom_registry_entry_t *atom_registry_get(int index);

#endif // ATOM_REGISTRY_H
