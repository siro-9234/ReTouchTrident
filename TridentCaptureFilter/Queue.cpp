#include "Queue.h"
#include "ReTouchClient.h"
#include "DeviceContext.h"
#include "GlobalContext.h"

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

static volatile LONG g_LastCompletedDeviceIoctlCode = 0;
static volatile LONG g_LastCompletedDeviceIoctlStatus = 0;
static volatile LONG g_LastCompletedDeviceIoctlInformation = 0;

static volatile LONG g_ReadCompleted = 0;
static volatile LONG g_LastReadStatus = 0;
static volatile LONG g_LastReadInformation = 0;

static volatile LONG g_LastReadDataLength = 0;
static UCHAR g_LastReadData[64] = {};

static volatile LONG g_LastDecodedTouchX = 0;
static volatile LONG g_LastDecodedTouchY = 0;
static volatile LONG g_LastDecodeTouchReportSucceeded = 0;
static volatile LONG g_LastDecodeTouchReportFailed = 0;
static volatile LONG g_LastDecodedTipSwitch = 0;

static volatile LONG g_LastFrameContactCount = 0;
static volatile LONG g_LastFrameX = 0;
static volatile LONG g_LastFrameY = 0;
static volatile LONG g_LastFrameIsDown = 0;

static volatile LONG g_ReTouchInterfaceQueryCount = 0;
static volatile LONG g_ReTouchInterfaceFound = 0;
static volatile LONG g_LastReTouchInterfaceStatus = 0;

static volatile LONG g_ReTouchSubmitAttemptCount = 0;
static volatile LONG g_ReTouchSubmitContextNullCount = 0;
static volatile LONG g_ReTouchSubmitCompletedCount = 0;
static volatile LONG g_LastReTouchSubmitStatus = 0;

static volatile LONG g_LastCompletionClientPointerLow = 0;

static volatile LONG g_LastCompletionPhysicalDeviceObjectLow = 0;
static volatile LONG g_LastCompletionWdmDeviceObjectLow = 0;

static volatile LONG g_LastCompletionDeviceInstanceId = 0;

static volatile LONG g_LastSubmitClientInstanceId = 0;

typedef struct _TOUCH_POINT
{
    USHORT X;
    USHORT Y;
    BOOLEAN TipSwitch;
} TOUCH_POINT, * PTOUCH_POINT;

