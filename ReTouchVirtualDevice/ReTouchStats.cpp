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

static volatile LONG g_LastActiveContactCount = 0;
static volatile LONG g_LastFirstContactFlags = 0;
static volatile LONG g_LastFirstContactId = 0;
static volatile LONG g_LastFirstContactX = 0;
static volatile LONG g_LastFirstContactY = 0;
static volatile LONG g_LastReportContactCount = 0;

static volatile LONG g_ReceivedSubmitFrameIoctlCount = 0;
static volatile LONG g_ReceivedContactCount = 0;
static volatile LONG g_ReceivedFirstContactId = 0;
static volatile LONG g_ReceivedFirstContactIsDown = 0;
static volatile LONG g_ReceivedFirstContactX = 0;
static volatile LONG g_ReceivedFirstContactY = 0;

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

VOID ReTouchStatsRecordTouchReport(
    _In_ UCHAR ActiveContactCount,
    _In_ UCHAR FirstContactFlags,
    _In_ UCHAR FirstContactId,
    _In_ USHORT FirstContactX,
    _In_ USHORT FirstContactY,
    _In_ UCHAR ReportContactCount
)
{
    InterlockedExchange(&g_LastActiveContactCount, ActiveContactCount);
    InterlockedExchange(&g_LastFirstContactFlags, FirstContactFlags);
    InterlockedExchange(&g_LastFirstContactId, FirstContactId);
    InterlockedExchange(&g_LastFirstContactX, FirstContactX);
    InterlockedExchange(&g_LastFirstContactY, FirstContactY);
    InterlockedExchange(&g_LastReportContactCount, ReportContactCount);
}

VOID ReTouchStatsRecordReceivedFrame(
    _In_opt_ PRETOUCH_FRAME Frame
)
{
    InterlockedIncrement(&g_ReceivedSubmitFrameIoctlCount);

    if (Frame == nullptr)
    {
        InterlockedExchange(&g_ReceivedContactCount, 0);
        InterlockedExchange(&g_ReceivedFirstContactId, 0);
        InterlockedExchange(&g_ReceivedFirstContactIsDown, 0);
        InterlockedExchange(&g_ReceivedFirstContactX, 0);
        InterlockedExchange(&g_ReceivedFirstContactY, 0);
        return;
    }

    InterlockedExchange(&g_ReceivedContactCount, Frame->ContactCount);
    InterlockedExchange(&g_ReceivedFirstContactId, Frame->Contacts[0].Id);
    InterlockedExchange(&g_ReceivedFirstContactIsDown, Frame->Contacts[0].IsDown);
    InterlockedExchange(&g_ReceivedFirstContactX, Frame->Contacts[0].X);
    InterlockedExchange(&g_ReceivedFirstContactY, Frame->Contacts[0].Y);
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

    Stats->LastActiveContactCount =
        InterlockedCompareExchange(&g_LastActiveContactCount, 0, 0);

    Stats->LastFirstContactFlags =
        InterlockedCompareExchange(&g_LastFirstContactFlags, 0, 0);

    Stats->LastFirstContactId =
        InterlockedCompareExchange(&g_LastFirstContactId, 0, 0);

    Stats->LastFirstContactX =
        InterlockedCompareExchange(&g_LastFirstContactX, 0, 0);

    Stats->LastFirstContactY =
        InterlockedCompareExchange(&g_LastFirstContactY, 0, 0);

    Stats->LastReportContactCount =
        InterlockedCompareExchange(&g_LastReportContactCount, 0, 0);

    Stats->ReceivedSubmitFrameIoctlCount =
        InterlockedCompareExchange(&g_ReceivedSubmitFrameIoctlCount, 0, 0);

    Stats->ReceivedContactCount =
        InterlockedCompareExchange(&g_ReceivedContactCount, 0, 0);

    Stats->ReceivedFirstContactId =
        InterlockedCompareExchange(&g_ReceivedFirstContactId, 0, 0);

    Stats->ReceivedFirstContactIsDown =
        InterlockedCompareExchange(&g_ReceivedFirstContactIsDown, 0, 0);

    Stats->ReceivedFirstContactX =
        InterlockedCompareExchange(&g_ReceivedFirstContactX, 0, 0);

    Stats->ReceivedFirstContactY =
        InterlockedCompareExchange(&g_ReceivedFirstContactY, 0, 0);
}