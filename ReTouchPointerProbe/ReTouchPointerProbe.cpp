#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <vector>

struct CursorSnapshot
{
    LARGE_INTEGER Counter;
    LARGE_INTEGER Frequency;
    POINT Position;
    DWORD Flags;
    HCURSOR CursorHandle;
    HWND ForegroundWindow;
};

static CursorSnapshot g_PreviousSnapshot = {};
static bool g_HasPreviousSnapshot = false;

static const char* MessageName(UINT message)
{
    switch (message)
    {
    case WM_POINTERENTER:
        return "WM_POINTERENTER";

    case WM_POINTERLEAVE:
        return "WM_POINTERLEAVE";

    case WM_POINTERACTIVATE:
        return "WM_POINTERACTIVATE";

    case WM_POINTERDOWN:
        return "WM_POINTERDOWN";

    case WM_POINTERUPDATE:
        return "WM_POINTERUPDATE";

    case WM_POINTERUP:
        return "WM_POINTERUP";

    case WM_POINTERCAPTURECHANGED:
        return "WM_POINTERCAPTURECHANGED";

    case WM_MOUSEACTIVATE:
        return "WM_MOUSEACTIVATE";

    case WM_MOUSEMOVE:
        return "WM_MOUSEMOVE";

    case WM_LBUTTONDOWN:
        return "WM_LBUTTONDOWN";

    case WM_LBUTTONUP:
        return "WM_LBUTTONUP";

    case WM_ACTIVATE:
        return "WM_ACTIVATE";

    case WM_SETFOCUS:
        return "WM_SETFOCUS";

    case WM_KILLFOCUS:
        return "WM_KILLFOCUS";

    default:
        return "UNKNOWN";
    }
}

static const char* PointerTypeName(POINTER_INPUT_TYPE pointerType)
{
    switch (pointerType)
    {
    case PT_POINTER:
        return "PT_POINTER";

    case PT_TOUCH:
        return "PT_TOUCH";

    case PT_PEN:
        return "PT_PEN";

    case PT_MOUSE:
        return "PT_MOUSE";

    case PT_TOUCHPAD:
        return "PT_TOUCHPAD";

    default:
        return "PT_UNKNOWN";
    }
}

static void PrintQpcPrefix(
    const LARGE_INTEGER& counter,
    const LARGE_INTEGER& frequency
)
{
    std::printf(
        "[QPC=%lld Freq=%lld] ",
        counter.QuadPart,
        frequency.QuadPart
    );
}

static void PrintCurrentQpcPrefix()
{
    LARGE_INTEGER counter = {};
    LARGE_INTEGER frequency = {};

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    PrintQpcPrefix(counter, frequency);
}

static void GetWindowTextForLog(
    HWND hwnd,
    wchar_t* buffer,
    int bufferCount
)
{
    if (buffer == nullptr || bufferCount <= 0)
    {
        return;
    }

    buffer[0] = L'\0';

    if (hwnd != nullptr)
    {
        GetWindowTextW(
            hwnd,
            buffer,
            bufferCount
        );
    }
}

static void GetWindowClassForLog(
    HWND hwnd,
    wchar_t* buffer,
    int bufferCount
)
{
    if (buffer == nullptr || bufferCount <= 0)
    {
        return;
    }

    buffer[0] = L'\0';

    if (hwnd != nullptr)
    {
        GetClassNameW(
            hwnd,
            buffer,
            bufferCount
        );
    }
}

