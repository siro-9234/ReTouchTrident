#pragma once

#include <ntddk.h>
#include <wdf.h>

#include "DeviceContext.h"

namespace ReTouchClient
{
    NTSTATUS Initialize(_Inout_ PCLIENT_CONTEXT Client);
    VOID Shutdown(_Inout_ PCLIENT_CONTEXT Client);

    NTSTATUS CreateWorkItem(
        _Inout_ PCLIENT_CONTEXT Client,
        _In_ WDFDEVICE Device
    );

    VOID ReTouchClientWorkItem(
        _In_ WDFWORKITEM WorkItem
    );

    NTSTATUS SubmitFrame(
        _Inout_ PCLIENT_CONTEXT Client,
        _In_ PRETOUCH_FRAME Frame
    );

    NTSTATUS SendFrameToDevice(
        _Inout_ PCLIENT_CONTEXT Client,
        _In_ PRETOUCH_FRAME Frame
    );

    LONG GetInitializeCount(_In_ PCLIENT_CONTEXT Client);
    LONG GetShutdownCount(_In_ PCLIENT_CONTEXT Client);

    LONG GetSubmitFrameCount(_In_ PCLIENT_CONTEXT Client);
    LONG GetLastSubmitFrameStatus(_In_ PCLIENT_CONTEXT Client);
    LONG GetLastSubmitFrameContactCount(_In_ PCLIENT_CONTEXT Client);

    LONG GetQueryInterfaceCount(_In_ PCLIENT_CONTEXT Client);
    LONG GetInterfaceFound(_In_ PCLIENT_CONTEXT Client);
    LONG GetLastQueryInterfaceStatus(_In_ PCLIENT_CONTEXT Client);

    LONG GetOpenCount(_In_ PCLIENT_CONTEXT Client);
    LONG GetOpenSucceeded(_In_ PCLIENT_CONTEXT Client);
    LONG GetLastOpenStatus(_In_ PCLIENT_CONTEXT Client);

    LONG GetTestSubmitCount(_In_ PCLIENT_CONTEXT Client);
    LONG GetTestSubmitSucceeded(_In_ PCLIENT_CONTEXT Client);
    LONG GetLastTestSubmitStatus(_In_ PCLIENT_CONTEXT Client);

    LONG GetWorkItemEnqueueCount(_In_ PCLIENT_CONTEXT Client);
    LONG GetWorkItemRunCount(_In_ PCLIENT_CONTEXT Client);
    LONG GetLastWorkItemContactCount(_In_ PCLIENT_CONTEXT Client);
}