#include <apgcore/metadata/atom_catalog.h>
#include <atom/atom_capability.h>
#include <atom_registry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static char *read_catalog_json(void) {
    FILE *file = tmpfile();
    if (!file)
        return NULL;
    apg_atom_catalog_write_json(file);
    long size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *buffer = malloc((size_t)size + 1u);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    size_t read_len = fread(buffer, 1u, (size_t)size, file);
    fclose(file);
    buffer[read_len] = '\0';
    return buffer;
}

static int expect_field(
    const char                    *atom,
    apg_atom_contract_section_t    section,
    const char                    *key,
    apg_atom_contract_field_type_t type,
    bool                           required
) {
    apg_atom_contract_field_t field;
    if (!apg_atom_contract_find_field(atom, section, key, &field))
        return fail("metadata contract field is missing");
    if (field.type != type)
        return fail("metadata contract field type is wrong");
    if (field.required != required)
        return fail("metadata contract field required flag is wrong");
    if (apg_atom_contract_field_required(atom, section, key) != required)
        return fail("metadata required query is wrong");
    if (apg_atom_contract_field_type(atom, section, key) != type)
        return fail("metadata type query is wrong");
    return 0;
}

static int test_runtime_registry_contract(void) {
    atom_registry_init();
    const int count = atom_registry_count();
    if (count <= 0)
        return fail("runtime atom registry is empty");

    for (int i = 0; i < count; ++i) {
        const atom_registry_entry_t *entry = atom_registry_get(i);
        if (!entry || !entry->name || !entry->category || !entry->thunk)
            return fail("runtime atom registry entry is incomplete");
        if (entry->flags == 0u || entry->maturity > APG_ATOM_MATURITY_PRODUCTION)
            return fail("runtime atom registry capability metadata is invalid");
        if (atom_registry_find(entry->name) != entry)
            return fail("runtime atom registry lookup is not canonical");
        if (apg_atom_profile_supported(entry->name, "wasm_realtime") != ((entry->flags & APG_ATOM_WASM_SAFE) != 0u) ||
            apg_atom_profile_supported(entry->name, "m7_static") != ((entry->flags & APG_ATOM_M7_SAFE) != 0u))
            return fail("target profile compatibility is not derived from registry flags");

        const bool expected_wasm = (entry->flags & APG_ATOM_WASM_SAFE) != 0u;
        const bool expected_m7   = (entry->flags & APG_ATOM_M7_SAFE) != 0u;
        if (apg_atom_profile_supported(entry->name, "wasm_realtime") != expected_wasm)
            return fail("wasm profile support does not match registry flags");
        if (apg_atom_profile_supported(entry->name, "m7_static") != expected_m7)
            return fail("m7 profile support does not match registry flags");
        if (!apg_atom_profile_supported(entry->name, "desktop_full") ||
            !apg_atom_profile_supported(entry->name, "offline_render"))
            return fail("desktop/offline profile support is unexpectedly disabled");

        for (int j = i + 1; j < count; ++j) {
            const atom_registry_entry_t *other = atom_registry_get(j);
            if (other && other->name && strcmp(entry->name, other->name) == 0)
                return fail("runtime atom registry contains a duplicate name");
        }
    }
    if (apg_atom_profile_supported("missing_atom", "desktop_full"))
        return fail("desktop profile accepted an unknown atom");

    const char *legacy_names[] = {"freq_fft", "freq_ifft", "freq_multiply"};
    for (size_t i = 0; i < sizeof(legacy_names) / sizeof(legacy_names[0]); ++i) {
        const atom_registry_entry_t *entry = atom_registry_find(legacy_names[i]);
        if (!entry || (entry->flags & APG_ATOM_LEGACY) == 0u || (entry->flags & APG_ATOM_EXPERIMENTAL) == 0u ||
            (entry->flags & APG_ATOM_RT_SAFE) != 0u || entry->maturity != APG_ATOM_MATURITY_EXPERIMENTAL)
            return fail("legacy spectral atom capability classification is invalid");
    }

    return 0;
}

