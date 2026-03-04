// control.c - driver control plane (non-operational hv actions)
#include "driver/control/control.h"
#include <ntstrsafe.h>
#include "driver/core/stats.h"
#include "driver/util/ringlog.h"

#define HVCTL_LOG_CAP 64

typedef struct hvctl_log_entry_t {
    ULONGLONG Seq;
    CHAR Text[HVCTL_LOG_ENTRY_LEN];
} hvctl_log_entry_t;

static hvctl_state_t g_hvctl;
static hvctl_log_entry_t g_log[HVCTL_LOG_CAP];
static ULONG g_log_head = 0;

static VOID hvctl_log_add(const CHAR* Text) {
    KIRQL old_irql;
    KeAcquireSpinLock(&g_hvctl.Lock, &old_irql);
    ULONGLONG seq = ++g_hvctl.LogSeq;
    hvctl_log_entry_t* e = &g_log[g_log_head % HVCTL_LOG_CAP];
    e->Seq = seq;
    RtlZeroMemory(e->Text, sizeof(e->Text));
    if (Text) {
        (VOID)RtlStringCchCopyA(e->Text, HVCTL_LOG_ENTRY_LEN, Text);
    }
    g_log_head++;
    KeReleaseSpinLock(&g_hvctl.Lock, old_irql);
}

static ULONG hvctl_any_cpu_launched(const hv_state_t* hv) {
    if (!hv || !hv->cpus) return 0;
    for (ULONG i = 0; i < hv->cpu_count; ++i) {
        if (hv->cpus[i].launched) return 1;
    }
    return 0;
}

static VOID hvctl_build_status(hvctl_status_t* out_status) {
    if (!out_status) return;
    RtlZeroMemory(out_status, sizeof(*out_status));
    out_status->Version = HV_CONTROL_VERSION;
    out_status->DriverState = g_hvctl.DriverState;
    out_status->PolicyFlags = HVCTL_STATUS_STAGED_ONLY;
    out_status->LastError = g_hvctl.LastError;
    out_status->ConfigRevision = g_hvctl.ConfigRevision;
    out_status->LogSeq = g_hvctl.LogSeq;

    ULONGLONG now = 0;
    KeQuerySystemTimePrecise((PLARGE_INTEGER)&now);
    out_status->LastUpdate100ns = now;

    hv_state_t* hv = g_hvctl.Backend;
    if (hv && hv->initialized) {
        out_status->HvState = hvctl_any_cpu_launched(hv) ? HVCTL_HV_RUNNING : HVCTL_HV_INITIALIZED;
        out_status->FeatureFlags = hv_query_features(hv);
        out_status->ConfigFlags = hv_config_flags(&hv->active_config);
    } else {
        out_status->HvState = HVCTL_HV_OFF;
        out_status->FeatureFlags = hv_query_features_auto();
        out_status->ConfigFlags = g_hvctl.StagedConfig.Flags;
    }
}

static NTSTATUS hvctl_handle_get_status(hvctl_response_t* resp) {
    resp->Result = HVCTL_RES_OK;
    hvctl_build_status(&resp->Payload.Status);
    return STATUS_SUCCESS;
}

static NTSTATUS hvctl_handle_get_config(hvctl_response_t* resp) {
    resp->Result = HVCTL_RES_OK;
    resp->Payload.Config = g_hvctl.StagedConfig;
    return STATUS_SUCCESS;
}

static NTSTATUS hvctl_handle_set_config(const hvctl_request_t* req, hvctl_response_t* resp) {
    hvctl_config_t cfg = req->Payload.Config;
    if (cfg.LogLevel > 3 || cfg.VmexitSample > 1000) {
        resp->Result = HVCTL_RES_INVALID;
        return STATUS_INVALID_PARAMETER;
    }
    g_hvctl.StagedConfig = cfg;
    g_hvctl.ConfigRevision++;
    resp->Result = HVCTL_RES_OK;
    resp->Payload.Config = g_hvctl.StagedConfig;
    return STATUS_SUCCESS;
}

static NTSTATUS hvctl_handle_feature(const hvctl_request_t* req, hvctl_response_t* resp) {
    ULONG mask = 0;
    switch (req->Payload.Feature.FeatureId) {
        case HVCTL_FEAT_VMX: mask = HVCTL_CFG_VMX; break;
        case HVCTL_FEAT_EPT: mask = HVCTL_CFG_EPT; break;
        case HVCTL_FEAT_VPID: mask = HVCTL_CFG_VPID; break;
        case HVCTL_FEAT_SVM: mask = HVCTL_CFG_SVM; break;
        case HVCTL_FEAT_NPT: mask = HVCTL_CFG_NPT; break;
        default:
            resp->Result = HVCTL_RES_INVALID;
            return STATUS_INVALID_PARAMETER;
    }
    if (req->Payload.Feature.Enable) g_hvctl.StagedConfig.Flags |= mask;
    else g_hvctl.StagedConfig.Flags &= ~mask;
    g_hvctl.ConfigRevision++;
    resp->Result = HVCTL_RES_OK;
    resp->Payload.Config = g_hvctl.StagedConfig;
    return STATUS_SUCCESS;
}

