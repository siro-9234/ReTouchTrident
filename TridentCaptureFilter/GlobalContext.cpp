#include "GlobalContext.h"

static TRIDENT_GLOBAL_STATS g_TridentGlobalStats;

extern "C"
VOID
TridentGlobalInitialize()
{
    RtlZeroMemory(&g_TridentGlobalStats, sizeof(g_TridentGlobalStats));
}

extern "C"
PTRIDENT_GLOBAL_STATS
TridentGetGlobalStats()
{
    return &g_TridentGlobalStats;
}

extern "C"
VOID
TridentGlobalRecordFilterActivity(
    _In_ LONG DeviceInstanceId,
    _In_ LONG ClientInstanceId,
    _In_opt_ PVOID DeviceContextPointer,
    _In_opt_ PVOID ClientPointer,
    _In_opt_ PVOID PhysicalDeviceObject,
    _In_opt_ PVOID WdmDeviceObject
)
{
    InterlockedExchange(&g_TridentGlobalStats.LastDeviceInstanceId, DeviceInstanceId);
    InterlockedExchange(&g_TridentGlobalStats.LastClientInstanceId, ClientInstanceId);

    InterlockedExchangePointer(&g_TridentGlobalStats.LastDeviceContextPointer, DeviceContextPointer);
    InterlockedExchangePointer(&g_TridentGlobalStats.LastClientPointer, ClientPointer);
    InterlockedExchangePointer(&g_TridentGlobalStats.LastPhysicalDeviceObject, PhysicalDeviceObject);
    InterlockedExchangePointer(&g_TridentGlobalStats.LastWdmDeviceObject, WdmDeviceObject);
}

extern "C"
VOID
TridentGlobalSnapshot(
    _Out_ PTRIDENT_GLOBAL_STATS_SNAPSHOT Snapshot
)
{
    RtlZeroMemory(Snapshot, sizeof(*Snapshot));

    Snapshot->ControlDeviceCreateAttempt = g_TridentGlobalStats.ControlDeviceCreateAttempt;
    Snapshot->ControlDeviceCreateSucceeded = g_TridentGlobalStats.ControlDeviceCreateSucceeded;
    Snapshot->ControlDeviceIoctlStatsCount = g_TridentGlobalStats.ControlDeviceIoctlStatsCount;

    Snapshot->LastDeviceInstanceId = g_TridentGlobalStats.LastDeviceInstanceId;
    Snapshot->LastClientInstanceId = g_TridentGlobalStats.LastClientInstanceId;

    Snapshot->ReadCompletionCount = g_TridentGlobalStats.ReadCompletionCount;
    Snapshot->DecodeSuccessCount = g_TridentGlobalStats.DecodeSuccessCount;
    Snapshot->SubmitAttemptCount = g_TridentGlobalStats.SubmitAttemptCount;
    Snapshot->SubmitCompletedCount = g_TridentGlobalStats.SubmitCompletedCount;
    Snapshot->SubmitFrameCount = g_TridentGlobalStats.SubmitFrameCount;

    Snapshot->WorkItemEnqueueCount = g_TridentGlobalStats.WorkItemEnqueueCount;
    Snapshot->WorkItemRunCount = g_TridentGlobalStats.WorkItemRunCount;
    Snapshot->LastWorkItemContactCount = g_TridentGlobalStats.LastWorkItemContactCount;
    Snapshot->LastSubmitFrameStatus = g_TridentGlobalStats.LastSubmitFrameStatus;
    Snapshot->LastSendFrameStatus = g_TridentGlobalStats.LastSendFrameStatus;

    Snapshot->OpenInterfaceAttemptCount = g_TridentGlobalStats.OpenInterfaceAttemptCount;
    Snapshot->OpenInterfaceSucceededCount = g_TridentGlobalStats.OpenInterfaceSucceededCount;
    Snapshot->LastOpenInterfaceStatus = g_TridentGlobalStats.LastOpenInterfaceStatus;
    Snapshot->LastOpenedRootSystemNumber = g_TridentGlobalStats.LastOpenedRootSystemNumber;

    Snapshot->LastReadDataLength = g_TridentGlobalStats.LastReadDataLength;
    Snapshot->LastRawByte0 = g_TridentGlobalStats.LastRawByte0;
    Snapshot->LastRawByte1 = g_TridentGlobalStats.LastRawByte1;
    Snapshot->LastRawByte2 = g_TridentGlobalStats.LastRawByte2;
    Snapshot->LastRawByte3 = g_TridentGlobalStats.LastRawByte3;
    Snapshot->LastRawByte4 = g_TridentGlobalStats.LastRawByte4;
    Snapshot->LastRawByte5 = g_TridentGlobalStats.LastRawByte5;
    Snapshot->LastRawByte6 = g_TridentGlobalStats.LastRawByte6;
    Snapshot->LastRawByte7 = g_TridentGlobalStats.LastRawByte7;
    Snapshot->LastRawByte8 = g_TridentGlobalStats.LastRawByte8;
    Snapshot->LastRawByte9 = g_TridentGlobalStats.LastRawByte9;
    Snapshot->LastRawByte10 = g_TridentGlobalStats.LastRawByte10;
    Snapshot->LastRawByte11 = g_TridentGlobalStats.LastRawByte11;
    Snapshot->LastRawByte12 = g_TridentGlobalStats.LastRawByte12;
    Snapshot->LastRawByte13 = g_TridentGlobalStats.LastRawByte13;
    Snapshot->LastRawByte14 = g_TridentGlobalStats.LastRawByte14;
    Snapshot->LastRawByte15 = g_TridentGlobalStats.LastRawByte15;

    Snapshot->TipCandidateByte0Bit0 = g_TridentGlobalStats.TipCandidateByte0Bit0;
    Snapshot->TipCandidateByte1Bit0 = g_TridentGlobalStats.TipCandidateByte1Bit0;
    Snapshot->TipCandidateByte2Bit0 = g_TridentGlobalStats.TipCandidateByte2Bit0;
    Snapshot->TipCandidateByte6Bit0 = g_TridentGlobalStats.TipCandidateByte6Bit0;
    Snapshot->TipCandidateByte7Bit0 = g_TridentGlobalStats.TipCandidateByte7Bit0;
    Snapshot->LastDecodedTipSwitch = g_TridentGlobalStats.LastDecodedTipSwitch;

    Snapshot->LastDeviceContextPointer = reinterpret_cast<ULONG_PTR>(g_TridentGlobalStats.LastDeviceContextPointer);
    Snapshot->LastClientPointer = reinterpret_cast<ULONG_PTR>(g_TridentGlobalStats.LastClientPointer);
    Snapshot->LastPhysicalDeviceObject = reinterpret_cast<ULONG_PTR>(g_TridentGlobalStats.LastPhysicalDeviceObject);
    Snapshot->LastWdmDeviceObject = reinterpret_cast<ULONG_PTR>(g_TridentGlobalStats.LastWdmDeviceObject);
}