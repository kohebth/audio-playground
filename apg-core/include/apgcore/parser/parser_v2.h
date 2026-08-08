#ifndef AUDIO_PLAYGROUND_APGCORE_PARSER_V2_H
#define AUDIO_PLAYGROUND_APGCORE_PARSER_V2_H

#include <stddef.h>

#include <yaml/arena.h>
#include <yaml/error.h>
#include <yaml/node.h>

/*
 * Parse a v2 contract YAML document into an arena-owned syntax graph.
 * This module performs YAML lexing/parsing only; semantic validation belongs to validators/loaders.
 */
uc_status apg_v2_parse_string(const char *src, size_t src_len, uc_arena *arena, uc_node **out_root, uc_error *err);
uc_status apg_v2_parse_file(const char *path, uc_arena *arena, uc_node **out_root, uc_error *err);

#endif // AUDIO_PLAYGROUND_APGCORE_PARSER_V2_H
