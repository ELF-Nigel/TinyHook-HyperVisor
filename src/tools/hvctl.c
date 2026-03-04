// hvctl.c - usermode control client (status/config only)
#include <windows.h>
#include <stdio.h>
#include "public/hv_control.h"

static int hvctl_open(HANDLE* out) {
    HANDLE h = CreateFileA("\\\\.\\hvcontrol", GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    *out = h;
    return 1;
}

static void print_status(const hvctl_status_t* st) {
    printf("driver_state=%u hv_state=%u features=0x%08lx config=0x%08lx\n",
           st->DriverState, st->HvState, (unsigned long)st->FeatureFlags, (unsigned long)st->ConfigFlags);
    printf("policy_flags=0x%08lx last_error=0x%08lx config_rev=%llu log_seq=%llu\n",
           (unsigned long)st->PolicyFlags, (unsigned long)st->LastError,
           (unsigned long long)st->ConfigRevision, (unsigned long long)st->LogSeq);
}

static void print_features(const hvctl_feature_list_t* list) {
    printf("features (%lu):\n", (unsigned long)list->Count);
    for (ULONG i = 0; i < list->Count && i < 32; ++i) {
        printf(" - [%s] %lu %s\n",
               list->Entries[i].Enabled ? "on" : "off",
               (unsigned long)list->Entries[i].Id,
               list->Entries[i].Name);
    }
}

static void print_exit_stats(const hvctl_exit_stats_t* st) {
    printf("vmexit: total=%llu cpuid=%llu rdtsc=%llu rdmsr=%llu wrmsr=%llu\n",
           (unsigned long long)st->TotalExits,
           (unsigned long long)st->CpuidExits,
           (unsigned long long)st->RdtscExits,
           (unsigned long long)st->MsrReads,
           (unsigned long long)st->MsrWrites);
    printf("vmexit: cr=%llu io=%llu ept=%llu vmcall=%llu last=0x%llx\n",
           (unsigned long long)st->CrAccessExits,
           (unsigned long long)st->IoExits,
           (unsigned long long)st->EptExits,
           (unsigned long long)st->VmcallExits,
           (unsigned long long)st->LastReason);
}

static void print_hvlog(const hvctl_hvlog_chunk_t* log) {
    printf("hvlog: entries=%lu dropped=%lu cursor=%llu\n",
           (unsigned long)log->Count,
           (unsigned long)log->Dropped,
           (unsigned long long)log->Cursor);
    for (ULONG i = 0; i < log->Count && i < HVCTL_HVLOG_MAX_ENTRIES; ++i) {
        const hvctl_hvlog_entry_t* e = &log->Entries[i];
        printf(" - ts=%llu type=%lu data1=0x%llx data2=0x%llx\n",
               (unsigned long long)e->Timestamp100ns,
               (unsigned long)e->EventType,
               (unsigned long long)e->Data1,
               (unsigned long long)e->Data2);
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    HANDLE h = NULL;
    if (!hvctl_open(&h)) {
        printf("hvctl: failed to open \\\\.\\hvcontrol\n");
        return 1;
    }

    hvctl_request_t req;
    hvctl_response_t resp;
    DWORD bytes = 0;
    ZeroMemory(&req, sizeof(req));
    ZeroMemory(&resp, sizeof(resp));

    req.Header.Version = HV_CONTROL_VERSION;
    req.Header.Command = IOCTL_HV_GET_STATUS;
    req.Header.Size = sizeof(req);

    BOOL ok = DeviceIoControl(h, IOCTL_HV_GET_STATUS,
                              &req, sizeof(req),
                              &resp, sizeof(resp),
                              &bytes, NULL);
    if (!ok || bytes < sizeof(resp)) {
        printf("hvctl: ioctl failed (gle=%lu)\n", (unsigned long)GetLastError());
        CloseHandle(h);
        return 1;
    }

    print_status(&resp.Payload.Status);

    ZeroMemory(&req, sizeof(req));
    ZeroMemory(&resp, sizeof(resp));
    req.Header.Version = HV_CONTROL_VERSION;
    req.Header.Command = IOCTL_HV_LIST_FEATURES;
    req.Header.Size = sizeof(req);

    ok = DeviceIoControl(h, IOCTL_HV_LIST_FEATURES,
                         &req, sizeof(req),
                         &resp, sizeof(resp),
                         &bytes, NULL);
    if (ok && bytes >= sizeof(resp)) {
        print_features(&resp.Payload.Features);
    }

    ZeroMemory(&req, sizeof(req));
    ZeroMemory(&resp, sizeof(resp));
    req.Header.Version = HV_CONTROL_VERSION;
    req.Header.Command = IOCTL_HV_GET_EXIT_STATS;
    req.Header.Size = sizeof(req);

    ok = DeviceIoControl(h, IOCTL_HV_GET_EXIT_STATS,
                         &req, sizeof(req),
                         &resp, sizeof(resp),
                         &bytes, NULL);
    if (ok && bytes >= sizeof(resp)) {
        print_exit_stats(&resp.Payload.ExitStats);
    }

    ZeroMemory(&req, sizeof(req));
    ZeroMemory(&resp, sizeof(resp));
    req.Header.Version = HV_CONTROL_VERSION;
    req.Header.Command = IOCTL_HV_GET_HVLOG;
    req.Header.Size = sizeof(req);
    req.Payload.HvLogCursor = 0;

    ok = DeviceIoControl(h, IOCTL_HV_GET_HVLOG,
                         &req, sizeof(req),
                         &resp, sizeof(resp),
                         &bytes, NULL);
    if (ok && bytes >= sizeof(resp)) {
        print_hvlog(&resp.Payload.HvLog);
    }
    CloseHandle(h);
    return 0;
}
