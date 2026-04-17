#include <atom/dsp_atoms.h>

#define CHUNK_LENGTH 512

Signal src_convert_format(Signal signal, uint32_t from_format, uint32_t to_format) {
    if (signal == NULL) return NULL;
    
    // For now, this atom acts as a pass-through as Signal is always float*
    // In a more complex implementation, this might handle endianness or fixed-point conversion
    
    return signal;
}
