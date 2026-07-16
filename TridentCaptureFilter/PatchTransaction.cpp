#include "PatchTransaction.h"


VOID
TridentPatchTransaction::Initialize(
    _Out_ PTRIDENT_PATCH_TRANSACTION Transaction
)
{
    if (Transaction == nullptr)
    {
        return;
    }

    RtlZeroMemory(
        Transaction,
        sizeof(TRIDENT_PATCH_TRANSACTION)
    );
}


NTSTATUS
TridentPatchTransaction::Prepare(
    _Out_ PTRIDENT_PATCH_TRANSACTION Transaction,
    _In_ PVOID TargetAddress,
    _In_reads_bytes_(Length) const UCHAR* OriginalBytes,
    _In_ SIZE_T Length
)
{
    if (Transaction == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // A failed retry must never preserve an older prepared state.
    //
    Initialize(
        Transaction
    );

    if (TargetAddress == nullptr ||
        OriginalBytes == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Length == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (Length > TRIDENT_PATCH_MAX_BYTES)
    {
        return STATUS_BUFFER_TOO_SMALL;
    }

    Transaction->TargetAddress =
        TargetAddress;

    RtlCopyMemory(
        Transaction->OriginalBytes,
        OriginalBytes,
        Length
    );

    Transaction->Length =
        Length;

    Transaction->Prepared = TRUE;
    Transaction->Applied = FALSE;

    return STATUS_SUCCESS;
}

VOID
TridentPatchTransaction::Reset(
    _Inout_ PTRIDENT_PATCH_TRANSACTION Transaction
)
{
    if (Transaction == nullptr)
    {
        return;
    }


    RtlZeroMemory(
        Transaction,
        sizeof(TRIDENT_PATCH_TRANSACTION)
    );
}
