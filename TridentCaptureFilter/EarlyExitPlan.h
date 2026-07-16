#pragma once

#include <ntddk.h>

typedef struct _TRIDENT_EARLY_EXIT_PLAN
{
    //
    // Discovery results.
    //
    PVOID CandidateAddress;
    PVOID SkipTargetAddress;

    //
    // Validation state.
    //
    BOOLEAN Validated;

    //
    // Execution state.
    //
    BOOLEAN Applied;

} TRIDENT_EARLY_EXIT_PLAN, * PTRIDENT_EARLY_EXIT_PLAN;

EXTERN_C
TRIDENT_EARLY_EXIT_PLAN
g_EarlyExitPlan;

class TridentEarlyExitPlan
{
public:
    static VOID Initialize(
        _Out_ PTRIDENT_EARLY_EXIT_PLAN Plan
    );

    static NTSTATUS Build(
        _Out_ PTRIDENT_EARLY_EXIT_PLAN Plan,
        _In_ PVOID CandidateAddress,
        _In_ PVOID SkipTargetAddress
    );

    static BOOLEAN Validate(
        _In_ const TRIDENT_EARLY_EXIT_PLAN* Plan
    );

};