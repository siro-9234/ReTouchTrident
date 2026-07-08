#include "Device.h"
#include "Queue.h"
#include "ReTouchClient.h"

extern "C"
NTSTATUS
TridentEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    UNREFERENCED_PARAMETER(Driver);

    WdfFdoInitSetFilter(DeviceInit);

    WDFDEVICE device;

    NTSTATUS status = WdfDeviceCreate(
        &DeviceInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &device
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_TRIDENT_CAPTURE,
        nullptr
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = ReTouchClient::Initialize(device);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    WDF_IO_QUEUE_CONFIG queueConfig;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchParallel
    );

    queueConfig.EvtIoInternalDeviceControl =
        TridentEvtIoInternalDeviceControl;

    queueConfig.EvtIoDeviceControl =
        TridentEvtIoDeviceControl;

    queueConfig.EvtIoRead =
        TridentEvtIoRead;

    WDFQUEUE queue;

    status = WdfIoQueueCreate(
        device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    );

    if (!NT_SUCCESS(status))
    {
        ReTouchClient::Shutdown();
        return status;
    }

    return STATUS_SUCCESS;
}