#include "pch.h"
#include "TridentInputHook.h"

namespace
{
    static HHOOK g_CbtHook = nullptr;

    static TridentHookSharedState*
        g_SharedState = nullptr;

    static TridentHookSharedState*
        GetSharedState()
    {
        TridentHookSharedState* currentState =
            static_cast<TridentHookSharedState*>(
                InterlockedCompareExchangePointer(
                    reinterpret_cast<
                    PVOID volatile*
                    >(
                        &g_SharedState
                        ),
                    nullptr,
                    nullptr
                )
                );

        if (currentState != nullptr)
        {
            return currentState;
        }

        SetLastError(0);

        HANDLE mappingHandle =
            OpenFileMappingW(
                FILE_MAP_WRITE,
                FALSE,
                TRIDENT_INPUT_HOOK_SHARED_MEMORY_NAME
            );

        if (mappingHandle == nullptr)
        {
            return nullptr;
        }

        SetLastError(0);

        void* mappedView =
            MapViewOfFile(
                mappingHandle,
                FILE_MAP_WRITE,
                0,
                0,
                sizeof(
                    TridentHookSharedState
                    )
            );

        CloseHandle(
            mappingHandle
        );

        mappingHandle =
            nullptr;

        if (mappedView == nullptr)
        {
            return nullptr;
        }

        TridentHookSharedState* mappedState =
            static_cast<
            TridentHookSharedState*
            >(
                mappedView
                );

        const bool hasValidLayout =
            mappedState->Signature ==
            TRIDENT_INPUT_HOOK_SHARED_SIGNATURE &&
            mappedState->Version ==
            TRIDENT_INPUT_HOOK_SHARED_VERSION &&
            mappedState->Capacity ==
            TRIDENT_INPUT_HOOK_EVENT_CAPACITY;

        if (!hasValidLayout)
        {
            UnmapViewOfFile(
                mappedView
            );

            return nullptr;
        }

        TridentHookSharedState* installedState =
            static_cast<TridentHookSharedState*>(
                InterlockedCompareExchangePointer(
                    reinterpret_cast<
                    PVOID volatile*
                    >(
                        &g_SharedState
                        ),
                    mappedState,
                    nullptr
                )
                );

        if (installedState != nullptr)
        {
            UnmapViewOfFile(
                mappedState
            );

            return installedState;
        }

        return mappedState;
    }

    static void WriteHookEvent(
        TridentHookEventType eventType,
        int hookCode,
        HWND targetWindow,
        HWND otherWindow,
        bool mouseActivation
    )
    {
        TridentHookSharedState* sharedState =
            GetSharedState();

        if (sharedState == nullptr)
        {
            return;
        }

        LARGE_INTEGER qpc = {};

        if (!QueryPerformanceCounter(
            &qpc
        ))
        {
            qpc.QuadPart = 0;
        }

        CURSORINFO cursorInfo = {};
        cursorInfo.cbSize =
            sizeof(CURSORINFO);

        BOOL cursorInfoResult =
            GetCursorInfo(
                &cursorInfo
            );

        const LONG64 sequence =
            InterlockedIncrement64(
                &sharedState->WriteSequence
            );

        const std::uint32_t slotIndex =
            static_cast<std::uint32_t>(
                (sequence - 1) %
                TRIDENT_INPUT_HOOK_EVENT_CAPACITY
                );

        TridentHookEvent* event =
            &sharedState->Events[
                slotIndex
            ];

        InterlockedExchange64(
            &event->CommittedSequence,
            0
        );

        event->Qpc =
            qpc.QuadPart;

        event->EventType =
            static_cast<std::uint32_t>(
                eventType
                );

        event->HookCode =
            hookCode;

        event->TargetHwnd =
            static_cast<std::uint64_t>(
                reinterpret_cast<
                ULONG_PTR
                >(
                    targetWindow
                    )
                );

        event->OtherHwnd =
            static_cast<std::uint64_t>(
                reinterpret_cast<
                ULONG_PTR
                >(
                    otherWindow
                    )
                );

        event->ThreadId =
            GetCurrentThreadId();

        event->ProcessId =
            GetCurrentProcessId();

        event->MouseActivation =
            mouseActivation ? 1u : 0u;

        event->CursorInfoValid =
            cursorInfoResult ? 1u : 0u;

        if (cursorInfoResult)
        {
            event->CursorFlags =
                cursorInfo.flags;

            event->CursorX =
                cursorInfo.ptScreenPos.x;

            event->CursorY =
                cursorInfo.ptScreenPos.y;
        }
        else
        {
            event->CursorFlags = 0;
            event->CursorX = 0;
            event->CursorY = 0;
        }

        MemoryBarrier();

        InterlockedExchange64(
            &event->CommittedSequence,
            sequence
        );
    }

    static LRESULT CALLBACK TridentCbtProc(
        int code,
        WPARAM wParam,
        LPARAM lParam
    )
    {
        if (code < 0)
        {
            return CallNextHookEx(
                nullptr,
                code,
                wParam,
                lParam
            );
        }

        switch (code)
        {
        case HCBT_ACTIVATE:
        {
            HWND targetWindow =
                reinterpret_cast<HWND>(
                    wParam
                    );

            HWND previouslyActiveWindow =
                nullptr;

            bool mouseActivation =
                false;

            const CBTACTIVATESTRUCT*
                activateInfo =
                reinterpret_cast<
                const CBTACTIVATESTRUCT*
                >(
                    lParam
                    );

            if (activateInfo != nullptr)
            {
                previouslyActiveWindow =
                    activateInfo->hWndActive;

                mouseActivation =
                    activateInfo->fMouse != FALSE;
            }

            WriteHookEvent(
                TridentHookEventType::
                CbtActivate,
                code,
                targetWindow,
                previouslyActiveWindow,
                mouseActivation
            );

            break;
        }

        case HCBT_SETFOCUS:
        {
            HWND receivingFocusWindow =
                reinterpret_cast<HWND>(
                    wParam
                    );

            HWND losingFocusWindow =
                reinterpret_cast<HWND>(
                    lParam
                    );

            WriteHookEvent(
                TridentHookEventType::
                CbtSetFocus,
                code,
                receivingFocusWindow,
                losingFocusWindow,
                false
            );

            break;
        }

        default:
            break;
        }

        return CallNextHookEx(
            nullptr,
            code,
            wParam,
            lParam
        );
    }

    static HMODULE GetThisModuleHandle()
    {
        HMODULE moduleHandle =
            nullptr;

        BOOL result =
            GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(
                    &TridentCbtProc
                    ),
                &moduleHandle
            );

        if (!result)
        {
            return nullptr;
        }

        return moduleHandle;
    }
}

extern "C"
__declspec(dllexport)
BOOL WINAPI TridentInstallCbtHook()
{
    if (g_CbtHook != nullptr)
    {
        return TRUE;
    }

    HMODULE moduleHandle =
        GetThisModuleHandle();

    if (moduleHandle == nullptr)
    {
        return FALSE;
    }

    HHOOK hook =
        SetWindowsHookExW(
            WH_CBT,
            TridentCbtProc,
            moduleHandle,
            0
        );

    if (hook == nullptr)
    {
        return FALSE;
    }

    g_CbtHook = hook;

    return TRUE;
}

extern "C"
__declspec(dllexport)
BOOL WINAPI TridentUninstallCbtHook()
{
    if (g_CbtHook == nullptr)
    {
        return TRUE;
    }

    HHOOK hook =
        g_CbtHook;

    if (!UnhookWindowsHookEx(
        hook
    ))
    {
        return FALSE;
    }

    g_CbtHook = nullptr;

    return TRUE;
}