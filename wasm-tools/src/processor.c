#include <apg/wasm/abi.h>

uint32_t apg_wasm_processor_abi_version(void) { return APG_WASM_ABI_VERSION; }

uint32_t apg_wasm_processor_capabilities(void) { return APG_WASM_CAP_NONE; }
