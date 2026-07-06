#pragma once

#include <ntddk.h>
#include <wdf.h>

NTSTATUS QueueInitialize(
    WDFDEVICE Device
);