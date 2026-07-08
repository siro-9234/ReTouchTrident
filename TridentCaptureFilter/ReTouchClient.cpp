#include "ReTouchClient.h"

#include <initguid.h>

// {7B3F8C21-29F4-4B6A-9C11-2F8A4E72D190}
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
    static HANDLE g_DeviceHandle = nullptr;

    static volatile LONG g_InitializeCount = 0;
    static volatile LONG g_ShutdownCount = 0;
    static volatile LONG g_SubmitFrameCount = 0;
    static volatile LONG g_LastSubmitFrameStatus = 0;
    static volatile LONG g_LastSubmitFrameContactCount = 0;

    static volatile LONG g_QueryInterfaceCount = 0;
    static volatile LONG g_InterfaceFound = 0;
    static volatile LONG g_LastQueryInterfaceStatus = 0;

    static volatile LONG g_OpenCount = 0;
    static volatile LONG g_OpenSucceeded = 0;
    static volatile LONG g_LastOpenStatus = 0;

    static volatile LONG g_TestSubmitCount = 0;
    static volatile LONG g_TestSubmitSucceeded = 0;
    static volatile LONG g_LastTestSubmitStatus = 0;
    static volatile LONG g_TestSubmitIssued = 0;

    static RETOUCH_FRAME g_TestFrame = {};

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

        InterlockedIncrement(&g_QueryInterfaceCount);

        NTSTATUS status = IoGetDeviceInterfaces(
            &GUID_DEVINTERFACE_RETOUCH,
            nullptr,
            DEVICE_INTERFACE_INCLUDE_NONACTIVE,
            SymbolicLinkList
        );

        InterlockedExchange(&g_LastQueryInterfaceStatus, status);

        if (NT_SUCCESS(status) &&
            *SymbolicLinkList != nullptr &&
            (*SymbolicLinkList)[0] != UNICODE_NULL)
        {
            InterlockedExchange(&g_InterfaceFound, 1);
        }
        else
        {
            InterlockedExchange(&g_InterfaceFound, 0);
        }

        return status;
    }

    static
        NTSTATUS
        OpenFirstInterface(
            _In_z_ PWSTR SymbolicLink
        )
    {
        InterlockedIncrement(&g_OpenCount);

        if (SymbolicLink == nullptr || SymbolicLink[0] == UNICODE_NULL)
        {
            InterlockedExchange(&g_OpenSucceeded, 0);
            InterlockedExchange(&g_LastOpenStatus, STATUS_OBJECT_NAME_NOT_FOUND);
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        if (g_DeviceHandle != nullptr)
        {
            ZwClose(g_DeviceHandle);
            g_DeviceHandle = nullptr;
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

        InterlockedExchange(&g_LastOpenStatus, status);

        if (NT_SUCCESS(status))
        {
            g_DeviceHandle = handle;
            InterlockedExchange(&g_OpenSucceeded, 1);
        }
        else
        {
            InterlockedExchange(&g_OpenSucceeded, 0);

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
        InterlockedIncrement(&g_TestSubmitCount);

        if (g_DeviceHandle == nullptr)
        {
            InterlockedExchange(&g_TestSubmitSucceeded, 0);
            InterlockedExchange(&g_LastTestSubmitStatus, STATUS_DEVICE_NOT_READY);
            return STATUS_DEVICE_NOT_READY;
        }

        RtlZeroMemory(&g_TestFrame, sizeof(g_TestFrame));
        g_TestFrame.ContactCount = 0;

        IO_STATUS_BLOCK ioStatus = {};

        const ULONG ioctlCode =
            static_cast<ULONG>(IOCTL_RETOUCH_SUBMIT_FRAME);

        const ULONG inputLength =
            static_cast<ULONG>(sizeof(g_TestFrame));

        NTSTATUS status = ZwDeviceIoControlFile(
            g_DeviceHandle,
            nullptr,
            nullptr,
            nullptr,
            &ioStatus,
            ioctlCode,
            &g_TestFrame,
            inputLength,
            nullptr,
            0UL
        );

        InterlockedExchange(&g_LastTestSubmitStatus, status);

        if (NT_SUCCESS(status))
        {
            InterlockedExchange(&g_TestSubmitSucceeded, 1);
        }
        else
        {
            InterlockedExchange(&g_TestSubmitSucceeded, 0);
        }

        return status;
    }

    NTSTATUS Initialize()
    {
        InterlockedIncrement(&g_InitializeCount);

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
            LONG alreadyIssued = InterlockedCompareExchange(
                &g_TestSubmitIssued,
                1,
                0
            );

            if (alreadyIssued == 0)
            {
                SubmitOneTestFrame();
            }
        }

        return status;
    }

    VOID Shutdown()
    {
        InterlockedIncrement(&g_ShutdownCount);

        if (g_DeviceHandle != nullptr)
        {
            ZwClose(g_DeviceHandle);
            g_DeviceHandle = nullptr;
        }

        InterlockedExchange(&g_OpenSucceeded, 0);
    }

    NTSTATUS SubmitFrame(
        _In_ PRETOUCH_FRAME Frame
    )
    {
        InterlockedIncrement(&g_SubmitFrameCount);

        if (Frame == nullptr)
        {
            InterlockedExchange(&g_LastSubmitFrameStatus, STATUS_INVALID_PARAMETER);
            InterlockedExchange(&g_LastSubmitFrameContactCount, 0);
            return STATUS_INVALID_PARAMETER;
        }

        InterlockedExchange(&g_LastSubmitFrameStatus, STATUS_SUCCESS);
        InterlockedExchange(&g_LastSubmitFrameContactCount, Frame->ContactCount);

        return STATUS_SUCCESS;
    }

    LONG GetInitializeCount() { return InterlockedCompareExchange(&g_InitializeCount, 0, 0); }
    LONG GetShutdownCount() { return InterlockedCompareExchange(&g_ShutdownCount, 0, 0); }
    LONG GetSubmitFrameCount() { return InterlockedCompareExchange(&g_SubmitFrameCount, 0, 0); }
    LONG GetLastSubmitFrameStatus() { return InterlockedCompareExchange(&g_LastSubmitFrameStatus, 0, 0); }
    LONG GetLastSubmitFrameContactCount() { return InterlockedCompareExchange(&g_LastSubmitFrameContactCount, 0, 0); }

    LONG GetQueryInterfaceCount() { return InterlockedCompareExchange(&g_QueryInterfaceCount, 0, 0); }
    LONG GetInterfaceFound() { return InterlockedCompareExchange(&g_InterfaceFound, 0, 0); }
    LONG GetLastQueryInterfaceStatus() { return InterlockedCompareExchange(&g_LastQueryInterfaceStatus, 0, 0); }

    LONG GetOpenCount() { return InterlockedCompareExchange(&g_OpenCount, 0, 0); }
    LONG GetOpenSucceeded() { return InterlockedCompareExchange(&g_OpenSucceeded, 0, 0); }
    LONG GetLastOpenStatus() { return InterlockedCompareExchange(&g_LastOpenStatus, 0, 0); }

    LONG GetTestSubmitCount() { return InterlockedCompareExchange(&g_TestSubmitCount, 0, 0); }
    LONG GetTestSubmitSucceeded() { return InterlockedCompareExchange(&g_TestSubmitSucceeded, 0, 0); }
    LONG GetLastTestSubmitStatus() { return InterlockedCompareExchange(&g_LastTestSubmitStatus, 0, 0); }
}