static NTSTATUS hvctl_handle_get_log(const hvctl_request_t* req, hvctl_response_t* resp) {
    ULONGLONG cursor = req->Payload.LogCursor;
    hvctl_log_chunk_t* out = &resp->Payload.Log;
    RtlZeroMemory(out, sizeof(*out));
    ULONG count = 0;
    for (ULONG i = 0; i < HVCTL_LOG_CAP && count < HVCTL_LOG_MAX_ENTRIES; ++i) {
        hvctl_log_entry_t* e = &g_log[i];
        if (e->Seq > cursor && e->Seq != 0) {
            CHAR* dst = &out->Entries[count * HVCTL_LOG_ENTRY_LEN];
            RtlStringCchCopyA(dst, HVCTL_LOG_ENTRY_LEN, e->Text);
            count++;
            out->Cursor = e->Seq;
        }
    }
    out->Count = count;
    resp->Result = HVCTL_RES_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS hvctl_handle_list_features(hvctl_response_t* resp) {
    hvctl_feature_list_t* out = &resp->Payload.Features;
    RtlZeroMemory(out, sizeof(*out));

    hv_feature_snapshot_t snap;
    hv_feature_snapshot(&snap);
    out->Count = snap.Count;
    for (ULONG i = 0; i < snap.Count && i < 32; ++i) {
        out->Entries[i].Id = snap.Entries[i].Id;
        out->Entries[i].Enabled = snap.Entries[i].Enabled ? 1 : 0;
        RtlZeroMemory(out->Entries[i].Name, sizeof(out->Entries[i].Name));
        RtlStringCchCopyA(out->Entries[i].Name, HVCTL_LOG_ENTRY_LEN, snap.Entries[i].Name);
    }
    resp->Result = HVCTL_RES_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS hvctl_handle_get_exit_stats(hvctl_response_t* resp) {
    hv_exit_stats_t snap;
    hv_exit_stats_snapshot(&snap);
    resp->Payload.ExitStats.TotalExits = snap.TotalExits;
    resp->Payload.ExitStats.CpuidExits = snap.CpuidExits;
    resp->Payload.ExitStats.RdtscExits = snap.RdtscExits;
    resp->Payload.ExitStats.MsrReads = snap.MsrReads;
    resp->Payload.ExitStats.MsrWrites = snap.MsrWrites;
    resp->Payload.ExitStats.CrAccessExits = snap.CrAccessExits;
    resp->Payload.ExitStats.IoExits = snap.IoExits;
    resp->Payload.ExitStats.EptExits = snap.EptExits;
    resp->Payload.ExitStats.VmcallExits = snap.VmcallExits;
    resp->Payload.ExitStats.LastReason = snap.LastReason;
    for (ULONG i = 0; i < 64; ++i) {
        resp->Payload.ExitStats.ReasonCounts[i] = snap.ReasonCounts[i];
    }
    resp->Result = HVCTL_RES_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS hvctl_handle_get_hvlog(const hvctl_request_t* req, hvctl_response_t* resp) {
    hv_ringlog_chunk_t chunk;
    hv_ringlog_read(req->Payload.HvLogCursor, &chunk);

    hvctl_hvlog_chunk_t* out = &resp->Payload.HvLog;
    RtlZeroMemory(out, sizeof(*out));
    out->Cursor = chunk.Cursor;
    out->Count = chunk.Count;
    out->Dropped = chunk.Dropped;
    for (ULONG i = 0; i < chunk.Count && i < HVCTL_HVLOG_MAX_ENTRIES; ++i) {
        out->Entries[i].Timestamp100ns = chunk.Entries[i].Timestamp100ns;
        out->Entries[i].EventType = chunk.Entries[i].EventType;
        out->Entries[i].Reserved = 0;
        out->Entries[i].Data1 = chunk.Entries[i].Data1;
        out->Entries[i].Data2 = chunk.Entries[i].Data2;
    }
    resp->Result = HVCTL_RES_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS hvctl_complete(PIRP Irp, NTSTATUS Status, ULONG_PTR Info) {
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

static NTSTATUS hvctl_dispatch_create(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    return hvctl_complete(Irp, STATUS_SUCCESS, 0);
}

static NTSTATUS hvctl_dispatch_close(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    return hvctl_complete(Irp, STATUS_SUCCESS, 0);
}

static NTSTATUS hvctl_dispatch_device_control(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    ULONG in_len = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG out_len = stack->Parameters.DeviceIoControl.OutputBufferLength;

    if (!Irp->AssociatedIrp.SystemBuffer || out_len < sizeof(hvctl_response_t)) {
        return hvctl_complete(Irp, STATUS_BUFFER_TOO_SMALL, 0);
    }

    hvctl_request_t* req = (hvctl_request_t*)Irp->AssociatedIrp.SystemBuffer;
    hvctl_response_t* resp = (hvctl_response_t*)Irp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(resp, sizeof(*resp));

    if (in_len >= sizeof(hvctl_request_t)) {
        resp->Header = req->Header;
    } else {
        resp->Header.Version = HV_CONTROL_VERSION;
        resp->Header.Command = code;
        resp->Header.Size = sizeof(hvctl_response_t);
    }

    if (resp->Header.Version != HV_CONTROL_VERSION) {
        resp->Result = HVCTL_RES_INVALID;
        return hvctl_complete(Irp, STATUS_INVALID_PARAMETER, sizeof(hvctl_response_t));
    }

    NTSTATUS st = STATUS_SUCCESS;
    switch (code) {
        case IOCTL_HV_GET_STATUS:
            st = hvctl_handle_get_status(resp);
            break;
        case IOCTL_HV_GET_CONFIG:
            st = hvctl_handle_get_config(resp);
            break;
        case IOCTL_HV_SET_CONFIG:
            if (in_len < sizeof(hvctl_request_t)) return hvctl_complete(Irp, STATUS_BUFFER_TOO_SMALL, 0);
            st = hvctl_handle_set_config(req, resp);
            break;
        case IOCTL_HV_ENABLE_FEATURE:
        case IOCTL_HV_DISABLE_FEATURE:
            if (in_len < sizeof(hvctl_request_t)) return hvctl_complete(Irp, STATUS_BUFFER_TOO_SMALL, 0);
            st = hvctl_handle_feature(req, resp);
            break;
        case IOCTL_HV_QUERY_CPU:
            resp->Result = HVCTL_RES_NOT_SUPPORTED;
            st = STATUS_NOT_SUPPORTED;
            break;
        case IOCTL_HV_GET_LOG:
            if (in_len < sizeof(hvctl_request_t)) return hvctl_complete(Irp, STATUS_BUFFER_TOO_SMALL, 0);
            st = hvctl_handle_get_log(req, resp);
            break;
        case IOCTL_HV_LIST_FEATURES:
            st = hvctl_handle_list_features(resp);
            break;
        case IOCTL_HV_GET_EXIT_STATS:
            st = hvctl_handle_get_exit_stats(resp);
            break;
        case IOCTL_HV_GET_HVLOG:
            if (in_len < sizeof(hvctl_request_t)) return hvctl_complete(Irp, STATUS_BUFFER_TOO_SMALL, 0);
            st = hvctl_handle_get_hvlog(req, resp);
            break;
        default:
            resp->Result = HVCTL_RES_NOT_SUPPORTED;
            st = STATUS_NOT_SUPPORTED;
            break;
    }

    return hvctl_complete(Irp, st, sizeof(hvctl_response_t));
}

NTSTATUS hvctl_attach_driver(PDRIVER_OBJECT DriverObject) {
    if (!DriverObject) return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(&g_hvctl, sizeof(g_hvctl));
    KeInitializeSpinLock(&g_hvctl.Lock);
    hv_stats_init();
    hv_ringlog_init();

    g_hvctl.DriverState = HVCTL_DRIVER_STARTING;
    g_hvctl.StagedConfig.Flags = 0;
    g_hvctl.StagedConfig.LogLevel = 0;
    g_hvctl.StagedConfig.VmexitSample = 0;
    g_hvctl.ConfigRevision = 1;

    RtlInitUnicodeString(&g_hvctl.DeviceName, HV_CONTROL_DEVICE_NT);
    RtlInitUnicodeString(&g_hvctl.SymbolicName, HV_CONTROL_DEVICE_DOS);

    NTSTATUS st = IoCreateDevice(DriverObject, 0, &g_hvctl.DeviceName,
                                 FILE_DEVICE_UNKNOWN, 0, FALSE, &g_hvctl.Device);
    if (!NT_SUCCESS(st)) {
        g_hvctl.DriverState = HVCTL_DRIVER_ERROR;
        g_hvctl.LastError = st;
        return st;
    }

    st = IoCreateSymbolicLink(&g_hvctl.SymbolicName, &g_hvctl.DeviceName);
    if (!NT_SUCCESS(st)) {
        IoDeleteDevice(g_hvctl.Device);
        g_hvctl.Device = NULL;
        g_hvctl.DriverState = HVCTL_DRIVER_ERROR;
        g_hvctl.LastError = st;
        return st;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = hvctl_dispatch_create;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = hvctl_dispatch_close;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = hvctl_dispatch_device_control;

    g_hvctl.DriverState = HVCTL_DRIVER_ACTIVE;
    g_hvctl.Initialized = 1;
    hvctl_log_add("control attached");
    return STATUS_SUCCESS;
}

VOID hvctl_detach_driver(VOID) {
    hvctl_log_add("control detach");
    if (g_hvctl.Device) {
        IoDeleteSymbolicLink(&g_hvctl.SymbolicName);
        IoDeleteDevice(g_hvctl.Device);
        g_hvctl.Device = NULL;
    }
    g_hvctl.DriverState = HVCTL_DRIVER_STOPPED;
}

VOID hvctl_set_backend(hv_state_t* Hv) {
    KIRQL old_irql;
    KeAcquireSpinLock(&g_hvctl.Lock, &old_irql);
    g_hvctl.Backend = Hv;
    KeReleaseSpinLock(&g_hvctl.Lock, old_irql);
}
