// map.c - per-cpu permission maps (skeleton)
#include "driver/util/map.h"

int hv_perm_map_set(hv_perm_map_t* m, void* gpa, int r, int w, int x) {
    if (!m || !gpa || m->count >= 128) return STATUS_INVALID_PARAMETER;
    m->entries[m->count].gpa = gpa;
    m->entries[m->count].r = r;
    m->entries[m->count].w = w;
    m->entries[m->count].x = x;
    m->count++;
    return STATUS_SUCCESS;
}
