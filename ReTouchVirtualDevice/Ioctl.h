#pragma once

#include <ntddk.h>
#include <wdm.h>

#define RETOUCH_MAX_CONTACTS 10

#define FILE_DEVICE_RETOUCH 0x8000

#define IOCTL_RETOUCH_SUBMIT_FRAME \
    CTL_CODE(FILE_DEVICE_RETOUCH, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)

#pragma pack(push, 1)

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