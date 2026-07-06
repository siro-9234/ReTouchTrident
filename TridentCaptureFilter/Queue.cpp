#include "Queue.h"

static volatile LONG g_InternalIoctlCount = 0;
static volatile LONG g_ReadReportReceived = 0;
static volatile LONG g_ReadReportCompleted = 0;
static volatile LONG g_ReadReportSendFailed = 0;
static volatile LONG g_ReadReportBufferRetrieved = 0;
static volatile LONG g_ReadReportBufferRetrieveFailed = 0;
static volatile LONG g_DeviceIoctlCount = 0;
static volatile LONG g_GetStatsIoctlCount = 0;
static volatile LONG g_LastInternalIoctlCode = 0;
static volatile LONG g_LastDeviceIoctlCode = 0;
static volatile LONG g_ReadRequestCount = 0;
static volatile LONG g_NonStatsDeviceIoctlCount = 0;
static volatile LONG g_LastNonStatsDeviceIoctlCode = 0;
static volatile LONG g_NonStatsDeviceIoctlCodes[8] = {};
static volatile LONG g_NonStatsDeviceIoctlWriteIndex = 0;
static volatile LONG g_HidGetDeviceAttributesCount = 0;
static volatile LONG g_HidGetReportDescriptorCount = 0;

static volatile LONG g_HidGetDeviceAttributesCompleted = 0;
static volatile LONG g_HidGetDeviceAttributesFailed = 0;
static volatile LONG g_LastHidGetDeviceAttributesStatus = 0;

static volatile LONG g_HidGetReportDescriptorCompleted = 0;
static volatile LONG g_HidGetReportDescriptorFailed = 0;
static volatile LONG g_LastHidGetReportDescriptorStatus = 0;

static
VOID
TridentRecordNonStatsDeviceIoctl(
    _In_ ULONG IoControlCode
)
{
    LONG index = InterlockedIncrement(&g_NonStatsDeviceIoctlWriteIndex) - 1;
    LONG slot = index % 8;

    InterlockedExchange(
        &g_NonStatsDeviceIoctlCodes[slot],
        static_cast<LONG>(IoControlCode)
    );
}

extern "C"
VOID
TridentHidInfoCompletion(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    UNREFERENCED_PARAMETER(Target);

    ULONG ioControlCode = static_cast<ULONG>(
        reinterpret_cast<ULONG_PTR>(Context)
        );

    NTSTATUS status = Params->IoStatus.Status;

    if (ioControlCode == TRIDENT_OBS_IOCTL_HID_GET_DEVICE_ATTRIBUTES)
    {
        InterlockedIncrement(&g_HidGetDeviceAttributesCompleted);

        InterlockedExchange(
            &g_LastHidGetDeviceAttributesStatus,
            status
        );

        if (!NT_SUCCESS(status))
        {
            InterlockedIncrement(&g_HidGetDeviceAttributesFailed);
        }
    }
    else if (ioControlCode == TRIDENT_OBS_IOCTL_HID_GET_REPORT_DESCRIPTOR)
    {
        InterlockedIncrement(&g_HidGetReportDescriptorCompleted);

        InterlockedExchange(
            &g_LastHidGetReportDescriptorStatus,
            status
        );

        if (!NT_SUCCESS(status))
        {
            InterlockedIncrement(&g_HidGetReportDescriptorFailed);
        }
    }

    WdfRequestComplete(
        Request,
        status
    );
}

extern "C"
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

    if (NT_SUCCESS(Params->IoStatus.Status))
    {
        PVOID buffer = nullptr;
        size_t length = 0;

        NTSTATUS status = WdfRequestRetrieveOutputBuffer(
            Request,
            1,
            &buffer,
            &length
        );

        if (NT_SUCCESS(status))
        {
            InterlockedIncrement(&g_ReadReportBufferRetrieved);
        }
        else
        {
            InterlockedIncrement(&g_ReadReportBufferRetrieveFailed);
        }
    }

    WdfRequestComplete(
        Request,
        Params->IoStatus.Status
    );
}

extern "C"
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

    InterlockedExchange(
        &g_LastInternalIoctlCode,
        static_cast<LONG>(IoControlCode)
    );

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

