#ifndef ATOM_REGISTRY_H
#define ATOM_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <apgcore/runtime/process.h>
#include <apgcore/runtime/spectral.h>
#include <apgcore/runtime/stream.h>

typedef struct {
    void                        *out;
    const void                  *in;
    const void                  *config;
    void                        *state;
    const apg_process_context_t *context;
    const apg_spectral_info_t   *spectral_info;
    const apg_stream_context_t  *stream_context;
    apg_stream_result_t          stream_result;
} atom_call_t;

typedef void (*atom_thunk_fn)(atom_call_t *call);

typedef enum {
    APG_ATOM_DISPATCH_PROCESS,
    APG_ATOM_DISPATCH_FFT,
    APG_ATOM_DISPATCH_IFFT,
    APG_ATOM_DISPATCH_MULTIPLY,
    APG_ATOM_DISPATCH_WINDOW,
    APG_ATOM_DISPATCH_OVERLAP_ADD,
    APG_ATOM_DISPATCH_OVERLAP_SAVE,
    APG_ATOM_DISPATCH_STREAM,
} apg_atom_dispatch_t;

// ─────────────────────────────────────────────
// Field descriptor — describes one struct member
// ─────────────────────────────────────────────

typedef enum {
    FIELD_FLOAT,     // float scalar
    FIELD_INT,       // int scalar
    FIELD_SIGNAL,    // float* (pointer to signal buffer)
    FIELD_BUFFER,    // float* (pointer to separately allocated buffer)
    FIELD_FLOAT_PTR, // float* (pointer, e.g., transfer table)
    FIELD_FLOAT_PP,  // float** (pointer to pointer array)
} atom_field_type_t;

typedef struct {
    const char       *name;           // field name (matches YAML key)
    atom_field_type_t type;           // field type
    size_t            offset;         // offsetof within the struct
    size_t            buffer_samples; // FIELD_BUFFER capacity in float samples
} atom_field_desc_t;

// ─────────────────────────────────────────────
// Registry entry — one per atom function
// ─────────────────────────────────────────────

typedef struct {
    const char              *name;         // atom name, e.g. "detect_envelope"
    const char              *category;     // atom category, e.g. "detect"
    atom_thunk_fn            thunk;        // generic wrapper function
    apg_atom_dispatch_t      dispatch;     // process/spectral/stream execution contract
    size_t                   out_size;     // sizeof(out struct)
    size_t                   in_size;      // sizeof(in struct), 0 if void*
    size_t                   config_size;  // sizeof(params struct), 0 if void*
    size_t                   state_size;   // sizeof(state struct), 0 if void*
    const atom_field_desc_t *input_fields; // optional input layout descriptors
    int                      n_input_fields;
    const atom_field_desc_t *state_fields; // layout descriptors for state
    int                      n_state_fields;
    const atom_field_desc_t *config_fields; // layout descriptors for config
    int                      n_config_fields;
    uint32_t                 flags;    // APG_ATOM_* capability flags
    uint32_t                 maturity; // apg_atom_maturity_t value
} atom_registry_entry_t;

// Return input-field descriptors for an atom registry entry.
// `out_len` receives the number of descriptors when available.
const atom_field_desc_t *atom_registry_in_fields(const atom_registry_entry_t *atom, size_t *out_len);

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
