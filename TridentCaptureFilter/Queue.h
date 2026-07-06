#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>

extern "C"
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL TridentEvtIoInternalDeviceControl;

extern "C"
EVT_WDF_REQUEST_COMPLETION_ROUTINE TridentReadReportCompletion;

extern "C"
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL TridentEvtIoDeviceControl;

#define IOCTL_TRIDENT_GET_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA)

typedef struct _TRIDENT_STATS
{
    LONG InternalIoctlCount;
    LONG ReadReportReceived;
    LONG ReadReportCompleted;
    LONG ReadReportSendFailed;
    LONG ReadReportBufferRetrieved;
    LONG ReadReportBufferRetrieveFailed;
} TRIDENT_STATS, * PTRIDENT_STATS;