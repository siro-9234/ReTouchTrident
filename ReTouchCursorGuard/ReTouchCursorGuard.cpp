// ReTouchCursorGuard.cpp
#include <windows.h>
#include <cstdio>
#include <cstdint>

struct CursorSnapshot
{
    LARGE_INTEGER Counter;
    LARGE_INTEGER Frequency;
    POINT Position;
    DWORD Flags;
};

static CursorSnapshot g_PreviousCursor = {};
static bool g_HasPreviousCursor = false;

static uint64_t g_RawMouseMoveCount = 0;
static uint64_t g_RawMouseButtonCount = 0;

static POINT g_LastPhysicalCursorPosition = {};
static bool g_HasLastPhysicalCursorPosition = false;

static bool g_IsTouchActive = false;
static bool g_IsTouchRecoveryCooldownActive = false;
static LARGE_INTEGER g_TouchRecoveryBlockUntilQpc = {};

static void PrintQpcPrefix()
{
    LARGE_INTEGER counter = {};
    LARGE_INTEGER frequency = {};

    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);

    std::printf(
        "[QPC=%lld Freq=%lld] ",
        counter.QuadPart,
        frequency.QuadPart
    );
}

static bool GetCursorSnapshot(CursorSnapshot* snapshot)
{
    if (snapshot == nullptr)
    {
        return false;
    }

    CURSORINFO cursorInfo = {};
    cursorInfo.cbSize = sizeof(CURSORINFO);

    if (!GetCursorInfo(&cursorInfo))
    {
        return false;
    }

    if (!QueryPerformanceCounter(&snapshot->Counter))
    {
        return false;
    }

    if (!QueryPerformanceFrequency(&snapshot->Frequency))
    {
        return false;
    }

    snapshot->Position = cursorInfo.ptScreenPos;
    snapshot->Flags = cursorInfo.flags;

    return true;
}

static void PrintCursorSnapshot(
    const char* label,
    const CursorSnapshot& snapshot
)
{
    std::printf(
        "[QPC=%lld Freq=%lld] %s Cursor=(%ld,%ld) CursorFlags=0x%08lX RawMoveCount=%llu RawButtonCount=%llu\n",
        snapshot.Counter.QuadPart,
        snapshot.Frequency.QuadPart,
        label,
        snapshot.Position.x,
        snapshot.Position.y,
        snapshot.Flags,
        static_cast<unsigned long long>(g_RawMouseMoveCount),
        static_cast<unsigned long long>(g_RawMouseButtonCount)
    );
}

static bool RegisterRawMouse(HWND hwnd)
{
    RAWINPUTDEVICE device = {};

    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = RIDEV_INPUTSINK;
    device.hwndTarget = hwnd;

    return RegisterRawInputDevices(
        &device,
        1,
        sizeof(RAWINPUTDEVICE)
    ) != FALSE;
}

static bool IsTouchInputProtectionActive()
{
    if (g_IsTouchActive)
    {
        return true;
    }

    if (!g_IsTouchRecoveryCooldownActive)
    {
        return false;
    }

    LARGE_INTEGER now = {};

    if (!QueryPerformanceCounter(&now))
    {
        return true;
    }

    if (now.QuadPart < g_TouchRecoveryBlockUntilQpc.QuadPart)
    {
        return true;
    }

    g_IsTouchRecoveryCooldownActive = false;

    PrintQpcPrefix();
    std::printf("TouchRecoveryCooldown expired\n");

    return false;
}

