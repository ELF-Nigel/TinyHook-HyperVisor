# elf hypervisor EFI (EDK2)

This is a minimal EDK2-based bootloader scaffold that can chainload the OS loader
and host hypervisor setup logic.

## Build (EDK2)
1. Clone edk2 and initialize submodules.
2. Copy `elfhvPkg` into `edk2/`.
3. Build:
```
edksetup.bat
build -a X64 -t VS2022 -p elfhvPkg/elfhvPkg.dsc
```
4. Output: `Build/elfhvPkg/DEBUG_VS2022/X64/elfhv.efi`

## Notes
- This is a bootloader scaffold. It does not enable VT-x/AMD-V yet.
- It chainloads the Windows boot manager by default.
