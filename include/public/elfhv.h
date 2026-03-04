// elfhv.h - elf hypervisor skeleton (windows x64)
// credits: discord: chefendpoint | telegram: elf_nigel
#pragma once

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4201)
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum elfhv_status_t {
    ELFHV_OK = 0,
    ELFHV_ERR_UNSUPPORTED,
    ELFHV_ERR_NOT_PRESENT,
    ELFHV_ERR_ALREADY_ENABLED,
    ELFHV_ERR_INVALID_ARGS,
    ELFHV_ERR_INTERNAL
} elfhv_status_t;

typedef struct elfhv_caps_t {
    int vmx_supported;
    int ept_supported;
    int vpid_supported;
} elfhv_caps_t;

typedef struct elfhv_config_t {
    int enable_ept;
    int enable_vpid;
    int enable_preemption_timer;
} elfhv_config_t;

typedef struct elfhv_state_t {
    int initialized;
    int running;
    elfhv_caps_t caps;
    elfhv_config_t cfg;
} elfhv_state_t;

// core api
int elfhv_query_caps(elfhv_caps_t* out_caps);
int elfhv_init(elfhv_state_t* st, const elfhv_config_t* cfg);
int elfhv_start(elfhv_state_t* st);
int elfhv_stop(elfhv_state_t* st);
void elfhv_shutdown(elfhv_state_t* st);

#ifdef __cplusplus
}
#endif

#ifdef _MSC_VER
#  pragma warning(pop)
#endif
