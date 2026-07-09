#include <ntddk.h>
#include <wdf.h>
#include <wdmsec.h>

#include "ControlDevice.h"
#include "GlobalContext.h"

extern "C"
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL TridentControlEvtIoDeviceControl;

extern "C"
NTSTATUS
TridentCreateControlDevice(
    _In_ WDFDRIVER Driver
)
{
    NTSTATUS status;
    PWDFDEVICE_INIT deviceInit;
    WDFDEVICE controlDevice;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE queue;
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicLinkName;
    PTRIDENT_GLOBAL_STATS globalStats;

    globalStats = TridentGetGlobalStats();
    InterlockedIncrement(&globalStats->ControlDeviceCreateAttempt);

    RtlInitUnicodeString(&deviceName, L"\\Device\\TridentCaptureControl");
    RtlInitUnicodeString(&symbolicLinkName, L"\\DosDevices\\TridentCaptureControl");

    deviceInit = WdfControlDeviceInitAllocate(Driver, &SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
    if (deviceInit == nullptr)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = WdfDeviceInitAssignName(deviceInit, &deviceName);
    if (!NT_SUCCESS(status))
    {
        WdfDeviceInitFree(deviceInit);
        return status;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&deviceAttributes);

    status = WdfDeviceCreate(&deviceInit, &deviceAttributes, &controlDevice);
    if (!NT_SUCCESS(status))
    {
        if (deviceInit != nullptr)
        {
            WdfDeviceInitFree(deviceInit);
        }

        return status;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = TridentControlEvtIoDeviceControl;

    status = WdfIoQueueCreate(
        controlDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    );

    if (!NT_SUCCESS(status))
    {
        WdfObjectDelete(controlDevice);
        return status;
    }

    status = WdfDeviceCreateSymbolicLink(controlDevice, &symbolicLinkName);
    if (!NT_SUCCESS(status))
    {
        WdfObjectDelete(controlDevice);
        return status;
    }

    WdfControlFinishInitializing(controlDevice);

    InterlockedIncrement(&globalStats->ControlDeviceCreateSucceeded);

    return STATUS_SUCCESS;
}

extern "C"
VOID
TridentControlEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status;
    size_t information;
    PTRIDENT_GLOBAL_STATS globalStats;
    PTRIDENT_GLOBAL_STATS_SNAPSHOT outputBuffer;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    status = STATUS_SUCCESS;
    information = 0;
    globalStats = TridentGetGlobalStats();
    outputBuffer = nullptr;

    InterlockedIncrement(&globalStats->ControlDeviceIoctlStatsCount);

    if (IoControlCode == IOCTL_TRIDENT_GET_GLOBAL_STATS)
    {
        status = WdfRequestRetrieveOutputBuffer(
            Request,
            sizeof(TRIDENT_GLOBAL_STATS_SNAPSHOT),
            reinterpret_cast<PVOID*>(&outputBuffer),
            nullptr
        );

        if (NT_SUCCESS(status))
        {
            TridentGlobalSnapshot(outputBuffer);
            information = sizeof(TRIDENT_GLOBAL_STATS_SNAPSHOT);
        }

        WdfRequestCompleteWithInformation(Request, status, information);
        return;
    }

    status = STATUS_INVALID_DEVICE_REQUEST;
    WdfRequestComplete(Request, status);
}