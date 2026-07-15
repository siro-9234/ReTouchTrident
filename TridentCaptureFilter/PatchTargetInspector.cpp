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

    const UCHAR* const bytes =
        Result->OriginalBytes;

    //
    // Expected layout:
    //
    // 33 DB                         xor ebx, ebx
    // 4C 8B 15 xx xx xx xx         mov r10, qword ptr [rip+disp32]
    // E8 xx xx xx xx               call rel32
    // 33 FF                         xor edi, edi
    //
    const BOOLEAN hasExpectedLayout =
        bytes[0] == 0x33 &&
        bytes[1] == 0xDB &&

        bytes[2] == 0x4C &&
        bytes[3] == 0x8B &&
        bytes[4] == 0x15 &&

        bytes[9] == 0xE8 &&

        bytes[14] == 0x33 &&
        bytes[15] == 0xFF;

    if (!hasExpectedLayout)
    {
        RtlZeroMemory(
            Result,
            sizeof(*Result)
        );

        return STATUS_NOT_SUPPORTED;
    }

    Result->InstructionAligned = TRUE;
    Result->ContainsRipRelative = TRUE;
    Result->ContainsRelativeCall = TRUE;

    return STATUS_SUCCESS;
}