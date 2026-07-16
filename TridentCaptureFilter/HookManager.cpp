#include "HookManager.h"
#include "PatchTargetInspector.h"
#include "PatchTransaction.h"
#include "EarlyExitPlan.h"
#include "TransactionValidator.h"

namespace
{
    volatile LONG g_State = TridentHookStateUninitialized;
    PVOID g_CandidateAddress = nullptr;
    PVOID g_SkipTargetAddress = nullptr;

    TRIDENT_PATCH_INSPECTION_RESULT g_InspectionResult = {};

    TRIDENT_PATCH_TRANSACTION g_PatchTransaction = {};
}

VOID
TridentHookManager::Initialize()
{
    g_CandidateAddress = nullptr;
    g_SkipTargetAddress = nullptr;

    RtlZeroMemory(
        &g_InspectionResult,
        sizeof(g_InspectionResult)
    );

    TridentPatchTransaction::Initialize(
        &g_PatchTransaction
    );

    TridentEarlyExitPlan::Initialize(
        &g_EarlyExitPlan
    );

    InterlockedExchange(
        &g_State,
        TridentHookStateUnavailable
    );
}

VOID
TridentHookManager::Reset()
{
    TridentPatchTransaction::Reset(
        &g_PatchTransaction
    );

    TridentEarlyExitPlan::Initialize(
        &g_EarlyExitPlan
    );

    g_CandidateAddress = nullptr;
    g_SkipTargetAddress = nullptr;

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
        SkipTargetAddress == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    const ULONG_PTR candidate =
        reinterpret_cast<ULONG_PTR>(CandidateAddress);

    const ULONG_PTR skipTarget =
        reinterpret_cast<ULONG_PTR>(SkipTargetAddress);

    if (skipTarget <= candidate)
    {
        return STATUS_INVALID_PARAMETER;
    }

    const TRIDENT_HOOK_STATE currentState =
        GetState();

    if (currentState != TridentHookStateUnavailable)
    {
        if (currentState == TridentHookStateReady &&
            g_CandidateAddress == CandidateAddress &&
            g_SkipTargetAddress == SkipTargetAddress)
        {
            return STATUS_SUCCESS;
        }

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

    //
    // Fail closed before rebuilding.
    //
    // A failed retry must never leave a previously prepared
    // transaction or validated plan available.
    //
    RtlZeroMemory(
        &g_InspectionResult,
        sizeof(g_InspectionResult)
    );

    TridentPatchTransaction::Reset(
        &g_PatchTransaction
    );

    TridentEarlyExitPlan::Initialize(
        &g_EarlyExitPlan
    );

    if (g_CandidateAddress == nullptr ||
        g_SkipTargetAddress == nullptr)
    {
        return STATUS_INVALID_ADDRESS;
    }

    //
    // Construct all artifacts locally. Publish them to the
    // global state only after every validation has succeeded.
    //
    TRIDENT_PATCH_INSPECTION_RESULT inspectionResult = {};
    TRIDENT_PATCH_TRANSACTION patchTransaction = {};
    TRIDENT_EARLY_EXIT_PLAN earlyExitPlan = {};

    NTSTATUS status =
        TridentPatchTargetInspector::Inspect(
            g_CandidateAddress,
            &inspectionResult
        );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status =
        TridentPatchTransaction::Prepare(
            &patchTransaction,
            inspectionResult.TargetAddress,
            inspectionResult.OriginalBytes,
            inspectionResult.InspectedLength
        );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status =
        TridentEarlyExitPlan::Build(
            &earlyExitPlan,
            g_CandidateAddress,
            g_SkipTargetAddress
        );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    //
    // Cross-layer consistency validation.
    //
    if (!TridentTransactionValidator::Validate(
        &patchTransaction) ||
        earlyExitPlan.Validated == FALSE ||
        !TridentEarlyExitPlan::Validate(
            &earlyExitPlan) ||
        inspectionResult.InstructionAligned == FALSE ||
        inspectionResult.ContainsRipRelative == FALSE ||
        inspectionResult.ContainsRelativeCall == FALSE ||
        inspectionResult.TargetAddress !=
        g_CandidateAddress ||
        patchTransaction.TargetAddress !=
        g_CandidateAddress ||
        earlyExitPlan.CandidateAddress !=
        g_CandidateAddress ||
        earlyExitPlan.SkipTargetAddress !=
        g_SkipTargetAddress ||
        patchTransaction.Length !=
        inspectionResult.InspectedLength)
    {
        return STATUS_NOT_SUPPORTED;
    }

    if (RtlCompareMemory(
        patchTransaction.OriginalBytes,
        inspectionResult.OriginalBytes,
        patchTransaction.Length
    ) != patchTransaction.Length)
    {
        return STATUS_DATA_ERROR;
    }

    //
    // Publish only the completely validated result.
    //
    RtlCopyMemory(
        &g_InspectionResult,
        &inspectionResult,
        sizeof(g_InspectionResult)
    );

    RtlCopyMemory(
        &g_PatchTransaction,
        &patchTransaction,
        sizeof(g_PatchTransaction)
    );

    RtlCopyMemory(
        &g_EarlyExitPlan,
        &earlyExitPlan,
        sizeof(g_EarlyExitPlan)
    );

    return STATUS_SUCCESS;
}

NTSTATUS
TridentHookManager::Enable()
{
    if (GetState() != TridentHookStateReady)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (!TridentTransactionValidator::Validate(
        &g_PatchTransaction))
    {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Validate() now checks only the contents of the plan.
    // This flag proves that Build() completed successfully.
    //
    if (g_EarlyExitPlan.Validated == FALSE)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    if (!TridentEarlyExitPlan::Validate(
        &g_EarlyExitPlan))
    {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // All layers must describe the same control path.
    //
    if (g_PatchTransaction.TargetAddress !=
        g_CandidateAddress ||
        g_EarlyExitPlan.CandidateAddress !=
        g_CandidateAddress ||
        g_EarlyExitPlan.SkipTargetAddress !=
        g_SkipTargetAddress)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    //
    // No supported execution mechanism has been selected.
    // Do not report Enabled until the Early Exit plan has
    // actually been applied and can be safely rolled back.
    //
    return STATUS_NOT_SUPPORTED;
}

NTSTATUS
TridentHookManager::Disable()
{
    const TRIDENT_HOOK_STATE state =
        GetState();

    if (state == TridentHookStateUnavailable ||
        state == TridentHookStateReady)
    {
        //
        // Nothing may be reported as disabled successfully if
        // either artifact claims that an execution change exists.
        //
        if (g_PatchTransaction.Applied != FALSE ||
            g_EarlyExitPlan.Applied != FALSE)
        {
            return STATUS_INVALID_DEVICE_STATE;
        }

        //
        // Unavailable:
        // Discovery or preparation never produced an applicable plan.
        //
        // Ready:
        // A plan exists, but it has not been applied.
        //
        return STATUS_SUCCESS;
    }

    if (state == TridentHookStateEnabled)
    {
        //
        // A real implementation must restore the original executable
        // state before reporting successful disablement.
        //
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Uninitialized and Failed states are not trusted as clean,
    // reversible states.
    //
    return STATUS_INVALID_DEVICE_STATE;
}

TRIDENT_HOOK_STATE
TridentHookManager::GetState()
{
    return static_cast<TRIDENT_HOOK_STATE>(InterlockedCompareExchange(&g_State, 0, 0));
}
