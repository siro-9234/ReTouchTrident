#include "HookManager.h"
#include "PatchTargetInspector.h"

namespace
{
    volatile LONG g_State = TridentHookStateUninitialized;
    PVOID g_CandidateAddress = nullptr;
    PVOID g_SkipTargetAddress = nullptr;

    UCHAR g_OriginalBytes[16] = {};
    SIZE_T g_OriginalLength = 0;

    TRIDENT_PATCH_INSPECTION_RESULT g_InspectionResult = {};
}

VOID
TridentHookManager::Initialize()
{
    g_CandidateAddress = nullptr;
    g_SkipTargetAddress = nullptr;

    RtlZeroMemory(g_OriginalBytes, sizeof(g_OriginalBytes));
    g_OriginalLength = 0;

    RtlZeroMemory(
        &g_InspectionResult,
        sizeof(g_InspectionResult)
    );

    InterlockedExchange(
        &g_State,
        TridentHookStateUnavailable
    );
}

VOID
TridentHookManager::Reset()
{
    // No executable memory is modified by the scaffold, so reset is state-only.
    g_CandidateAddress = nullptr;
    g_SkipTargetAddress = nullptr;

    RtlZeroMemory(g_OriginalBytes, sizeof(g_OriginalBytes));
    g_OriginalLength = 0;

    RtlZeroMemory(
        &g_InspectionResult,
        sizeof(g_InspectionResult)
    );

    InterlockedExchange(
        &g_State,
        TridentHookStateUnavailable
    );
}

NTSTATUS
TridentHookManager::Configure(
    _In_ PVOID CandidateAddress,
    _In_ PVOID SkipTargetAddress
)
{
    if (CandidateAddress == nullptr ||
        SkipTargetAddress == nullptr ||
        SkipTargetAddress <= CandidateAddress)
    {
        InterlockedExchange(
            &g_State,
            TridentHookStateFailed
        );

        return STATUS_INVALID_PARAMETER;
    }

    const LONG currentState =
        InterlockedCompareExchange(
            &g_State,
            0,
            0
        );

    if (currentState == TridentHookStateReady)
    {
        if (g_CandidateAddress == CandidateAddress &&
            g_SkipTargetAddress == SkipTargetAddress)
        {
            return STATUS_SUCCESS;
        }

        return STATUS_INVALID_DEVICE_STATE;
    }

    if (currentState == TridentHookStateFailed ||
        g_CandidateAddress != nullptr ||
        g_SkipTargetAddress != nullptr)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    g_CandidateAddress = CandidateAddress;
    g_SkipTargetAddress = SkipTargetAddress;

    MemoryBarrier();

    InterlockedExchange(
        &g_State,
        TridentHookStateReady
    );

    return STATUS_SUCCESS;
}

NTSTATUS
TridentHookManager::Prepare()
{
    if (GetState() != TridentHookStateReady)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (g_CandidateAddress == nullptr)
    {
        return STATUS_INVALID_ADDRESS;
    }

    const NTSTATUS status =
        TridentPatchTargetInspector::Inspect(
            g_CandidateAddress,
            &g_InspectionResult
        );

    if (!NT_SUCCESS(status))
    {
        RtlZeroMemory(
            &g_InspectionResult,
            sizeof(g_InspectionResult)
        );

        return status;
    }

    RtlCopyMemory(
        g_OriginalBytes,
        g_InspectionResult.OriginalBytes,
        sizeof(g_OriginalBytes)
    );

    g_OriginalLength =
        g_InspectionResult.InspectedLength;

    return STATUS_SUCCESS;
}

NTSTATUS
TridentHookManager::Enable()
{
    if (InterlockedCompareExchange(
        &g_State,
        TridentHookStateEnabled,
        TridentHookStateReady) != TridentHookStateReady)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
TridentHookManager::Disable()
{
    if (GetState() == TridentHookStateEnabled)
    {
        // A future implementation must restore original state before returning success.
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

TRIDENT_HOOK_STATE
TridentHookManager::GetState()
{
    return static_cast<TRIDENT_HOOK_STATE>(InterlockedCompareExchange(&g_State, 0, 0));
}
