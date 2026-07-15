#include "HookManager.h"

namespace
{
    volatile LONG g_State = TridentHookStateUninitialized;
    PVOID g_CandidateAddress = nullptr;
    PVOID g_SkipTargetAddress = nullptr;
}

VOID
TridentHookManager::Initialize()
{
    g_CandidateAddress = nullptr;
    g_SkipTargetAddress = nullptr;
    InterlockedExchange(&g_State, TridentHookStateUnavailable);
}

VOID
TridentHookManager::Reset()
{
    // No executable memory is modified by the scaffold, so reset is state-only.
    g_CandidateAddress = nullptr;
    g_SkipTargetAddress = nullptr;
    InterlockedExchange(&g_State, TridentHookStateUnavailable);
}

NTSTATUS
TridentHookManager::Configure(
    _In_ PVOID CandidateAddress,
    _In_ PVOID SkipTargetAddress
)
{
    if (CandidateAddress == nullptr || SkipTargetAddress == nullptr)
    {
        InterlockedExchange(&g_State, TridentHookStateFailed);
        return STATUS_INVALID_PARAMETER;
    }

    g_CandidateAddress = CandidateAddress;
    g_SkipTargetAddress = SkipTargetAddress;
    InterlockedExchange(&g_State, TridentHookStateReady);
    return STATUS_SUCCESS;
}

NTSTATUS
TridentHookManager::Enable()
{
    UNREFERENCED_PARAMETER(g_CandidateAddress);
    UNREFERENCED_PARAMETER(g_SkipTargetAddress);

    // Deliberately unavailable in the safe scaffold. No patching is performed.
    return STATUS_NOT_SUPPORTED;
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
