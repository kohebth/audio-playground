#ifndef AUDIO_PLAYGROUND_RUNTIME_H
#define AUDIO_PLAYGROUND_RUNTIME_H

#include <atom_registry.h>
#include <stdbool.h>
#include <stdint.h>

// ─────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────

#define RT_MAX_SIGNALS   32
#define RT_MAX_STEPS     32
#define RT_MAX_PARAMS    16
#define RT_MAX_INTERNALS 16
#define RT_CHUNK_LENGTH  512

// ─────────────────────────────────────────────
// Runtime context — host-provided values
// ─────────────────────────────────────────────

typedef struct {
    float    sample_rate;
    int      chunk_length;
} runtime_context_t;

// ─────────────────────────────────────────────
// Signal table — named float buffers
// ─────────────────────────────────────────────

typedef struct {
    const char *name;
    float      *buffer;        // points into signal_pool
} rt_signal_t;

// ─────────────────────────────────────────────
// Named value — for params and internals
// ─────────────────────────────────────────────

typedef struct {
    const char *name;
    float       value;
} rt_named_value_t;

// ─────────────────────────────────────────────
// Pipeline step — one atom invocation
// ─────────────────────────────────────────────

typedef struct {
    const char                *id;        // step id from YAML
    const atom_registry_entry_t *atom;   // resolved atom entry
    void *out;                            // allocated out struct
    void *in;                             // allocated in struct
    void *config;                         // allocated config struct
    void *state;                          // allocated state struct
} rt_step_t;

// ─────────────────────────────────────────────
// Unit instance — a fully instantiated unit
// ─────────────────────────────────────────────

typedef struct {
    char            name[64];
    runtime_context_t ctx;

    // User-facing params
    rt_named_value_t params[RT_MAX_PARAMS];
    int              n_params;

    // Designer internals
    rt_named_value_t internals[RT_MAX_INTERNALS];
    int              n_internals;

    // Signal table
    rt_signal_t      signals[RT_MAX_SIGNALS];
    int              n_signals;

    // Pipeline
    rt_step_t        steps[RT_MAX_STEPS];
    int              n_steps;

    // Memory pool for signal buffers
    float           *signal_pool;
    int              signal_pool_count;

    // Memory pool for state buffers (delay lines)
    float           *state_pool;
    int              state_pool_used;

    // Dirty flag — reconfigure on param change
    volatile bool    is_changed;
} runtime_unit_t;

// ─────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────

// Load a unit from a YAML file and instantiate it
runtime_unit_t *runtime_unit_load(const char *yaml_path, runtime_context_t ctx);

// Process one chunk of audio through the unit
void runtime_unit_process(runtime_unit_t *unit, float *in, float *out);

// Update a user-facing parameter by name
bool runtime_unit_set_param(runtime_unit_t *unit, const char *name, float value);

// Free all resources
void runtime_unit_destroy(runtime_unit_t *unit);

#endif // AUDIO_PLAYGROUND_RUNTIME_H