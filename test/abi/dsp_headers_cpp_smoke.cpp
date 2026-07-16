extern "C" {
#include <atom/dsp_atoms.h>
}

int main() {
    const auto legacy  = &generation_dc;
    const auto process = &generation_dc_process;
    (void)legacy;
    (void)process;
    return 0;
}
