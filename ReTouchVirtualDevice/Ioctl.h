#pragma once

#include <ntddk.h>
#include <wdm.h>

#define RETOUCH_MAX_CONTACTS 10

#define FILE_DEVICE_RETOUCH 0x8000

#define IOCTL_RETOUCH_SUBMIT_FRAME \
    CTL_CODE(FILE_DEVICE_RETOUCH, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)

#define IOCTL_RETOUCH_GET_STATS \
    CTL_CODE(FILE_DEVICE_RETOUCH, 0x802, METHOD_BUFFERED, FILE_READ_DATA)

#pragma pack(push, 1)

typedef struct _RETOUCH_STATS
{
    LONG DeviceAddCount;
    LONG DeviceCleanupCount;
    LONG QueueInitializeCount;

    LONG VirtualTouchInitializeCount;
    LONG VirtualTouchInitializeStatus;

    LONG VhfCreateStatus;
    LONG VhfStartCount;

    LONG SubmitFrameCount;
    LONG LastSubmitFrameStatus;
    LONG LastContactCount;

    LONG GetFeatureCount;
    LONG LastGetFeatureReportId;

    LONG SetFeatureCount;
    LONG LastSetFeatureReportId;

    LONG WdmDeviceObjectNull;

    LONG LastActiveContactCount;
    LONG LastFirstContactFlags;
    LONG LastFirstContactId;
    LONG LastFirstContactX;
    LONG LastFirstContactY;
    LONG LastReportContactCount;

    LONG ReceivedSubmitFrameIoctlCount;
    LONG ReceivedContactCount;
    LONG ReceivedFirstContactId;
    LONG ReceivedFirstContactIsDown;
    LONG ReceivedFirstContactX;
    LONG ReceivedFirstContactY;

} RETOUCH_STATS, * PRETOUCH_STATS;

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

#pragma pack(pop)