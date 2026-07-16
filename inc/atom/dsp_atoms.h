#ifndef DSP_ATOMS_H
#define DSP_ATOMS_H

#include <apgcore/runtime/process.h>
#include <apgcore/runtime/spectral.h>

#include "atom_definitions.h"
#include "dsp_types.h"

// clang-format off
#define APG_DECLARE_LEGACY_ATOM(atom_name) \
    void atom_name(atom_name##_out_t *, atom_name##_in_t *, atom_name##_params_t *, atom_name##_state_t *);

#define APG_DECLARE_PROCESS_ATOM(atom_name)                   \
    void atom_name##_process(                                \
        atom_name##_out_t *,                                 \
        atom_name##_in_t *,                                  \
        atom_name##_params_t *,                              \
        atom_name##_state_t *,                               \
        const apg_process_info_t *                           \
    );

#define APG_DECLARE_SPECTRAL_PROCESS_ATOM(atom_name)          \
    void atom_name##_process(                                \
        atom_name##_out_t *,                                 \
        atom_name##_in_t *,                                  \
        atom_name##_params_t *,                              \
        atom_name##_state_t *,                               \
        const apg_spectral_info_t *                          \
    );

#define APG_DECLARE_SPECTRAL_VARIANT_ATOM(atom_name)          \
    void atom_name##_spectral_process(                       \
        atom_name##_out_t *,                                 \
        atom_name##_in_t *,                                  \
        atom_name##_params_t *,                              \
        atom_name##_state_t *,                               \
        const apg_spectral_info_t *                          \
    );

#define APG_DECLARE_DISPATCH_PROCESS(atom_name)  APG_DECLARE_PROCESS_ATOM(atom_name)
#define APG_DECLARE_DISPATCH_FFT(atom_name)      APG_DECLARE_SPECTRAL_PROCESS_ATOM(atom_name)
#define APG_DECLARE_DISPATCH_IFFT(atom_name)     APG_DECLARE_SPECTRAL_PROCESS_ATOM(atom_name)
#define APG_DECLARE_DISPATCH_MULTIPLY(atom_name) APG_DECLARE_SPECTRAL_PROCESS_ATOM(atom_name)
#define APG_DECLARE_DISPATCH_WINDOW(atom_name) \
    APG_DECLARE_PROCESS_ATOM(atom_name)        \
    APG_DECLARE_SPECTRAL_VARIANT_ATOM(atom_name)
#define APG_DECLARE_DISPATCH_OVERLAP_ADD(atom_name)  APG_DECLARE_DISPATCH_WINDOW(atom_name)
#define APG_DECLARE_DISPATCH_OVERLAP_SAVE(atom_name) APG_DECLARE_DISPATCH_WINDOW(atom_name)

#define APG_DECLARE_ATOM(atom_name, category, input_count, config_count, state_count, flags, maturity, dispatch) \
    APG_DECLARE_LEGACY_ATOM(atom_name)                                                                           \
    APG_DECLARE_DISPATCH_##dispatch(atom_name)
APG_ATOM_DEFINITIONS(APG_DECLARE_ATOM)
// clang-format on

#undef APG_DECLARE_ATOM
#undef APG_DECLARE_DISPATCH_OVERLAP_SAVE
#undef APG_DECLARE_DISPATCH_OVERLAP_ADD
#undef APG_DECLARE_DISPATCH_WINDOW
#undef APG_DECLARE_DISPATCH_MULTIPLY
#undef APG_DECLARE_DISPATCH_IFFT
#undef APG_DECLARE_DISPATCH_FFT
#undef APG_DECLARE_DISPATCH_PROCESS
#undef APG_DECLARE_SPECTRAL_VARIANT_ATOM
#undef APG_DECLARE_SPECTRAL_PROCESS_ATOM
#undef APG_DECLARE_PROCESS_ATOM
#undef APG_DECLARE_LEGACY_ATOM

#endif // DSP_ATOMS_H
