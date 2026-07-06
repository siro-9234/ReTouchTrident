#pragma once

#include <ntddk.h>
#include <wdf.h>

typedef struct _DEVICE_CONTEXT
{
    WDFDEVICE Device;

} DEVICE_CONTEXT, * PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    DEVICE_CONTEXT,
    DeviceGetContext
);