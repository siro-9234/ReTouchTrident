#include <ntddk.h>
#include <wdf.h>

#include "Device.h"
#include "GlobalContext.h"
#include "ControlDevice.h"
#include "Win32kManager.h"
#include "TridentLog.h"

extern "C"
DRIVER_INITIALIZE DriverEntry;

EVT_WDF_OBJECT_CONTEXT_CLEANUP
TridentEvtDriverContextCleanup;


VOID
TridentEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    TridentWin32kManager::Shutdown();
}


extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDRIVER driver;

    TridentGlobalInitialize();

    //
    // Cursor suppression is optional and must never prevent the capture
    // filter from loading. This path currently performs discovery,
    // validation, inspection, and transaction preparation only.
    //
    const NTSTATUS win32kStatus =
        TridentWin32kManager::Initialize();

    if (!NT_SUCCESS(win32kStatus))
    {
        TridentLogWarning(
            "Win32kManager unavailable; filter continues without cursor suppression: 0x%08X",
            win32kStatus
        );
    }

    WDF_DRIVER_CONFIG_INIT(
        &config,
        TridentEvtDeviceAdd
    );

    WDF_OBJECT_ATTRIBUTES_INIT(
        &attributes
    );

    attributes.EvtCleanupCallback =
        TridentEvtDriverContextCleanup;

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &attributes,
        &config,
        &driver
    );

    if (!NT_SUCCESS(status))
    {
        //
        // No WDF driver object exists, so its cleanup callback cannot run.
        //
        TridentWin32kManager::Shutdown();

        return status;
    }

    status = TridentCreateControlDevice(
        driver
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    return STATUS_SUCCESS;
}