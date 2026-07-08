#pragma once

#include <ntddk.h>
#include <wdf.h>

#include "Queue.h"

namespace ReTouchClient
{
    NTSTATUS Initialize(
        _In_ WDFDEVICE Device
    );

    VOID Shutdown();

    NTSTATUS SubmitFrame(
        _In_ PRETOUCH_FRAME Frame
    );

    NTSTATUS SendFrameToDevice(
        _In_ PRETOUCH_FRAME Frame
    );

    LONG GetInitializeCount();
    LONG GetShutdownCount();

    LONG GetSubmitFrameCount();
    LONG GetLastSubmitFrameStatus();
    LONG GetLastSubmitFrameContactCount();

    LONG GetQueryInterfaceCount();
    LONG GetInterfaceFound();
    LONG GetLastQueryInterfaceStatus();

    LONG GetOpenCount();
    LONG GetOpenSucceeded();
    LONG GetLastOpenStatus();

    LONG GetTestSubmitCount();
    LONG GetTestSubmitSucceeded();
    LONG GetLastTestSubmitStatus();

    LONG GetWorkItemEnqueueCount();
    LONG GetWorkItemRunCount();
    LONG GetLastWorkItemContactCount();
}