static void PrintWindowIdentity(
    const char* label,
    HWND hwnd
)
{
    wchar_t title[256] = {};
    wchar_t className[256] = {};

    DWORD processId = 0;
    DWORD threadId = 0;

    if (hwnd != nullptr)
    {
        threadId = GetWindowThreadProcessId(
            hwnd,
            &processId
        );

        GetWindowTextForLog(
            hwnd,
            title,
            static_cast<int>(
                sizeof(title) / sizeof(title[0])
                )
        );

        GetWindowClassForLog(
            hwnd,
            className,
            static_cast<int>(
                sizeof(className) / sizeof(className[0])
                )
        );
    }

    PrintCurrentQpcPrefix();

    std::printf(
        "%s "
        "Hwnd=%p ThreadId=%lu ProcessId=%lu "
        "ClassName=\"%ls\" Title=\"%ls\"\n",
        label,
        hwnd,
        threadId,
        processId,
        className,
        title
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
    snapshot->CursorHandle = cursorInfo.hCursor;
    snapshot->ForegroundWindow = GetForegroundWindow();

    return true;
}

static void PrintCursorSnapshot(
    const char* label,
    const CursorSnapshot& snapshot
)
{
    PrintQpcPrefix(
        snapshot.Counter,
        snapshot.Frequency
    );

    std::printf(
        "%s "
        "Cursor=(%ld,%ld) "
        "CursorFlags=0x%08lX "
        "hCursor=%p "
        "ForegroundHwnd=%p\n",
        label,
        snapshot.Position.x,
        snapshot.Position.y,
        snapshot.Flags,
        snapshot.CursorHandle,
        snapshot.ForegroundWindow
    );
}

static void PrintGuiSnapshot(const char* label)
{
    HWND foregroundWindow = GetForegroundWindow();
    HWND shellWindow = GetShellWindow();
    HWND desktopWindow = GetDesktopWindow();

    GUITHREADINFO guiThreadInfo = {};
    guiThreadInfo.cbSize = sizeof(GUITHREADINFO);

    SetLastError(0);

    BOOL guiResult = GetGUIThreadInfo(
        0,
        &guiThreadInfo
    );

    DWORD guiError = GetLastError();

    PrintCurrentQpcPrefix();

    std::printf(
        "%s "
        "ForegroundHwnd=%p "
        "ShellHwnd=%p "
        "DesktopHwnd=%p "
        "GetGUIThreadInfoResult=%d "
        "GetGUIThreadInfoError=%lu "
        "GuiFlags=0x%08lX "
        "ActiveHwnd=%p "
        "FocusHwnd=%p "
        "CaptureHwnd=%p "
        "CaretHwnd=%p\n",
        label,
        foregroundWindow,
        shellWindow,
        desktopWindow,
        guiResult ? 1 : 0,
        guiError,
        guiThreadInfo.flags,
        guiThreadInfo.hwndActive,
        guiThreadInfo.hwndFocus,
        guiThreadInfo.hwndCapture,
        guiThreadInfo.hwndCaret
    );

    PrintWindowIdentity(
        "Gui.Foreground",
        foregroundWindow
    );

    PrintWindowIdentity(
        "Gui.Shell",
        shellWindow
    );

    PrintWindowIdentity(
        "Gui.Desktop",
        desktopWindow
    );

    PrintWindowIdentity(
        "Gui.Active",
        guiThreadInfo.hwndActive
    );

    PrintWindowIdentity(
        "Gui.Focus",
        guiThreadInfo.hwndFocus
    );

    PrintWindowIdentity(
        "Gui.Capture",
        guiThreadInfo.hwndCapture
    );

    PrintWindowIdentity(
        "Gui.Caret",
        guiThreadInfo.hwndCaret
    );
}

static void PrintMessageEvent(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    LARGE_INTEGER counter = {};
    LARGE_INTEGER frequency = {};

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    POINT clientPoint = {};

    clientPoint.x = GET_X_LPARAM(lParam);
    clientPoint.y = GET_Y_LPARAM(lParam);

    POINT screenPoint = clientPoint;

    if (message == WM_MOUSEMOVE ||
        message == WM_LBUTTONDOWN ||
        message == WM_LBUTTONUP)
    {
        ClientToScreen(
            hwnd,
            &screenPoint
        );
    }

    PrintQpcPrefix(
        counter,
        frequency
    );

    std::printf(
        "Message=%s(0x%04X) "
        "hwnd=%p "
        "wParam=0x%p "
        "lParam=0x%p "
        "ClientPoint=(%ld,%ld) "
        "ScreenPoint=(%ld,%ld)\n",
        MessageName(message),
        message,
        hwnd,
        reinterpret_cast<void*>(wParam),
        reinterpret_cast<void*>(lParam),
        clientPoint.x,
        clientPoint.y,
        screenPoint.x,
        screenPoint.y
    );
}

static void PrintPointerInfoDetail(
    const char* label,
    const POINTER_INFO& pointerInfo
)
{
    PrintCurrentQpcPrefix();

    std::printf(
        "%s "
        "PointerType=%s(%u) "
        "PointerId=%u "
        "FrameId=%u "
        "PointerFlags=0x%08lX "
        "SourceDevice=%p "
        "TargetHwnd=%p "
        "Pixel=(%ld,%ld) "
        "Himetric=(%ld,%ld) "
        "PixelRaw=(%ld,%ld) "
        "HimetricRaw=(%ld,%ld) "
        "Time=%lu "
        "HistoryCount=%u "
        "InputData=%ld "
        "KeyStates=0x%08lX "
        "PerformanceCount=%llu "
        "ButtonChangeType=%u\n",
        label,
        PointerTypeName(pointerInfo.pointerType),
        static_cast<unsigned int>(
            pointerInfo.pointerType
            ),
        pointerInfo.pointerId,
        pointerInfo.frameId,
        pointerInfo.pointerFlags,
        pointerInfo.sourceDevice,
        pointerInfo.hwndTarget,
        pointerInfo.ptPixelLocation.x,
        pointerInfo.ptPixelLocation.y,
        pointerInfo.ptHimetricLocation.x,
        pointerInfo.ptHimetricLocation.y,
        pointerInfo.ptPixelLocationRaw.x,
        pointerInfo.ptPixelLocationRaw.y,
        pointerInfo.ptHimetricLocationRaw.x,
        pointerInfo.ptHimetricLocationRaw.y,
        pointerInfo.dwTime,
        pointerInfo.historyCount,
        pointerInfo.InputData,
        pointerInfo.dwKeyStates,
        static_cast<unsigned long long>(
            pointerInfo.PerformanceCount
            ),
        static_cast<unsigned int>(
            pointerInfo.ButtonChangeType
            )
    );
}

static void PrintPointerTouchInfoDetail(
    const POINTER_TOUCH_INFO& touchInfo
)
{
    PrintCurrentQpcPrefix();

    std::printf(
        "PointerTouchInfo "
        "TouchFlags=0x%08lX "
        "TouchMask=0x%08lX "
        "Contact=(%ld,%ld)-(%ld,%ld) "
        "ContactRaw=(%ld,%ld)-(%ld,%ld) "
        "Orientation=%u "
        "Pressure=%u\n",
        touchInfo.touchFlags,
        touchInfo.touchMask,
        touchInfo.rcContact.left,
        touchInfo.rcContact.top,
        touchInfo.rcContact.right,
        touchInfo.rcContact.bottom,
        touchInfo.rcContactRaw.left,
        touchInfo.rcContactRaw.top,
        touchInfo.rcContactRaw.right,
        touchInfo.rcContactRaw.bottom,
        touchInfo.orientation,
        touchInfo.pressure
    );
}

static void PrintPointerInfoHistory(UINT32 pointerId)
{
    constexpr UINT32 MaximumHistoryEntries = 128;

    std::vector<POINTER_INFO> history(
        MaximumHistoryEntries
    );

    UINT32 entryCount = MaximumHistoryEntries;

    SetLastError(0);

    BOOL historyResult = GetPointerInfoHistory(
        pointerId,
        &entryCount,
        history.data()
    );

    DWORD historyError = GetLastError();

    PrintCurrentQpcPrefix();

    std::printf(
        "GetPointerInfoHistory "
        "PointerId=%u "
        "Result=%d "
        "Error=%lu "
        "EntryCount=%u "
        "Capacity=%u\n",
        pointerId,
        historyResult ? 1 : 0,
        historyError,
        entryCount,
        MaximumHistoryEntries
    );

    if (!historyResult)
    {
        return;
    }

    for (UINT32 index = 0;
        index < entryCount;
        ++index)
    {
        const POINTER_INFO& entry =
            history[index];

        PrintCurrentQpcPrefix();

        std::printf(
            "PointerHistory[%u] "
            "PointerType=%s(%u) "
            "PointerId=%u "
            "FrameId=%u "
            "Flags=0x%08lX "
            "TargetHwnd=%p "
            "Pixel=(%ld,%ld) "
            "PixelRaw=(%ld,%ld) "
            "Time=%lu "
            "HistoryCount=%u "
            "PerformanceCount=%llu\n",
            index,
            PointerTypeName(entry.pointerType),
            static_cast<unsigned int>(
                entry.pointerType
                ),
            entry.pointerId,
            entry.frameId,
            entry.pointerFlags,
            entry.hwndTarget,
            entry.ptPixelLocation.x,
            entry.ptPixelLocation.y,
            entry.ptPixelLocationRaw.x,
            entry.ptPixelLocationRaw.y,
            entry.dwTime,
            entry.historyCount,
            static_cast<unsigned long long>(
                entry.PerformanceCount
                )
        );
    }
}

static void PrintPointerMessageEvent(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    LARGE_INTEGER counter = {};
    LARGE_INTEGER frequency = {};

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);

    UINT32 pointerId =
        GET_POINTERID_WPARAM(wParam);

    POINTER_INPUT_TYPE pointerType =
        PT_POINTER;

    SetLastError(0);

    BOOL pointerTypeResult = GetPointerType(
        pointerId,
        &pointerType
    );

    DWORD pointerTypeError = GetLastError();

    POINTER_INFO pointerInfo = {};

    SetLastError(0);

    BOOL pointerInfoResult = GetPointerInfo(
        pointerId,
        &pointerInfo
    );

    DWORD pointerInfoError = GetLastError();

    CURSORINFO cursorInfo = {};
    cursorInfo.cbSize = sizeof(CURSORINFO);

    SetLastError(0);

    BOOL cursorInfoResult = GetCursorInfo(
        &cursorInfo
    );

    DWORD cursorInfoError = GetLastError();

    HWND foregroundWindow =
        GetForegroundWindow();

    HWND shellWindow =
        GetShellWindow();

    HWND desktopWindow =
        GetDesktopWindow();

    POINT messagePoint = {};
    HWND pointerActivateWindow = nullptr;

    if (message == WM_POINTERACTIVATE)
    {
        pointerActivateWindow =
            reinterpret_cast<HWND>(lParam);
    }
    else
    {
        messagePoint.x =
            GET_X_LPARAM(lParam);

        messagePoint.y =
            GET_Y_LPARAM(lParam);
    }

    HWND windowFromMessagePoint = nullptr;

    if (message != WM_POINTERACTIVATE)
    {
        windowFromMessagePoint =
            WindowFromPoint(messagePoint);
    }

    HWND windowFromPointerPoint = nullptr;
    HWND pointerPointRoot = nullptr;
    HWND pointerPointRootOwner = nullptr;

    if (pointerInfoResult)
    {
        windowFromPointerPoint =
            WindowFromPoint(
                pointerInfo.ptPixelLocation
            );

        if (windowFromPointerPoint != nullptr)
        {
            pointerPointRoot =
                GetAncestor(
                    windowFromPointerPoint,
                    GA_ROOT
                );

            pointerPointRootOwner =
                GetAncestor(
                    windowFromPointerPoint,
                    GA_ROOTOWNER
                );
        }
    }

    PrintQpcPrefix(
        counter,
        frequency
    );

    std::printf(
        "PointerMessage "
        "Message=%s(0x%04X) "
        "ReceiverHwnd=%p "
        "wParam=0x%p "
        "lParam=0x%p "
        "PointerId=%u "
        "IsNew=%d "
        "IsInRange=%d "
        "IsInContact=%d "
        "IsPrimary=%d "
        "FirstButton=%d "
        "SecondButton=%d "
        "ThirdButton=%d "
        "FourthButton=%d "
        "FifthButton=%d "
        "MessagePoint=(%ld,%ld) "
        "PointerActivateHwnd=%p "
        "PointerTypeResult=%d "
        "PointerTypeError=%lu "
        "PointerType=%s(%u) "
        "PointerInfoResult=%d "
        "PointerInfoError=%lu "
        "CursorInfoResult=%d "
        "CursorInfoError=%lu "
        "CursorFlags=0x%08lX "
        "CursorPos=(%ld,%ld) "
        "hCursor=%p "
        "ForegroundHwnd=%p "
        "ShellHwnd=%p "
        "DesktopHwnd=%p "
        "WindowFromMessagePoint=%p "
        "WindowFromPointerPoint=%p "
        "PointerPointRoot=%p "
        "PointerPointRootOwner=%p\n",
        MessageName(message),
        message,
        hwnd,
        reinterpret_cast<void*>(wParam),
        reinterpret_cast<void*>(lParam),
        pointerId,
        IS_POINTER_NEW_WPARAM(wParam) ? 1 : 0,
        IS_POINTER_INRANGE_WPARAM(wParam) ? 1 : 0,
        IS_POINTER_INCONTACT_WPARAM(wParam) ? 1 : 0,
        IS_POINTER_PRIMARY_WPARAM(wParam) ? 1 : 0,
        IS_POINTER_FIRSTBUTTON_WPARAM(wParam) ? 1 : 0,
        IS_POINTER_SECONDBUTTON_WPARAM(wParam) ? 1 : 0,
        IS_POINTER_THIRDBUTTON_WPARAM(wParam) ? 1 : 0,
        IS_POINTER_FOURTHBUTTON_WPARAM(wParam) ? 1 : 0,
        IS_POINTER_FIFTHBUTTON_WPARAM(wParam) ? 1 : 0,
        messagePoint.x,
        messagePoint.y,
        pointerActivateWindow,
        pointerTypeResult ? 1 : 0,
        pointerTypeError,
        PointerTypeName(pointerType),
        static_cast<unsigned int>(pointerType),
        pointerInfoResult ? 1 : 0,
        pointerInfoError,
        cursorInfoResult ? 1 : 0,
        cursorInfoError,
        cursorInfo.flags,
        cursorInfo.ptScreenPos.x,
        cursorInfo.ptScreenPos.y,
        cursorInfo.hCursor,
        foregroundWindow,
        shellWindow,
        desktopWindow,
        windowFromMessagePoint,
        windowFromPointerPoint,
        pointerPointRoot,
        pointerPointRootOwner
    );

    if (pointerInfoResult)
    {
        PrintPointerInfoDetail(
            "PointerInfo",
            pointerInfo
        );

        PrintWindowIdentity(
            "PointerInfo.Target",
            pointerInfo.hwndTarget
        );
    }

    PrintWindowIdentity(
        "PointerMessage.Receiver",
        hwnd
    );

    PrintWindowIdentity(
        "PointerMessage.Foreground",
        foregroundWindow
    );

    PrintWindowIdentity(
        "PointerMessage.Shell",
        shellWindow
    );

    PrintWindowIdentity(
        "PointerMessage.Desktop",
        desktopWindow
    );

    PrintWindowIdentity(
        "PointerMessage.ActivateWindow",
        pointerActivateWindow
    );

    PrintWindowIdentity(
        "PointerMessage.WindowFromMessagePoint",
        windowFromMessagePoint
    );

    PrintWindowIdentity(
        "PointerMessage.WindowFromPointerPoint",
        windowFromPointerPoint
    );

    PrintWindowIdentity(
        "PointerMessage.PointerPointRoot",
        pointerPointRoot
    );

    PrintWindowIdentity(
        "PointerMessage.PointerPointRootOwner",
        pointerPointRootOwner
    );

    if (pointerTypeResult &&
        pointerType == PT_TOUCH)
    {
        POINTER_TOUCH_INFO touchInfo = {};

        SetLastError(0);

        BOOL touchInfoResult =
            GetPointerTouchInfo(
                pointerId,
                &touchInfo
            );

        DWORD touchInfoError =
            GetLastError();

        PrintCurrentQpcPrefix();

        std::printf(
            "GetPointerTouchInfo "
            "PointerId=%u "
            "Result=%d "
            "Error=%lu\n",
            pointerId,
            touchInfoResult ? 1 : 0,
            touchInfoError
        );

        if (touchInfoResult)
        {
            PrintPointerTouchInfoDetail(
                touchInfo
            );
        }
    }

    if (message == WM_POINTERUPDATE)
    {
        PrintPointerInfoHistory(
            pointerId
        );
    }

    PrintGuiSnapshot(
        "PointerMessage.GuiSnapshot"
    );
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
        PrintCursorSnapshot(
            "MARK(F8)",
            current
        );

        PrintGuiSnapshot(
            "MARK(F8)"
        );
    }

    if (!g_HasPreviousSnapshot)
    {
        g_PreviousSnapshot = current;
        g_HasPreviousSnapshot = true;

        PrintCursorSnapshot(
            "START",
            current
        );

        PrintGuiSnapshot(
            "START"
        );

        return;
    }

    if (g_PreviousSnapshot.Position.x !=
        current.Position.x ||
        g_PreviousSnapshot.Position.y !=
        current.Position.y)
    {
        PrintQpcPrefix(
            current.Counter,
            current.Frequency
        );

        std::printf(
            "CursorPos : "
            "(%ld,%ld) -> (%ld,%ld) "
            "CursorFlags=0x%08lX "
            "hCursor=%p\n",
            g_PreviousSnapshot.Position.x,
            g_PreviousSnapshot.Position.y,
            current.Position.x,
            current.Position.y,
            current.Flags,
            current.CursorHandle
        );
    }

    if (g_PreviousSnapshot.Flags !=
        current.Flags ||
        g_PreviousSnapshot.CursorHandle !=
        current.CursorHandle)
    {
        PrintQpcPrefix(
            current.Counter,
            current.Frequency
        );

        std::printf(
            "CursorStateChanged "
            "Flags=0x%08lX -> 0x%08lX "
            "hCursor=%p -> %p "
            "Cursor=(%ld,%ld)\n",
            g_PreviousSnapshot.Flags,
            current.Flags,
            g_PreviousSnapshot.CursorHandle,
            current.CursorHandle,
            current.Position.x,
            current.Position.y
        );

        POINT cursorPoint = current.Position;

        HWND windowFromPoint =
            WindowFromPoint(cursorPoint);

        HWND rootWindow = nullptr;
        HWND rootOwnerWindow = nullptr;

        if (windowFromPoint != nullptr)
        {
            rootWindow =
                GetAncestor(
                    windowFromPoint,
                    GA_ROOT
                );

            rootOwnerWindow =
                GetAncestor(
                    windowFromPoint,
                    GA_ROOTOWNER
                );
        }

        PrintWindowIdentity(
            "CursorStateChanged.WindowFromPoint",
            windowFromPoint
        );

        PrintWindowIdentity(
            "CursorStateChanged.Root",
            rootWindow
        );

        PrintWindowIdentity(
            "CursorStateChanged.RootOwner",
            rootOwnerWindow
        );

        PrintGuiSnapshot(
            "CursorStateChanged.GuiSnapshot"
        );
    }

    if (g_PreviousSnapshot.ForegroundWindow !=
        current.ForegroundWindow)
    {
        PrintQpcPrefix(
            current.Counter,
            current.Frequency
        );

        std::printf(
            "ForegroundChanged : "
            "%p -> %p "
            "Cursor=(%ld,%ld) "
            "CursorFlags=0x%08lX\n",
            g_PreviousSnapshot.ForegroundWindow,
            current.ForegroundWindow,
            current.Position.x,
            current.Position.y,
            current.Flags
        );

        PrintWindowIdentity(
            "ForegroundChanged.Previous",
            g_PreviousSnapshot.ForegroundWindow
        );

        PrintWindowIdentity(
            "ForegroundChanged.Current",
            current.ForegroundWindow
        );

        PrintGuiSnapshot(
            "ForegroundChanged.GuiSnapshot"
        );
    }

    g_PreviousSnapshot = current;

    UNREFERENCED_PARAMETER(hwnd);
}