int main(void) {
    char *json = read_catalog_json();
    if (!json)
        return fail("failed to write atom catalog json");

    if (!strstr(json, "\"schema\":\"apg.atom_catalog.v2\""))
        return fail("catalog schema is missing");
    if (!strstr(json, "\"name\":\"generation_dc\""))
        return fail("generation_dc atom is missing");
    if (!strstr(json, "\"category\":\"generation\""))
        return fail("generation category is missing");
    if (!strstr(json, "\"outputs\":[{\"name\":\"signal\",\"type\":\"signal\"}]"))
        return fail("generation_dc output contract is missing");
    if (!strstr(json, "\"config\":[{\"name\":\"value\",\"type\":\"scalar\"}]"))
        return fail("generation_dc config contract is missing");
    if (!strstr(json, "\"name\":\"generation_lfo\""))
        return fail("generation_lfo atom is missing");
    if (!strstr(json, "\"name\":\"frequency\",\"type\":\"float\""))
        return fail("generation_lfo frequency contract is missing");
    if (!strstr(json, "\"name\":\"detect_threshold\""))
        return fail("detect_threshold atom is missing");
    if (!strstr(json, "\"outputs\":[{\"name\":\"gate\",\"type\":\"signal\"}]"))
        return fail("detect_threshold gate output contract is missing");
    if (!strstr(json, "\"name\":\"mix_matrix\""))
        return fail("mix_matrix atom is missing");
    if (!strstr(json, "\"name\":\"signals\",\"type\":\"signal_array\""))
        return fail("signal array contract is missing");
    if (!strstr(json, "\"name\":\"coefficients\",\"type\":\"float_matrix\""))
        return fail("float matrix contract is missing");
    if (!strstr(json, "\"name\":\"delay_line\""))
        return fail("delay_line atom is missing");
    if (!strstr(json, "\"stateful\":true"))
        return fail("stateful flag is missing");
    if (!strstr(json, "\"buffer_samples\":192000"))
        return fail("state buffer capacity is missing");
    if (!strstr(json, "\"profiles\":{\"desktop_full\":true"))
        return fail("profile hints are missing");
    if (!apg_atom_profile_known("desktop_full") || !apg_atom_profile_known("wasm_realtime") ||
        !apg_atom_profile_known("m7_static") || !apg_atom_profile_known("offline_render"))
        return fail("known profile lookup failed");
    if (apg_atom_profile_known("toaster_realtime"))
        return fail("unknown profile lookup succeeded");
    if (!apg_atom_profile_supported("generation_dc", "m7_static"))
        return fail("m7-supported atom profile check failed");
    if (apg_atom_profile_supported("src_downsample", "m7_static"))
        return fail("m7-restricted atom profile check failed");
    if (apg_atom_profile_supported("generation_dc", "toaster_realtime"))
        return fail("unknown profile support check succeeded");
    if (apg_atom_profile_supported(NULL, "m7_static"))
        return fail("missing atom profile check failed");
    if (!apg_atom_known("generation_dc"))
        return fail("known atom lookup failed");
    if (apg_atom_known("not_a_real_atom"))
        return fail("unknown atom lookup succeeded");

    if (apg_atom_contract_field_count("generation_dc", APG_ATOM_CONTRACT_CONFIG) != 1u)
        return fail("generation_dc metadata field count failed");
    if (expect_field("generation_dc", APG_ATOM_CONTRACT_CONFIG, "value", APG_ATOM_FIELD_SCALAR, true))
        return 1;
    if (expect_field("generation_lfo", APG_ATOM_CONTRACT_CONFIG, "sample_rate", APG_ATOM_FIELD_FLOAT, false))
        return 1;
    if (expect_field("filter_biquad", APG_ATOM_CONTRACT_CONFIG, "sample_rate", APG_ATOM_FIELD_FLOAT, false))
        return 1;
    if (expect_field("mix_matrix", APG_ATOM_CONTRACT_IN, "signals", APG_ATOM_FIELD_SIGNAL_ARRAY, true))
        return 1;
    if (expect_field("mix_matrix", APG_ATOM_CONTRACT_CONFIG, "coefficients", APG_ATOM_FIELD_FLOAT_MATRIX, true))
        return 1;
    if (expect_field("filter_comb_fb", APG_ATOM_CONTRACT_IN, "delay", APG_ATOM_FIELD_SIGNAL_OPTIONAL, false))
        return 1;
    if (apg_atom_contract_find_field("generation_dc", APG_ATOM_CONTRACT_CONFIG, "missing", NULL))
        return fail("unknown metadata field lookup succeeded");
    if (apg_atom_contract_field_type("missing_atom", APG_ATOM_CONTRACT_IN, "signal") != APG_ATOM_FIELD_UNKNOWN)
        return fail("unknown atom metadata type lookup failed");

    free(json);
    return test_runtime_registry_contract();
}
