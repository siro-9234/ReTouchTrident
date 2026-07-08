#pragma once

#include <ntddk.h>
#include <wdf.h>

#include "Queue.h"

namespace ReTouchClient
{
    struct RETOUCH_CLIENT_CONTEXT
    {
        HANDLE DeviceHandle;
        WDFDEVICE Device;
        WDFWORKITEM WorkItem;

        RETOUCH_FRAME LatestFrame;
        KSPIN_LOCK FrameLock;

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

    extern RETOUCH_CLIENT_CONTEXT g_Client;

    VOID ReTouchClientWorkItem(
        _In_ WDFWORKITEM WorkItem
    );

    NTSTATUS CreateWorkItem(
        _In_ WDFDEVICE Device
    );
}