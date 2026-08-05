#ifndef AUDIO_PLAYGROUND_APGCORE_PROJECT_COMPILER_V2_H
#define AUDIO_PLAYGROUND_APGCORE_PROJECT_COMPILER_V2_H

#include <apgcore/compiler/compiler_v2.h>
#include <apgcore/validator/project_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>

typedef struct {
    apg_unit_v2_t          expanded_unit;
    apg_v2_compiled_unit_t plan;
} apg_project_v2_compiled_t;

uc_status apg_project_v2_compile(
    const apg_project_v2_resolved_t *project, uc_arena *arena, apg_project_v2_compiled_t *out, uc_error *err
);

#endif // AUDIO_PLAYGROUND_APGCORE_PROJECT_COMPILER_V2_H
