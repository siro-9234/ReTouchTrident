#pragma once

#include <ntddk.h>

#define TRIDENT_PATCH_INSPECTION_BYTES 16

typedef struct _TRIDENT_PATCH_INSPECTION_RESULT
{
    PVOID TargetAddress;

    SIZE_T InspectedLength;

    BOOLEAN InstructionAligned;

    BOOLEAN ContainsRipRelative;

    BOOLEAN ContainsRelativeCall;

    UCHAR OriginalBytes[TRIDENT_PATCH_INSPECTION_BYTES];

} TRIDENT_PATCH_INSPECTION_RESULT,
* PTRIDENT_PATCH_INSPECTION_RESULT;


class TridentPatchTargetInspector
{
public:

    static NTSTATUS Inspect(
        _In_ PVOID TargetAddress,
        _Out_ PTRIDENT_PATCH_INSPECTION_RESULT Result
    );

};