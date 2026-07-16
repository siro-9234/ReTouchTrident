#pragma once

#include <ntddk.h>

#include "PatchTransaction.h"

class TridentTransactionValidator
{
public:

    //
    // Validate a prepared transaction.
    //
    // This routine never modifies the transaction.
    // It only verifies that the transaction is internally
    // consistent before later phases use it.
    //
    static BOOLEAN Validate(
        _In_ const TRIDENT_PATCH_TRANSACTION* Transaction
    );
};