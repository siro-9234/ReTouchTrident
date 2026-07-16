#pragma once

#include <ntddk.h>
#include <wdf.h>

#define IOCTL_TRIDENT_GET_GLOBAL_STATS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_READ_DATA)

extern "C"
NTSTATUS
TridentCreateControlDevice(
    _In_ WDFDRIVER Driver
);

extern "C"
NTSTATUS
TridentInitializeControlDeviceLifecycle(
    _In_ WDFDRIVER Driver
);