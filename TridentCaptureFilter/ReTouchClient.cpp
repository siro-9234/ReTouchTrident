#include "ReTouchClient.h"

#include <initguid.h>

DEFINE_GUID(
    GUID_DEVINTERFACE_RETOUCH,
    0x7b3f8c21, 0x29f4, 0x4b6a,
    0x9c, 0x11, 0x2f, 0x8a, 0x4e, 0x72, 0xd1, 0x90
);

#define FILE_DEVICE_RETOUCH 0x8000

#define IOCTL_RETOUCH_SUBMIT_FRAME \
    CTL_CODE(FILE_DEVICE_RETOUCH, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)

namespace ReTouchClient
{
    struct RETOUCH_CLIENT_CONTEXT
    {
        HANDLE DeviceHandle;
        WDFDEVICE Device;
        WDFWORKITEM WorkItem;

        RETOUCH_FRAME LatestFrame;
        FAST_MUTEX FrameLock;
        volatile LONG WorkItemQueued;

        volatile LONG InitializeCount;
        volatile LONG ShutdownCount;

        volatile LONG SubmitFrameCount;
        volatile LONG LastSubmitFrameStatus;
        volatile LONG LastSubmitFrameContactCount;

        volatile LONG QueryInterfaceCount;
        volatile LONG InterfaceFound;
        volatile LONG LastQueryInterfaceStatus;

        volatile LONG OpenCount;
        volatile LONG OpenSucceeded;
        volatile LONG LastOpenStatus;

        volatile LONG TestSubmitCount;
        volatile LONG TestSubmitSucceeded;
        volatile LONG LastTestSubmitStatus;

        volatile LONG WorkItemEnqueueCount;
        volatile LONG WorkItemRunCount;
        volatile LONG LastWorkItemContactCount;
    };

    static RETOUCH_CLIENT_CONTEXT g_Client = {};

