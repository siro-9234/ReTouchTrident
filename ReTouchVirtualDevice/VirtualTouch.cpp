#include "VirtualTouch.h"
#include "HidDescriptor.h"
#include "ReTouchStats.h"

VHFHANDLE VirtualTouch::m_VhfHandle = nullptr;

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
            (PRETOUCH_MAX_COUNT_FEATURE_REPORT)HidTransferPacket->reportBuffer;

        report->ReportId = RETOUCH_REPORT_ID_MAX_COUNT;
        report->MaximumContactCount = RETOUCH_MAX_CONTACTS;

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

NTSTATUS VirtualTouch::SubmitFrame(
    PRETOUCH_FRAME frame
)
{

    if (m_VhfHandle == nullptr)
    {
        return STATUS_DEVICE_NOT_READY;
    }

    if (frame == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RETOUCH_TOUCH_REPORT report = {};
    report.ReportId = RETOUCH_REPORT_ID_TOUCH;
    report.ScanTime = 0;

    UCHAR activeCount = 0;

    for (UCHAR i = 0; i < RETOUCH_MAX_CONTACTS; i++)
    {
        report.Contacts[i].ContactId = frame->Contacts[i].Id;
        report.Contacts[i].X = frame->Contacts[i].X;
        report.Contacts[i].Y = frame->Contacts[i].Y;

        if (frame->Contacts[i].IsDown != 0)
        {
            report.Contacts[i].Flags =
                RETOUCH_FLAG_TIP_SWITCH |
                RETOUCH_FLAG_IN_RANGE |
                RETOUCH_FLAG_CONFIDENCE;

            activeCount++;
        }
        else
        {
            report.Contacts[i].Flags = 0;
        }
    }

    report.ContactCount = activeCount;

    HID_XFER_PACKET packet = {};
    packet.reportBuffer = (PUCHAR)&report;
    packet.reportBufferLen = sizeof(report);
    packet.reportId = RETOUCH_REPORT_ID_TOUCH;

    NTSTATUS status = VhfReadReportSubmit(m_VhfHandle, &packet);

    ReTouchStatsRecordSubmitFrame(
        status,
        activeCount
    );

    return status;
}