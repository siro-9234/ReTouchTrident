#include <ntddk.h>
#include <wdf.h>

#include "Device.h"

extern "C"
{
    DRIVER_INITIALIZE DriverEntry;
}

NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
)
{

    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, ReTouchEvtDeviceAdd);

    return WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );
}