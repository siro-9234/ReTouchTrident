#include "ReTouchClient.h"

namespace ReTouchClient
{
    static volatile LONG g_InitializeCount = 0;
    static volatile LONG g_ShutdownCount = 0;
    static volatile LONG g_SubmitFrameCount = 0;
    static volatile LONG g_LastSubmitFrameStatus = 0;
    static volatile LONG g_LastSubmitFrameContactCount = 0;

    NTSTATUS Initialize()
    {
        InterlockedIncrement(&g_InitializeCount);

        return STATUS_SUCCESS;
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
}