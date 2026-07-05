#ifndef AUDIO_PLAYGROUND_APGCORE_PROJECT_VALIDATOR_V2_H
#define AUDIO_PLAYGROUND_APGCORE_PROJECT_VALIDATOR_V2_H

#include <apgcore/validator/project_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>
#include <yaml/node.h>

/*
 * Validate a parsed project contract graph and fill the public project model.
 * Unit path resolution remains in the project loader because it needs the project file location.
 */
uc_status apg_project_v2_validate_root(const uc_node *root, uc_arena *arena, apg_project_v2_t *out, uc_error *err);

/*
 * Validate a resolved project graph (project plus loaded units) before compiling.
 * This validates route wiring, route targets, and per-instance parameter overrides.
 */
uc_status apg_project_v2_validate_resolved(const apg_project_v2_resolved_t *project, uc_error *err);

#endif // AUDIO_PLAYGROUND_APGCORE_PROJECT_VALIDATOR_V2_H
