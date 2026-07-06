#include <ntddk.h>
#include <wdf.h>

#include "VirtualTouch.h"
#include "Queue.h"
#include "DeviceInterface.h"

extern "C"
{
    DRIVER_INITIALIZE DriverEntry;
}

EVT_WDF_DRIVER_DEVICE_ADD DeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP DeviceCleanup;

VOID DeviceCleanup(WDFOBJECT DeviceObject)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    KdPrint(("ReTouch Trident: Device cleanup\n"));
    VirtualTouch::Shutdown();
}

NTSTATUS DeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
)
{
    UNREFERENCED_PARAMETER(Driver);

    KdPrint(("ReTouch Trident: DeviceAdd start\n"));

    NTSTATUS status;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = DeviceCleanup;

    WDFDEVICE device;

    status = WdfDeviceCreate(
        &DeviceInit,
        &attributes,
        &device
    );

    if (!NT_SUCCESS(status))
    {
        KdPrint(("ReTouch Trident: WdfDeviceCreate failed 0x%08X\n", status));
        return status;
    }

    KdPrint(("ReTouch Trident: WDFDEVICE created\n"));

    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_RETOUCH,
        nullptr
    );

    if (!NT_SUCCESS(status))
    {
        KdPrint(("ReTouch Trident: WdfDeviceCreateDeviceInterface failed 0x%08X\n", status));
        return status;
    }

    KdPrint(("ReTouch Trident: Device interface created\n"));

    status = QueueInitialize(device);

    if (!NT_SUCCESS(status))
    {
        KdPrint(("ReTouch Trident: QueueInitialize failed 0x%08X\n", status));
        return status;
    }

    KdPrint(("ReTouch Trident: Queue initialized\n"));

    status = VirtualTouch::Initialize(device);

    KdPrint(("ReTouch Trident: VirtualTouch::Initialize returned 0x%08X\n", status));

    return status;
}

NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
)
{
    KdPrint(("ReTouch Trident: DriverEntry\n"));

    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, DeviceAdd);

    return WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );
}