static void ApplyCursorRecovery()
{
    if (!g_HasLastPhysicalCursorPosition)
    {
        PrintQpcPrefix();
        std::printf("ApplyCorrection skipped. LastPhysical is invalid.\n");
        return;
    }

    CURSORINFO beforeCursorInfo = {};
    beforeCursorInfo.cbSize = sizeof(CURSORINFO);

    if (GetCursorInfo(&beforeCursorInfo))
    {
        PrintQpcPrefix();
        std::printf(
            "BeforeCorrection CursorFlags=0x%08lX CursorPos=(%ld,%ld) hCursor=%p LastPhysical=(%ld,%ld)\n",
            beforeCursorInfo.flags,
            beforeCursorInfo.ptScreenPos.x,
            beforeCursorInfo.ptScreenPos.y,
            beforeCursorInfo.hCursor,
            g_LastPhysicalCursorPosition.x,
            g_LastPhysicalCursorPosition.y
        );
    }

    SetLastError(0);

    BOOL setResult = SetCursorPos(
        g_LastPhysicalCursorPosition.x,
        g_LastPhysicalCursorPosition.y
    );

    DWORD setError = GetLastError();

    PrintQpcPrefix();

    std::printf(
        "ApplyCorrection SetCursorPos(%ld,%ld) result=%d GetLastError=%lu\n",
        g_LastPhysicalCursorPosition.x,
        g_LastPhysicalCursorPosition.y,
        setResult ? 1 : 0,
        setError
    );

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = 1;
    input.mi.dy = 0;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;

    SetLastError(0);

    UINT sent = SendInput(
        1,
        &input,
        sizeof(INPUT)
    );

    DWORD sendInputError = GetLastError();

    PrintQpcPrefix();

    std::printf(
        "SendInputTest dx=1 dy=0 sent=%u GetLastError=%lu LastPhysical=(%ld,%ld)\n",
        sent,
        sendInputError,
        g_LastPhysicalCursorPosition.x,
        g_LastPhysicalCursorPosition.y
    );

    SetLastError(0);

    BOOL finalSetResult = SetCursorPos(
        g_LastPhysicalCursorPosition.x,
        g_LastPhysicalCursorPosition.y
    );

    DWORD finalSetError = GetLastError();

    CURSORINFO finalCursorInfo = {};
    finalCursorInfo.cbSize = sizeof(CURSORINFO);

    if (GetCursorInfo(&finalCursorInfo))
    {
        PrintQpcPrefix();

        std::printf(
            "FinalCorrection SetCursorPosResult=%d SetCursorPosError=%lu CursorFlags=0x%08lX CursorPos=(%ld,%ld) hCursor=%p LastPhysical=(%ld,%ld)\n",
            finalSetResult ? 1 : 0,
            finalSetError,
            finalCursorInfo.flags,
            finalCursorInfo.ptScreenPos.x,
            finalCursorInfo.ptScreenPos.y,
            finalCursorInfo.hCursor,
            g_LastPhysicalCursorPosition.x,
            g_LastPhysicalCursorPosition.y
        );
    }

    LARGE_INTEGER now = {};
    LARGE_INTEGER frequency = {};

    if (QueryPerformanceCounter(&now) &&
        QueryPerformanceFrequency(&frequency))
    {
        g_TouchRecoveryBlockUntilQpc.QuadPart =
            now.QuadPart + (frequency.QuadPart / 10);

        g_IsTouchRecoveryCooldownActive = true;

        PrintQpcPrefix();
        std::printf(
            "TouchRecoveryCooldown started DurationMs=100 BlockUntilQpc=%lld\n",
            g_TouchRecoveryBlockUntilQpc.QuadPart
        );
    }

    PrintQpcPrefix();
    std::printf("RecoveryFinished\n");
}

