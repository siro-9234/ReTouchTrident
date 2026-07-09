#pragma once

#include <ntddk.h>
#include <wdf.h>

typedef struct _TRIDENT_GLOBAL_STATS
{
    LONG ControlDeviceCreateAttempt;
    LONG ControlDeviceCreateSucceeded;
    LONG ControlDeviceIoctlStatsCount;

    LONG LastDeviceInstanceId;
    LONG LastClientInstanceId;

    LONG ReadCompletionCount;
    LONG DecodeSuccessCount;
    LONG SubmitAttemptCount;
    LONG SubmitCompletedCount;
    LONG SubmitFrameCount;

    LONG WorkItemEnqueueCount;
    LONG WorkItemRunCount;
    LONG LastWorkItemContactCount;
    LONG LastSubmitFrameStatus;
    LONG LastSendFrameStatus;

    LONG OpenInterfaceAttemptCount;
    LONG OpenInterfaceSucceededCount;
    LONG LastOpenInterfaceStatus;
    LONG LastOpenedRootSystemNumber;

    LONG LastReadDataLength;
    LONG LastRawByte0;
    LONG LastRawByte1;
    LONG LastRawByte2;
    LONG LastRawByte3;
    LONG LastRawByte4;
    LONG LastRawByte5;
    LONG LastRawByte6;
    LONG LastRawByte7;
    LONG LastRawByte8;
    LONG LastRawByte9;
    LONG LastRawByte10;
    LONG LastRawByte11;
    LONG LastRawByte12;
    LONG LastRawByte13;
    LONG LastRawByte14;
    LONG LastRawByte15;

    LONG TipCandidateByte0Bit0;
    LONG TipCandidateByte1Bit0;
    LONG TipCandidateByte2Bit0;
    LONG TipCandidateByte6Bit0;
    LONG TipCandidateByte7Bit0;
    LONG LastDecodedTipSwitch;

    PVOID LastDeviceContextPointer;
    PVOID LastClientPointer;
    PVOID LastPhysicalDeviceObject;
    PVOID LastWdmDeviceObject;
} TRIDENT_GLOBAL_STATS, * PTRIDENT_GLOBAL_STATS;

typedef struct _TRIDENT_GLOBAL_STATS_SNAPSHOT
{
    LONG ControlDeviceCreateAttempt;
    LONG ControlDeviceCreateSucceeded;
    LONG ControlDeviceIoctlStatsCount;

    LONG LastDeviceInstanceId;
    LONG LastClientInstanceId;

    LONG ReadCompletionCount;
    LONG DecodeSuccessCount;
    LONG SubmitAttemptCount;
    LONG SubmitCompletedCount;
    LONG SubmitFrameCount;

    LONG WorkItemEnqueueCount;
    LONG WorkItemRunCount;
    LONG LastWorkItemContactCount;
    LONG LastSubmitFrameStatus;
    LONG LastSendFrameStatus;

    LONG OpenInterfaceAttemptCount;
    LONG OpenInterfaceSucceededCount;
    LONG LastOpenInterfaceStatus;
    LONG LastOpenedRootSystemNumber;

    LONG LastReadDataLength;
    LONG LastRawByte0;
    LONG LastRawByte1;
    LONG LastRawByte2;
    LONG LastRawByte3;
    LONG LastRawByte4;
    LONG LastRawByte5;
    LONG LastRawByte6;
    LONG LastRawByte7;
    LONG LastRawByte8;
    LONG LastRawByte9;
    LONG LastRawByte10;
    LONG LastRawByte11;
    LONG LastRawByte12;
    LONG LastRawByte13;
    LONG LastRawByte14;
    LONG LastRawByte15;

    LONG TipCandidateByte0Bit0;
    LONG TipCandidateByte1Bit0;
    LONG TipCandidateByte2Bit0;
    LONG TipCandidateByte6Bit0;
    LONG TipCandidateByte7Bit0;
    LONG LastDecodedTipSwitch;

    ULONG_PTR LastDeviceContextPointer;
    ULONG_PTR LastClientPointer;
    ULONG_PTR LastPhysicalDeviceObject;
    ULONG_PTR LastWdmDeviceObject;
} TRIDENT_GLOBAL_STATS_SNAPSHOT, * PTRIDENT_GLOBAL_STATS_SNAPSHOT;

extern "C"
VOID
TridentGlobalInitialize();

extern "C"
PTRIDENT_GLOBAL_STATS
TridentGetGlobalStats();

extern "C"
VOID
TridentGlobalRecordFilterActivity(
    _In_ LONG DeviceInstanceId,
    _In_ LONG ClientInstanceId,
    _In_opt_ PVOID DeviceContextPointer,
    _In_opt_ PVOID ClientPointer,
    _In_opt_ PVOID PhysicalDeviceObject,
    _In_opt_ PVOID WdmDeviceObject
);

extern "C"
VOID
TridentGlobalSnapshot(
    _Out_ PTRIDENT_GLOBAL_STATS_SNAPSHOT Snapshot
);