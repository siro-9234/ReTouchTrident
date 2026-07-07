#pragma once

#include <ntddk.h>

#include "Queue.h"

namespace ReTouchClient
{
    NTSTATUS Initialize();

    VOID Shutdown();

    NTSTATUS SubmitFrame(
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
}