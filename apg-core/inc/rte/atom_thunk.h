#ifndef AUDIO_PLAYGROUND_ATOM_THUNK_H
#define AUDIO_PLAYGROUND_ATOM_THUNK_H

#include <atom/atom_definitions.h>
#include <atom_registry.h>

#define APG_DECLARE_THUNK(atom_name, category, input_count, config_count, state_count, flags, maturity, dispatch) \
    extern void atom_name##_thunk(atom_call_t *call);
APG_ATOM_DEFINITIONS(APG_DECLARE_THUNK)
#undef APG_DECLARE_THUNK

#endif // AUDIO_PLAYGROUND_ATOM_THUNK_H
