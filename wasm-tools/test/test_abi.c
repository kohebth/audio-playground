#include <apg/wasm/abi.h>

#include <stdio.h>

static int fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(void) {
    if (apg_wasm_control_abi_version() != APG_WASM_ABI_VERSION ||
        apg_wasm_processor_abi_version() != APG_WASM_ABI_VERSION)
        return fail("control and processor ABI versions differ");

    if (apg_wasm_control_capabilities() != APG_WASM_CAP_WORKSPACE)
        return fail("control module workspace capability is incorrect");
    if (apg_wasm_processor_capabilities() != APG_WASM_CAP_NONE)
        return fail("processor module advertises unfinished capabilities");

    return 0;
}
