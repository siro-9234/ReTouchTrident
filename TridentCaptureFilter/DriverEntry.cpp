#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>

static volatile LONG g_InternalIoctlCount = 0;
static volatile LONG g_ReadReportReceived = 0;
static volatile LONG g_ReadReportCompleted = 0;
static volatile LONG g_ReadReportSendFailed = 0;

extern "C"
{
    DRIVER_INITIALIZE DriverEntry;

    EVT_WDF_DRIVER_DEVICE_ADD TridentEvtDeviceAdd;
    EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL TridentEvtIoInternalDeviceControl;
    EVT_WDF_REQUEST_COMPLETION_ROUTINE TridentReadReportCompletion;
}

VOID
TridentReadReportCompletion(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    InterlockedIncrement(&g_ReadReportCompleted);

    WdfRequestComplete(
        Request,
        Params->IoStatus.Status
    );
}

VOID
TridentEvtIoInternalDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    InterlockedIncrement(&g_InternalIoctlCount);

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    WDFIOTARGET target = WdfDeviceGetIoTarget(device);

    WdfRequestFormatRequestUsingCurrentType(Request);

    if (IoControlCode == IOCTL_HID_READ_REPORT)
    {
        InterlockedIncrement(&g_ReadReportReceived);

        WdfRequestSetCompletionRoutine(
            Request,
            TridentReadReportCompletion,
            nullptr
        );

        if (!WdfRequestSend(Request, target, WDF_NO_SEND_OPTIONS))
        {
            NTSTATUS status = WdfRequestGetStatus(Request);

            InterlockedIncrement(&g_ReadReportSendFailed);

            WdfRequestComplete(Request, status);
        }

        return;
    }

    if (!WdfRequestSend(Request, target, WDF_NO_SEND_OPTIONS))
    {
        NTSTATUS status = WdfRequestGetStatus(Request);

        WdfRequestComplete(Request, status);
    }
}

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

    WDF_IO_QUEUE_CONFIG queueConfig;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchParallel
    );

    queueConfig.EvtIoInternalDeviceControl =
        TridentEvtIoInternalDeviceControl;

    WDFQUEUE queue;

    status = WdfIoQueueCreate(
        device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    );

    return status;
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;

    WDF_DRIVER_CONFIG_INIT(
        &config,
        TridentEvtDeviceAdd
    );

    return WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );
}