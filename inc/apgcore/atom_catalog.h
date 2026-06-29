#ifndef AUDIO_PLAYGROUND_APGCORE_ATOM_CATALOG_H
#define AUDIO_PLAYGROUND_APGCORE_ATOM_CATALOG_H

#include <stdio.h>

/*
 * Write a UI-facing JSON catalog for the registered atom table.
 * The output is deterministic and intended for tooling/tests; callers own the FILE stream.
 */
void apg_atom_catalog_write_json(FILE *out);

#endif // AUDIO_PLAYGROUND_APGCORE_ATOM_CATALOG_H
