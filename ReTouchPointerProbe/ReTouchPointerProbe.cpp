#include <windows.h>
#include <windowsx.h>
#include <cstdio>

struct CursorSnapshot
{
    LARGE_INTEGER Counter;
    LARGE_INTEGER Frequency;
    POINT Position;
    DWORD Flags;
    HWND ForegroundWindow;
};

static CursorSnapshot g_PreviousSnapshot = {};
static bool g_HasPreviousSnapshot = false;

static const char* MessageName(UINT message)
{
    switch (message)
    {
    case WM_POINTERACTIVATE: return "WM_POINTERACTIVATE";
    case WM_POINTERDOWN: return "WM_POINTERDOWN";
    case WM_POINTERUPDATE: return "WM_POINTERUPDATE";
    case WM_POINTERUP: return "WM_POINTERUP";
    case WM_POINTERCAPTURECHANGED: return "WM_POINTERCAPTURECHANGED";
    case WM_MOUSEACTIVATE: return "WM_MOUSEACTIVATE";
    case WM_MOUSEMOVE: return "WM_MOUSEMOVE";
    case WM_LBUTTONDOWN: return "WM_LBUTTONDOWN";
    case WM_LBUTTONUP: return "WM_LBUTTONUP";
    case WM_ACTIVATE: return "WM_ACTIVATE";
    case WM_SETFOCUS: return "WM_SETFOCUS";
    case WM_KILLFOCUS: return "WM_KILLFOCUS";
    default: return "UNKNOWN";
    }
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

    if (!QueryPerformanceFrequency(&snapshot->Frequency))
    {
        return false;
    }

    if (!QueryPerformanceCounter(&snapshot->Counter))
    {
        return false;
    }

    snapshot->Position = cursorInfo.ptScreenPos;
    snapshot->Flags = cursorInfo.flags;
    snapshot->ForegroundWindow = GetForegroundWindow();

    return true;
}

static void PrintQpcPrefix(const LARGE_INTEGER& counter, const LARGE_INTEGER& frequency)
{
    std::printf("[QPC=%lld Freq=%lld] ",
        counter.QuadPart,
        frequency.QuadPart);
}

static void PrintCursorSnapshot(const char* label, const CursorSnapshot& snapshot)
{
    PrintQpcPrefix(snapshot.Counter, snapshot.Frequency);

    std::printf("%s Cursor=(%ld,%ld) CursorFlags=0x%08lX ForegroundHWND=0x%p\n",
        label,
        snapshot.Position.x,
        snapshot.Position.y,
        snapshot.Flags,
        snapshot.ForegroundWindow);
}

static void PrintMessageEvent(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    LARGE_INTEGER counter = {};
    LARGE_INTEGER frequency = {};

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    POINT point = {};
    point.x = GET_X_LPARAM(lParam);
    point.y = GET_Y_LPARAM(lParam);

    PrintQpcPrefix(counter, frequency);

    std::printf(
        "Message=%s(0x%04X) hwnd=0x%p wParam=0x%p lParam=0x%p Point=(%ld,%ld)\n",
        MessageName(message),
        message,
        hwnd,
        reinterpret_cast<void*>(wParam),
        reinterpret_cast<void*>(lParam),
        point.x,
        point.y);
}

static void PollCursorAndKeys(HWND hwnd)
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

    if (!g_HasPreviousSnapshot)
    {
        g_PreviousSnapshot = current;
        g_HasPreviousSnapshot = true;
        PrintCursorSnapshot("START", current);
        return;
    }

    if (g_PreviousSnapshot.Position.x != current.Position.x ||
        g_PreviousSnapshot.Position.y != current.Position.y)
    {
        PrintQpcPrefix(current.Counter, current.Frequency);

        std::printf("CursorPos : (%ld,%ld) -> (%ld,%ld) CursorFlags=0x%08lX\n",
            g_PreviousSnapshot.Position.x,
            g_PreviousSnapshot.Position.y,
            current.Position.x,
            current.Position.y,
            current.Flags);
    }

    if (g_PreviousSnapshot.Flags != current.Flags)
    {
        PrintQpcPrefix(current.Counter, current.Frequency);

        std::printf("CursorFlags : 0x%08lX -> 0x%08lX Cursor=(%ld,%ld)\n",
            g_PreviousSnapshot.Flags,
            current.Flags,
            current.Position.x,
            current.Position.y);
    }

    if (g_PreviousSnapshot.ForegroundWindow != current.ForegroundWindow)
    {
        PrintQpcPrefix(current.Counter, current.Frequency);

        std::printf("ForegroundChanged : 0x%p -> 0x%p Cursor=(%ld,%ld)\n",
            g_PreviousSnapshot.ForegroundWindow,
            current.ForegroundWindow,
            current.Position.x,
            current.Position.y);
    }

    g_PreviousSnapshot = current;

    UNREFERENCED_PARAMETER(hwnd);
}

static LRESULT CALLBACK ReTouchPointerProbeWndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        SetTimer(hwnd, 1, 5, nullptr);
        return 0;

    case WM_TIMER:
        PollCursorAndKeys(hwnd);
        return 0;

    case WM_POINTERACTIVATE:
        PrintMessageEvent(hwnd, message, wParam, lParam);
        return PA_ACTIVATE;

    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP:
    case WM_POINTERCAPTURECHANGED:
        PrintMessageEvent(hwnd, message, wParam, lParam);
        return 0;

    case WM_MOUSEACTIVATE:
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_ACTIVATE:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        PrintMessageEvent(hwnd, message, wParam, lParam);
        break;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

static bool RegisterProbeWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.lpfnWndProc = ReTouchPointerProbeWndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"ReTouchPointerProbeWindowClass";
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    return RegisterClassExW(&windowClass) != 0;
}

static HWND CreateProbeWindow(HINSTANCE instance)
{
    return CreateWindowExW(
        0,
        L"ReTouchPointerProbeWindowClass",
        L"ReTouchPointerProbe - touch inside this window",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        360,
        nullptr,
        nullptr,
        instance,
        nullptr);
}

int main()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);

    std::printf("ReTouchPointerProbe started.\n");
    std::printf("Touch inside the ReTouchPointerProbe window.\n");
    std::printf("Press F8 to mark.\n");
    std::printf("Press ESC to exit.\n\n");

    if (!RegisterProbeWindowClass(instance))
    {
        std::printf("RegisterClassExW failed. GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HWND hwnd = CreateProbeWindow(instance);

    if (hwnd == nullptr)
    {
        std::printf("CreateWindowExW failed. GetLastError=%lu\n", GetLastError());
        return 1;
    }

    MSG message = {};

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    std::printf("ReTouchPointerProbe stopped.\n");
    return 0;
}