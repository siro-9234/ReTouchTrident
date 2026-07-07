#pragma once

#include <ntddk.h>
#include <wdf.h>

extern "C"
EVT_WDF_DRIVER_DEVICE_ADD ReTouchEvtDeviceAdd;

extern "C"
EVT_WDF_OBJECT_CONTEXT_CLEANUP ReTouchEvtDeviceCleanup;