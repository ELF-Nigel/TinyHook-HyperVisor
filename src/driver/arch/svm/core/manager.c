// manager.c - api manager (skeleton)
#include "driver/core/manager.h"

void hv_manager_init(hv_manager_t* m) {
    if (!m) return;
    m->initialized = 1;
    m->policy_strict = 0;
}

void hv_manager_shutdown(hv_manager_t* m) {
    if (!m) return;
    m->initialized = 0;
}
