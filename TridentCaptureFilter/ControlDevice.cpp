#include <ntddk.h>
#include <wdf.h>
#include <wdmsec.h>

#include "ControlDevice.h"
#include "GlobalContext.h"
namespace
{
    //
    // Published only after the control device has been fully initialized.
    // A later lifecycle step will delete this object after the final
    // PnP device has been removed.
    //
    PVOID volatile g_ControlDeviceObject = nullptr;

    WDFCOLLECTION g_FilterDeviceCollection = nullptr;
    WDFWAITLOCK g_FilterDeviceCollectionLock = nullptr;
}
    extern "C"
    EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL TridentControlEvtIoDeviceControl;

    extern "C"
        NTSTATUS
        TridentCreateControlDevice(
            _In_ WDFDRIVER Driver
        )
    {
        if (Driver == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        PTRIDENT_GLOBAL_STATS globalStats =
            TridentGetGlobalStats();

        InterlockedIncrement(
            &globalStats->ControlDeviceCreateAttempt
        );

        //
        // DriverEntry currently calls this routine once. Keep the operation
        // idempotent so a later move into the PnP-device lifecycle does not
        // create a second control device.
        //
        if (InterlockedCompareExchangePointer(
            &g_ControlDeviceObject,
            nullptr,
            nullptr) != nullptr)
        {
            return STATUS_SUCCESS;
        }

        UNICODE_STRING deviceName;
        UNICODE_STRING symbolicLinkName;

        RtlInitUnicodeString(
            &deviceName,
            L"\\Device\\TridentCaptureControl"
        );

        RtlInitUnicodeString(
            &symbolicLinkName,
            L"\\DosDevices\\TridentCaptureControl"
        );

        PWDFDEVICE_INIT deviceInit =
            WdfControlDeviceInitAllocate(
                Driver,
                &SDDL_DEVOBJ_SYS_ALL_ADM_ALL
            );

        if (deviceInit == nullptr)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        NTSTATUS status =
            WdfDeviceInitAssignName(
                deviceInit,
                &deviceName
            );

        if (!NT_SUCCESS(status))
        {
            WdfDeviceInitFree(
                deviceInit
            );

            return status;
        }

        WDF_OBJECT_ATTRIBUTES deviceAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT(
            &deviceAttributes
        );

        WDFDEVICE controlDevice = nullptr;

        status =
            WdfDeviceCreate(
                &deviceInit,
                &deviceAttributes,
                &controlDevice
            );

        if (!NT_SUCCESS(status))
        {
            if (deviceInit != nullptr)
            {
                WdfDeviceInitFree(
                    deviceInit
                );
            }

            return status;
        }

        WDF_IO_QUEUE_CONFIG queueConfig;

        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
            &queueConfig,
            WdfIoQueueDispatchSequential
        );

        queueConfig.EvtIoDeviceControl =
            TridentControlEvtIoDeviceControl;

        WDFQUEUE queue = nullptr;

        status =
            WdfIoQueueCreate(
                controlDevice,
                &queueConfig,
                WDF_NO_OBJECT_ATTRIBUTES,
                &queue
            );

        if (!NT_SUCCESS(status))
        {
            WdfObjectDelete(
                controlDevice
            );

            return status;
        }

        status =
            WdfDeviceCreateSymbolicLink(
                controlDevice,
                &symbolicLinkName
            );

        if (!NT_SUCCESS(status))
        {
            WdfObjectDelete(
                controlDevice
            );

            return status;
        }

        WdfControlFinishInitializing(
            controlDevice
        );

        //
        // Publish only the completely initialized object.
        //
        const PVOID previousControlDevice =
            InterlockedCompareExchangePointer(
                &g_ControlDeviceObject,
                reinterpret_cast<PVOID>(controlDevice),
                nullptr
            );

        if (previousControlDevice != nullptr)
        {
            //
            // Another valid control device was published first.
            //
            WdfObjectDelete(
                controlDevice
            );

            return STATUS_SUCCESS;
        }

        InterlockedIncrement(
            &globalStats->ControlDeviceCreateSucceeded
        );

        return STATUS_SUCCESS;
    }

    extern "C"
    VOID
    TridentControlEvtIoDeviceControl(
        _In_ WDFQUEUE Queue,
        _In_ WDFREQUEST Request,
        _In_ size_t OutputBufferLength,
        _In_ size_t InputBufferLength,
        _In_ ULONG IoControlCode
    )
    {
        NTSTATUS status;
        size_t information;
        PTRIDENT_GLOBAL_STATS globalStats;
        PTRIDENT_GLOBAL_STATS_SNAPSHOT outputBuffer;

        UNREFERENCED_PARAMETER(Queue);
        UNREFERENCED_PARAMETER(OutputBufferLength);
        UNREFERENCED_PARAMETER(InputBufferLength);

        status = STATUS_SUCCESS;
        information = 0;
        globalStats = TridentGetGlobalStats();
        outputBuffer = nullptr;

        InterlockedIncrement(&globalStats->ControlDeviceIoctlStatsCount);

        if (IoControlCode == IOCTL_TRIDENT_GET_GLOBAL_STATS)
        {
            status = WdfRequestRetrieveOutputBuffer(
                Request,
                sizeof(TRIDENT_GLOBAL_STATS_SNAPSHOT),
                reinterpret_cast<PVOID*>(&outputBuffer),
                nullptr
            );

            if (NT_SUCCESS(status))
            {
                TridentGlobalSnapshot(outputBuffer);
                information = sizeof(TRIDENT_GLOBAL_STATS_SNAPSHOT);
            }

            WdfRequestCompleteWithInformation(Request, status, information);
            return;
        }

        status = STATUS_INVALID_DEVICE_REQUEST;
        WdfRequestComplete(Request, status);
    }

    extern "C"
        NTSTATUS
        TridentInitializeControlDeviceLifecycle(
            _In_ WDFDRIVER Driver
        )
    {
        if (Driver == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (g_FilterDeviceCollection != nullptr ||
            g_FilterDeviceCollectionLock != nullptr)
        {
            return STATUS_INVALID_DEVICE_STATE;
        }

        WDF_OBJECT_ATTRIBUTES attributes;
        WDF_OBJECT_ATTRIBUTES_INIT(
            &attributes
        );

        attributes.ParentObject =
            Driver;

        WDFCOLLECTION collection = nullptr;

        NTSTATUS status =
            WdfCollectionCreate(
                &attributes,
                &collection
            );

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        WDFWAITLOCK collectionLock = nullptr;

        status =
            WdfWaitLockCreate(
                &attributes,
                &collectionLock
            );

        if (!NT_SUCCESS(status))
        {
            WdfObjectDelete(
                collection
            );

            return status;
        }

        //
        // Publish only after both lifecycle objects exist.
        //
        g_FilterDeviceCollection =
            collection;

        g_FilterDeviceCollectionLock =
            collectionLock;

        return STATUS_SUCCESS;
    }