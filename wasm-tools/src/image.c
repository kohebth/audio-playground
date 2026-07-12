#include "image.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <atom_registry.h>

#define APG_WASM_IMAGE_MAGIC       0x57475041u
#define APG_WASM_IMAGE_HEADER_SIZE 32u

typedef struct {
    unsigned char *data;
    size_t         size;
    size_t         capacity;
    bool           ok;
} image_writer_t;

typedef struct {
    const unsigned char *data;
    size_t               size;
    size_t               offset;
    bool                 ok;
} image_reader_t;

static void set_error(uc_error *error, uc_status status, const char *message) {
    if (!error)
        return;
    uc_loc location = {0, 0};
    uc_error_set(error, status, location, "%s", message);
}

static bool size_to_u32(size_t value, uint32_t *out) {
    if (value > UINT32_MAX)
        return false;
    *out = (uint32_t)value;
    return true;
}

static void writer_reserve(image_writer_t *writer, size_t bytes) {
    if (!writer || !writer->ok || bytes > SIZE_MAX - writer->size) {
        if (writer)
            writer->ok = false;
        return;
    }
    const size_t required = writer->size + bytes;
    if (required <= writer->capacity)
        return;
    size_t capacity = writer->capacity ? writer->capacity : 4096u;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            writer->ok = false;
            return;
        }
        capacity *= 2u;
    }
    unsigned char *data = realloc(writer->data, capacity);
    if (!data) {
        writer->ok = false;
        return;
    }
    writer->data     = data;
    writer->capacity = capacity;
}

static void writer_bytes(image_writer_t *writer, const void *data, size_t bytes) {
    writer_reserve(writer, bytes);
    if (!writer->ok)
        return;
    if (bytes > 0u)
        memcpy(writer->data + writer->size, data, bytes);
    writer->size += bytes;
}

static void writer_u32(image_writer_t *writer, uint32_t value) {
    unsigned char bytes[4] = {
        (unsigned char)(value & 0xffu),
        (unsigned char)((value >> 8u) & 0xffu),
        (unsigned char)((value >> 16u) & 0xffu),
        (unsigned char)((value >> 24u) & 0xffu),
    };
    writer_bytes(writer, bytes, sizeof(bytes));
}

static void writer_u64(image_writer_t *writer, uint64_t value) {
    writer_u32(writer, (uint32_t)(value & UINT32_MAX));
    writer_u32(writer, (uint32_t)(value >> 32u));
}

static void writer_f32(image_writer_t *writer, float value) {
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    writer_u32(writer, bits);
}

static void writer_size(image_writer_t *writer, size_t value) {
    uint32_t encoded = 0u;
    if (!size_to_u32(value, &encoded)) {
        writer->ok = false;
        return;
    }
    writer_u32(writer, encoded);
}

static void writer_string(image_writer_t *writer, const char *text) {
    if (!text) {
        writer_u32(writer, UINT32_MAX);
        return;
    }
    const size_t length = strlen(text);
    writer_size(writer, length);
    writer_bytes(writer, text, length);
}

static void writer_size_array(image_writer_t *writer, const size_t *items, size_t count) {
    writer_size(writer, count);
    for (size_t i = 0u; i < count; ++i)
        writer_size(writer, items[i]);
}

static void writer_string_array(image_writer_t *writer, const char *const *items, size_t count) {
    writer_size(writer, count);
    for (size_t i = 0u; i < count; ++i)
        writer_string(writer, items[i]);
}

static void writer_refreshes(image_writer_t *writer, const apg_v2_registry_scalar_refresh_t *items, size_t count) {
    writer_size(writer, count);
    for (size_t i = 0u; i < count; ++i) {
        writer_string(writer, items[i].key);
        writer_u32(writer, (uint32_t)items[i].kind);
        writer_size(writer, items[i].param_index);
        writer_f32(writer, items[i].number);
        writer_size(writer, items[i].storage_offset);
        writer_u32(writer, (uint32_t)items[i].field_type);
        writer_u32(writer, items[i].config ? 1u : 0u);
    }
}