static LRESULT CALLBACK ReTouchPointerProbeWndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (message)
    {
    case WM_CREATE:
        SetTimer(
            hwnd,
            1,
            5,
            nullptr
        );

        return 0;

    case WM_TIMER:
        PollCursorAndKeys(hwnd);
        return 0;

    case WM_POINTERACTIVATE:
        PrintPointerMessageEvent(
            hwnd,
            message,
            wParam,
            lParam
        );

        PrintCurrentQpcPrefix();

        std::printf(
            "WM_POINTERACTIVATE returning PA_NOACTIVATE\n"
        );

        return PA_NOACTIVATE;

    case WM_POINTERENTER:
    case WM_POINTERLEAVE:
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP:
    case WM_POINTERCAPTURECHANGED:
        PrintPointerMessageEvent(
            hwnd,
            message,
            wParam,
            lParam
        );

        return 0;

    case WM_MOUSEACTIVATE:
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_ACTIVATE:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        PrintMessageEvent(
            hwnd,
            message,
            wParam,
            lParam
        );

        break;

    case WM_DESTROY:
        KillTimer(
            hwnd,
            1
        );

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wParam,
        lParam
    );
}

static bool RegisterProbeWindowClass(
    HINSTANCE instance
)
{
    WNDCLASSEXW windowClass = {};

    windowClass.cbSize =
        sizeof(WNDCLASSEXW);

    windowClass.lpfnWndProc =
        ReTouchPointerProbeWndProc;

    windowClass.hInstance =
        instance;

    windowClass.lpszClassName =
        L"ReTouchPointerProbeWindowClass";

    windowClass.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW
        );

    windowClass.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1
            );

    return RegisterClassExW(
        &windowClass
    ) != 0;
}

