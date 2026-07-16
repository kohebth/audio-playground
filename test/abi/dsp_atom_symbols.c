#include <atom/dsp_atoms.h>

// clang-format off
#define APG_REFERENCE_LEGACY_ATOM(atom_name)        \
    do {                                            \
        void (*volatile symbol)(                    \
            atom_name##_out_t *,                    \
            atom_name##_in_t *,                     \
            atom_name##_params_t *,                 \
            atom_name##_state_t *                   \
        ) = atom_name;                              \
        (void)symbol;                               \
    } while (0)

#define APG_REFERENCE_PROCESS_ATOM(atom_name)       \
    do {                                            \
        void (*volatile symbol)(                    \
            atom_name##_out_t *,                    \
            atom_name##_in_t *,                     \
            atom_name##_params_t *,                 \
            atom_name##_state_t *,                  \
            const apg_process_info_t *              \
        ) = atom_name##_process;                    \
        (void)symbol;                               \
    } while (0)

#define APG_REFERENCE_SPECTRAL_PROCESS_ATOM(atom_name) \
    do {                                                  \
        void (*volatile symbol)(                          \
            atom_name##_out_t *,                          \
            atom_name##_in_t *,                           \
            atom_name##_params_t *,                       \
            atom_name##_state_t *,                        \
            const apg_spectral_info_t *                   \
        ) = atom_name##_process;                          \
        (void)symbol;                                     \
    } while (0)

#define APG_REFERENCE_SPECTRAL_VARIANT_ATOM(atom_name) \
    do {                                                  \
        void (*volatile symbol)(                          \
            atom_name##_out_t *,                          \
            atom_name##_in_t *,                           \
            atom_name##_params_t *,                       \
            atom_name##_state_t *,                        \
            const apg_spectral_info_t *                   \
        ) = atom_name##_spectral_process;                 \
        (void)symbol;                                     \
    } while (0)

#define APG_REFERENCE_DISPATCH_PROCESS(atom_name)  APG_REFERENCE_PROCESS_ATOM(atom_name)
#define APG_REFERENCE_DISPATCH_FFT(atom_name)      APG_REFERENCE_SPECTRAL_PROCESS_ATOM(atom_name)
#define APG_REFERENCE_DISPATCH_IFFT(atom_name)     APG_REFERENCE_SPECTRAL_PROCESS_ATOM(atom_name)
#define APG_REFERENCE_DISPATCH_MULTIPLY(atom_name) APG_REFERENCE_SPECTRAL_PROCESS_ATOM(atom_name)
#define APG_REFERENCE_DISPATCH_WINDOW(atom_name) \
    APG_REFERENCE_PROCESS_ATOM(atom_name);       \
    APG_REFERENCE_SPECTRAL_VARIANT_ATOM(atom_name)
#define APG_REFERENCE_DISPATCH_OVERLAP_ADD(atom_name)  APG_REFERENCE_DISPATCH_WINDOW(atom_name)
#define APG_REFERENCE_DISPATCH_OVERLAP_SAVE(atom_name) APG_REFERENCE_DISPATCH_WINDOW(atom_name)

#define APG_REFERENCE_ATOM(atom_name, category, input_count, config_count, state_count, flags, maturity, dispatch) \
    APG_REFERENCE_LEGACY_ATOM(atom_name);                                                                          \
    APG_REFERENCE_DISPATCH_##dispatch(atom_name);
// clang-format on

int main(void) {
    APG_ATOM_DEFINITIONS(APG_REFERENCE_ATOM)
    return 0;
}
