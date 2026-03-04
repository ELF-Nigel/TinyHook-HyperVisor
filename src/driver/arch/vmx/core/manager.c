// manager.c - api manager (skeleton)
#include "driver/core/manager.h"

void hv_manager_init(hv_manager_t* m) {
    if (!m) return;
    m->initialized = 1;
    m->policy_strict = 0;
    hv_lock_init(&m->lock);
    m->io_count = 0;
    m->cpuid_count = 0;
}

void hv_manager_shutdown(hv_manager_t* m) {
    if (!m) return;
    m->initialized = 0;
}

int hv_manager_io_set(hv_manager_t* m, USHORT port, int read, int write) {
    if (!m) return STATUS_INVALID_PARAMETER;
    hv_lock_acquire(&m->lock);
    for (ULONG i = 0; i < m->io_count; ++i) {
        if (m->io_table[i].port == port) {
            m->io_table[i].read = (UCHAR)(read != 0);
            m->io_table[i].write = (UCHAR)(write != 0);
            m->io_table[i].enabled = 1;
            hv_lock_release(&m->lock);
            return STATUS_SUCCESS;
        }
    }
    if (m->io_count >= 256) { hv_lock_release(&m->lock); return STATUS_INSUFFICIENT_RESOURCES; }
    m->io_table[m->io_count].port = port;
    m->io_table[m->io_count].read = (UCHAR)(read != 0);
    m->io_table[m->io_count].write = (UCHAR)(write != 0);
    m->io_table[m->io_count].enabled = 1;
    m->io_count++;
    hv_lock_release(&m->lock);
    return STATUS_SUCCESS;
}

int hv_manager_io_clear(hv_manager_t* m, USHORT port) {
    if (!m) return STATUS_INVALID_PARAMETER;
    hv_lock_acquire(&m->lock);
    for (ULONG i = 0; i < m->io_count; ++i) {
        if (m->io_table[i].port == port) {
            m->io_table[i] = m->io_table[m->io_count - 1];
            m->io_count--;
            hv_lock_release(&m->lock);
            return STATUS_SUCCESS;
        }
    }
    hv_lock_release(&m->lock);
    return STATUS_NOT_FOUND;
}

int hv_manager_cpuid_set(hv_manager_t* m, ULONG leaf, ULONG subleaf) {
    if (!m) return STATUS_INVALID_PARAMETER;
    hv_lock_acquire(&m->lock);
    for (ULONG i = 0; i < m->cpuid_count; ++i) {
        if (m->cpuid_table[i].leaf == leaf && m->cpuid_table[i].subleaf == subleaf) {
            m->cpuid_table[i].enabled = 1;
            hv_lock_release(&m->lock);
            return STATUS_SUCCESS;
        }
    }
    if (m->cpuid_count >= 128) { hv_lock_release(&m->lock); return STATUS_INSUFFICIENT_RESOURCES; }
    m->cpuid_table[m->cpuid_count].leaf = leaf;
    m->cpuid_table[m->cpuid_count].subleaf = subleaf;
    m->cpuid_table[m->cpuid_count].enabled = 1;
    m->cpuid_count++;
    hv_lock_release(&m->lock);
    return STATUS_SUCCESS;
}

int hv_manager_cpuid_clear(hv_manager_t* m, ULONG leaf, ULONG subleaf) {
    if (!m) return STATUS_INVALID_PARAMETER;
    hv_lock_acquire(&m->lock);
    for (ULONG i = 0; i < m->cpuid_count; ++i) {
        if (m->cpuid_table[i].leaf == leaf && m->cpuid_table[i].subleaf == subleaf) {
            m->cpuid_table[i] = m->cpuid_table[m->cpuid_count - 1];
            m->cpuid_count--;
            hv_lock_release(&m->lock);
            return STATUS_SUCCESS;
        }
    }
    hv_lock_release(&m->lock);
    return STATUS_NOT_FOUND;
}

int hv_manager_io_get(hv_manager_t* m, USHORT port, int* read, int* write) {
    if (!m || !read || !write) return STATUS_INVALID_PARAMETER;
    hv_lock_acquire(&m->lock);
    for (ULONG i = 0; i < m->io_count; ++i) {
        if (m->io_table[i].port == port) {
            *read = m->io_table[i].read;
            *write = m->io_table[i].write;
            hv_lock_release(&m->lock);
            return STATUS_SUCCESS;
        }
    }
    hv_lock_release(&m->lock);
    return STATUS_NOT_FOUND;
}

int hv_manager_cpuid_get(hv_manager_t* m, ULONG leaf, ULONG subleaf) {
    if (!m) return STATUS_INVALID_PARAMETER;
    hv_lock_acquire(&m->lock);
    for (ULONG i = 0; i < m->cpuid_count; ++i) {
        if (m->cpuid_table[i].leaf == leaf && m->cpuid_table[i].subleaf == subleaf) {
            hv_lock_release(&m->lock);
            return STATUS_SUCCESS;
        }
    }
    hv_lock_release(&m->lock);
    return STATUS_NOT_FOUND;
}

ULONG hv_manager_io_count(hv_manager_t* m) {
    if (!m) return 0;
    return m->io_count;
}

ULONG hv_manager_cpuid_count(hv_manager_t* m) {
    if (!m) return 0;
    return m->cpuid_count;
}

void hv_manager_io_clear_all(hv_manager_t* m) {
    if (!m) return;
    hv_lock_acquire(&m->lock);
    m->io_count = 0;
    hv_lock_release(&m->lock);
}

void hv_manager_cpuid_clear_all(hv_manager_t* m) {
    if (!m) return;
    hv_lock_acquire(&m->lock);
    m->cpuid_count = 0;
    hv_lock_release(&m->lock);
}
