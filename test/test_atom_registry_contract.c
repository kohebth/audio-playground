#include <atom/atom_capability.h>
#include <atom/atom_definitions.h>
#include <atom_registry.h>

#include <stdio.h>
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
} canonical_atom_t;

#define CANONICAL_ATOM(name, category, input_count, config_count, state_count, flags, maturity, dispatch) \
    {#name, #category, input_count, config_count, state_count, flags, maturity},
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
            actual->maturity != expected->maturity)
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
    return 0;
}
