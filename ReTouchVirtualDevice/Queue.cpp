#include "Queue.h"
#include "Ioctl.h"
#include "VirtualTouch.h"

VOID EvtIoDeviceControl(
    WDFQUEUE Queue,
    WDFREQUEST Request,
    size_t OutputBufferLength,
    size_t InputBufferLength,
    ULONG IoControlCode
)
{
    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    NTSTATUS status = STATUS_SUCCESS;

    KdPrint(("ReTouch Trident: IOCTL received 0x%08X, input=%llu\n",
        IoControlCode,
        (unsigned long long)InputBufferLength));

    switch (IoControlCode)
    {
    case IOCTL_RETOUCH_SUBMIT_FRAME:
    {
        KdPrint(("ReTouch Trident: IOCTL_RETOUCH_SUBMIT_FRAME\n"));

        if (InputBufferLength < sizeof(RETOUCH_FRAME))
        {
            KdPrint(("ReTouch Trident: input buffer too small\n"));
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        PRETOUCH_FRAME frame = nullptr;

        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(RETOUCH_FRAME),
            (PVOID*)&frame,
            nullptr);

        if (!NT_SUCCESS(status))
        {
            KdPrint(("ReTouch Trident: WdfRequestRetrieveInputBuffer failed 0x%08X\n", status));
            break;
        }

        if (frame->ContactCount > RETOUCH_MAX_CONTACTS)
        {
            KdPrint(("ReTouch Trident: invalid ContactCount %u\n", frame->ContactCount));
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        KdPrint(("ReTouch Trident: SubmitFrame ContactCount=%u\n", frame->ContactCount));

        status = VirtualTouch::SubmitFrame(frame);

        KdPrint(("ReTouch Trident: SubmitFrame returned 0x%08X\n", status));
        break;
    }

    default:
        KdPrint(("ReTouch Trident: unknown IOCTL 0x%08X\n", IoControlCode));
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestComplete(Request, status);
}

NTSTATUS QueueInitialize(
    WDFDEVICE Device
)
{
    KdPrint(("ReTouch Trident: QueueInitialize start\n"));

    WDF_IO_QUEUE_CONFIG config;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &config,
        WdfIoQueueDispatchSequential);

    config.EvtIoDeviceControl = EvtIoDeviceControl;

    NTSTATUS status = WdfIoQueueCreate(
        Device,
        &config,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE);

    KdPrint(("ReTouch Trident: WdfIoQueueCreate returned 0x%08X\n", status));

    return status;
}