#ifndef AUDIO_PLAYGROUND_APGCORE_UNIT_VALIDATOR_V2_H
#define AUDIO_PLAYGROUND_APGCORE_UNIT_VALIDATOR_V2_H

#include <apgcore/validator/unit_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>
#include <yaml/node.h>

/*
 * Validate a parsed unit contract graph and fill the public unit model.
 * The parser owns syntax; this module owns semantic checks against atom metadata and schema rules.
 */
uc_status apg_unit_v2_validate_root(const uc_node *root, uc_arena *arena, apg_unit_v2_t *out, uc_error *err);

#endif // AUDIO_PLAYGROUND_APGCORE_UNIT_VALIDATOR_V2_H
