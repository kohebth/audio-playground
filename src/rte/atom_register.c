#include <atom/atom_definitions.h>
#include <atom/atom_field_descriptors.h>
#include <atom/dsp_atoms.h>
#include <atom_registry.h>
#include <atom_thunk.h>

#include <stddef.h>
#include <string.h>

#define APG_REGISTRY_INPUT_0(name)                .input_fields = NULL, .n_input_fields = 0
#define APG_REGISTRY_INPUT_1(name)                .input_fields = name##_in_fields, .n_input_fields = 1
#define APG_REGISTRY_INPUT_2(name)                .input_fields = name##_in_fields, .n_input_fields = 2
#define APG_REGISTRY_CONFIG_0(name)               .config_fields = NULL, .n_config_fields = 0
#define APG_REGISTRY_CONFIG_1(name)               .config_fields = name##_config_fields, .n_config_fields = 1
#define APG_REGISTRY_CONFIG_2(name)               .config_fields = name##_config_fields, .n_config_fields = 2
#define APG_REGISTRY_CONFIG_3(name)               .config_fields = name##_config_fields, .n_config_fields = 3
#define APG_REGISTRY_CONFIG_4(name)               .config_fields = name##_config_fields, .n_config_fields = 4
#define APG_REGISTRY_CONFIG_5(name)               .config_fields = name##_config_fields, .n_config_fields = 5
#define APG_REGISTRY_STATE_0(name)                .state_fields = NULL, .n_state_fields = 0
#define APG_REGISTRY_STATE_1(name)                .state_fields = name##_state_fields, .n_state_fields = 1
#define APG_REGISTRY_STATE_2(name)                .state_fields = name##_state_fields, .n_state_fields = 2
#define APG_REGISTRY_STATE_3(name)                .state_fields = name##_state_fields, .n_state_fields = 3
#define APG_REGISTRY_STATE_4(name)                .state_fields = name##_state_fields, .n_state_fields = 4
#define APG_REGISTRY_STATE_5(name)                .state_fields = name##_state_fields, .n_state_fields = 5
#define APG_EXPAND_FIELD(kind, count, name)       APG_EXPAND_FIELD_INNER(kind, count, name)
#define APG_EXPAND_FIELD_INNER(kind, count, name) APG_REGISTRY_##kind##_##count(name)

#define APG_REGISTRY_ATOM(                                                                                   \
    atom_name, category_name, input_count, config_count, state_count, capabilities, maturity_level, dispatch \
)                                                                                                            \
    {                                                                                                        \
        .name        = #atom_name,                                                                           \
        .category    = #category_name,                                                                       \
        .thunk       = atom_name##_thunk,                                                                    \
        .out_size    = sizeof(atom_name##_out_t),                                                            \
        .in_size     = sizeof(atom_name##_in_t),                                                             \
        .config_size = sizeof(atom_name##_params_t),                                                         \
        .state_size  = sizeof(atom_name##_state_t),                                                          \
        APG_EXPAND_FIELD(INPUT, input_count, atom_name),                                                     \
        APG_EXPAND_FIELD(CONFIG, config_count, atom_name),                                                   \
        APG_EXPAND_FIELD(STATE, state_count, atom_name),                                                     \
        .flags    = capabilities,                                                                            \
        .maturity = maturity_level,                                                                          \
    },

static atom_registry_entry_t g_registry[]     = {APG_ATOM_DEFINITIONS(APG_REGISTRY_ATOM)};
static const int             g_registry_count = (int)(sizeof(g_registry) / sizeof(g_registry[0]));

void atom_registry_init(void) {}

const atom_field_desc_t *atom_registry_in_fields(const atom_registry_entry_t *atom, size_t *out_len) {
    if (out_len)
        *out_len = atom && atom->n_input_fields > 0 ? (size_t)atom->n_input_fields : 0u;
    return atom ? atom->input_fields : NULL;
}

const atom_registry_entry_t *atom_registry_find(const char *name) {
    for (int i = 0; name && i < g_registry_count; i++) {
        if (strcmp(g_registry[i].name, name) == 0)
            return &g_registry[i];
    }
    return NULL;
}

int atom_registry_count(void) { return g_registry_count; }

const atom_registry_entry_t *atom_registry_get(int index) {
    return index >= 0 && index < g_registry_count ? &g_registry[index] : NULL;
}
