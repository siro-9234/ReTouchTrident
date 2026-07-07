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

extern "C"
EVT_WDF_REQUEST_COMPLETION_ROUTINE TridentDeviceControlCompletion;

extern "C"
EVT_WDF_REQUEST_COMPLETION_ROUTINE TridentReadCompletion;

#define IOCTL_TRIDENT_GET_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA)
#define TRIDENT_OBS_IOCTL_HID_GET_DEVICE_ATTRIBUTES 0x000B01A8
#define TRIDENT_OBS_IOCTL_HID_GET_REPORT_DESCRIPTOR 0x000B01BE
#define RETOUCH_MAX_CONTACTS 10

typedef struct _RETOUCH_CONTACT
{
    UCHAR Id;
    UCHAR IsDown;
    USHORT X;
    USHORT Y;
} RETOUCH_CONTACT, * PRETOUCH_CONTACT;

typedef struct _RETOUCH_FRAME
{
    UCHAR ContactCount;
    RETOUCH_CONTACT Contacts[RETOUCH_MAX_CONTACTS];
} RETOUCH_FRAME, * PRETOUCH_FRAME;


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

    LONG LastCompletedDeviceIoctlCode;
    LONG LastCompletedDeviceIoctlStatus;
    LONG LastCompletedDeviceIoctlInformation;

    LONG ReadCompleted;
    LONG LastReadStatus;
    LONG LastReadInformation;

    LONG LastReadDataLength;
    UCHAR LastReadData[64];

    LONG LastDecodedTouchX;
    LONG LastDecodedTouchY;
    LONG LastDecodeTouchReportSucceeded;
    LONG LastDecodeTouchReportFailed;

    LONG LastDecodedTipSwitch;

    LONG LastFrameContactCount;
    LONG LastFrameX;
    LONG LastFrameY;
    LONG LastFrameIsDown;

    LONG ReTouchInterfaceQueryCount;
    LONG ReTouchInterfaceFound;
    LONG LastReTouchInterfaceStatus;

    LONG ReTouchClientInitializeCount;
    LONG ReTouchClientShutdownCount;
    LONG ReTouchClientSubmitFrameCount;
    LONG ReTouchClientLastSubmitFrameStatus;
    LONG ReTouchClientLastSubmitFrameContactCount;

    LONG ReTouchClientQueryInterfaceCount;
    LONG ReTouchClientInterfaceFound;
    LONG ReTouchClientLastQueryInterfaceStatus;
} TRIDENT_STATS, * PTRIDENT_STATS;