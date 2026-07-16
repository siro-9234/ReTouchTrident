#include "EarlyExitPlan.h"

TRIDENT_EARLY_EXIT_PLAN
g_EarlyExitPlan = {};

VOID
TridentEarlyExitPlan::Initialize(
    _Out_ PTRIDENT_EARLY_EXIT_PLAN Plan
)
{
    if (Plan == nullptr)
    {
        return;
    }

    RtlZeroMemory(
        Plan,
        sizeof(*Plan)
    );
}

NTSTATUS
TridentEarlyExitPlan::Build(
    _Out_ PTRIDENT_EARLY_EXIT_PLAN Plan,
    _In_ PVOID CandidateAddress,
    _In_ PVOID SkipTargetAddress
)
{
    if (Plan == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Fail closed. A failed build must never leave an older,
    // previously validated plan behind.
    //
    Initialize(
        Plan
    );

    if (CandidateAddress == nullptr ||
        SkipTargetAddress == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    const ULONG_PTR candidate =
        reinterpret_cast<ULONG_PTR>(
            CandidateAddress
            );

    const ULONG_PTR skipTarget =
        reinterpret_cast<ULONG_PTR>(
            SkipTargetAddress
            );

    if (skipTarget <= candidate)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Plan->CandidateAddress =
        CandidateAddress;

    Plan->SkipTargetAddress =
        SkipTargetAddress;

    Plan->Applied = FALSE;

    //
    // Validate the plan contents before marking it validated.
    //
    if (!Validate(
        Plan))
    {
        Initialize(
            Plan
        );

        return STATUS_NOT_SUPPORTED;
    }

    Plan->Validated = TRUE;

    return STATUS_SUCCESS;
}

BOOLEAN
TridentEarlyExitPlan::Validate(
    _In_ const TRIDENT_EARLY_EXIT_PLAN* Plan
)
{
    if (Plan == nullptr)
    {
        return FALSE;
    }

    if (Plan->CandidateAddress == nullptr ||
        Plan->SkipTargetAddress == nullptr)
    {
        return FALSE;
    }

    if (Plan->Applied != FALSE)
    {
        return FALSE;
    }

    const ULONG_PTR candidate =
        reinterpret_cast<ULONG_PTR>(
            Plan->CandidateAddress
            );

    const ULONG_PTR skipTarget =
        reinterpret_cast<ULONG_PTR>(
            Plan->SkipTargetAddress
            );

    if (skipTarget <= candidate)
    {
        return FALSE;
    }

    return TRUE;
}