static HWND CreateProbeWindow(
    HINSTANCE instance
)
{
    return CreateWindowExW(
        0,
        L"ReTouchPointerProbeWindowClass",
        L"ReTouchPointerProbe v2 - touch inside this window",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        360,
        nullptr,
        nullptr,
        instance,
        nullptr
    );
}

int main()
{
    HINSTANCE instance =
        GetModuleHandleW(nullptr);

    std::printf(
        "ReTouchPointerProbe v2 started.\n"
    );

    std::printf(
        "Touch inside the ReTouchPointerProbe window.\n"
    );

    std::printf(
        "RegisterPointerInputTarget is NOT used.\n"
    );

    std::printf(
        "Press F8 to mark.\n"
    );

    std::printf(
        "Press ESC to exit.\n\n"
    );

    if (!RegisterProbeWindowClass(instance))
    {
        std::printf(
            "RegisterClassExW failed. "
            "GetLastError=%lu\n",
            GetLastError()
        );

        return 1;
    }

    HWND hwnd =
        CreateProbeWindow(instance);

    if (hwnd == nullptr)
    {
        std::printf(
            "CreateWindowExW failed. "
            "GetLastError=%lu\n",
            GetLastError()
        );

        return 1;
    }

    MSG message = {};

    while (GetMessageW(
        &message,
        nullptr,
        0,
        0
    ) > 0)
    {
        TranslateMessage(
            &message
        );

        DispatchMessageW(
            &message
        );
    }

    std::printf(
        "ReTouchPointerProbe v2 stopped.\n"
    );

    return 0;
}