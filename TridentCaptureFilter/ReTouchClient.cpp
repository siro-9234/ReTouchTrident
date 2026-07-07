#include "ReTouchClient.h"

#include <initguid.h>

// {7B3F8C21-29F4-4B6A-9C11-2F8A4E72D190}
DEFINE_GUID(
    GUID_DEVINTERFACE_RETOUCH,
    0x7b3f8c21, 0x29f4, 0x4b6a,
    0x9c, 0x11, 0x2f, 0x8a, 0x4e, 0x72, 0xd1, 0x90
);

namespace ReTouchClient
{
    static volatile LONG g_InitializeCount = 0;
    static volatile LONG g_ShutdownCount = 0;
    static volatile LONG g_SubmitFrameCount = 0;
    static volatile LONG g_LastSubmitFrameStatus = 0;
    static volatile LONG g_LastSubmitFrameContactCount = 0;

    static volatile LONG g_QueryInterfaceCount = 0;
    static volatile LONG g_InterfaceFound = 0;
    static volatile LONG g_LastQueryInterfaceStatus = 0;

    static
        NTSTATUS
        QueryInterface()
    {
        InterlockedIncrement(&g_QueryInterfaceCount);

        PWSTR symbolicLinkList = nullptr;

        NTSTATUS status = IoGetDeviceInterfaces(
            &GUID_DEVINTERFACE_RETOUCH,
            nullptr,
            DEVICE_INTERFACE_INCLUDE_NONACTIVE,
            &symbolicLinkList
        );

        InterlockedExchange(&g_LastQueryInterfaceStatus, status);

        if (NT_SUCCESS(status) &&
            symbolicLinkList != nullptr &&
            symbolicLinkList[0] != UNICODE_NULL)
        {
            InterlockedExchange(&g_InterfaceFound, 1);
        }
        else
        {
            InterlockedExchange(&g_InterfaceFound, 0);
        }

        if (symbolicLinkList != nullptr)
        {
            ExFreePool(symbolicLinkList);
        }

        return status;
    }

    NTSTATUS Initialize()
    {
        InterlockedIncrement(&g_InitializeCount);

        return QueryInterface();
    }

    VOID Shutdown()
    {
        InterlockedIncrement(&g_ShutdownCount);
    }

    NTSTATUS SubmitFrame(
        _In_ PRETOUCH_FRAME Frame
    )
    {
        InterlockedIncrement(&g_SubmitFrameCount);

        if (Frame == nullptr)
        {
            InterlockedExchange(
                &g_LastSubmitFrameStatus,
                STATUS_INVALID_PARAMETER
            );

            InterlockedExchange(
                &g_LastSubmitFrameContactCount,
                0
            );

            return STATUS_INVALID_PARAMETER;
        }

        InterlockedExchange(
            &g_LastSubmitFrameStatus,
            STATUS_SUCCESS
        );

        InterlockedExchange(
            &g_LastSubmitFrameContactCount,
            Frame->ContactCount
        );

        return STATUS_SUCCESS;
    }

    LONG GetInitializeCount()
    {
        return InterlockedCompareExchange(&g_InitializeCount, 0, 0);
    }

    LONG GetShutdownCount()
    {
        return InterlockedCompareExchange(&g_ShutdownCount, 0, 0);
    }

    LONG GetSubmitFrameCount()
    {
        return InterlockedCompareExchange(&g_SubmitFrameCount, 0, 0);
    }

    LONG GetLastSubmitFrameStatus()
    {
        return InterlockedCompareExchange(&g_LastSubmitFrameStatus, 0, 0);
    }

    LONG GetLastSubmitFrameContactCount()
    {
        return InterlockedCompareExchange(&g_LastSubmitFrameContactCount, 0, 0);
    }

    LONG GetQueryInterfaceCount()
    {
        return InterlockedCompareExchange(&g_QueryInterfaceCount, 0, 0);
    }

    LONG GetInterfaceFound()
    {
        return InterlockedCompareExchange(&g_InterfaceFound, 0, 0);
    }

    LONG GetLastQueryInterfaceStatus()
    {
        return InterlockedCompareExchange(&g_LastQueryInterfaceStatus, 0, 0);
    }
}