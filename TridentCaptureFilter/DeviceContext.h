#pragma once

#include <ntddk.h>
#include <wdf.h>

#include "Queue.h"

namespace ReTouchClient
{
    typedef struct _CLIENT_CONTEXT
    {
        volatile LONG ClientInstanceId;

        HANDLE DeviceHandle;
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

        volatile LONG DebugClientPointerLow;

    } CLIENT_CONTEXT, * PCLIENT_CONTEXT;
}

typedef struct _DEVICE_CONTEXT
{
    volatile LONG DeviceInstanceId;

    volatile LONG PhysicalDeviceObjectLow;
    volatile LONG WdmDeviceObjectLow;

    ReTouchClient::CLIENT_CONTEXT ReTouchClient;

} DEVICE_CONTEXT, * PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    DEVICE_CONTEXT,
    DeviceGetContext
);