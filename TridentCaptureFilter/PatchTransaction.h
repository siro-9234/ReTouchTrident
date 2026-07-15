#pragma once
#pragma once

#include <ntddk.h>

#define TRIDENT_PATCH_MAX_BYTES 16


typedef struct _TRIDENT_PATCH_TRANSACTION
{
    PVOID TargetAddress;

    UCHAR OriginalBytes[TRIDENT_PATCH_MAX_BYTES];

    SIZE_T Length;

    BOOLEAN Prepared;

    BOOLEAN Applied;

} TRIDENT_PATCH_TRANSACTION,
* PTRIDENT_PATCH_TRANSACTION;


class TridentPatchTransaction
{
public:

    static VOID Initialize(
        _Out_ PTRIDENT_PATCH_TRANSACTION Transaction
    );


    static NTSTATUS Prepare(
        _Out_ PTRIDENT_PATCH_TRANSACTION Transaction,
        _In_ PVOID TargetAddress,
        _In_reads_bytes_(Length) const UCHAR* OriginalBytes,
        _In_ SIZE_T Length
    );


    static VOID Reset(
        _Inout_ PTRIDENT_PATCH_TRANSACTION Transaction
    );
};