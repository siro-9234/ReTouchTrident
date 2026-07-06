#pragma once

#include <initguid.h>
#include <ntddk.h>
#include <wdf.h>

// {7C3A92D8-6C4D-4D8E-9F35-123456789ABC}
DEFINE_GUID(
    GUID_DEVINTERFACE_TRIDENT_CAPTURE,
    0x7c3a92d8, 0x6c4d, 0x4d8e,
    0x9f, 0x35, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc
);

extern "C"
EVT_WDF_DRIVER_DEVICE_ADD TridentEvtDeviceAdd;