static void writer_ports(image_writer_t *writer, const apg_v2_registry_audio_port_t *ports, size_t count) {
    writer_size(writer, count);
    for (size_t i = 0u; i < count; ++i) {
        writer_string(writer, ports[i].port_name);
        writer_size(writer, ports[i].channel_count);
        writer_size(writer, ports[i].meter_index);
        writer_size_array(writer, ports[i].signal_indices, ports[i].channel_count);
    }
}

static void writer_nodes(image_writer_t *writer, const apg_v2_registry_t *registry) {
    writer_size(writer, registry->nodes_len);
    for (size_t i = 0u; i < registry->nodes_len; ++i) {
        const apg_v2_registry_node_layout_t *node = &registry->node_layouts[i];
        writer_string(writer, node->node_id);
        writer_string(writer, node->atom_name);
        writer_size(writer, node->out_size);
        writer_size(writer, node->in_size);
        writer_size(writer, node->config_size);
        writer_size(writer, node->state_size);
        writer_size(writer, node->out_offset);
        writer_size(writer, node->in_offset);
        writer_size(writer, node->config_offset);
        writer_size(writer, node->state_offset);
        writer_size_array(writer, node->state_buffer_samples_by_index, node->state_buffers_len);
        writer_size_array(writer, node->state_buffer_sample_offsets_by_index, node->state_buffers_len);
        writer_size(writer, node->state_buffer_table_offset);
        writer_size(writer, node->state_buffer_samples);
        writer_size(writer, node->signal_array_pointer_slots);
        writer_size(writer, node->signal_array_pool_offset);

        writer_size(writer, node->signal_bindings_len);
        for (size_t binding_index = 0u; binding_index < node->signal_bindings_len; ++binding_index) {
            const apg_v2_registry_signal_binding_t *binding = &node->signal_bindings[binding_index];
            writer_string(writer, binding->key);
            writer_size(writer, binding->storage_offset);
            writer_size(writer, binding->signal_index);
            writer_size_array(writer, binding->signal_array_indices, binding->signal_array_len);
            writer_size(writer, binding->signal_array_offset);
            writer_u32(writer, binding->is_input ? 1u : 0u);
            writer_u32(writer, binding->is_signal_array ? 1u : 0u);
        }

        writer_refreshes(writer, node->config_refreshes, node->config_refreshes_len);
        writer_refreshes(writer, node->input_refreshes, node->input_refreshes_len);
        writer_size(writer, node->mix_matrix_coefficients_len);
        for (size_t coefficient = 0u; coefficient < node->mix_matrix_coefficients_len; ++coefficient)
            writer_f32(writer, node->mix_matrix_coefficients[coefficient]);
        writer_size(writer, node->mix_matrix_num_out);
        writer_size(writer, node->mix_matrix_num_in);
        writer_u32(writer, node->has_spectral_info ? 1u : 0u);
        writer_u32(writer, node->spectral_info.fft_size);
        writer_u32(writer, node->spectral_info.bin_count);
        writer_u32(writer, node->spectral_info.hop_size);
    }
}

