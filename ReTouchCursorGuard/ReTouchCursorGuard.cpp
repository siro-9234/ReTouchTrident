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

        if (moved)
        {
            ++g_RawMouseMoveCount;

            PrintQpcPrefix();

            if (isAbsolutePromotedMouse)
            {
                POINT currentCursor = {};
                GetCursorPos(&currentCursor);

                if (mouse.usButtonFlags == RI_MOUSE_LEFT_BUTTON_UP &&
                    g_HasLastPhysicalCursorPosition)
                {
                    PrintQpcPrefix();

                    std::printf(
                        "ApplyCorrection SetCursorPos(%ld,%ld)\n",
                        g_LastPhysicalCursorPosition.x,
                        g_LastPhysicalCursorPosition.y
                    );

                    SetCursorPos(
                        g_LastPhysicalCursorPosition.x,
                        g_LastPhysicalCursorPosition.y
                    );

                    GetCursorPos(&currentCursor);
                }

                LONG deltaX =
                    currentCursor.x - g_LastPhysicalCursorPosition.x;

                LONG deltaY =
                    currentCursor.y - g_LastPhysicalCursorPosition.y;

                std::printf(
                    "CorrectionCandidate "
                    "Absolute=(%ld,%ld) "
                    "CurrentCursor=(%ld,%ld) "
                    "LastPhysical=(%ld,%ld) "
                    "Delta=(%ld,%ld) "
                    "Flags=0x%04X "
                    "ButtonFlags=0x%04X "
                    "MoveCount=%llu\n",
                    mouse.lLastX,
                    mouse.lLastY,
                    currentCursor.x,
                    currentCursor.y,
                    g_LastPhysicalCursorPosition.x,
                    g_LastPhysicalCursorPosition.y,
                    deltaX,
                    deltaY,
                    mouse.usFlags,
                    mouse.usButtonFlags,
                    static_cast<unsigned long long>(g_RawMouseMoveCount)
                );
            }
            else if (isRelativePhysicalMouse)
            {
                POINT cursorPosition = {};

                if (GetCursorPos(&cursorPosition))
                {
                    g_LastPhysicalCursorPosition = cursorPosition;
                    g_HasLastPhysicalCursorPosition = true;
                }

                std::printf(
                    "RelativePhysicalMouseMove dx=%ld dy=%ld Flags=0x%04X ButtonFlags=0x%04X ButtonData=%hu MoveCount=%llu LastPhysical=(%ld,%ld) LastPhysicalValid=%d\n",
                    mouse.lLastX,
                    mouse.lLastY,
                    mouse.usFlags,
                    mouse.usButtonFlags,
                    mouse.usButtonData,
                    static_cast<unsigned long long>(g_RawMouseMoveCount),
                    g_LastPhysicalCursorPosition.x,
                    g_LastPhysicalCursorPosition.y,
                    g_HasLastPhysicalCursorPosition ? 1 : 0
                );
            }
            else
            {
                std::printf(
                    "OtherMouseMove x=%ld y=%ld Flags=0x%04X ButtonFlags=0x%04X ButtonData=%hu MoveCount=%llu LastPhysicalValid=%d LastPhysical=(%ld,%ld)\n",
                    mouse.lLastX,
                    mouse.lLastY,
                    mouse.usFlags,
                    mouse.usButtonFlags,
                    mouse.usButtonData,
                    static_cast<unsigned long long>(g_RawMouseMoveCount),
                    g_HasLastPhysicalCursorPosition ? 1 : 0,
                    g_LastPhysicalCursorPosition.x,
                    g_LastPhysicalCursorPosition.y
                );
            }
        }

        if (button)
        {
            ++g_RawMouseButtonCount;

            PrintQpcPrefix();

            if (isAbsolutePromotedMouse)
            {
                std::printf(
                    "AbsolutePromotedMouseButton Flags=0x%04X ButtonData=%hu ButtonCount=%llu LastPhysicalValid=%d LastPhysical=(%ld,%ld)\n",
                    mouse.usButtonFlags,
                    mouse.usButtonData,
                    static_cast<unsigned long long>(g_RawMouseButtonCount),
                    g_HasLastPhysicalCursorPosition ? 1 : 0,
                    g_LastPhysicalCursorPosition.x,
                    g_LastPhysicalCursorPosition.y
                );
            }
            else if (isRelativePhysicalMouse)
            {
                std::printf(
                    "RelativePhysicalMouseButton Flags=0x%04X ButtonData=%hu ButtonCount=%llu LastPhysical=(%ld,%ld) LastPhysicalValid=%d\n",
                    mouse.usButtonFlags,
                    mouse.usButtonData,
                    static_cast<unsigned long long>(g_RawMouseButtonCount),
                    g_LastPhysicalCursorPosition.x,
                    g_LastPhysicalCursorPosition.y,
                    g_HasLastPhysicalCursorPosition ? 1 : 0
                );
            }
            else
            {
                std::printf(
                    "OtherMouseButton MouseFlags=0x%04X ButtonFlags=0x%04X ButtonData=%hu ButtonCount=%llu LastPhysicalValid=%d LastPhysical=(%ld,%ld)\n",
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