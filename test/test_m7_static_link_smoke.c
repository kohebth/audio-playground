#include "apg_project_m7.h"

int main(void) {
    apg_m7_project_init();
    apg_m7_project_refresh_params();
    apg_m7_project_process_block();
    return 0;
}
