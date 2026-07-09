#include "VirtualTouch.h"
#include "HidDescriptor.h"
#include "ReTouchStats.h"

#include <ntstrsafe.h>

VHFHANDLE VirtualTouch::m_VhfHandle = nullptr;

static volatile LONG g_NextContactId = 0;
static volatile LONG g_CurrentContactId = 0;
static volatile LONG g_PreviousTouchDown = 0;
static volatile LONG g_LastTouchX = 0;
static volatile LONG g_LastTouchY = 0;

static WDFDEVICE g_ReTouchDevice = nullptr;

typedef struct _RETOUCH_LOG_WORKITEM_CONTEXT
{
    CHAR Line[256];

} RETOUCH_LOG_WORKITEM_CONTEXT, * PRETOUCH_LOG_WORKITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    RETOUCH_LOG_WORKITEM_CONTEXT,
    ReTouchGetLogWorkItemContext
);

EVT_VHF_ASYNC_OPERATION EvtVhfGetFeature;
EVT_VHF_ASYNC_OPERATION EvtVhfSetFeature;
EVT_WDF_WORKITEM ReTouchLogWorkItem;

static NTSTATUS
ReTouchLogAppendLinePassive(
    _In_z_ PCSTR Line
)
{
    NTSTATUS status;
    HANDLE fileHandle = NULL;
    IO_STATUS_BLOCK ioStatusBlock = {};
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING fileName;
    LARGE_INTEGER byteOffset;

    if (Line == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    RtlInitUnicodeString(
        &fileName,
        L"\\??\\C:\\ReTouchTridentLog.txt"
    );

    InitializeObjectAttributes(
        &objectAttributes,
        &fileName,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    status = ZwCreateFile(
        &fileHandle,
        FILE_APPEND_DATA | SYNCHRONIZE,
        &objectAttributes,
        &ioStatusBlock,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN_IF,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE,
        NULL,
        0
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    byteOffset.LowPart = FILE_WRITE_TO_END_OF_FILE;
    byteOffset.HighPart = -1;

    status = ZwWriteFile(
        fileHandle,
        NULL,
        NULL,
        NULL,
        &ioStatusBlock,
        (PVOID)Line,
        (ULONG)strlen(Line),
        &byteOffset,
        NULL
    );

    ZwClose(fileHandle);

    return status;
}

VOID
ReTouchLogWorkItem(
    _In_ WDFWORKITEM WorkItem
)
{
    PRETOUCH_LOG_WORKITEM_CONTEXT logContext;

    logContext = ReTouchGetLogWorkItemContext(WorkItem);

    if (logContext != nullptr)
    {
        (VOID)ReTouchLogAppendLinePassive(logContext->Line);
    }

    WdfObjectDelete(WorkItem);
}

static VOID
ReTouchLogTouchSubmit(
    _In_ BOOLEAN IsTouchDown,
    _In_ USHORT X,
    _In_ USHORT Y,
    _In_ UCHAR ContactId
)
{
    NTSTATUS status;
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_WORKITEM_CONFIG workItemConfig;
    WDFWORKITEM workItem = NULL;
    PRETOUCH_LOG_WORKITEM_CONTEXT logContext;

    if (g_ReTouchDevice == nullptr)
    {
        return;
    }

    counter = KeQueryPerformanceCounter(&frequency);

    WDF_WORKITEM_CONFIG_INIT(
        &workItemConfig,
        ReTouchLogWorkItem
    );

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &attributes,
        RETOUCH_LOG_WORKITEM_CONTEXT
    );

    attributes.ParentObject = g_ReTouchDevice;

    status = WdfWorkItemCreate(
        &workItemConfig,
        &attributes,
        &workItem
    );

    if (!NT_SUCCESS(status))
    {
        return;
    }

    logContext = ReTouchGetLogWorkItemContext(workItem);

    if (logContext == nullptr)
    {
        WdfObjectDelete(workItem);
        return;
    }

    status = RtlStringCbPrintfA(
        logContext->Line,
        sizeof(logContext->Line),
        "QPC=%lld Freq=%lld Event=%s X=%hu Y=%hu ContactId=%hhu\r\n",
        counter.QuadPart,
        frequency.QuadPart,
        IsTouchDown ? "TouchDownSubmitFrame" : "TouchUpSubmitFrame",
        X,
        Y,
        ContactId
    );

    if (!NT_SUCCESS(status))
    {
        WdfObjectDelete(workItem);
        return;
    }

    WdfWorkItemEnqueue(workItem);
}

VOID
EvtVhfGetFeature(
    _In_ PVOID VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ PVOID VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
)
{
    UNREFERENCED_PARAMETER(VhfClientContext);
    UNREFERENCED_PARAMETER(VhfOperationContext);

    NTSTATUS status = STATUS_NOT_SUPPORTED;

    if (HidTransferPacket != nullptr)
    {
        ReTouchStatsRecordGetFeature(
            HidTransferPacket->reportId
        );
    }

    if (HidTransferPacket != nullptr &&
        HidTransferPacket->reportId == RETOUCH_REPORT_ID_MAX_COUNT &&
        HidTransferPacket->reportBuffer != nullptr &&
        HidTransferPacket->reportBufferLen >= sizeof(RETOUCH_MAX_COUNT_FEATURE_REPORT))
    {
        PRETOUCH_MAX_COUNT_FEATURE_REPORT report =
            reinterpret_cast<PRETOUCH_MAX_COUNT_FEATURE_REPORT>(
                HidTransferPacket->reportBuffer
                );

        report->ReportId = RETOUCH_REPORT_ID_MAX_COUNT;
        report->MaximumContactCount = 1;

        status = STATUS_SUCCESS;
    }
    else if (HidTransferPacket != nullptr &&
        HidTransferPacket->reportId == RETOUCH_REPORT_ID_DEVICE_MODE &&
        HidTransferPacket->reportBuffer != nullptr &&
        HidTransferPacket->reportBufferLen >= sizeof(RETOUCH_DEVICE_MODE_FEATURE_REPORT))
    {
        PRETOUCH_DEVICE_MODE_FEATURE_REPORT report =
            reinterpret_cast<PRETOUCH_DEVICE_MODE_FEATURE_REPORT>(
                HidTransferPacket->reportBuffer
                );

        report->ReportId = RETOUCH_REPORT_ID_DEVICE_MODE;
        report->DeviceMode = RETOUCH_DEVICE_MODE_TOUCH;
        report->DeviceIdentifier = RETOUCH_DEVICE_IDENTIFIER;

        status = STATUS_SUCCESS;
    }
    else if (HidTransferPacket != nullptr &&
        HidTransferPacket->reportId == RETOUCH_REPORT_ID_CERT_BLOB &&
        HidTransferPacket->reportBuffer != nullptr &&
        HidTransferPacket->reportBufferLen >= sizeof(RETOUCH_CERTIFICATION_BLOB_FEATURE_REPORT))
    {
        PRETOUCH_CERTIFICATION_BLOB_FEATURE_REPORT report =
            reinterpret_cast<PRETOUCH_CERTIFICATION_BLOB_FEATURE_REPORT>(
                HidTransferPacket->reportBuffer
                );

        RtlZeroMemory(
            report,
            sizeof(RETOUCH_CERTIFICATION_BLOB_FEATURE_REPORT)
        );

        report->ReportId = RETOUCH_REPORT_ID_CERT_BLOB;

        status = STATUS_SUCCESS;
    }

    VhfAsyncOperationComplete(
        VhfOperationHandle,
        status
    );
}

VOID
EvtVhfSetFeature(
    _In_ PVOID VhfClientContext,
    _In_ VHFOPERATIONHANDLE VhfOperationHandle,
    _In_opt_ PVOID VhfOperationContext,
    _In_ PHID_XFER_PACKET HidTransferPacket
)
{
    UNREFERENCED_PARAMETER(VhfClientContext);
    UNREFERENCED_PARAMETER(VhfOperationContext);

    NTSTATUS status = STATUS_NOT_SUPPORTED;

    if (HidTransferPacket != nullptr)
    {
        ReTouchStatsRecordSetFeature(
            HidTransferPacket->reportId
        );
    }

    if (HidTransferPacket != nullptr &&
        HidTransferPacket->reportId == RETOUCH_REPORT_ID_DEVICE_MODE &&
        HidTransferPacket->reportBuffer != nullptr &&
        HidTransferPacket->reportBufferLen >= sizeof(RETOUCH_DEVICE_MODE_FEATURE_REPORT))
    {
        PRETOUCH_DEVICE_MODE_FEATURE_REPORT report =
            reinterpret_cast<PRETOUCH_DEVICE_MODE_FEATURE_REPORT>(
                HidTransferPacket->reportBuffer
                );

        report->ReportId = RETOUCH_REPORT_ID_DEVICE_MODE;
        report->DeviceMode = RETOUCH_DEVICE_MODE_TOUCH;
        report->DeviceIdentifier = RETOUCH_DEVICE_IDENTIFIER;

        status = STATUS_SUCCESS;
    }

    VhfAsyncOperationComplete(
        VhfOperationHandle,
        status
    );
}

NTSTATUS VirtualTouch::Initialize(WDFDEVICE Device)
{
    g_ReTouchDevice = Device;

    PDEVICE_OBJECT pdo = WdfDeviceWdmGetDeviceObject(Device);

    ReTouchStatsRecordWdmDeviceObjectNull(
        pdo == nullptr ? TRUE : FALSE
    );

    if (pdo == nullptr)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    VHF_CONFIG config;

    VHF_CONFIG_INIT(
        &config,
        pdo,
        (USHORT)g_ReportDescriptorSize,
        (PUCHAR)g_ReportDescriptor
    );

    config.VendorID = 0x1234;
    config.ProductID = 0x5678;
    config.VersionNumber = 1;
    config.EvtVhfAsyncOperationGetFeature = EvtVhfGetFeature;
    config.EvtVhfAsyncOperationSetFeature = EvtVhfSetFeature;

    NTSTATUS status = VhfCreate(
        &config,
        &m_VhfHandle
    );

    ReTouchStatsRecordVhfCreate(status);

    if (!NT_SUCCESS(status))
    {
        g_ReTouchDevice = nullptr;
        return status;
    }

    VhfStart(m_VhfHandle);

    ReTouchStatsRecordVhfStart();

    return STATUS_SUCCESS;
}

void VirtualTouch::Shutdown()
{
    if (m_VhfHandle != nullptr)
    {
        VhfDelete(m_VhfHandle, TRUE);
        m_VhfHandle = nullptr;
    }

    g_ReTouchDevice = nullptr;
}

NTSTATUS VirtualTouch::SubmitTouch(
    UCHAR contactId,
    USHORT x,
    USHORT y,
    BOOLEAN touching
)
{
    RETOUCH_FRAME frame = {};
    frame.ContactCount = touching ? 1 : 0;

    if (contactId >= RETOUCH_MAX_CONTACTS)
        return STATUS_INVALID_PARAMETER;

    frame.Contacts[contactId].Id = contactId;
    frame.Contacts[contactId].IsDown = touching ? 1 : 0;
    frame.Contacts[contactId].X = x;
    frame.Contacts[contactId].Y = y;

    return SubmitFrame(&frame);
}

static USHORT
ReTouchScaleCoordinateTo4095(
    _In_ USHORT Value,
    _In_ USHORT Minimum,
    _In_ USHORT Maximum
)
{
    if (Maximum <= Minimum)
    {
        return 0;
    }

    if (Value <= Minimum)
    {
        return 0;
    }

    if (Value >= Maximum)
    {
        return 4095;
    }

    ULONG scaled =
        ((static_cast<ULONG>(Value - Minimum)) * 4095UL) /
        static_cast<ULONG>(Maximum - Minimum);

    if (scaled > 4095UL)
    {
        scaled = 4095UL;
    }

    return static_cast<USHORT>(scaled);
}

NTSTATUS VirtualTouch::SubmitFrame(
    PRETOUCH_FRAME frame
)
{
    if (m_VhfHandle == nullptr)
    {
        ReTouchStatsRecordSubmitFrame(STATUS_DEVICE_NOT_READY, 0);
        return STATUS_DEVICE_NOT_READY;
    }

    if (frame == nullptr)
    {
        ReTouchStatsRecordSubmitFrame(STATUS_INVALID_PARAMETER, 0);
        return STATUS_INVALID_PARAMETER;
    }

    RETOUCH_TOUCH_REPORT report = {};
    report.ReportId = RETOUCH_REPORT_ID_TOUCH;
    report.ScanTime = 0;
    report.Width = 1;
    report.Height = 1;
    report.Azimuth = 0;

    BOOLEAN isTouching = frame->ContactCount > 0 ? TRUE : FALSE;

    LONG previousTouchDown =
        InterlockedCompareExchange(&g_PreviousTouchDown, 0, 0);

    if (isTouching)
    {
        if (previousTouchDown == 0)
        {
            LONG nextId = InterlockedIncrement(&g_NextContactId);
            InterlockedExchange(&g_CurrentContactId, nextId & 0x7F);
        }

        InterlockedExchange(&g_PreviousTouchDown, 1);

        LONG currentId =
            InterlockedCompareExchange(&g_CurrentContactId, 0, 0);

        USHORT scaledX =
            ReTouchScaleCoordinateTo4095(frame->Contacts[0].X, 196, 16241);

        USHORT scaledY =
            ReTouchScaleCoordinateTo4095(frame->Contacts[0].Y, 227, 9364);

        InterlockedExchange(&g_LastTouchX, scaledX);
        InterlockedExchange(&g_LastTouchY, scaledY);

        report.Flags = 0x01;
        report.ContactId = static_cast<UCHAR>(currentId & 0x7F);
        report.X = scaledX;
        report.Y = scaledY;
        report.ContactCount = 1;
    }
    else
    {
        InterlockedExchange(&g_PreviousTouchDown, 0);

        LONG currentId =
            InterlockedCompareExchange(&g_CurrentContactId, 0, 0);

        LONG lastX =
            InterlockedCompareExchange(&g_LastTouchX, 0, 0);

        LONG lastY =
            InterlockedCompareExchange(&g_LastTouchY, 0, 0);

        report.Flags = 0;
        report.ContactId = static_cast<UCHAR>(currentId & 0x7F);
        report.X = static_cast<USHORT>(lastX);
        report.Y = static_cast<USHORT>(lastY);
        report.ContactCount = 0;
    }

    ReTouchStatsRecordTouchReport(
        report.ContactCount,
        report.Flags,
        report.ContactId,
        report.X,
        report.Y,
        report.ContactCount
    );

    HID_XFER_PACKET packet = {};
    packet.reportBuffer = reinterpret_cast<PUCHAR>(&report);
    packet.reportBufferLen = sizeof(report);
    packet.reportId = RETOUCH_REPORT_ID_TOUCH;

    ReTouchLogTouchSubmit(
        report.ContactCount > 0 ? TRUE : FALSE,
        report.X,
        report.Y,
        report.ContactId
    );

    NTSTATUS status = VhfReadReportSubmit(m_VhfHandle, &packet);

    ReTouchStatsRecordSubmitFrame(status, report.ContactCount);

    return status;
}