// state.h - hypervisor state machine (non-operational)
#pragma once
#include <ntddk.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hv_state_code_t {
    HV_STATE_NONE = 0,
    HV_STATE_INIT,
    HV_STATE_READY,
    HV_STATE_RUNNING,
    HV_STATE_STOPPING,
    HV_STATE_ERROR
} hv_state_code_t;

typedef enum hv_state_event_t {
    HV_EVT_INIT_OK = 1,
    HV_EVT_INIT_FAIL,
    HV_EVT_START,
    HV_EVT_STOP,
    HV_EVT_FAIL
} hv_state_event_t;

typedef struct hv_state_transition_t {
    hv_state_code_t From;
    hv_state_event_t Event;
    hv_state_code_t To;
} hv_state_transition_t;

hv_state_code_t hv_state_next(hv_state_code_t current, hv_state_event_t event);

#ifdef __cplusplus
}
#endif
