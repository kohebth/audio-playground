#include <atom/atom_definitions.h>
#include <atom/dsp_atoms.h>
#include <atom_registry.h>
#include <atom_thunk.h>

#define APG_DEFINE_THUNK_PROCESS(atom_name)                                                                     \
    void atom_name##_thunk(atom_call_t *call) {                                                                 \
        atom_name##_process(                                                                                    \
            (atom_name##_out_t *)call->out, (atom_name##_in_t *)call->in, (atom_name##_params_t *)call->config, \
            (atom_name##_state_t *)call->state, call->info                                                      \
        );                                                                                                      \
    }

#define APG_DEFINE_THUNK_FFT(atom_name)                                                                         \
    void atom_name##_thunk(atom_call_t *call) {                                                                 \
        atom_name##_process(                                                                                    \
            (atom_name##_out_t *)call->out, (atom_name##_in_t *)call->in, (atom_name##_params_t *)call->config, \
            (atom_name##_state_t *)call->state, call->spectral_info                                             \
        );                                                                                                      \
    }
#define APG_DEFINE_THUNK_IFFT(atom_name)     APG_DEFINE_THUNK_FFT(atom_name)
#define APG_DEFINE_THUNK_MULTIPLY(atom_name) APG_DEFINE_THUNK_FFT(atom_name)

#define APG_DEFINE_THUNK_WINDOW(atom_name)                                                                      \
    void atom_name##_thunk(atom_call_t *call) {                                                                 \
        atom_name##_spectral_process(                                                                           \
            (atom_name##_out_t *)call->out, (atom_name##_in_t *)call->in, (atom_name##_params_t *)call->config, \
            (atom_name##_state_t *)call->state, call->spectral_info                                             \
        );                                                                                                      \
    }
#define APG_DEFINE_THUNK_OVERLAP_ADD(atom_name)  APG_DEFINE_THUNK_WINDOW(atom_name)
#define APG_DEFINE_THUNK_OVERLAP_SAVE(atom_name) APG_DEFINE_THUNK_WINDOW(atom_name)

#define APG_DEFINE_ATOM_THUNK(atom_name, category, input_count, config_count, state_count, flags, maturity, dispatch) \
    APG_DEFINE_THUNK_##dispatch(atom_name)
APG_ATOM_DEFINITIONS(APG_DEFINE_ATOM_THUNK)
#undef APG_DEFINE_ATOM_THUNK
