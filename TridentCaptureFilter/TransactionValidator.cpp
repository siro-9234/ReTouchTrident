#include "TransactionValidator.h"

BOOLEAN
TridentTransactionValidator::Validate(
    _In_ const TRIDENT_PATCH_TRANSACTION* Transaction
)
{
    if (Transaction == nullptr)
    {
        return FALSE;
    }

    if (Transaction->TargetAddress == nullptr)
    {
        return FALSE;
    }

    if (Transaction->Length != TRIDENT_PATCH_MAX_BYTES)
    {
        return FALSE;
    }

    if (Transaction->Prepared == FALSE)
    {
        return FALSE;
    }

    if (Transaction->Applied != FALSE)
    {
        return FALSE;
    }

    return TRUE;
}