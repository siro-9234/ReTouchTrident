#include "ReTouchLogger.h"

#include <ntstrsafe.h>

static WDFDEVICE g_ReTouchLoggerDevice = nullptr;

typedef struct _RETOUCH_LOG_WORKITEM_CONTEXT
{
    CHAR Line[256];

} RETOUCH_LOG_WORKITEM_CONTEXT, * PRETOUCH_LOG_WORKITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    RETOUCH_LOG_WORKITEM_CONTEXT,
    ReTouchGetLogWorkItemContext
);

EVT_WDF_WORKITEM ReTouchLoggerWorkItem;

static NTSTATUS
ReTouchLoggerAppendLinePassive(
    _In_z_ PCSTR Line
)
{
    NTSTATUS status;
    HANDLE fileHandle = NULL;
    IO_STATUS_BLOCK ioStatusBlock = {};
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING fileName;
    LARGE_INTEGER byteOffset;

    if (Line == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    RtlInitUnicodeString(
        &fileName,
        L"\\??\\C:\\ReTouchTridentLog.txt"
    );

    InitializeObjectAttributes(
        &objectAttributes,
        &fileName,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    status = ZwCreateFile(
        &fileHandle,
        FILE_APPEND_DATA | SYNCHRONIZE,
        &objectAttributes,
        &ioStatusBlock,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ,
        FILE_OPEN_IF,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE,
        NULL,
        0
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    byteOffset.LowPart = FILE_WRITE_TO_END_OF_FILE;
    byteOffset.HighPart = -1;

    status = ZwWriteFile(
        fileHandle,
        NULL,
        NULL,
        NULL,
        &ioStatusBlock,
        (PVOID)Line,
        (ULONG)strlen(Line),
        &byteOffset,
        NULL
    );

    ZwClose(fileHandle);

    return status;
}

VOID
ReTouchLoggerWorkItem(
    _In_ WDFWORKITEM WorkItem
)
{
    PRETOUCH_LOG_WORKITEM_CONTEXT logContext;

    logContext = ReTouchGetLogWorkItemContext(WorkItem);

    if (logContext != nullptr)
    {
        (VOID)ReTouchLoggerAppendLinePassive(logContext->Line);
    }

    WdfObjectDelete(WorkItem);
}

VOID
ReTouchLoggerInitialize(
    _In_ WDFDEVICE Device
)
{
    g_ReTouchLoggerDevice = Device;
}

VOID
ReTouchLoggerShutdown()
{
    g_ReTouchLoggerDevice = nullptr;
}

VOID
ReTouchLoggerLogTouchTransition(
    _In_ BOOLEAN IsTouchDown,
    _In_ USHORT X,
    _In_ USHORT Y,
    _In_ UCHAR ContactId
)
{
    NTSTATUS status;
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_WORKITEM_CONFIG workItemConfig;
    WDFWORKITEM workItem = NULL;
    PRETOUCH_LOG_WORKITEM_CONTEXT logContext;

    if (g_ReTouchLoggerDevice == nullptr)
    {
        return;
    }

    counter = KeQueryPerformanceCounter(&frequency);

    WDF_WORKITEM_CONFIG_INIT(
        &workItemConfig,
        ReTouchLoggerWorkItem
    );

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
        &attributes,
        RETOUCH_LOG_WORKITEM_CONTEXT
    );

    attributes.ParentObject = g_ReTouchLoggerDevice;

    status = WdfWorkItemCreate(
        &workItemConfig,
        &attributes,
        &workItem
    );

    if (!NT_SUCCESS(status))
    {
        return;
    }

    logContext = ReTouchGetLogWorkItemContext(workItem);

    if (logContext == nullptr)
    {
        WdfObjectDelete(workItem);
        return;
    }

    status = RtlStringCbPrintfA(
        logContext->Line,
        sizeof(logContext->Line),
        "QPC=%lld Freq=%lld Event=%s X=%hu Y=%hu ContactId=%hhu\r\n",
        counter.QuadPart,
        frequency.QuadPart,
        IsTouchDown ? "TouchDownTransition" : "TouchUpTransition",
        X,
        Y,
        ContactId
    );

    if (!NT_SUCCESS(status))
    {
        WdfObjectDelete(workItem);
        return;
    }

    WdfWorkItemEnqueue(workItem);
}