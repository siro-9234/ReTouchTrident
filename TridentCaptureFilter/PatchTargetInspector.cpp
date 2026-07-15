#include "PatchTargetInspector.h"


NTSTATUS
TridentPatchTargetInspector::Inspect(
    _In_ PVOID TargetAddress,
    _Out_ PTRIDENT_PATCH_INSPECTION_RESULT Result
)
{
    if (TargetAddress == nullptr ||
        Result == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }


    RtlZeroMemory(
        Result,
        sizeof(TRIDENT_PATCH_INSPECTION_RESULT)
    );


    Result->TargetAddress = TargetAddress;

    Result->InspectedLength =
        TRIDENT_PATCH_INSPECTION_BYTES;


    RtlCopyMemory(
        Result->OriginalBytes,
        TargetAddress,
        TRIDENT_PATCH_INSPECTION_BYTES
    );


    return STATUS_SUCCESS;
}