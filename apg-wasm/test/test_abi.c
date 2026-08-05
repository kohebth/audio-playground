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

    if (apg_wasm_control_capabilities() != (APG_WASM_CAP_WORKSPACE | APG_WASM_CAP_PREPARED_IMAGE))
        return fail("control module capabilities are incorrect");
    const uint32_t processor_capabilities =
        APG_WASM_CAP_PREPARED_IMAGE | APG_WASM_CAP_PROCESS | APG_WASM_CAP_CONTROLS | APG_WASM_CAP_METERS;
    if (apg_wasm_processor_capabilities() != processor_capabilities)
        return fail("processor module capabilities are incorrect");

    return 0;
}