static uint32_t image_checksum(const unsigned char *data, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static void patch_u32(unsigned char *data, size_t offset, uint32_t value) {
    data[offset]      = (unsigned char)(value & 0xffu);
    data[offset + 1u] = (unsigned char)((value >> 8u) & 0xffu);
    data[offset + 2u] = (unsigned char)((value >> 16u) & 0xffu);
    data[offset + 3u] = (unsigned char)((value >> 24u) & 0xffu);
}

bool apg_wasm_image_build(
    const apg_v2_registry_t *registry, uint64_t revision, unsigned char **out_data, size_t *out_size, uc_error *error
) {
    if (!registry || !out_data || !out_size || !error) {
        set_error(error, UC_E_TYPE, "prepared image build arguments are invalid");
        return false;
    }
    *out_data             = NULL;
    *out_size             = 0u;
    image_writer_t writer = {.ok = true};
    writer_u32(&writer, APG_WASM_IMAGE_MAGIC);
    writer_u32(&writer, APG_WASM_IMAGE_VERSION);
    writer_u32(&writer, 0u);
    writer_u32(&writer, 0u);
    writer_u64(&writer, revision);
    writer_u32(&writer, registry->frame_capacity);
    writer_f32(&writer, registry->sample_rate);

    writer_string_array(&writer, registry->signal_names, registry->signals_len);
    writer_size(&writer, registry->signal_samples);
    writer_string_array(&writer, registry->param_names, registry->params_len);
    writer_size(&writer, registry->params_len);
    for (size_t i = 0u; i < registry->params_len; ++i)
        writer_f32(&writer, registry->param_defaults[i]);
    writer_size(&writer, registry->params_len);
    for (size_t i = 0u; i < registry->params_len; ++i)
        writer_u32(&writer, registry->param_smoothing_frames[i]);
    writer_size(&writer, registry->input_meters_len);
    writer_size(&writer, registry->output_meters_len);

    writer_size(&writer, registry->control_targets_len);
    for (size_t i = 0u; i < registry->control_targets_len; ++i) {
        writer_string(&writer, registry->control_targets[i].port_name);
        writer_string(&writer, registry->control_targets[i].param_name);
        writer_size(&writer, registry->control_targets[i].param_index);
    }
    writer_size(&writer, registry->bypassed_instances_len);
    for (size_t i = 0u; i < registry->bypassed_instances_len; ++i) {
        writer_string(&writer, registry->bypass_instances[i].instance_id);
        writer_size(&writer, registry->bypass_instances[i].instance_id_len);
        writer_size(&writer, registry->bypass_instances[i].input_index);
        writer_size(&writer, registry->bypass_instances[i].output_index);
    }
    writer_size_array(
        &writer, registry->bypass_index_by_node, registry->bypass_index_by_node ? registry->nodes_len : 0u
    );
    writer_size_array(&writer, registry->project_mute_output_indices, registry->project_mute_output_indices_len);
    writer_ports(&writer, registry->input_audio_ports, registry->input_audio_ports_len);
    writer_ports(&writer, registry->output_audio_ports, registry->output_audio_ports_len);
    writer_nodes(&writer, registry);
    writer_size(&writer, registry->schedule_len);
    for (size_t i = 0u; i < registry->schedule_len; ++i)
        writer_u32(&writer, registry->schedule[i]);
    writer_size(&writer, registry->state_buffers_len);
    writer_size(&writer, registry->state_buffer_samples);
    writer_size(&writer, registry->atom_storage_bytes);
    writer_size(&writer, registry->signal_array_pointer_slots);

    uint32_t total_size = 0u;
    if (!writer.ok || !size_to_u32(writer.size, &total_size)) {
        free(writer.data);
        set_error(error, UC_E_RANGE, "prepared image exceeds the 32-bit WASM ABI limit");
        return false;
    }
    patch_u32(writer.data, 8u, total_size);
    patch_u32(
        writer.data, 12u,
        image_checksum(writer.data + APG_WASM_IMAGE_HEADER_SIZE, writer.size - APG_WASM_IMAGE_HEADER_SIZE)
    );
    *out_data     = writer.data;
    *out_size     = writer.size;
    error->status = UC_OK;
    return true;
}

static uint32_t reader_u32(image_reader_t *reader) {
    if (!reader || !reader->ok || reader->offset > reader->size || reader->size - reader->offset < 4u) {
        if (reader)
            reader->ok = false;
        return 0u;
    }
    const unsigned char *bytes = reader->data + reader->offset;
    reader->offset += 4u;
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

static uint64_t reader_u64(image_reader_t *reader) {
    const uint64_t low  = reader_u32(reader);
    const uint64_t high = reader_u32(reader);
    return low | (high << 32u);
}

static float reader_f32(image_reader_t *reader) {
    const uint32_t bits  = reader_u32(reader);
    float          value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void *arena_array(uc_arena *arena, size_t count, size_t item_size, image_reader_t *reader) {
    if (item_size != 0u && count > SIZE_MAX / item_size) {
        reader->ok = false;
        return NULL;
    }
    if (count == 0u)
        return NULL;
    void *items = uc_arena_alloc(arena, count * item_size, _Alignof(max_align_t));
    if (!items)
        reader->ok = false;
    else
        memset(items, 0, count * item_size);
    return items;
}

static const char *reader_string(image_reader_t *reader, uc_arena *arena) {
    const uint32_t length = reader_u32(reader);
    if (!reader->ok || length == UINT32_MAX)
        return NULL;
    if (reader->offset > reader->size || (size_t)length > reader->size - reader->offset) {
        reader->ok = false;
        return NULL;
    }
    char *text = uc_arena_strndup(arena, (const char *)(reader->data + reader->offset), length);
    reader->offset += length;
    if (!text)
        reader->ok = false;
    return text;
}

static size_t *reader_size_array(image_reader_t *reader, uc_arena *arena, size_t *out_count) {
    const size_t count = reader_u32(reader);
    size_t      *items = arena_array(arena, count, sizeof(*items), reader);
    for (size_t i = 0u; reader->ok && i < count; ++i)
        items[i] = reader_u32(reader);
    *out_count = count;
    return items;
}

static const char **reader_string_array(image_reader_t *reader, uc_arena *arena, size_t *out_count) {
    const size_t count = reader_u32(reader);
    const char **items = arena_array(arena, count, sizeof(*items), reader);
    for (size_t i = 0u; reader->ok && i < count; ++i)
        items[i] = reader_string(reader, arena);
    *out_count = count;
    return items;
}

static apg_v2_registry_scalar_refresh_t *reader_refreshes(image_reader_t *reader, uc_arena *arena, size_t *out_count) {
    const size_t                      count = reader_u32(reader);
    apg_v2_registry_scalar_refresh_t *items = arena_array(arena, count, sizeof(*items), reader);
    for (size_t i = 0u; reader->ok && i < count; ++i) {
        items[i].key            = reader_string(reader, arena);
        items[i].kind           = (apg_v2_binding_kind_t)reader_u32(reader);
        items[i].param_index    = reader_u32(reader);
        items[i].number         = reader_f32(reader);
        items[i].storage_offset = reader_u32(reader);
        items[i].field_type     = (atom_field_type_t)reader_u32(reader);
        items[i].config         = reader_u32(reader) != 0u;
        if (items[i].kind != APG_BIND_PARAM && items[i].kind != APG_BIND_LITERAL)
            reader->ok = false;
        if (items[i].field_type != FIELD_FLOAT && items[i].field_type != FIELD_INT)
            reader->ok = false;
    }
    *out_count = count;
    return items;
}

static apg_v2_registry_audio_port_t *reader_ports(image_reader_t *reader, uc_arena *arena, size_t *out_count) {
    const size_t                  count = reader_u32(reader);
    apg_v2_registry_audio_port_t *ports = arena_array(arena, count, sizeof(*ports), reader);
    for (size_t i = 0u; reader->ok && i < count; ++i) {
        ports[i].port_name      = reader_string(reader, arena);
        ports[i].channel_count  = reader_u32(reader);
        ports[i].meter_index    = reader_u32(reader);
        size_t signal_count     = 0u;
        ports[i].signal_indices = reader_size_array(reader, arena, &signal_count);
        if (signal_count != ports[i].channel_count)
            reader->ok = false;
    }
    *out_count = count;
    return ports;
}

static bool hydrate_nodes(image_reader_t *reader, uc_arena *arena, apg_v2_registry_t *registry, uc_error *error) {
    registry->nodes_len    = reader_u32(reader);
    registry->node_layouts = arena_array(arena, registry->nodes_len, sizeof(*registry->node_layouts), reader);
    atom_registry_init();
    for (size_t i = 0u; reader->ok && i < registry->nodes_len; ++i) {
        apg_v2_registry_node_layout_t *node = &registry->node_layouts[i];
        node->node_id                       = reader_string(reader, arena);
        node->atom_name                     = reader_string(reader, arena);
        const atom_registry_entry_t *atom   = atom_registry_find(node->atom_name);
        if (!atom) {
            set_error(error, UC_E_MISSING, "prepared image references an unknown atom");
            reader->ok = false;
            break;
        }
        node->thunk                   = atom->thunk;
        node->state_fields            = atom->state_fields;
        node->n_state_fields          = atom->n_state_fields;
        node->out_size                = reader_u32(reader);
        node->in_size                 = reader_u32(reader);
        node->config_size             = reader_u32(reader);
        node->state_size              = reader_u32(reader);
        const size_t atom_out_size    = atom->out_size > 0u ? atom->out_size : 1u;
        const size_t atom_in_size     = atom->in_size > 0u ? atom->in_size : 1u;
        const size_t atom_config_size = atom->config_size > 0u ? atom->config_size : 1u;
        const size_t atom_state_size  = atom->state_size > 0u ? atom->state_size : 1u;
        if (node->out_size != atom_out_size || node->in_size != atom_in_size || node->config_size != atom_config_size ||
            node->state_size != atom_state_size) {
            set_error(error, UC_E_RANGE, "prepared image atom ABI sizes do not match the processor build");
            reader->ok = false;
            break;
        }
        node->out_offset                           = reader_u32(reader);
        node->in_offset                            = reader_u32(reader);
        node->config_offset                        = reader_u32(reader);
        node->state_offset                         = reader_u32(reader);
        size_t state_samples_count                 = 0u;
        size_t state_offsets_count                 = 0u;
        node->state_buffer_samples_by_index        = reader_size_array(reader, arena, &state_samples_count);
        node->state_buffer_sample_offsets_by_index = reader_size_array(reader, arena, &state_offsets_count);
        if (state_samples_count != state_offsets_count) {
            set_error(error, UC_E_RANGE, "prepared image state buffer tables have different lengths");
            reader->ok = false;
            break;
        }
        node->state_buffers_len          = state_samples_count;
        node->state_buffer_table_offset  = reader_u32(reader);
        node->state_buffer_samples       = reader_u32(reader);
        node->signal_array_pointer_slots = reader_u32(reader);
        node->signal_array_pool_offset   = reader_u32(reader);

        node->signal_bindings_len = reader_u32(reader);
        node->signal_bindings = arena_array(arena, node->signal_bindings_len, sizeof(*node->signal_bindings), reader);
        for (size_t binding_index = 0u; reader->ok && binding_index < node->signal_bindings_len; ++binding_index) {
            apg_v2_registry_signal_binding_t *binding = &node->signal_bindings[binding_index];
            binding->key                              = reader_string(reader, arena);
            binding->storage_offset                   = reader_u32(reader);
            binding->signal_index                     = reader_u32(reader);
            binding->signal_array_indices             = reader_size_array(reader, arena, &binding->signal_array_len);
            binding->signal_array_offset              = reader_u32(reader);
            binding->is_input                         = reader_u32(reader) != 0u;
            binding->is_signal_array                  = reader_u32(reader) != 0u;
            if (!binding->is_signal_array && binding->signal_array_len != 0u) {
                set_error(error, UC_E_RANGE, "prepared image scalar signal binding contains array indexes");
                reader->ok = false;
            }
        }
        node->config_refreshes            = reader_refreshes(reader, arena, &node->config_refreshes_len);
        node->input_refreshes             = reader_refreshes(reader, arena, &node->input_refreshes_len);
        node->mix_matrix_coefficients_len = reader_u32(reader);
        node->mix_matrix_coefficients =
            arena_array(arena, node->mix_matrix_coefficients_len, sizeof(*node->mix_matrix_coefficients), reader);
        for (size_t coefficient = 0u; reader->ok && coefficient < node->mix_matrix_coefficients_len; ++coefficient)
            node->mix_matrix_coefficients[coefficient] = reader_f32(reader);
        node->mix_matrix_num_out = reader_u32(reader);
        node->mix_matrix_num_in  = reader_u32(reader);
        if (node->mix_matrix_num_out > 0u) {
            if (node->mix_matrix_num_in == 0u || node->mix_matrix_num_out > SIZE_MAX / node->mix_matrix_num_in ||
                node->mix_matrix_num_out * node->mix_matrix_num_in != node->mix_matrix_coefficients_len) {
                set_error(error, UC_E_RANGE, "prepared image mix matrix dimensions are invalid");
                reader->ok = false;
                break;
            }
            node->mix_matrix_row_pointers =
                arena_array(arena, node->mix_matrix_num_out, sizeof(*node->mix_matrix_row_pointers), reader);
            for (size_t row = 0u; reader->ok && row < node->mix_matrix_num_out; ++row)
                node->mix_matrix_row_pointers[row] = node->mix_matrix_coefficients + row * node->mix_matrix_num_in;
        } else if (node->mix_matrix_coefficients_len != 0u || node->mix_matrix_num_in != 0u) {
            set_error(error, UC_E_RANGE, "prepared image mix matrix metadata is inconsistent");
            reader->ok = false;
            break;
        }
        node->has_spectral_info       = reader_u32(reader) != 0u;
        node->spectral_info.fft_size  = reader_u32(reader);
        node->spectral_info.bin_count = reader_u32(reader);
        node->spectral_info.hop_size  = reader_u32(reader);
    }
    return reader->ok;
}

bool apg_wasm_image_hydrate(
    const unsigned char *data,
    size_t               size,
    uc_arena            *arena,
    apg_v2_registry_t   *out_registry,
    uint64_t            *out_revision,
    uc_error            *error
) {
    if (!data || !arena || !out_registry || !out_revision || !error || size < APG_WASM_IMAGE_HEADER_SIZE) {
        set_error(error, UC_E_TYPE, "prepared image hydrate arguments are invalid");
        return false;
    }
    memset(out_registry, 0, sizeof(*out_registry));
    image_reader_t reader     = {.data = data, .size = size, .ok = true};
    const uint32_t magic      = reader_u32(&reader);
    const uint32_t version    = reader_u32(&reader);
    const uint32_t total_size = reader_u32(&reader);
    const uint32_t checksum   = reader_u32(&reader);
    if (magic != APG_WASM_IMAGE_MAGIC || version != APG_WASM_IMAGE_VERSION || total_size != size ||
        checksum != image_checksum(data + APG_WASM_IMAGE_HEADER_SIZE, size - APG_WASM_IMAGE_HEADER_SIZE)) {
        set_error(error, UC_E_RANGE, "prepared image header or checksum is invalid");
        return false;
    }
    *out_revision                = reader_u64(&reader);
    out_registry->frame_capacity = reader_u32(&reader);
    out_registry->sample_rate    = reader_f32(&reader);
    out_registry->signal_names   = reader_string_array(&reader, arena, &out_registry->signals_len);
    out_registry->signal_samples = reader_u32(&reader);
    out_registry->param_names    = reader_string_array(&reader, arena, &out_registry->params_len);
    const size_t defaults_count  = reader_u32(&reader);
    out_registry->param_defaults = arena_array(arena, defaults_count, sizeof(*out_registry->param_defaults), &reader);
    for (size_t i = 0u; reader.ok && i < defaults_count; ++i)
        out_registry->param_defaults[i] = reader_f32(&reader);
    const size_t smoothing_count = reader_u32(&reader);
    out_registry->param_smoothing_frames =
        arena_array(arena, smoothing_count, sizeof(*out_registry->param_smoothing_frames), &reader);
    for (size_t i = 0u; reader.ok && i < smoothing_count; ++i)
        out_registry->param_smoothing_frames[i] = reader_u32(&reader);
    if (defaults_count != out_registry->params_len || smoothing_count != out_registry->params_len)
        reader.ok = false;
    out_registry->input_meters_len  = reader_u32(&reader);
    out_registry->output_meters_len = reader_u32(&reader);

    out_registry->control_targets_len = reader_u32(&reader);
    out_registry->control_targets =
        arena_array(arena, out_registry->control_targets_len, sizeof(*out_registry->control_targets), &reader);
    for (size_t i = 0u; reader.ok && i < out_registry->control_targets_len; ++i) {
        out_registry->control_targets[i].port_name   = reader_string(&reader, arena);
        out_registry->control_targets[i].param_name  = reader_string(&reader, arena);
        out_registry->control_targets[i].param_index = reader_u32(&reader);
    }
    out_registry->bypassed_instances_len = reader_u32(&reader);
    out_registry->bypass_instances =
        arena_array(arena, out_registry->bypassed_instances_len, sizeof(*out_registry->bypass_instances), &reader);
    for (size_t i = 0u; reader.ok && i < out_registry->bypassed_instances_len; ++i) {
        out_registry->bypass_instances[i].instance_id     = reader_string(&reader, arena);
        out_registry->bypass_instances[i].instance_id_len = reader_u32(&reader);
        out_registry->bypass_instances[i].input_index     = reader_u32(&reader);
        out_registry->bypass_instances[i].output_index    = reader_u32(&reader);
        if (!out_registry->bypass_instances[i].instance_id ||
            strlen(out_registry->bypass_instances[i].instance_id) != out_registry->bypass_instances[i].instance_id_len)
            reader.ok = false;
    }
    size_t bypass_map_count            = 0u;
    out_registry->bypass_index_by_node = reader_size_array(&reader, arena, &bypass_map_count);
    out_registry->project_mute_output_indices =
        reader_size_array(&reader, arena, &out_registry->project_mute_output_indices_len);
    out_registry->input_audio_ports  = reader_ports(&reader, arena, &out_registry->input_audio_ports_len);
    out_registry->output_audio_ports = reader_ports(&reader, arena, &out_registry->output_audio_ports_len);
    if (!hydrate_nodes(&reader, arena, out_registry, error) ||
        (out_registry->bypassed_instances_len > 0u && bypass_map_count != out_registry->nodes_len) ||
        (out_registry->bypassed_instances_len == 0u && bypass_map_count != 0u))
        reader.ok = false;
    out_registry->schedule_len = reader_u32(&reader);
    uint32_t *schedule         = arena_array(arena, out_registry->schedule_len, sizeof(*schedule), &reader);
    for (size_t i = 0u; reader.ok && i < out_registry->schedule_len; ++i)
        schedule[i] = reader_u32(&reader);
    out_registry->schedule                   = schedule;
    out_registry->state_buffers_len          = reader_u32(&reader);
    out_registry->state_buffer_samples       = reader_u32(&reader);
    out_registry->atom_storage_bytes         = reader_u32(&reader);
    out_registry->signal_array_pointer_slots = reader_u32(&reader);
    if (!reader.ok || reader.offset != reader.size || out_registry->frame_capacity == 0u) {
        if (error->status == UC_OK) {
            uc_loc location = {0, 0};
            uc_error_set(
                error, UC_E_RANGE, location, "prepared image payload failed at byte %zu of %zu", reader.offset,
                reader.size
            );
        }
        return false;
    }
    error->status = UC_OK;
    return true;
}
