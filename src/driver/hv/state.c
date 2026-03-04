// state.c - hypervisor state machine (non-operational)
#include "driver/core/state.h"

static const hv_state_transition_t g_transitions[] = {
    { HV_STATE_NONE,     HV_EVT_INIT_OK,  HV_STATE_READY },
    { HV_STATE_NONE,     HV_EVT_INIT_FAIL,HV_STATE_ERROR },
    { HV_STATE_READY,    HV_EVT_START,    HV_STATE_RUNNING },
    { HV_STATE_RUNNING,  HV_EVT_STOP,     HV_STATE_STOPPING },
    { HV_STATE_STOPPING, HV_EVT_STOP,     HV_STATE_READY },
    { HV_STATE_READY,    HV_EVT_FAIL,     HV_STATE_ERROR },
    { HV_STATE_RUNNING,  HV_EVT_FAIL,     HV_STATE_ERROR }
};

hv_state_code_t hv_state_next(hv_state_code_t current, hv_state_event_t event) {
    for (ULONG i = 0; i < (ULONG)(sizeof(g_transitions)/sizeof(g_transitions[0])); ++i) {
        if (g_transitions[i].From == current && g_transitions[i].Event == event) {
            return g_transitions[i].To;
        }
    }
    return current;
}
