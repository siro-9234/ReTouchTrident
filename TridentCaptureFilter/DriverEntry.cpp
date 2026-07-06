#include <ntddk.h>
#include <wdf.h>

#include "Device.h"

extern "C"
DRIVER_INITIALIZE DriverEntry;

extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;

    WDF_DRIVER_CONFIG_INIT(
        &config,
        TridentEvtDeviceAdd
    );

    return WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );
}