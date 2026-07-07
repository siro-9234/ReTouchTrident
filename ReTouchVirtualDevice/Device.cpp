#include "Device.h"
#include "ReTouchStats.h"
#include "VirtualTouch.h"
#include "Queue.h"
#include "DeviceInterface.h"

VOID
ReTouchEvtDeviceCleanup(
    _In_ WDFOBJECT DeviceObject
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    ReTouchStatsRecordDeviceCleanup();

    VirtualTouch::Shutdown();
}

NTSTATUS
ReTouchEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    UNREFERENCED_PARAMETER(Driver);

    ReTouchStatsRecordDeviceAdd();

    WdfDeviceInitSetDeviceType(
        DeviceInit,
        FILE_DEVICE_UNKNOWN
    );

    WdfDeviceInitSetExclusive(
        DeviceInit,
        FALSE
    );

    NTSTATUS status;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = ReTouchEvtDeviceCleanup;

    WDFDEVICE device;

    status = WdfDeviceCreate(
        &DeviceInit,
        &attributes,
        &device
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_RETOUCH,
        nullptr
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = QueueInitialize(device);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = VirtualTouch::Initialize(device);

    ReTouchStatsRecordVirtualTouchInitialize(status);

    return STATUS_SUCCESS;
}