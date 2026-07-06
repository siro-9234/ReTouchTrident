#include "Queue.h"

static volatile LONG g_InternalIoctlCount = 0;
static volatile LONG g_ReadReportReceived = 0;
static volatile LONG g_ReadReportCompleted = 0;
static volatile LONG g_ReadReportSendFailed = 0;
static volatile LONG g_ReadReportBufferRetrieved = 0;
static volatile LONG g_ReadReportBufferRetrieveFailed = 0;

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

    if (IoControlCode == IOCTL_TRIDENT_GET_STATS)
    {
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

        WdfRequestCompleteWithInformation(
            Request,
            STATUS_SUCCESS,
            sizeof(TRIDENT_STATS)
        );

        return;
    }

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    WDFIOTARGET target = WdfDeviceGetIoTarget(device);

    WdfRequestFormatRequestUsingCurrentType(Request);

    if (!WdfRequestSend(Request, target, WDF_NO_SEND_OPTIONS))
    {
        NTSTATUS status = WdfRequestGetStatus(Request);
        WdfRequestComplete(Request, status);
    }
}