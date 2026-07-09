#pragma once

#include <ntddk.h>
#include <wdf.h>

VOID
ReTouchLoggerInitialize(
    _In_ WDFDEVICE Device
);

VOID
ReTouchLoggerShutdown();

VOID
ReTouchLoggerLogTouchTransition(
    _In_ BOOLEAN IsTouchDown,
    _In_ USHORT X,
    _In_ USHORT Y,
    _In_ UCHAR ContactId
);