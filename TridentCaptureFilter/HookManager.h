#pragma once

#include <ntddk.h>

typedef enum _TRIDENT_HOOK_STATE
{
    TridentHookStateUninitialized = 0,
    TridentHookStateUnavailable,
    TridentHookStateReady,
    TridentHookStateEnabled,
    TridentHookStateFailed
} TRIDENT_HOOK_STATE;

namespace TridentHookManager
{
    VOID Initialize();
    VOID Reset();

    NTSTATUS Configure(
        _In_ PVOID CandidateAddress,
        _In_ PVOID SkipTargetAddress
    );

    NTSTATUS Enable();
    NTSTATUS Disable();
    TRIDENT_HOOK_STATE GetState();
}
