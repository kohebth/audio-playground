#ifndef AUDIO_PLAYGROUND_APGCORE_ATOM_CATALOG_H
#define AUDIO_PLAYGROUND_APGCORE_ATOM_CATALOG_H

#include <stdbool.h>
#include <stdio.h>

bool apg_atom_profile_supported(const char *name, const char *profile);

/*
 * Write a UI-facing JSON catalog for the registered atom table.
 * The output is deterministic and intended for tooling/tests; callers own the FILE stream.
 */
void apg_atom_catalog_write_json(FILE *out);

#endif // AUDIO_PLAYGROUND_APGCORE_ATOM_CATALOG_H
