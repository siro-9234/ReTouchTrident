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

VOID ReTouchStatsRecordGetFeature(
    _In_ UCHAR ReportId
);

VOID ReTouchStatsRecordWdmDeviceObjectNull(
    _In_ BOOLEAN IsNull
);

VOID ReTouchStatsSnapshot(
    _Out_ PRETOUCH_STATS Stats
);