extern "C"
VOID
TridentEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    UNREFERENCED_PARAMETER(InputBufferLength);

    InterlockedIncrement(&g_DeviceIoctlCount);

    InterlockedExchange(
        &g_LastDeviceIoctlCode,
        static_cast<LONG>(IoControlCode)
    );

    if (IoControlCode == IOCTL_TRIDENT_GET_STATS)
    {
        InterlockedIncrement(&g_GetStatsIoctlCount);

        if (OutputBufferLength < sizeof(TRIDENT_STATS))
        {
            WdfRequestComplete(
                Request,
                STATUS_BUFFER_TOO_SMALL
            );
            return;
        }

        PTRIDENT_STATS stats = nullptr;
        size_t length = 0;

        NTSTATUS status = WdfRequestRetrieveOutputBuffer(
            Request,
            sizeof(TRIDENT_STATS),
            reinterpret_cast<PVOID*>(&stats),
            &length
        );

        if (!NT_SUCCESS(status))
        {
            WdfRequestComplete(Request, status);
            return;
        }

        stats->InternalIoctlCount =
            InterlockedCompareExchange(&g_InternalIoctlCount, 0, 0);

        stats->DeviceIoctlCount =
            InterlockedCompareExchange(&g_DeviceIoctlCount, 0, 0);

        stats->GetStatsIoctlCount =
            InterlockedCompareExchange(&g_GetStatsIoctlCount, 0, 0);

        stats->NonStatsDeviceIoctlCount =
            InterlockedCompareExchange(&g_NonStatsDeviceIoctlCount, 0, 0);

        stats->LastNonStatsDeviceIoctlCode =
            InterlockedCompareExchange(&g_LastNonStatsDeviceIoctlCode, 0, 0);

        for (int i = 0; i < 8; i++)
        {
            stats->NonStatsDeviceIoctlCodes[i] =
                InterlockedCompareExchange(
                    &g_NonStatsDeviceIoctlCodes[i],
                    0,
                    0
                );
        }

        stats->HidGetDeviceAttributesCount =
            InterlockedCompareExchange( &g_HidGetDeviceAttributesCount, 0, 0);

        stats->HidGetReportDescriptorCount =
            InterlockedCompareExchange(&g_HidGetReportDescriptorCount, 0, 0);

        stats->ReadRequestCount =
            InterlockedCompareExchange(&g_ReadRequestCount, 0, 0);

        stats->LastInternalIoctlCode =
            InterlockedCompareExchange(&g_LastInternalIoctlCode, 0, 0);

        stats->LastDeviceIoctlCode =
            InterlockedCompareExchange(&g_LastDeviceIoctlCode, 0, 0);

        stats->ReadReportReceived =
            InterlockedCompareExchange(&g_ReadReportReceived, 0, 0);

        stats->ReadReportCompleted =
            InterlockedCompareExchange(&g_ReadReportCompleted, 0, 0);

        stats->ReadReportSendFailed =
            InterlockedCompareExchange(&g_ReadReportSendFailed, 0, 0);

        stats->ReadReportBufferRetrieved =
            InterlockedCompareExchange(&g_ReadReportBufferRetrieved, 0, 0);

        stats->ReadReportBufferRetrieveFailed =
            InterlockedCompareExchange(&g_ReadReportBufferRetrieveFailed, 0, 0);

        stats->HidGetDeviceAttributesCompleted =
            InterlockedCompareExchange(&g_HidGetDeviceAttributesCompleted, 0, 0);

        stats->HidGetDeviceAttributesFailed =
            InterlockedCompareExchange(&g_HidGetDeviceAttributesFailed, 0, 0);

        stats->LastHidGetDeviceAttributesStatus =
            InterlockedCompareExchange(&g_LastHidGetDeviceAttributesStatus, 0, 0);

        stats->HidGetReportDescriptorCompleted =
            InterlockedCompareExchange(&g_HidGetReportDescriptorCompleted, 0, 0);

        stats->HidGetReportDescriptorFailed =
            InterlockedCompareExchange(&g_HidGetReportDescriptorFailed, 0, 0);

        stats->LastHidGetReportDescriptorStatus =
            InterlockedCompareExchange(&g_LastHidGetReportDescriptorStatus, 0, 0);

        WdfRequestCompleteWithInformation(
            Request,
            STATUS_SUCCESS,
            sizeof(TRIDENT_STATS)
        );

        return;
    }

    InterlockedIncrement(&g_NonStatsDeviceIoctlCount);

    InterlockedExchange(
        &g_LastNonStatsDeviceIoctlCode,
        static_cast<LONG>(IoControlCode)
    );

    TridentRecordNonStatsDeviceIoctl(IoControlCode);

    switch (IoControlCode)
    {
    case TRIDENT_OBS_IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        InterlockedIncrement(&g_HidGetDeviceAttributesCount);
        break;

    case TRIDENT_OBS_IOCTL_HID_GET_REPORT_DESCRIPTOR:
        InterlockedIncrement(&g_HidGetReportDescriptorCount);
        break;

    default:
        break;
    }

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    WDFIOTARGET target = WdfDeviceGetIoTarget(device);

    WdfRequestFormatRequestUsingCurrentType(Request);

    if (IoControlCode == TRIDENT_OBS_IOCTL_HID_GET_DEVICE_ATTRIBUTES ||
        IoControlCode == TRIDENT_OBS_IOCTL_HID_GET_REPORT_DESCRIPTOR)
    {
        WdfRequestSetCompletionRoutine(
            Request,
            TridentHidInfoCompletion,
            reinterpret_cast<WDFCONTEXT>(
                static_cast<ULONG_PTR>(IoControlCode)
                )
        );
    }

    if (!WdfRequestSend(Request, target, WDF_NO_SEND_OPTIONS))
    {
        NTSTATUS status = WdfRequestGetStatus(Request);
        WdfRequestComplete(Request, status);
    }
}

extern "C"
VOID
TridentEvtIoRead(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t Length
)
{
    UNREFERENCED_PARAMETER(Length);

    InterlockedIncrement(&g_ReadRequestCount);

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    WDFIOTARGET target = WdfDeviceGetIoTarget(device);

    WdfRequestFormatRequestUsingCurrentType(Request);

    if (!WdfRequestSend(Request, target, WDF_NO_SEND_OPTIONS))
    {
        NTSTATUS status = WdfRequestGetStatus(Request);
        WdfRequestComplete(Request, status);
    }
}