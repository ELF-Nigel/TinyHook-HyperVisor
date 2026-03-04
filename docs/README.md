# elf hypervisor

A Windows x64 hypervisor skeleton (Intel VT-x + AMD-V) that mirrors elf's philosophy: minimal, self-contained, fast iteration. This repo ships a usermode-safe build that compiles in CI, plus placeholders for a real kernel driver implementation.

## Status
- Usermode skeleton builds in CI
- Kernel driver implementation is stubbed and will be added next

## Build (CI)
CI compiles a usermode stub with `elfhv_usermode` to verify API integration.

## Build (Kernel Driver)
Requires Windows WDK + MSVC. The kernel driver code will live under `src/driver/`.

## Credits
Discord: chefendpoint
Telegram: ELF_Nigel

## Driver Skeleton (WDM)
- `src/driver/driver_entry.c` provides DriverEntry/Unload and a device object.
- `src/driver/vmx.c` has VT-x capability checks and VMXON region allocation placeholders.

## Next Steps
- implement CR4.VMXE, IA32_FEATURE_CONTROL, VMXON/VMXOFF
- add VMCS setup and EPT
- wire hypercore-managed lifecycle policies

## Usermode Controller
`src/tools/hvctl.c` is a no-op stub (ioctl control is disabled by design).

## Build (Usermode)
Compile with MSVC:
```
cl /nologo /W4 src\tools\hvctl.c
```

## Build (Driver)
Use WDK + MSVC. Driver sources are in `src/driver/`.

## Advanced Skeleton Additions
- VMCS constants and exit reason enums (`src/driver/vmcs.h`)
- MSR/IO bitmap allocation placeholders
- EPT scaffolding + per-CPU state
- VM-exit dispatch with reason mapping

## VMCS Host/Guest State
`src/driver/vmx.c` now includes a `vmx_setup_host_guest_state()` placeholder where host/guest CRs, segment selectors, and control fields should be written.

## AMD-V Skeleton
- `src/driver/svm.h` / `src/driver/svm.c` provide AMD-V stubs (SVM enable + VMCB allocation).
- Wired into per-CPU state in `hv.c`.

## Stats
VM-exit counters are tracked inside the hypercore.

## Inline Hook Interfaces (Skeleton)
`src/driver/hook.h` provides stub interfaces for inline hook registration and enable/disable.

## Additional Hook Skeletons
- EPT/NPT hooks
- MSR hooks
- IO port hooks
- Syscall hooks
- CPUID hooks
All return STATUS_NOT_IMPLEMENTED and are safe stubs for you to fill in.

## EFI Skeleton
See `efi/` for a placeholder UEFI loader.

## More Hook Skeletons
- VMCALL hooks
- Page-fault hooks
- Interrupt/NMI hooks
- Syscall table hooks
- Guest memory hooks

## Hypercall Interface
`src/driver/hypercall.h/.c` provides a stub hypercall dispatcher.

## VM-exit Router (Skeleton)
`src/driver/vmexit.h/.c` provides a simple routing table for VM-exit reasons.

## Hook Map
`hv_hook_map_t` allows registering up to 64 stub handlers.

## Diagnostics Tools
- `src/driver/diag.h/.c` provides VMX/SVM dump stubs.

## Advanced API Manager
- `src/driver/manager.h/.c` provides a central API manager skeleton.

## Write Options
- `src/driver/write.h/.c` provides write-mode stubs (direct/EPT/NPT).

## EPT/NPT Trapping (Annotated Skeleton)
- `src/driver/write.c` outlines where EPT/NPT read/write/execute trapping would be wired.
- `src/driver/ept.c` contains identity map placeholders and EPTP setup notes.
- `src/driver/svm.c` includes NPT TODOs.

## Execute Trap + EPT Violation (Annotated)
- `vmexit_handle_execute_trap` and `vmexit_handle_ept_violation` are stubbed in `src/driver/vmx.c`.
- `src/driver/vmexit.c` includes routing for EPT violations.

## AMD NPT Violation (Annotated)
- `svm_handle_npt_violation` is stubbed in `src/driver/svm.c`.

## Per-CPU Permission Maps
`src/driver/map.h/.c` provides a fixed-size GPA permission map skeleton.
