#include <apgcore/host/host_v2.h>
#include <apgcore/measure/measure_v2.h>
#include <apgcore/parser/parser_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/validator/project_v2.h>
#include <apgcore/validator/unit_v2.h>

int main(void) {
    apg_v2_host_unit_t    *unit_host    = 0;
    apg_v2_host_project_t *project_host = 0;
    apg_v2_runtime_t      *runtime      = 0;

    return unit_host || project_host || runtime ? 1 : 0;
}
