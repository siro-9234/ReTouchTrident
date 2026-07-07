#include "ReTouchStats.h"

static volatile LONG g_DeviceAddCount = 0;
static volatile LONG g_DeviceCleanupCount = 0;
static volatile LONG g_QueueInitializeCount = 0;

static volatile LONG g_VirtualTouchInitializeCount = 0;
static volatile LONG g_VirtualTouchInitializeStatus = 0;

static volatile LONG g_VhfCreateStatus = 0;
static volatile LONG g_VhfStartCount = 0;

static volatile LONG g_SubmitFrameCount = 0;
static volatile LONG g_LastSubmitFrameStatus = 0;
static volatile LONG g_LastContactCount = 0;

static volatile LONG g_GetFeatureCount = 0;
static volatile LONG g_LastGetFeatureReportId = 0;

static volatile LONG g_WdmDeviceObjectNull = 0;

VOID ReTouchStatsRecordDeviceAdd()
{
    InterlockedIncrement(&g_DeviceAddCount);
}

VOID ReTouchStatsRecordDeviceCleanup()
{
    InterlockedIncrement(&g_DeviceCleanupCount);
}

VOID ReTouchStatsRecordQueueInitialize()
{
    InterlockedIncrement(&g_QueueInitializeCount);
}

VOID ReTouchStatsRecordVirtualTouchInitialize(
    _In_ NTSTATUS Status
)
{
    InterlockedIncrement(&g_VirtualTouchInitializeCount);
    InterlockedExchange(&g_VirtualTouchInitializeStatus, Status);
}

VOID ReTouchStatsRecordVhfCreate(
    _In_ NTSTATUS Status
)
{
    InterlockedExchange(&g_VhfCreateStatus, Status);
}

VOID ReTouchStatsRecordVhfStart()
{
    InterlockedIncrement(&g_VhfStartCount);
}

VOID ReTouchStatsRecordSubmitFrame(
    _In_ NTSTATUS Status,
    _In_ UCHAR ContactCount
)
{
    InterlockedIncrement(&g_SubmitFrameCount);
    InterlockedExchange(&g_LastSubmitFrameStatus, Status);
    InterlockedExchange(&g_LastContactCount, ContactCount);
}

VOID ReTouchStatsRecordGetFeature(
    _In_ UCHAR ReportId
)
{
    InterlockedIncrement(&g_GetFeatureCount);
    InterlockedExchange(&g_LastGetFeatureReportId, ReportId);
}

VOID ReTouchStatsRecordWdmDeviceObjectNull(
    _In_ BOOLEAN IsNull
)
{
    InterlockedExchange(
        &g_WdmDeviceObjectNull,
        IsNull ? 1 : 0
    );
}

VOID ReTouchStatsSnapshot(
    _Out_ PRETOUCH_STATS Stats
)
{
    RtlZeroMemory(Stats, sizeof(RETOUCH_STATS));

    Stats->DeviceAddCount =
        InterlockedCompareExchange(&g_DeviceAddCount, 0, 0);

    Stats->DeviceCleanupCount =
        InterlockedCompareExchange(&g_DeviceCleanupCount, 0, 0);

    Stats->QueueInitializeCount =
        InterlockedCompareExchange(&g_QueueInitializeCount, 0, 0);

    Stats->VirtualTouchInitializeCount =
        InterlockedCompareExchange(&g_VirtualTouchInitializeCount, 0, 0);

    Stats->VirtualTouchInitializeStatus =
        InterlockedCompareExchange(&g_VirtualTouchInitializeStatus, 0, 0);

    Stats->VhfCreateStatus =
        InterlockedCompareExchange(&g_VhfCreateStatus, 0, 0);

    Stats->VhfStartCount =
        InterlockedCompareExchange(&g_VhfStartCount, 0, 0);

    Stats->SubmitFrameCount =
        InterlockedCompareExchange(&g_SubmitFrameCount, 0, 0);

    Stats->LastSubmitFrameStatus =
        InterlockedCompareExchange(&g_LastSubmitFrameStatus, 0, 0);

    Stats->LastContactCount =
        InterlockedCompareExchange(&g_LastContactCount, 0, 0);

    Stats->GetFeatureCount =
        InterlockedCompareExchange(&g_GetFeatureCount, 0, 0);

    Stats->LastGetFeatureReportId =
        InterlockedCompareExchange(&g_LastGetFeatureReportId, 0, 0);

    Stats->WdmDeviceObjectNull =
        InterlockedCompareExchange(&g_WdmDeviceObjectNull, 0, 0);
}