static void HandleRawInput(LPARAM lParam)
{
    UINT size = 0;

    UINT result = GetRawInputData(
        reinterpret_cast<HRAWINPUT>(lParam),
        RID_INPUT,
        nullptr,
        &size,
        sizeof(RAWINPUTHEADER)
    );

    if (result == static_cast<UINT>(-1))
    {
        PrintQpcPrefix();
        std::printf("GetRawInputData(size) failed. GetLastError=%lu\n", GetLastError());
        return;
    }

    if (size == 0)
    {
        return;
    }

    BYTE* buffer = new BYTE[size];

    if (buffer == nullptr)
    {
        PrintQpcPrefix();
        std::printf("Raw input allocation failed. Size=%u\n", size);
        return;
    }

    result = GetRawInputData(
        reinterpret_cast<HRAWINPUT>(lParam),
        RID_INPUT,
        buffer,
        &size,
        sizeof(RAWINPUTHEADER)
    );

    if (result == static_cast<UINT>(-1))
    {
        PrintQpcPrefix();
        std::printf("GetRawInputData(data) failed. GetLastError=%lu\n", GetLastError());
        delete[] buffer;
        return;
    }

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);

    if (raw->header.dwType == RIM_TYPEMOUSE)
    {
        const RAWMOUSE& mouse = raw->data.mouse;

        bool moved =
            mouse.lLastX != 0 ||
            mouse.lLastY != 0;

        bool button =
            mouse.usButtonFlags != 0 ||
            mouse.usButtonData != 0;

        bool isAbsolutePromotedMouse =
            (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0 &&
            (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;

        bool isRelativePhysicalMouse =
            mouse.usFlags == MOUSE_MOVE_RELATIVE;

        bool isLeftButtonDown =
            (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0;

        bool isLeftButtonUp =
            (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) != 0;

        bool protectionActive = IsTouchInputProtectionActive();

        PrintQpcPrefix();

        std::printf(
            "RawEnter Flags=0x%04X ButtonFlags=0x%04X "
            "LastX=%ld LastY=%ld TouchActive=%d CooldownActive=%d ProtectionActive=%d\n",
            mouse.usFlags,
            mouse.usButtonFlags,
            mouse.lLastX,
            mouse.lLastY,
            g_IsTouchActive ? 1 : 0,
            g_IsTouchRecoveryCooldownActive ? 1 : 0,
            protectionActive ? 1 : 0
        );

        if (isAbsolutePromotedMouse && isLeftButtonDown)
        {
            g_IsTouchActive = true;

            PrintQpcPrefix();
            std::printf("TouchActive=1 by AbsolutePromoted LeftButtonDown\n");
        }

        if (moved)
        {
            ++g_RawMouseMoveCount;

            if (isAbsolutePromotedMouse)
            {
                if (isLeftButtonUp)
                {
                    PrintQpcPrefix();
                    std::printf("TouchUpTransition detected. Applying recovery before TouchActive=0.\n");

                    ApplyCursorRecovery();

                    g_IsTouchActive = false;

                    PrintQpcPrefix();
                    std::printf("TouchActive=0 by AbsolutePromoted LeftButtonUp. Cooldown remains active if started.\n");
                }

                POINT currentCursor = {};
                BOOL hasCurrentCursor = GetCursorPos(&currentCursor);

                PrintQpcPrefix();

                std::printf(
                    "AbsolutePromotedMouseMove "
                    "Absolute=(%ld,%ld) "
                    "CursorValid=%d Cursor=(%ld,%ld) "
                    "LastPhysicalValid=%d LastPhysical=(%ld,%ld) "
                    "TouchActive=%d CooldownActive=%d "
                    "Flags=0x%04X ButtonFlags=0x%04X MoveCount=%llu\n",
                    mouse.lLastX,
                    mouse.lLastY,
                    hasCurrentCursor ? 1 : 0,
                    currentCursor.x,
                    currentCursor.y,
                    g_HasLastPhysicalCursorPosition ? 1 : 0,
                    g_LastPhysicalCursorPosition.x,
                    g_LastPhysicalCursorPosition.y,
                    g_IsTouchActive ? 1 : 0,
                    g_IsTouchRecoveryCooldownActive ? 1 : 0,
                    mouse.usFlags,
                    mouse.usButtonFlags,
                    static_cast<unsigned long long>(g_RawMouseMoveCount)
                );
            }
            else if (isRelativePhysicalMouse)
            {
                POINT cursorPosition = {};
                BOOL hasCursorPosition = GetCursorPos(&cursorPosition);

                POINT lastPhysicalBefore = g_LastPhysicalCursorPosition;
                bool hadLastPhysicalBefore = g_HasLastPhysicalCursorPosition;

                bool shouldUpdateLastPhysical =
                    !IsTouchInputProtectionActive();

                if (shouldUpdateLastPhysical)
                {
                    if (!g_HasLastPhysicalCursorPosition)
                    {
                        if (hasCursorPosition)
                        {
                            g_LastPhysicalCursorPosition = cursorPosition;
                            g_HasLastPhysicalCursorPosition = true;
                        }
                    }
                    else
                    {
                        g_LastPhysicalCursorPosition.x += mouse.lLastX;
                        g_LastPhysicalCursorPosition.y += mouse.lLastY;
                    }
                }

                PrintQpcPrefix();

                std::printf(
                    "RelativePhysicalMouseMoveByRawDelta "
                    "dx=%ld dy=%ld "
                    "CursorValid=%d Cursor=(%ld,%ld) "
                    "TouchActive=%d CooldownActive=%d "
                    "LastPhysicalUpdate=%d "
                    "LastPhysicalBeforeValid=%d LastPhysicalBefore=(%ld,%ld) "
                    "LastPhysicalAfterValid=%d LastPhysicalAfter=(%ld,%ld) "
                    "Flags=0x%04X ButtonFlags=0x%04X MoveCount=%llu\n",
                    mouse.lLastX,
                    mouse.lLastY,
                    hasCursorPosition ? 1 : 0,
                    cursorPosition.x,
                    cursorPosition.y,
                    g_IsTouchActive ? 1 : 0,
                    g_IsTouchRecoveryCooldownActive ? 1 : 0,
                    shouldUpdateLastPhysical ? 1 : 0,
                    hadLastPhysicalBefore ? 1 : 0,
                    lastPhysicalBefore.x,
                    lastPhysicalBefore.y,
                    g_HasLastPhysicalCursorPosition ? 1 : 0,
                    g_LastPhysicalCursorPosition.x,
                    g_LastPhysicalCursorPosition.y,
                    mouse.usFlags,
                    mouse.usButtonFlags,
                    static_cast<unsigned long long>(g_RawMouseMoveCount)
                );
            }
        }

        if (button)
        {
            ++g_RawMouseButtonCount;

            PrintQpcPrefix();

            std::printf(
                "RawMouseButton "
                "IsRelativePhysical=%d IsAbsolutePromoted=%d "
                "TouchActive=%d CooldownActive=%d "
                "MouseFlags=0x%04X ButtonFlags=0x%04X ButtonData=%hu "
                "ButtonCount=%llu LastPhysicalValid=%d LastPhysical=(%ld,%ld)\n",
                isRelativePhysicalMouse ? 1 : 0,
                isAbsolutePromotedMouse ? 1 : 0,
                g_IsTouchActive ? 1 : 0,
                g_IsTouchRecoveryCooldownActive ? 1 : 0,
                mouse.usFlags,
                mouse.usButtonFlags,
                mouse.usButtonData,
                static_cast<unsigned long long>(g_RawMouseButtonCount),
                g_HasLastPhysicalCursorPosition ? 1 : 0,
                g_LastPhysicalCursorPosition.x,
                g_LastPhysicalCursorPosition.y
            );
        }
    }

    delete[] buffer;
}

static void PollCursorAndKeys()
{
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        PostQuitMessage(0);
        return;
    }

    CursorSnapshot current = {};

    if (!GetCursorSnapshot(&current))
    {
        return;
    }

    if (GetAsyncKeyState(VK_F8) & 0x0001)
    {
        PrintCursorSnapshot("MARK(F8)", current);
    }

    if (!g_HasPreviousCursor)
    {
        g_PreviousCursor = current;
        g_HasPreviousCursor = true;
        PrintCursorSnapshot("START", current);
        return;
    }

    if (g_PreviousCursor.Position.x != current.Position.x ||
        g_PreviousCursor.Position.y != current.Position.y)
    {
        std::printf(
            "[QPC=%lld Freq=%lld] CursorPos : (%ld,%ld) -> (%ld,%ld) CursorFlags=0x%08lX RawMoveCount=%llu RawButtonCount=%llu\n",
            current.Counter.QuadPart,
            current.Frequency.QuadPart,
            g_PreviousCursor.Position.x,
            g_PreviousCursor.Position.y,
            current.Position.x,
            current.Position.y,
            current.Flags,
            static_cast<unsigned long long>(g_RawMouseMoveCount),
            static_cast<unsigned long long>(g_RawMouseButtonCount)
        );
    }

    if (g_PreviousCursor.Flags != current.Flags)
    {
        std::printf(
            "[QPC=%lld Freq=%lld] CursorFlags : 0x%08lX -> 0x%08lX Cursor=(%ld,%ld) RawMoveCount=%llu RawButtonCount=%llu\n",
            current.Counter.QuadPart,
            current.Frequency.QuadPart,
            g_PreviousCursor.Flags,
            current.Flags,
            current.Position.x,
            current.Position.y,
            static_cast<unsigned long long>(g_RawMouseMoveCount),
            static_cast<unsigned long long>(g_RawMouseButtonCount)
        );
    }

    g_PreviousCursor = current;
}

static LRESULT CALLBACK ReTouchCursorGuardWndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (message)
    {
    case WM_CREATE:
        if (!RegisterRawMouse(hwnd))
        {
            std::printf("RegisterRawInputDevices failed. GetLastError=%lu\n", GetLastError());
            PostQuitMessage(1);
            return -1;
        }

        SetTimer(hwnd, 1, 5, nullptr);
        return 0;

    case WM_INPUT:
        HandleRawInput(lParam);
        return 0;

    case WM_TIMER:
        PollCursorAndKeys();
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

static bool RegisterGuardWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.lpfnWndProc = ReTouchCursorGuardWndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"ReTouchCursorGuardWindowClass";

    return RegisterClassExW(&windowClass) != 0;
}

static HWND CreateGuardWindow(HINSTANCE instance)
{
    return CreateWindowExW(
        0,
        L"ReTouchCursorGuardWindowClass",
        L"ReTouchCursorGuard v0",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        320,
        180,
        nullptr,
        nullptr,
        instance,
        nullptr
    );
}

int main()
{
    std::printf("ReTouchCursorGuard v0 started.\n");
    std::printf("Observation only. SetCursorPos is NOT used.\n");
    std::printf("Press F8 to mark.\n");
    std::printf("Press ESC to exit.\n\n");

    HINSTANCE instance = GetModuleHandleW(nullptr);

    if (!RegisterGuardWindowClass(instance))
    {
        std::printf("RegisterClassExW failed. GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateGuardWindow(instance);

    if (hwnd == nullptr)
    {
        std::printf("CreateWindowExW failed. GetLastError=%lu\n", GetLastError());
        return 1;
    }

    ShowWindow(hwnd, SW_HIDE);

    MSG message = {};

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    std::printf("ReTouchCursorGuard stopped.\n");
    return 0;
}