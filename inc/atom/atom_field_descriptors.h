#ifndef ATOM_FIELD_DESCRIPTORS_H
#define ATOM_FIELD_DESCRIPTORS_H

#include <atom/atom_definitions.h>
#include <atom_registry.h>

#define APG_DECLARE_FIELD_TABLE(name, kind, count)        APG_DECLARE_FIELD_TABLE_EXPAND(name, kind, count)
#define APG_DECLARE_FIELD_TABLE_EXPAND(name, kind, count) APG_DECLARE_FIELD_TABLE_##count(name, kind)
#define APG_DECLARE_FIELD_TABLE_0(name, kind)
#define APG_DECLARE_FIELD_TABLE_1(name, kind) extern const atom_field_desc_t name##_##kind##_fields[1];
#define APG_DECLARE_FIELD_TABLE_2(name, kind) extern const atom_field_desc_t name##_##kind##_fields[2];
#define APG_DECLARE_FIELD_TABLE_3(name, kind) extern const atom_field_desc_t name##_##kind##_fields[3];
#define APG_DECLARE_FIELD_TABLE_4(name, kind) extern const atom_field_desc_t name##_##kind##_fields[4];
#define APG_DECLARE_FIELD_TABLE_5(name, kind) extern const atom_field_desc_t name##_##kind##_fields[5];

#define APG_DECLARE_ATOM_FIELD_TABLES(                                                \
    name, category, input_count, config_count, state_count, flags, maturity, dispatch \
)                                                                                     \
    APG_DECLARE_FIELD_TABLE(name, in, input_count)                                    \
    APG_DECLARE_FIELD_TABLE(name, config, config_count)                               \
    APG_DECLARE_FIELD_TABLE(name, state, state_count)

APG_ATOM_DEFINITIONS(APG_DECLARE_ATOM_FIELD_TABLES)

#undef APG_DECLARE_ATOM_FIELD_TABLES
#undef APG_DECLARE_FIELD_TABLE
#undef APG_DECLARE_FIELD_TABLE_EXPAND
#undef APG_DECLARE_FIELD_TABLE_0
#undef APG_DECLARE_FIELD_TABLE_1
#undef APG_DECLARE_FIELD_TABLE_2
#undef APG_DECLARE_FIELD_TABLE_3
#undef APG_DECLARE_FIELD_TABLE_4
#undef APG_DECLARE_FIELD_TABLE_5

#endif // ATOM_FIELD_DESCRIPTORS_H
