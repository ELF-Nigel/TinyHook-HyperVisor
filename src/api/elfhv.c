// elfhv.c - usermode-safe skeleton
#include "public/elfhv.h"

#ifdef elfhv_usermode
// usermode build: return unsupported
#else
// kernel build: real implementation in driver/vmx.c
#endif

int elfhv_query_caps(elfhv_caps_t* out_caps) {
    if (!out_caps) return ELFHV_ERR_INVALID_ARGS;
#ifdef elfhv_usermode
    out_caps->vmx_supported = 0;
    out_caps->ept_supported = 0;
    out_caps->vpid_supported = 0;
    return ELFHV_ERR_UNSUPPORTED;
#else
    return ELFHV_ERR_INTERNAL;
#endif
}

int elfhv_init(elfhv_state_t* st, const elfhv_config_t* cfg) {
    if (!st || !cfg) return ELFHV_ERR_INVALID_ARGS;
    if (st->initialized) return ELFHV_ERR_ALREADY_ENABLED;
    st->cfg = *cfg;
    st->initialized = 1;
#ifdef elfhv_usermode
    return ELFHV_ERR_UNSUPPORTED;
#else
    return ELFHV_ERR_INTERNAL;
#endif
}

int elfhv_start(elfhv_state_t* st) {
    if (!st || !st->initialized) return ELFHV_ERR_INVALID_ARGS;
    if (st->running) return ELFHV_ERR_ALREADY_ENABLED;
#ifdef elfhv_usermode
    return ELFHV_ERR_UNSUPPORTED;
#else
    return ELFHV_ERR_INTERNAL;
#endif
}

int elfhv_stop(elfhv_state_t* st) {
    if (!st) return ELFHV_ERR_INVALID_ARGS;
#ifdef elfhv_usermode
    return ELFHV_ERR_UNSUPPORTED;
#else
    return ELFHV_ERR_INTERNAL;
#endif
}

void elfhv_shutdown(elfhv_state_t* st) {
    if (!st) return;
    st->running = 0;
    st->initialized = 0;
}