_Success_(return != FALSE)
static
BOOLEAN
DecodeTouchReport(
    _In_reads_bytes_(ReportLength) PUCHAR Report,
    _In_ size_t ReportLength,
    _Out_ PTOUCH_POINT Point
)
{
    if (Report == NULL || Point == NULL)
    {
        return FALSE;
    }

    if (ReportLength < 6)
    {
        return FALSE;
    }

    Point->X = (USHORT)(Report[2] | (Report[3] << 8));
    Point->Y = (USHORT)(Report[4] | (Report[5] << 8));
    Point->TipSwitch = (Report[1] & 0x01) ? TRUE : FALSE;

    return TRUE;
}

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
TridentDeviceControlCompletion(
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

    InterlockedExchange(&g_LastCompletedDeviceIoctlCode, static_cast<LONG>(ioControlCode));
    InterlockedExchange(&g_LastCompletedDeviceIoctlStatus, Params->IoStatus.Status);
    InterlockedExchange(&g_LastCompletedDeviceIoctlInformation, static_cast<LONG>(Params->IoStatus.Information));

    WdfRequestCompleteWithInformation(
        Request,
        Params->IoStatus.Status,
        Params->IoStatus.Information
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
        InterlockedExchange(&g_LastHidGetDeviceAttributesStatus, status);

        if (!NT_SUCCESS(status))
        {
            InterlockedIncrement(&g_HidGetDeviceAttributesFailed);
        }
    }
    else if (ioControlCode == TRIDENT_OBS_IOCTL_HID_GET_REPORT_DESCRIPTOR)
    {
        InterlockedIncrement(&g_HidGetReportDescriptorCompleted);
        InterlockedExchange(&g_LastHidGetReportDescriptorStatus, status);

        if (!NT_SUCCESS(status))
        {
            InterlockedIncrement(&g_HidGetReportDescriptorFailed);
        }
    }

    WdfRequestCompleteWithInformation(
        Request,
        status,
        Params->IoStatus.Information
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

    WdfRequestCompleteWithInformation(
        Request,
        Params->IoStatus.Status,
        Params->IoStatus.Information
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
    InterlockedExchange(&g_LastInternalIoctlCode, static_cast<LONG>(IoControlCode));

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
    InterlockedExchange(&g_LastDeviceIoctlCode, static_cast<LONG>(IoControlCode));

    if (IoControlCode == IOCTL_TRIDENT_GET_STATS)
    {
        InterlockedIncrement(&g_GetStatsIoctlCount);

        if (OutputBufferLength < sizeof(TRIDENT_STATS))
        {
            WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
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

        WDFDEVICE statsDevice = WdfIoQueueGetDevice(Queue);

        PDEVICE_CONTEXT deviceContext =
            DeviceGetContext(statsDevice);

        stats->StatsDeviceInstanceId =
            deviceContext->DeviceInstanceId;

        stats->CompletionDeviceInstanceId =
            InterlockedCompareExchange(&g_LastCompletionDeviceInstanceId, 0, 0);

        auto client =
            &deviceContext->ReTouchClient;

        stats->InternalIoctlCount = InterlockedCompareExchange(&g_InternalIoctlCount, 0, 0);
        stats->DeviceIoctlCount = InterlockedCompareExchange(&g_DeviceIoctlCount, 0, 0);
        stats->GetStatsIoctlCount = InterlockedCompareExchange(&g_GetStatsIoctlCount, 0, 0);
        stats->NonStatsDeviceIoctlCount = InterlockedCompareExchange(&g_NonStatsDeviceIoctlCount, 0, 0);
        stats->LastNonStatsDeviceIoctlCode = InterlockedCompareExchange(&g_LastNonStatsDeviceIoctlCode, 0, 0);

        for (int i = 0; i < 8; i++)
        {
            stats->NonStatsDeviceIoctlCodes[i] =
                InterlockedCompareExchange(&g_NonStatsDeviceIoctlCodes[i], 0, 0);
        }

        stats->HidGetDeviceAttributesCount = InterlockedCompareExchange(&g_HidGetDeviceAttributesCount, 0, 0);
        stats->HidGetReportDescriptorCount = InterlockedCompareExchange(&g_HidGetReportDescriptorCount, 0, 0);
        stats->ReadRequestCount = InterlockedCompareExchange(&g_ReadRequestCount, 0, 0);
        stats->LastInternalIoctlCode = InterlockedCompareExchange(&g_LastInternalIoctlCode, 0, 0);
        stats->LastDeviceIoctlCode = InterlockedCompareExchange(&g_LastDeviceIoctlCode, 0, 0);

        stats->ReadReportReceived = InterlockedCompareExchange(&g_ReadReportReceived, 0, 0);
        stats->ReadReportCompleted = InterlockedCompareExchange(&g_ReadReportCompleted, 0, 0);
        stats->ReadReportSendFailed = InterlockedCompareExchange(&g_ReadReportSendFailed, 0, 0);
        stats->ReadReportBufferRetrieved = InterlockedCompareExchange(&g_ReadReportBufferRetrieved, 0, 0);
        stats->ReadReportBufferRetrieveFailed = InterlockedCompareExchange(&g_ReadReportBufferRetrieveFailed, 0, 0);

        stats->HidGetDeviceAttributesCompleted = InterlockedCompareExchange(&g_HidGetDeviceAttributesCompleted, 0, 0);
        stats->HidGetDeviceAttributesFailed = InterlockedCompareExchange(&g_HidGetDeviceAttributesFailed, 0, 0);
        stats->LastHidGetDeviceAttributesStatus = InterlockedCompareExchange(&g_LastHidGetDeviceAttributesStatus, 0, 0);

        stats->HidGetReportDescriptorCompleted = InterlockedCompareExchange(&g_HidGetReportDescriptorCompleted, 0, 0);
        stats->HidGetReportDescriptorFailed = InterlockedCompareExchange(&g_HidGetReportDescriptorFailed, 0, 0);
        stats->LastHidGetReportDescriptorStatus = InterlockedCompareExchange(&g_LastHidGetReportDescriptorStatus, 0, 0);

        stats->LastCompletedDeviceIoctlCode = InterlockedCompareExchange(&g_LastCompletedDeviceIoctlCode, 0, 0);
        stats->LastCompletedDeviceIoctlStatus = InterlockedCompareExchange(&g_LastCompletedDeviceIoctlStatus, 0, 0);
        stats->LastCompletedDeviceIoctlInformation = InterlockedCompareExchange(&g_LastCompletedDeviceIoctlInformation, 0, 0);

        stats->ReadCompleted = InterlockedCompareExchange(&g_ReadCompleted, 0, 0);
        stats->LastReadStatus = InterlockedCompareExchange(&g_LastReadStatus, 0, 0);
        stats->LastReadInformation = InterlockedCompareExchange(&g_LastReadInformation, 0, 0);
        stats->LastReadDataLength = InterlockedCompareExchange(&g_LastReadDataLength, 0, 0);

        stats->LastDecodedTouchX = InterlockedCompareExchange(&g_LastDecodedTouchX, 0, 0);
        stats->LastDecodedTouchY = InterlockedCompareExchange(&g_LastDecodedTouchY, 0, 0);
        stats->LastDecodeTouchReportSucceeded = InterlockedCompareExchange(&g_LastDecodeTouchReportSucceeded, 0, 0);
        stats->LastDecodeTouchReportFailed = InterlockedCompareExchange(&g_LastDecodeTouchReportFailed, 0, 0);
        stats->LastDecodedTipSwitch = InterlockedCompareExchange(&g_LastDecodedTipSwitch, 0, 0);

        stats->LastFrameContactCount = InterlockedCompareExchange(&g_LastFrameContactCount, 0, 0);
        stats->LastFrameX = InterlockedCompareExchange(&g_LastFrameX, 0, 0);
        stats->LastFrameY = InterlockedCompareExchange(&g_LastFrameY, 0, 0);
        stats->LastFrameIsDown = InterlockedCompareExchange(&g_LastFrameIsDown, 0, 0);

        stats->ReTouchInterfaceQueryCount = InterlockedCompareExchange(&g_ReTouchInterfaceQueryCount, 0, 0);
        stats->ReTouchInterfaceFound = InterlockedCompareExchange(&g_ReTouchInterfaceFound, 0, 0);
        stats->LastReTouchInterfaceStatus = InterlockedCompareExchange(&g_LastReTouchInterfaceStatus, 0, 0);

        stats->ReTouchClientInitializeCount = ReTouchClient::GetInitializeCount(client);
        stats->ReTouchClientShutdownCount = ReTouchClient::GetShutdownCount(client);
        stats->ReTouchClientSubmitFrameCount = ReTouchClient::GetSubmitFrameCount(client);
        stats->ReTouchClientLastSubmitFrameStatus = ReTouchClient::GetLastSubmitFrameStatus(client);
        stats->ReTouchClientLastSubmitFrameContactCount = ReTouchClient::GetLastSubmitFrameContactCount(client);

        stats->ReTouchClientQueryInterfaceCount = ReTouchClient::GetQueryInterfaceCount(client);
        stats->ReTouchClientInterfaceFound = ReTouchClient::GetInterfaceFound(client);
        stats->ReTouchClientLastQueryInterfaceStatus = ReTouchClient::GetLastQueryInterfaceStatus(client);

        stats->ReTouchClientOpenCount = ReTouchClient::GetOpenCount(client);
        stats->ReTouchClientOpenSucceeded = ReTouchClient::GetOpenSucceeded(client);
        stats->ReTouchClientLastOpenStatus = ReTouchClient::GetLastOpenStatus(client);

        stats->ReTouchClientTestSubmitCount = ReTouchClient::GetTestSubmitCount(client);
        stats->ReTouchClientTestSubmitSucceeded = ReTouchClient::GetTestSubmitSucceeded(client);
        stats->ReTouchClientLastTestSubmitStatus = ReTouchClient::GetLastTestSubmitStatus(client);

        stats->ReTouchClientWorkItemEnqueueCount = ReTouchClient::GetWorkItemEnqueueCount(client);
        stats->ReTouchClientWorkItemRunCount = ReTouchClient::GetWorkItemRunCount(client);
        stats->ReTouchClientLastWorkItemContactCount = ReTouchClient::GetLastWorkItemContactCount(client);

        stats->ReTouchSubmitAttemptCount =
            InterlockedCompareExchange(&g_ReTouchSubmitAttemptCount, 0, 0);

        stats->ReTouchSubmitContextNullCount =
            InterlockedCompareExchange(&g_ReTouchSubmitContextNullCount, 0, 0);

        stats->ReTouchSubmitCompletedCount =
            InterlockedCompareExchange(&g_ReTouchSubmitCompletedCount, 0, 0);

        stats->LastReTouchSubmitStatus =
            InterlockedCompareExchange(&g_LastReTouchSubmitStatus, 0, 0);

        stats->ReTouchClientPointerLow =
            static_cast<ULONG>(PtrToUlong(client));

        stats->QueueClientPointerLow =
            static_cast<ULONG>(
                InterlockedCompareExchange(&g_LastCompletionClientPointerLow, 0, 0));

        stats->StatsDeviceInstanceId =
            deviceContext->DeviceInstanceId;

        stats->CompletionDeviceInstanceId =
            InterlockedCompareExchange(&g_LastCompletionDeviceInstanceId, 0, 0);

        stats->StatsClientInstanceId =
            client->ClientInstanceId;

        stats->SubmitClientInstanceId =
            InterlockedCompareExchange(&g_LastSubmitClientInstanceId, 0, 0);

        stats->StatsPhysicalDeviceObjectLow =
            static_cast<ULONG>(deviceContext->PhysicalDeviceObjectLow);

        stats->StatsWdmDeviceObjectLow =
            static_cast<ULONG>(deviceContext->WdmDeviceObjectLow);

        stats->CompletionPhysicalDeviceObjectLow =
            static_cast<ULONG>(InterlockedCompareExchange(&g_LastCompletionPhysicalDeviceObjectLow, 0, 0));

        stats->CompletionWdmDeviceObjectLow =
            static_cast<ULONG>(InterlockedCompareExchange(&g_LastCompletionWdmDeviceObjectLow, 0, 0));

        RtlCopyMemory(
            stats->LastReadData,
            g_LastReadData,
            sizeof(stats->LastReadData)
        );

        WdfRequestCompleteWithInformation(
            Request,
            STATUS_SUCCESS,
            sizeof(TRIDENT_STATS)
        );

        return;
    }

    InterlockedIncrement(&g_NonStatsDeviceIoctlCount);
    InterlockedExchange(&g_LastNonStatsDeviceIoctlCode, static_cast<LONG>(IoControlCode));

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

    WdfRequestSetCompletionRoutine(
        Request,
        TridentDeviceControlCompletion,
        reinterpret_cast<WDFCONTEXT>(
            static_cast<ULONG_PTR>(IoControlCode)
            )
    );

    if (!WdfRequestSend(Request, target, WDF_NO_SEND_OPTIONS))
    {
        NTSTATUS status = WdfRequestGetStatus(Request);
        WdfRequestComplete(Request, status);
    }
}

extern "C"
VOID
TridentReadCompletion(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT Context
)
{
    UNREFERENCED_PARAMETER(Target);

    PTRIDENT_GLOBAL_STATS globalStats = TridentGetGlobalStats();

    InterlockedIncrement(&g_ReadCompleted);
    InterlockedIncrement(&globalStats->ReadCompletionCount);

    InterlockedExchange(&g_LastReadStatus, Params->IoStatus.Status);
    InterlockedExchange(&g_LastReadInformation, static_cast<LONG>(Params->IoStatus.Information));

    if (NT_SUCCESS(Params->IoStatus.Status) &&
        Params->IoStatus.Information > 0)
    {
        PVOID buffer = nullptr;
        size_t length = 0;

        NTSTATUS retrieveStatus = WdfRequestRetrieveOutputBuffer(
            Request,
            1,
            &buffer,
            &length
        );

        if (NT_SUCCESS(retrieveStatus))
        {
            size_t copyLength = Params->IoStatus.Information;

            if (copyLength > sizeof(g_LastReadData))
            {
                copyLength = sizeof(g_LastReadData);
            }

            if (copyLength > length)
            {
                copyLength = length;
            }

            RtlCopyMemory(g_LastReadData, buffer, copyLength);

            InterlockedExchange(&g_LastReadDataLength, static_cast<LONG>(copyLength));
            InterlockedExchange(&globalStats->LastReadDataLength, static_cast<LONG>(copyLength));

            PUCHAR raw = static_cast<PUCHAR>(buffer);

            InterlockedExchange(&globalStats->LastRawByte0, copyLength > 0 ? raw[0] : 0);
            InterlockedExchange(&globalStats->LastRawByte1, copyLength > 1 ? raw[1] : 0);
            InterlockedExchange(&globalStats->LastRawByte2, copyLength > 2 ? raw[2] : 0);
            InterlockedExchange(&globalStats->LastRawByte3, copyLength > 3 ? raw[3] : 0);
            InterlockedExchange(&globalStats->LastRawByte4, copyLength > 4 ? raw[4] : 0);
            InterlockedExchange(&globalStats->LastRawByte5, copyLength > 5 ? raw[5] : 0);
            InterlockedExchange(&globalStats->LastRawByte6, copyLength > 6 ? raw[6] : 0);
            InterlockedExchange(&globalStats->LastRawByte7, copyLength > 7 ? raw[7] : 0);
            InterlockedExchange(&globalStats->LastRawByte8, copyLength > 8 ? raw[8] : 0);
            InterlockedExchange(&globalStats->LastRawByte9, copyLength > 9 ? raw[9] : 0);
            InterlockedExchange(&globalStats->LastRawByte10, copyLength > 10 ? raw[10] : 0);
            InterlockedExchange(&globalStats->LastRawByte11, copyLength > 11 ? raw[11] : 0);
            InterlockedExchange(&globalStats->LastRawByte12, copyLength > 12 ? raw[12] : 0);
            InterlockedExchange(&globalStats->LastRawByte13, copyLength > 13 ? raw[13] : 0);
            InterlockedExchange(&globalStats->LastRawByte14, copyLength > 14 ? raw[14] : 0);
            InterlockedExchange(&globalStats->LastRawByte15, copyLength > 15 ? raw[15] : 0);

            InterlockedExchange(&globalStats->TipCandidateByte0Bit0, copyLength > 0 ? ((raw[0] & 0x01) ? 1 : 0) : 0);
            InterlockedExchange(&globalStats->TipCandidateByte1Bit0, copyLength > 1 ? ((raw[1] & 0x01) ? 1 : 0) : 0);
            InterlockedExchange(&globalStats->TipCandidateByte2Bit0, copyLength > 2 ? ((raw[2] & 0x01) ? 1 : 0) : 0);
            InterlockedExchange(&globalStats->TipCandidateByte6Bit0, copyLength > 6 ? ((raw[6] & 0x01) ? 1 : 0) : 0);
            InterlockedExchange(&globalStats->TipCandidateByte7Bit0, copyLength > 7 ? ((raw[7] & 0x01) ? 1 : 0) : 0);

            TOUCH_POINT point = {};

            if (DecodeTouchReport(
                static_cast<PUCHAR>(buffer),
                copyLength,
                &point))
            {
                InterlockedIncrement(&globalStats->DecodeSuccessCount);

                InterlockedExchange(&g_LastDecodedTouchX, point.X);
                InterlockedExchange(&g_LastDecodedTouchY, point.Y);
                InterlockedExchange(&g_LastDecodeTouchReportSucceeded, 1);
                InterlockedExchange(&g_LastDecodedTipSwitch, point.TipSwitch ? 1 : 0);
                InterlockedExchange(&globalStats->LastDecodedTipSwitch, point.TipSwitch ? 1 : 0);

                RETOUCH_FRAME frame = {};

                frame.ContactCount = point.TipSwitch ? 1 : 0;

                frame.Contacts[0].Id = 0;
                frame.Contacts[0].IsDown = point.TipSwitch ? 1 : 0;
                frame.Contacts[0].X = point.X;
                frame.Contacts[0].Y = point.Y;

                InterlockedExchange(&g_LastFrameContactCount, frame.ContactCount);
                InterlockedExchange(&g_LastFrameX, frame.Contacts[0].X);
                InterlockedExchange(&g_LastFrameY, frame.Contacts[0].Y);
                InterlockedExchange(&g_LastFrameIsDown, frame.Contacts[0].IsDown);

                InterlockedIncrement(&g_ReTouchSubmitAttemptCount);
                InterlockedIncrement(&globalStats->SubmitAttemptCount);

                WDFDEVICE completionDevice =
                    reinterpret_cast<WDFDEVICE>(Context);

                if (completionDevice == nullptr)
                {
                    InterlockedIncrement(&g_ReTouchSubmitContextNullCount);
                }
                else
                {
                    PDEVICE_CONTEXT deviceContext =
                        DeviceGetContext(completionDevice);

                    InterlockedExchange(
                        &g_LastCompletionDeviceInstanceId,
                        deviceContext->DeviceInstanceId
                    );

                    InterlockedExchange(
                        &g_LastCompletionClientPointerLow,
                        static_cast<LONG>(PtrToUlong(&deviceContext->ReTouchClient))
                    );

                    InterlockedExchange(
                        &g_LastSubmitClientInstanceId,
                        deviceContext->ReTouchClient.ClientInstanceId
                    );

                    InterlockedExchange(
                        &g_LastCompletionPhysicalDeviceObjectLow,
                        deviceContext->PhysicalDeviceObjectLow
                    );

                    InterlockedExchange(
                        &g_LastCompletionWdmDeviceObjectLow,
                        deviceContext->WdmDeviceObjectLow
                    );

                    TridentGlobalRecordFilterActivity(
                        deviceContext->DeviceInstanceId,
                        deviceContext->ReTouchClient.ClientInstanceId,
                        deviceContext,
                        &deviceContext->ReTouchClient,
                        WdfDeviceWdmGetPhysicalDevice(completionDevice),
                        WdfDeviceWdmGetDeviceObject(completionDevice)
                    );

                    NTSTATUS submitStatus =
                        ReTouchClient::SubmitFrame(
                            &deviceContext->ReTouchClient,
                            &frame
                        );

                    InterlockedIncrement(&g_ReTouchSubmitCompletedCount);
                    InterlockedIncrement(&globalStats->SubmitCompletedCount);
                    InterlockedExchange(&g_LastReTouchSubmitStatus, submitStatus);
                }

                RtlZeroMemory(
                    buffer,
                    copyLength
                );
            }
            else
            {
                InterlockedExchange(&g_LastDecodeTouchReportFailed, 1);
            }
        }
    }

    WdfRequestCompleteWithInformation(
        Request,
        Params->IoStatus.Status,
        Params->IoStatus.Information
    );
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

    WdfRequestSetCompletionRoutine(
        Request,
        TridentReadCompletion,
        device
    );

    if (!WdfRequestSend(Request, target, WDF_NO_SEND_OPTIONS))
    {
        NTSTATUS status = WdfRequestGetStatus(Request);
        WdfRequestComplete(Request, status);
    }
}