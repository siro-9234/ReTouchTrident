#include "VirtualTouch.h"
#include "HidDescriptor.h"
#include "ReTouchStats.h"

VHFHANDLE VirtualTouch::m_VhfHandle = nullptr;

static volatile LONG g_NextContactId = 0;
static volatile LONG g_CurrentContactId = 0;
static volatile LONG g_PreviousTouchDown = 0;
static volatile LONG g_LastTouchX = 0;
static volatile LONG g_LastTouchY = 0;

EVT_VHF_ASYNC_OPERATION EvtVhfGetFeature;

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

NTSTATUS VirtualTouch::Initialize(WDFDEVICE Device)
{
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

    NTSTATUS status = VhfCreate(
        &config,
        &m_VhfHandle
    );

    ReTouchStatsRecordVhfCreate(status);

    if (!NT_SUCCESS(status))
    {
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
        ReTouchStatsRecordSubmitFrame(
            STATUS_DEVICE_NOT_READY,
            0
        );

        return STATUS_DEVICE_NOT_READY;
    }

    if (frame == nullptr)
    {
        ReTouchStatsRecordSubmitFrame(
            STATUS_INVALID_PARAMETER,
            0
        );

        return STATUS_INVALID_PARAMETER;
    }

    RETOUCH_TOUCH_REPORT report = {};
    report.ReportId = RETOUCH_REPORT_ID_TOUCH;
    report.ScanTime = 0;
    report.Width = 1;
    report.Height = 1;
    report.Azimuth = 0;

    BOOLEAN isTouching =
        frame->ContactCount > 0 ? TRUE : FALSE;

    LONG previousTouchDown =
        InterlockedCompareExchange(
            &g_PreviousTouchDown,
            0,
            0
        );

    if (isTouching)
    {
        if (previousTouchDown == 0)
        {
            LONG nextId =
                InterlockedIncrement(&g_NextContactId);

            InterlockedExchange(
                &g_CurrentContactId,
                nextId & 0x7F
            );
        }

        InterlockedExchange(
            &g_PreviousTouchDown,
            1
        );

        LONG currentId =
            InterlockedCompareExchange(
                &g_CurrentContactId,
                0,
                0
            );

        USHORT scaledX =
            ReTouchScaleCoordinateTo4095(
                frame->Contacts[0].X,
                196,
                16241
            );

        USHORT scaledY =
            ReTouchScaleCoordinateTo4095(
                frame->Contacts[0].Y,
                227,
                9364
            );

        InterlockedExchange(
            &g_LastTouchX,
            scaledX
        );

        InterlockedExchange(
            &g_LastTouchY,
            scaledY
        );

        report.Flags = RETOUCH_FLAG_TIP_SWITCH;
        report.ContactId = static_cast<UCHAR>(currentId & 0x7F);
        report.X = scaledX;
        report.Y = scaledY;
        report.ContactCount = 1;
    }
    else
    {
        InterlockedExchange(
            &g_PreviousTouchDown,
            0
        );

        LONG currentId =
            InterlockedCompareExchange(
                &g_CurrentContactId,
                0,
                0
            );

        LONG lastX =
            InterlockedCompareExchange(
                &g_LastTouchX,
                0,
                0
            );

        LONG lastY =
            InterlockedCompareExchange(
                &g_LastTouchY,
                0,
                0
            );

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

    NTSTATUS status = VhfReadReportSubmit(
        m_VhfHandle,
        &packet
    );

    ReTouchStatsRecordSubmitFrame(
        status,
        report.ContactCount
    );

    return status;
}