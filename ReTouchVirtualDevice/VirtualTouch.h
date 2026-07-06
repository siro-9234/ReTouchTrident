#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <vhf.h>

#include "TouchReport.h"
#include "Ioctl.h"

class VirtualTouch
{
public:
    static NTSTATUS Initialize(WDFDEVICE Device);
    static void Shutdown();

    static NTSTATUS SubmitTouch(
        UCHAR contactId,
        USHORT x,
        USHORT y,
        BOOLEAN touching
    );

    static NTSTATUS SubmitFrame(
        PRETOUCH_FRAME frame
    );

private:
    static VHFHANDLE m_VhfHandle;
};