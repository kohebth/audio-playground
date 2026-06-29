#ifndef AUDIO_PLAYGROUND_APGCORE_JSON_CONTRACT_V2_H
#define AUDIO_PLAYGROUND_APGCORE_JSON_CONTRACT_V2_H

#include <stdio.h>

void apg_v2_json_write_validate_unit(FILE *out, const char *path);
void apg_v2_json_write_validate_project(FILE *out, const char *path);
void apg_v2_json_write_inspect_unit(FILE *out, const char *path);
void apg_v2_json_write_inspect_project(FILE *out, const char *path);

#endif // AUDIO_PLAYGROUND_APGCORE_JSON_CONTRACT_V2_H
