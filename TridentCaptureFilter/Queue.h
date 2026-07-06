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

extern "C"
EVT_WDF_IO_QUEUE_IO_READ TridentEvtIoRead;

extern "C"
EVT_WDF_REQUEST_COMPLETION_ROUTINE TridentHidInfoCompletion;

#define IOCTL_TRIDENT_GET_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA)
#define TRIDENT_OBS_IOCTL_HID_GET_DEVICE_ATTRIBUTES 0x000B01A8
#define TRIDENT_OBS_IOCTL_HID_GET_REPORT_DESCRIPTOR 0x000B01BE

typedef struct _TRIDENT_STATS
{
    LONG InternalIoctlCount;
    LONG DeviceIoctlCount;
    LONG GetStatsIoctlCount;
    LONG ReadRequestCount;

    LONG NonStatsDeviceIoctlCount;
    LONG LastNonStatsDeviceIoctlCode;
    LONG NonStatsDeviceIoctlCodes[8];

    LONG HidGetDeviceAttributesCount;
    LONG HidGetReportDescriptorCount;

    LONG LastInternalIoctlCode;
    LONG LastDeviceIoctlCode;

    LONG ReadReportReceived;
    LONG ReadReportCompleted;
    LONG ReadReportSendFailed;
    LONG ReadReportBufferRetrieved;
    LONG ReadReportBufferRetrieveFailed;

    LONG HidGetDeviceAttributesCompleted;
    LONG HidGetDeviceAttributesFailed;
    LONG LastHidGetDeviceAttributesStatus;

    LONG HidGetReportDescriptorCompleted;
    LONG HidGetReportDescriptorFailed;
    LONG LastHidGetReportDescriptorStatus;
} TRIDENT_STATS, * PTRIDENT_STATS;