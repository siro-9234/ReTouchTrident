#pragma once

#include <ntddk.h>
#include "Ioctl.h"

VOID ReTouchStatsRecordDeviceAdd();
VOID ReTouchStatsRecordDeviceCleanup();
VOID ReTouchStatsRecordQueueInitialize();

VOID ReTouchStatsRecordVirtualTouchInitialize(
    _In_ NTSTATUS Status
);

VOID ReTouchStatsRecordVhfCreate(
    _In_ NTSTATUS Status
);

VOID ReTouchStatsRecordVhfStart();

VOID ReTouchStatsRecordSubmitFrame(
    _In_ NTSTATUS Status,
    _In_ UCHAR ContactCount
);

VOID ReTouchStatsRecordTouchReport(
    _In_ UCHAR ActiveContactCount,
    _In_ UCHAR FirstContactFlags,
    _In_ UCHAR FirstContactId,
    _In_ USHORT FirstContactX,
    _In_ USHORT FirstContactY,
    _In_ UCHAR ReportContactCount
);

VOID ReTouchStatsRecordReceivedFrame(
    _In_opt_ PRETOUCH_FRAME Frame
);

VOID ReTouchStatsRecordGetFeature(
    _In_ UCHAR ReportId
);

VOID ReTouchStatsRecordWdmDeviceObjectNull(
    _In_ BOOLEAN IsNull
);

VOID ReTouchStatsSnapshot(
    _Out_ PRETOUCH_STATS Stats
);