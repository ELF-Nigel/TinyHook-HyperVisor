// efi main skeleton
#include <Uefi.h>
#include <Library/UefiLib.h>

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    (void)ImageHandle; (void)SystemTable;
    return EFI_UNSUPPORTED;
}
