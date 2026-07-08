#include "ReTouchClient.h"
#include "ReTouchClient.Internal.h"

namespace ReTouchClient
{
    VOID ReTouchClientWorkItem(
        _In_ WDFWORKITEM WorkItem
    )
    {
        UNREFERENCED_PARAMETER(WorkItem);

        InterlockedIncrement(&g_Client.WorkItemRunCount);

        RETOUCH_FRAME frame = {};

        KIRQL oldIrql;
        KeAcquireSpinLock(
            &g_Client.FrameLock,
            &oldIrql
        );

        RtlCopyMemory(
            &frame,
            &g_Client.LatestFrame,
            sizeof(frame)
        );

        KeReleaseSpinLock(
            &g_Client.FrameLock,
            oldIrql
        );

        InterlockedExchange(
            &g_Client.LastWorkItemContactCount,
            frame.ContactCount
        );

        InterlockedExchange(
            &g_Client.WorkItemQueued,
            0
        );

        //
        // Mission 07C-1:
        // Do not send yet.
        // Actual ZwDeviceIoControlFile call will be added in the next step.
        //
    }

    NTSTATUS SubmitFrame(
        _In_ PRETOUCH_FRAME Frame
    )
    {
        InterlockedIncrement(&g_Client.SubmitFrameCount);

        if (Frame == nullptr)
        {
            InterlockedExchange(&g_Client.LastSubmitFrameStatus, STATUS_INVALID_PARAMETER);
            InterlockedExchange(&g_Client.LastSubmitFrameContactCount, 0);
            return STATUS_INVALID_PARAMETER;
        }

        InterlockedExchange(&g_Client.LastSubmitFrameStatus, STATUS_SUCCESS);
        InterlockedExchange(&g_Client.LastSubmitFrameContactCount, Frame->ContactCount);

        return STATUS_SUCCESS;
    }
    
}