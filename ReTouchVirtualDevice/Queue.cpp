#include "Queue.h"
#include "Ioctl.h"
#include "VirtualTouch.h"
#include "ReTouchStats.h"

VOID EvtIoDeviceControl(
    WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode
)
{
    UNREFERENCED_PARAMETER(Queue);

    NTSTATUS status = STATUS_SUCCESS;

    switch (IoControlCode)
    {
    case IOCTL_RETOUCH_GET_STATS:
    {
        if (OutputBufferLength < sizeof(RETOUCH_STATS))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        PRETOUCH_STATS stats = nullptr;

        status = WdfRequestRetrieveOutputBuffer(
            Request,
            sizeof(RETOUCH_STATS),
            reinterpret_cast<PVOID*>(&stats),
            nullptr
        );

        if (!NT_SUCCESS(status))
        {
            break;
        }

        ReTouchStatsSnapshot(stats);

        WdfRequestCompleteWithInformation(
            Request,
            STATUS_SUCCESS,
            sizeof(RETOUCH_STATS)
        );
        return;
    }

    case IOCTL_RETOUCH_SUBMIT_FRAME:
    {
        if (InputBufferLength < sizeof(RETOUCH_FRAME))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        PRETOUCH_FRAME frame = nullptr;

        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(RETOUCH_FRAME),
            reinterpret_cast<PVOID*>(&frame),
            nullptr
        );

        if (!NT_SUCCESS(status))
        {
            break;
        }

        ReTouchStatsRecordReceivedFrame(frame);

        if (frame->ContactCount > RETOUCH_MAX_CONTACTS)
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        status = VirtualTouch::SubmitFrame(frame);
        break;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestComplete(Request, status);
}

NTSTATUS QueueInitialize(
    WDFDEVICE Device
)
{
    WDF_IO_QUEUE_CONFIG config;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &config,
        WdfIoQueueDispatchSequential
    );

    config.EvtIoDeviceControl = EvtIoDeviceControl;

    NTSTATUS status = WdfIoQueueCreate(
        Device,
        &config,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE
    );

    if (NT_SUCCESS(status))
    {
        ReTouchStatsRecordQueueInitialize();
    }

    return status;
}