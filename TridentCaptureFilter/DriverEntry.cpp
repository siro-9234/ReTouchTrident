#include <ntddk.h>
#include <wdf.h>

#include "Device.h"
#include "GlobalContext.h"
#include "ControlDevice.h"
#include "Win32kManager.h"
#include "TridentLog.h"

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

    // The cursor-suppression feature is optional and must never prevent the
    // capture filter from loading. The current safe scaffold only performs
    // version discovery and fail-closed capability probing.
    const NTSTATUS win32kStatus = TridentWin32kManager::Initialize();
    if (!NT_SUCCESS(win32kStatus))
    {
        TridentLogWarning("Win32kManager unavailable; filter continues without cursor suppression: 0x%08X", win32kStatus);
    }

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