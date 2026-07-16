#include <atom/atom_capability.h>
#include <atom/atom_definitions.h>
#include <atom/dsp_atoms.h>
#include <atom_registry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int fail_entry(const char *msg, const atom_registry_entry_t *entry) {
    fprintf(stderr, "FAIL: %s: %s\n", msg, entry && entry->name ? entry->name : "<null>");
    return 1;
}

static int test_registry_entries_are_complete(void) {
    atom_registry_init();

    int count = atom_registry_count();
    if (count <= 0)
        return fail("atom registry is empty");

    for (int i = 0; i < count; i++) {
        const atom_registry_entry_t *entry = atom_registry_get(i);
        if (!entry)
            return fail("atom_registry_get returned NULL for a valid index");
        if (!entry->name || entry->name[0] == '\0')
            return fail_entry("registry entry has no name", entry);
        if (!entry->category || entry->category[0] == '\0')
            return fail_entry("registry entry has no category", entry);
        if (!entry->thunk)
            return fail_entry("registry entry has no thunk", entry);
        if (entry->flags == 0u)
            return fail_entry("registry entry has no capability flags", entry);
        if (entry->maturity > APG_ATOM_MATURITY_PRODUCTION)
            return fail_entry("registry entry has invalid maturity", entry);
    }

    return 0;
}

typedef struct {
    const char *name;
    const char *category;
    int         input_count;
    int         config_count;
    int         state_count;
    uint32_t    flags;
    uint32_t    maturity;
    size_t      out_size;
    size_t      in_size;
    size_t      config_size;
    size_t      state_size;
} canonical_atom_t;

