#include "VirtualTouch.h"
#include "HidDescriptor.h"

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

    KdPrint(("ReTouch Trident: GetFeature called\n"));

    NTSTATUS status = STATUS_NOT_SUPPORTED;

    if (HidTransferPacket != nullptr)
    {
        KdPrint(("ReTouch Trident: GetFeature reportId=%u len=%lu\n",
            HidTransferPacket->reportId,
            HidTransferPacket->reportBufferLen));
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

        KdPrint(("ReTouch Trident: GetFeature MaxContactCount=%u\n",
            report->MaximumContactCount));
    }

    VhfAsyncOperationComplete(
        VhfOperationHandle,
        status
    );
}

NTSTATUS VirtualTouch::Initialize(WDFDEVICE Device)
{
    KdPrint(("ReTouch Trident: VirtualTouch::Initialize start\n"));

    VHF_CONFIG config;

    VHF_CONFIG_INIT(
        &config,
        WdfDeviceWdmGetDeviceObject(Device),
        (USHORT)g_ReportDescriptorSize,
        (PUCHAR)g_ReportDescriptor
    );

    config.VendorID = 0x1234;
    config.ProductID = 0x5678;
    config.VersionNumber = 1;
    config.EvtVhfAsyncOperationGetFeature = EvtVhfGetFeature;

    KdPrint(("ReTouch Trident: ReportDescriptorSize=%lu\n", g_ReportDescriptorSize));

    NTSTATUS status = VhfCreate(
        &config,
        &m_VhfHandle
    );

    KdPrint(("ReTouch Trident: VhfCreate returned 0x%08X\n", status));

    if (!NT_SUCCESS(status))
        return status;

    VhfStart(m_VhfHandle);

    KdPrint(("ReTouch Trident: VHF started\n"));

    return STATUS_SUCCESS;
}

void VirtualTouch::Shutdown()
{
    KdPrint(("ReTouch Trident: VirtualTouch::Shutdown\n"));

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
    KdPrint(("ReTouch Trident: SubmitFrame start\n"));

    if (m_VhfHandle == nullptr)
    {
        KdPrint(("ReTouch Trident: SubmitFrame failed, VHF not ready\n"));
        return STATUS_DEVICE_NOT_READY;
    }

    if (frame == nullptr)
    {
        KdPrint(("ReTouch Trident: SubmitFrame failed, frame null\n"));
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

            KdPrint(("ReTouch Trident: contact %u down x=%u y=%u\n",
                frame->Contacts[i].Id,
                frame->Contacts[i].X,
                frame->Contacts[i].Y));
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

    KdPrint(("ReTouch Trident: VhfReadReportSubmit returned 0x%08X active=%u\n",
        status,
        activeCount));

    return status;
}