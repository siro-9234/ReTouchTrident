#include <ntddk.h>
#include <wdf.h>

#include "Device.h"
#include "GlobalContext.h"
#include "ControlDevice.h"

extern "C"
DRIVER_INITIALIZE DriverEntry;

extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;
    WDFDRIVER driver;

    TridentGlobalInitialize();

    WDF_DRIVER_CONFIG_INIT(
        &config,
        TridentEvtDeviceAdd
    );

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        &driver
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = TridentCreateControlDevice(driver);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    return STATUS_SUCCESS;
}