#define CANONICAL_ATOM(name, category, input_count, config_count, state_count, flags, maturity, dispatch) \
    {                                                                                                     \
        #name,                                                                                            \
        #category,                                                                                        \
        input_count,                                                                                      \
        config_count,                                                                                     \
        state_count,                                                                                      \
        flags,                                                                                            \
        maturity,                                                                                         \
        sizeof(name##_out_t),                                                                             \
        sizeof(name##_in_t),                                                                              \
        sizeof(name##_params_t),                                                                          \
        sizeof(name##_state_t),                                                                           \
    },
static const canonical_atom_t canonical_atoms[] = {APG_ATOM_DEFINITIONS(CANONICAL_ATOM)};
#undef CANONICAL_ATOM

static int test_registry_matches_canonical_definitions(void) {
    const size_t canonical_count = sizeof(canonical_atoms) / sizeof(canonical_atoms[0]);
    if ((size_t)atom_registry_count() != canonical_count)
        return fail("registry count differs from canonical atom definitions");
    for (size_t i = 0; i < canonical_count; i++) {
        const canonical_atom_t      *expected = &canonical_atoms[i];
        const atom_registry_entry_t *actual   = atom_registry_get((int)i);
        if (!actual || strcmp(actual->name, expected->name) != 0 || strcmp(actual->category, expected->category) != 0 ||
            actual->n_input_fields != expected->input_count || actual->n_config_fields != expected->config_count ||
            actual->n_state_fields != expected->state_count || actual->flags != expected->flags ||
            actual->maturity != expected->maturity || actual->out_size != expected->out_size ||
            actual->in_size != expected->in_size || actual->config_size != expected->config_size ||
            actual->state_size != expected->state_size)
            return fail_entry("registry row differs from canonical definition", actual);
    }
    return 0;
}

#define CONTRACT_ATOM_NAME(name, input_profile, output_profile, config_profile) #name,
static const char *const contract_atom_names[] = {APG_ATOM_CONTRACT_DEFINITIONS(CONTRACT_ATOM_NAME)};
#undef CONTRACT_ATOM_NAME

static int test_contract_atoms_are_canonical(void) {
    for (size_t i = 0; i < sizeof(contract_atom_names) / sizeof(contract_atom_names[0]); i++) {
        if (!atom_registry_find(contract_atom_names[i]))
            return fail("contract definition references an unregistered atom");
        for (size_t j = i + 1u; j < sizeof(contract_atom_names) / sizeof(contract_atom_names[0]); j++) {
            if (strcmp(contract_atom_names[i], contract_atom_names[j]) == 0)
                return fail("canonical contract atom is duplicated");
        }
    }
    return 0;
}

static int test_registry_names_are_unique(void) {
    int count = atom_registry_count();
    for (int i = 0; i < count; i++) {
        const atom_registry_entry_t *a = atom_registry_get(i);
        if (!a || !a->name)
            return fail("registry contains an unnamed atom");

        for (int j = i + 1; j < count; j++) {
            const atom_registry_entry_t *b = atom_registry_get(j);
            if (!b || !b->name)
                return fail("registry contains an unnamed atom");
            if (strcmp(a->name, b->name) == 0) {
                fprintf(stderr, "FAIL: duplicate atom registry entry: %s\n", a->name);
                return 1;
            }
        }
    }
    return 0;
}

static int test_registry_find_returns_canonical_entry(void) {
    int count = atom_registry_count();
    for (int i = 0; i < count; i++) {
        const atom_registry_entry_t *entry = atom_registry_get(i);
        const atom_registry_entry_t *found = atom_registry_find(entry->name);
        if (found != entry) {
            fprintf(stderr, "FAIL: atom_registry_find did not return canonical entry for %s\n", entry->name);
            return 1;
        }
    }
    return 0;
}

static int test_legacy_atoms_are_marked_experimental(void) {
    const char *legacy_names[] = {"freq_fft", "freq_ifft", "freq_multiply"};
    for (size_t i = 0; i < sizeof(legacy_names) / sizeof(legacy_names[0]); i++) {
        const atom_registry_entry_t *entry = atom_registry_find(legacy_names[i]);
        if (!entry)
            return fail("legacy atom is missing from registry");
        if ((entry->flags & APG_ATOM_LEGACY) == 0u)
            return fail_entry("legacy atom is not marked legacy", entry);
        if ((entry->flags & APG_ATOM_EXPERIMENTAL) == 0u)
            return fail_entry("legacy atom is not marked experimental", entry);
        if ((entry->flags & APG_ATOM_RT_SAFE) != 0u)
            return fail_entry("legacy atom should not be marked realtime-safe", entry);
        if (entry->maturity != APG_ATOM_MATURITY_EXPERIMENTAL)
            return fail_entry("legacy atom should have experimental maturity", entry);
    }
    return 0;
}

static void initialize_pointer_slots(void *storage, size_t size, float *samples) {
    for (size_t offset = 0u; offset + sizeof(samples) <= size; offset += sizeof(samples))
        memcpy((char *)storage + offset, &samples, sizeof(samples));
}

static void initialize_described_fields(
    void *storage, const atom_field_desc_t *fields, int field_count, float *samples, float **signal_array
) {
    for (int i = 0; storage && fields && i < field_count; i++) {
        const atom_field_desc_t *field = &fields[i];
        if (field->type == FIELD_SIGNAL || field->type == FIELD_BUFFER || field->type == FIELD_FLOAT_PTR) {
            memcpy((char *)storage + field->offset, &samples, sizeof(samples));
        } else if (field->type == FIELD_FLOAT_PP) {
            memcpy((char *)storage + field->offset, &signal_array, sizeof(signal_array));
        } else if (field->type == FIELD_INT && field->name && strcmp(field->name, "buffer_len") == 0) {
            const uint32_t buffer_len = 4096u;
            memcpy((char *)storage + field->offset, &buffer_len, sizeof(buffer_len));
        } else if (field->type == FIELD_INT) {
            const int value = 0;
            memcpy((char *)storage + field->offset, &value, sizeof(value));
        } else if (field->type == FIELD_FLOAT) {
            const float value = 0.0f;
            memcpy((char *)storage + field->offset, &value, sizeof(value));
        }
    }
}

static int atom_uses_spectral_context(const char *name) {
    return name && (strcmp(name, "freq_fft") == 0 || strcmp(name, "freq_ifft") == 0 ||
                    strcmp(name, "freq_multiply") == 0 || strcmp(name, "freq_window") == 0 ||
                    strcmp(name, "freq_overlap_add") == 0 || strcmp(name, "freq_overlap_save") == 0);
}

static int test_all_atoms_accept_required_frame_sizes(void) {
    static const uint32_t     frame_sizes[]   = {0u, 1u, 64u, 128u, 256u, 512u, 1024u};
    static float              samples[192000] = {0};
    float                    *signal_array[2] = {samples, samples};
    const apg_spectral_info_t spectral        = {.fft_size = 256u, .bin_count = 129u, .hop_size = 128u};

    for (int atom_index = 0; atom_index < atom_registry_count(); atom_index++) {
        const atom_registry_entry_t *entry       = atom_registry_get(atom_index);
        size_t                       out_size    = entry->out_size ? entry->out_size : 1u;
        size_t                       in_size     = entry->in_size ? entry->in_size : 1u;
        size_t                       config_size = entry->config_size ? entry->config_size : 1u;
        size_t                       state_size  = entry->state_size ? entry->state_size : 1u;
        void                        *out         = calloc(1u, out_size);
        void                        *in          = calloc(1u, in_size);
        void                        *config      = calloc(1u, config_size);
        void                        *state       = calloc(1u, state_size);
        if (!out || !in || !config || !state) {
            free(out);
            free(in);
            free(config);
            free(state);
            return fail_entry("frame smoke storage allocation failed", entry);
        }

        initialize_pointer_slots(out, out_size, samples);
        initialize_pointer_slots(in, in_size, samples);
        initialize_described_fields(in, entry->input_fields, entry->n_input_fields, samples, signal_array);
        initialize_described_fields(config, entry->config_fields, entry->n_config_fields, samples, signal_array);
        initialize_described_fields(state, entry->state_fields, entry->n_state_fields, samples, signal_array);

        entry->thunk(NULL);
        atom_call_t incomplete_call = {
            .out           = out,
            .in            = in,
            .config        = config,
            .state         = state,
            .spectral_info = &spectral,
        };
        incomplete_call.out = NULL;
        entry->thunk(&incomplete_call);
        incomplete_call.out = out;
        incomplete_call.in  = NULL;
        entry->thunk(&incomplete_call);
        incomplete_call.in     = in;
        incomplete_call.config = NULL;
        entry->thunk(&incomplete_call);
        incomplete_call.config = config;
        incomplete_call.state  = NULL;
        entry->thunk(&incomplete_call);

        for (size_t frame_index = 0u; frame_index < sizeof(frame_sizes) / sizeof(frame_sizes[0]); frame_index++) {
            const uint32_t frames = frame_sizes[frame_index];
            for (size_t sample_index = 0u; sample_index < 8192u; sample_index++)
                samples[sample_index] = frames == 0u ? -99.0f : 0.0f;
            const apg_process_info_t info = {
                .sample_rate   = 48000.0f,
                .frames        = frames,
                .output_frames = frames,
                .channels      = 1u,
            };
            atom_call_t call = {
                .out           = out,
                .in            = in,
                .config        = config,
                .state         = state,
                .info          = &info,
                .spectral_info = &spectral,
            };
            entry->thunk(&call);
            if (frames == 0u && !atom_uses_spectral_context(entry->name)) {
                for (size_t sample_index = 0u; sample_index < 1024u; sample_index++) {
                    if (samples[sample_index] != -99.0f) {
                        free(out);
                        free(in);
                        free(config);
                        free(state);
                        return fail_entry("zero-frame dispatch modified sample storage", entry);
                    }
                }
            }
        }

        free(out);
        free(in);
        free(config);
        free(state);
    }
    return 0;
}

static int test_all_process_entries_accept_null_abi(void) {
#define CALL_PROCESS_WITH_NULL_ABI(name, category, input_count, config_count, state_count, flags, maturity, dispatch) \
    name##_process(NULL, NULL, NULL, NULL, NULL);
    APG_ATOM_DEFINITIONS(CALL_PROCESS_WITH_NULL_ABI)
#undef CALL_PROCESS_WITH_NULL_ABI
    freq_window_spectral_process(NULL, NULL, NULL, NULL, NULL);
    freq_overlap_add_spectral_process(NULL, NULL, NULL, NULL, NULL);
    freq_overlap_save_spectral_process(NULL, NULL, NULL, NULL, NULL);
    return 0;
}

int main(void) {
    if (test_registry_entries_are_complete())
        return 1;
    if (test_registry_matches_canonical_definitions())
        return 1;
    if (test_contract_atoms_are_canonical())
        return 1;
    if (test_registry_names_are_unique())
        return 1;
    if (test_registry_find_returns_canonical_entry())
        return 1;
    if (test_legacy_atoms_are_marked_experimental())
        return 1;
    if (test_all_atoms_accept_required_frame_sizes())
        return 1;
    if (test_all_process_entries_accept_null_abi())
        return 1;
    return 0;
}
