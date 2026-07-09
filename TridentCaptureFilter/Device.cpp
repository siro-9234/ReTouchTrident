#include "Device.h"
#include "Queue.h"
#include "DeviceContext.h"
#include "ReTouchClient.h"

static volatile LONG g_NextDeviceInstanceId = 0;

extern "C"
NTSTATUS
TridentEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    UNREFERENCED_PARAMETER(Driver);

    WdfFdoInitSetFilter(DeviceInit);

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &attributes,
        DEVICE_CONTEXT
    );

    WDFDEVICE device;

    NTSTATUS status = WdfDeviceCreate(
        &DeviceInit,
        &attributes,
        &device
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    PDEVICE_CONTEXT context = DeviceGetContext(device);

    RtlZeroMemory(
        context,
        sizeof(DEVICE_CONTEXT)
    );

    context->DeviceInstanceId =
        InterlockedIncrement(&g_NextDeviceInstanceId);

    PDEVICE_OBJECT physicalDeviceObject =
        WdfDeviceWdmGetPhysicalDevice(device);

    PDEVICE_OBJECT wdmDeviceObject =
        WdfDeviceWdmGetDeviceObject(device);

    context->PhysicalDeviceObjectLow =
        static_cast<LONG>(PtrToUlong(physicalDeviceObject));

    context->WdmDeviceObjectLow =
        static_cast<LONG>(PtrToUlong(wdmDeviceObject));

    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_TRIDENT_CAPTURE,
        nullptr
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = ReTouchClient::Initialize(
        &context->ReTouchClient
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = ReTouchClient::CreateWorkItem(
        &context->ReTouchClient,
        device
    );

    if (!NT_SUCCESS(status))
    {
        ReTouchClient::Shutdown(
            &context->ReTouchClient
        );

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

    return status;
}