    NTSTATUS SendFrameToDevice(
        _In_ PRETOUCH_FRAME Frame
    )
    {
        if (Frame == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (g_Client.DeviceHandle == nullptr)
        {
            return STATUS_DEVICE_NOT_READY;
        }

        IO_STATUS_BLOCK ioStatus = {};

        const ULONG ioctlCode =
            static_cast<ULONG>(IOCTL_RETOUCH_SUBMIT_FRAME);

        const ULONG inputLength =
            static_cast<ULONG>(sizeof(RETOUCH_FRAME));

        return ZwDeviceIoControlFile(
            g_Client.DeviceHandle,
            nullptr,
            nullptr,
            nullptr,
            &ioStatus,
            ioctlCode,
            Frame,
            inputLength,
            nullptr,
            0UL
        );
    }

    static
        NTSTATUS
        QueryInterface(
            _Outptr_result_maybenull_ PWSTR* SymbolicLinkList
        )
    {
        if (SymbolicLinkList == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        *SymbolicLinkList = nullptr;

        InterlockedIncrement(&g_Client.QueryInterfaceCount);

        NTSTATUS status = IoGetDeviceInterfaces(
            &GUID_DEVINTERFACE_RETOUCH,
            nullptr,
            DEVICE_INTERFACE_INCLUDE_NONACTIVE,
            SymbolicLinkList
        );

        InterlockedExchange(&g_Client.LastQueryInterfaceStatus, status);

        if (NT_SUCCESS(status) &&
            *SymbolicLinkList != nullptr &&
            (*SymbolicLinkList)[0] != UNICODE_NULL)
        {
            InterlockedExchange(&g_Client.InterfaceFound, 1);
        }
        else
        {
            InterlockedExchange(&g_Client.InterfaceFound, 0);
        }

        return status;
    }

    static
        NTSTATUS
        OpenFirstInterface(
            _In_z_ PWSTR SymbolicLink
        )
    {
        InterlockedIncrement(&g_Client.OpenCount);

        if (SymbolicLink == nullptr || SymbolicLink[0] == UNICODE_NULL)
        {
            InterlockedExchange(&g_Client.OpenSucceeded, 0);
            InterlockedExchange(&g_Client.LastOpenStatus, STATUS_OBJECT_NAME_NOT_FOUND);
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        if (g_Client.DeviceHandle != nullptr)
        {
            ZwClose(g_Client.DeviceHandle);
            g_Client.DeviceHandle = nullptr;
        }

        UNICODE_STRING objectName;
        RtlInitUnicodeString(&objectName, SymbolicLink);

        OBJECT_ATTRIBUTES objectAttributes;
        InitializeObjectAttributes(
            &objectAttributes,
            &objectName,
            OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
            nullptr,
            nullptr
        );

        IO_STATUS_BLOCK ioStatus = {};
        HANDLE handle = nullptr;

        NTSTATUS status = ZwCreateFile(
            &handle,
            GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
            &objectAttributes,
            &ioStatus,
            nullptr,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_OPEN,
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
            nullptr,
            0
        );

        InterlockedExchange(&g_Client.LastOpenStatus, status);

        if (NT_SUCCESS(status))
        {
            g_Client.DeviceHandle = handle;
            InterlockedExchange(&g_Client.OpenSucceeded, 1);
        }
        else
        {
            InterlockedExchange(&g_Client.OpenSucceeded, 0);

            if (handle != nullptr)
            {
                ZwClose(handle);
            }
        }

        return status;
    }

    static
        NTSTATUS
        SubmitOneTestFrame()
    {
        InterlockedIncrement(&g_Client.TestSubmitCount);

        RETOUCH_FRAME frame = {};
        frame.ContactCount = 0;

        NTSTATUS status = SendFrameToDevice(&frame);

        InterlockedExchange(&g_Client.LastTestSubmitStatus, status);
        InterlockedExchange(&g_Client.TestSubmitSucceeded, NT_SUCCESS(status) ? 1 : 0);

        return status;
    }

    NTSTATUS Initialize(
        _In_ WDFDEVICE Device
    )
    {
        InterlockedIncrement(&g_Client.InitializeCount);

        g_Client.Device = Device;

        ExInitializeFastMutex(&g_Client.FrameLock);

        PWSTR symbolicLinkList = nullptr;

        NTSTATUS status = QueryInterface(&symbolicLinkList);

        if (NT_SUCCESS(status) &&
            symbolicLinkList != nullptr &&
            symbolicLinkList[0] != UNICODE_NULL)
        {
            status = OpenFirstInterface(symbolicLinkList);
        }

        if (symbolicLinkList != nullptr)
        {
            ExFreePool(symbolicLinkList);
        }

        if (NT_SUCCESS(status))
        {
            SubmitOneTestFrame();
        }

        return status;
    }

    VOID Shutdown()
    {
        InterlockedIncrement(&g_Client.ShutdownCount);

        if (g_Client.DeviceHandle != nullptr)
        {
            ZwClose(g_Client.DeviceHandle);
            g_Client.DeviceHandle = nullptr;
        }

        InterlockedExchange(&g_Client.OpenSucceeded, 0);
    }

    NTSTATUS SubmitFrame(
        _In_ PRETOUCH_FRAME Frame
    )
    {
        InterlockedIncrement(&g_Client.SubmitFrameCount);

        if (Frame == nullptr)
        {
            InterlockedExchange(&g_Client.LastSubmitFrameStatus, STATUS_INVALID_PARAMETER);
            InterlockedExchange(&g_Client.LastSubmitFrameContactCount, 0);
            return STATUS_INVALID_PARAMETER;
        }

        InterlockedExchange(&g_Client.LastSubmitFrameStatus, STATUS_SUCCESS);
        InterlockedExchange(&g_Client.LastSubmitFrameContactCount, Frame->ContactCount);

        return STATUS_SUCCESS;
    }

    LONG GetInitializeCount() { return InterlockedCompareExchange(&g_Client.InitializeCount, 0, 0); }
    LONG GetShutdownCount() { return InterlockedCompareExchange(&g_Client.ShutdownCount, 0, 0); }

    LONG GetSubmitFrameCount() { return InterlockedCompareExchange(&g_Client.SubmitFrameCount, 0, 0); }
    LONG GetLastSubmitFrameStatus() { return InterlockedCompareExchange(&g_Client.LastSubmitFrameStatus, 0, 0); }
    LONG GetLastSubmitFrameContactCount() { return InterlockedCompareExchange(&g_Client.LastSubmitFrameContactCount, 0, 0); }

    LONG GetQueryInterfaceCount() { return InterlockedCompareExchange(&g_Client.QueryInterfaceCount, 0, 0); }
    LONG GetInterfaceFound() { return InterlockedCompareExchange(&g_Client.InterfaceFound, 0, 0); }
    LONG GetLastQueryInterfaceStatus() { return InterlockedCompareExchange(&g_Client.LastQueryInterfaceStatus, 0, 0); }

    LONG GetOpenCount() { return InterlockedCompareExchange(&g_Client.OpenCount, 0, 0); }
    LONG GetOpenSucceeded() { return InterlockedCompareExchange(&g_Client.OpenSucceeded, 0, 0); }
    LONG GetLastOpenStatus() { return InterlockedCompareExchange(&g_Client.LastOpenStatus, 0, 0); }

    LONG GetTestSubmitCount() { return InterlockedCompareExchange(&g_Client.TestSubmitCount, 0, 0); }
    LONG GetTestSubmitSucceeded() { return InterlockedCompareExchange(&g_Client.TestSubmitSucceeded, 0, 0); }
    LONG GetLastTestSubmitStatus() { return InterlockedCompareExchange(&g_Client.LastTestSubmitStatus, 0, 0); }

    LONG GetWorkItemEnqueueCount() { return InterlockedCompareExchange(&g_Client.WorkItemEnqueueCount, 0, 0); }
    LONG GetWorkItemRunCount() { return InterlockedCompareExchange(&g_Client.WorkItemRunCount, 0, 0); }
    LONG GetLastWorkItemContactCount() { return InterlockedCompareExchange(&g_Client.LastWorkItemContactCount, 0, 0); }
}