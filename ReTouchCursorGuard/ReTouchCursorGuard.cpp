#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <unordered_set>
#include <vector>

#include "../TridentInputHook/TridentInputHook.h"
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

static std::unordered_set<HANDLE> g_ObservedRawInputDevices;

static constexpr ULONG_PTR g_MiWpSignature =
static_cast<ULONG_PTR>(0xFF515700);

static constexpr ULONG_PTR g_MiWpSignatureMask =
static_cast<ULONG_PTR>(0xFFFFFF00);

static constexpr ULONG_PTR g_MiWpTouchMask =
static_cast<ULONG_PTR>(0x00000080);

enum class RawMouseClassification
{
    PhysicalMouseCandidate,
    ConfirmedTouchPromotion,
    SuspiciousAbsoluteInput,
    SyntheticOrInjectedRelativeInput,
    OtherMouseInput
};

static POINT g_LastPhysicalCursorPosition = {};
static bool g_HasLastPhysicalCursorPosition = false;

static bool g_IsTouchActive = false;
static bool g_IsTouchRecoveryCooldownActive = false;
static LARGE_INTEGER g_TouchRecoveryBlockUntilQpc = {};

static HWND g_LastRealForegroundWindow = nullptr;
static DWORD g_LastRealForegroundThreadId = 0;
static DWORD g_LastRealForegroundProcessId = 0;
static wchar_t g_LastRealForegroundTitle[256] = {};

static HWINEVENTHOOK g_ForegroundWinEventHook = nullptr;
static HWINEVENTHOOK g_FocusWinEventHook = nullptr;

using TridentInstallObservationHooksFunction =
BOOL(WINAPI*)();

using TridentUninstallObservationHooksFunction =
BOOL(WINAPI*)();

static HANDLE
g_TridentHookSharedMapping =
nullptr;

static TridentHookSharedState*
g_TridentHookSharedState =
nullptr;

static HMODULE
g_TridentInputHookModule =
nullptr;

static TridentInstallObservationHooksFunction
g_TridentInstallObservationHooks =
nullptr;

static TridentUninstallObservationHooksFunction
g_TridentUninstallObservationHooks =
nullptr;

static bool
g_AreTridentObservationHooksInstalled =
false;

static LONG64
g_LastConsumedTridentHookSequence =
0;

static uint64_t
g_TridentHookDroppedByReaderCount =
0;

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

static const char* GetRawMouseClassificationName(
    RawMouseClassification classification
)
{
    switch (classification)
    {
    case RawMouseClassification::PhysicalMouseCandidate:
        return "PhysicalMouseCandidate";

    case RawMouseClassification::ConfirmedTouchPromotion:
        return "ConfirmedTouchPromotion";

    case RawMouseClassification::SuspiciousAbsoluteInput:
        return "SuspiciousAbsoluteInput";

    case RawMouseClassification::SyntheticOrInjectedRelativeInput:
        return "SyntheticOrInjectedRelativeInput";

    case RawMouseClassification::OtherMouseInput:
        return "OtherMouseInput";

    default:
        return "UnknownRawMouseClassification";
    }
}

static RawMouseClassification ClassifyRawMouseInput(
    const RAWINPUTHEADER& header,
    const RAWMOUSE& mouse,
    bool hasMiWpSignature,
    bool hasTouchSignature
)
{
    const bool isAbsolute =
        (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0;

    const bool isVirtualDesktop =
        (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;

    const bool isRelative =
        mouse.usFlags == MOUSE_MOVE_RELATIVE;

    if (hasMiWpSignature &&
        hasTouchSignature &&
        isAbsolute &&
        isVirtualDesktop)
    {
        return RawMouseClassification::
            ConfirmedTouchPromotion;
    }

    if (isAbsolute &&
        isVirtualDesktop)
    {
        return RawMouseClassification::
            SuspiciousAbsoluteInput;
    }

    if (header.hDevice == nullptr &&
        isRelative &&
        !hasMiWpSignature)
    {
        return RawMouseClassification::
            SyntheticOrInjectedRelativeInput;
    }

    if (header.hDevice != nullptr &&
        isRelative &&
        !hasMiWpSignature)
    {
        return RawMouseClassification::
            PhysicalMouseCandidate;
    }

    return RawMouseClassification::
        OtherMouseInput;
}

static void PrintRawInputDeviceIdentityOnce(
    const RAWINPUTHEADER& header
)
{
    HANDLE deviceHandle = header.hDevice;

    const auto insertResult =
        g_ObservedRawInputDevices.insert(deviceHandle);

    if (!insertResult.second)
    {
        return;
    }

    PrintQpcPrefix();

    std::printf(
        "RawInputDeviceFirstSeen "
        "HeaderType=%lu "
        "HeaderSize=%lu "
        "DeviceHandle=%p "
        "InputCode=0x%p\n",
        header.dwType,
        header.dwSize,
        deviceHandle,
        reinterpret_cast<void*>(header.wParam)
    );

    if (deviceHandle == nullptr)
    {
        PrintQpcPrefix();

        std::printf(
            "RawInputDeviceIdentity "
            "DeviceHandle=NULL "
            "DeviceNameQuerySkipped=1 "
            "DeviceInfoQuerySkipped=1\n"
        );

        return;
    }

    UINT deviceNameCharacterCount = 0;

    SetLastError(0);

    UINT deviceNameSizeResult =
        GetRawInputDeviceInfoW(
            deviceHandle,
            RIDI_DEVICENAME,
            nullptr,
            &deviceNameCharacterCount
        );

    DWORD deviceNameSizeError =
        GetLastError();

    PrintQpcPrefix();

    std::printf(
        "RawInputDeviceNameSize "
        "DeviceHandle=%p "
        "Result=%u "
        "GetLastError=%lu "
        "CharacterCount=%u\n",
        deviceHandle,
        deviceNameSizeResult,
        deviceNameSizeError,
        deviceNameCharacterCount
    );

    if (deviceNameSizeResult != static_cast<UINT>(-1) &&
        deviceNameCharacterCount > 0)
    {
        std::vector<wchar_t> deviceName(
            static_cast<size_t>(
                deviceNameCharacterCount
                ) + 1,
            L'\0'
        );

        UINT deviceNameBufferCharacterCount =
            static_cast<UINT>(
                deviceName.size()
                );

        SetLastError(0);

        UINT deviceNameResult =
            GetRawInputDeviceInfoW(
                deviceHandle,
                RIDI_DEVICENAME,
                deviceName.data(),
                &deviceNameBufferCharacterCount
            );

        DWORD deviceNameError =
            GetLastError();

        PrintQpcPrefix();

        if (deviceNameResult != static_cast<UINT>(-1))
        {
            std::printf(
                "RawInputDeviceName "
                "DeviceHandle=%p "
                "Result=%u "
                "GetLastError=%lu "
                "CharacterCount=%u "
                "Name=\"%ls\"\n",
                deviceHandle,
                deviceNameResult,
                deviceNameError,
                deviceNameBufferCharacterCount,
                deviceName.data()
            );
        }
        else
        {
            std::printf(
                "RawInputDeviceName "
                "DeviceHandle=%p "
                "Result=%u "
                "GetLastError=%lu "
                "CharacterCount=%u "
                "NameQueryFailed=1\n",
                deviceHandle,
                deviceNameResult,
                deviceNameError,
                deviceNameBufferCharacterCount
            );
        }
    }

    RID_DEVICE_INFO deviceInfo = {};
    deviceInfo.cbSize =
        sizeof(RID_DEVICE_INFO);

    UINT deviceInfoSize =
        sizeof(RID_DEVICE_INFO);

    SetLastError(0);

    UINT deviceInfoResult =
        GetRawInputDeviceInfoW(
            deviceHandle,
            RIDI_DEVICEINFO,
            &deviceInfo,
            &deviceInfoSize
        );

    DWORD deviceInfoError =
        GetLastError();

    PrintQpcPrefix();

    std::printf(
        "RawInputDeviceInfo "
        "DeviceHandle=%p "
        "Result=%u "
        "GetLastError=%lu "
        "ReturnedSize=%u "
        "DeviceType=%lu\n",
        deviceHandle,
        deviceInfoResult,
        deviceInfoError,
        deviceInfoSize,
        deviceInfo.dwType
    );

    if (deviceInfoResult == static_cast<UINT>(-1))
    {
        return;
    }

    if (deviceInfo.dwType == RIM_TYPEMOUSE)
    {
        PrintQpcPrefix();

        std::printf(
            "RawInputMouseDeviceInfo "
            "DeviceHandle=%p "
            "MouseId=0x%08lX "
            "NumberOfButtons=%lu "
            "SampleRate=%lu "
            "HasHorizontalWheel=%d\n",
            deviceHandle,
            deviceInfo.mouse.dwId,
            deviceInfo.mouse.dwNumberOfButtons,
            deviceInfo.mouse.dwSampleRate,
            deviceInfo.mouse.fHasHorizontalWheel ? 1 : 0
        );

        return;
    }

    if (deviceInfo.dwType == RIM_TYPEKEYBOARD)
    {
        PrintQpcPrefix();

        std::printf(
            "RawInputDeviceInfoUnexpectedType "
            "DeviceHandle=%p "
            "DeviceType=RIM_TYPEKEYBOARD\n",
            deviceHandle
        );

        return;
    }

    if (deviceInfo.dwType == RIM_TYPEHID)
    {
        PrintQpcPrefix();

        std::printf(
            "RawInputHidDeviceInfo "
            "DeviceHandle=%p "
            "VendorId=0x%04X "
            "ProductId=0x%04X "
            "VersionNumber=0x%04X "
            "UsagePage=0x%04X "
            "Usage=0x%04X\n",
            deviceHandle,
            deviceInfo.hid.dwVendorId,
            deviceInfo.hid.dwProductId,
            deviceInfo.hid.dwVersionNumber,
            deviceInfo.hid.usUsagePage,
            deviceInfo.hid.usUsage
        );

        return;
    }

    PrintQpcPrefix();

    std::printf(
        "RawInputDeviceInfoUnexpectedType "
        "DeviceHandle=%p "
        "DeviceType=%lu\n",
        deviceHandle,
        deviceInfo.dwType
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

static void PrintWindowTextForLog(HWND hwnd, wchar_t* title, int titleCount)
{
    if (title == nullptr || titleCount <= 0)
    {
        return;
    }

    title[0] = L'\0';

    if (hwnd != nullptr)
    {
        GetWindowTextW(hwnd, title, titleCount);
    }
}

static void PrintGuiFocusSnapshot(const char* label)
{
    HWND foregroundWindow = GetForegroundWindow();

    DWORD foregroundProcessId = 0;
    DWORD foregroundThreadId = 0;

    wchar_t foregroundTitle[256] = {};

    if (foregroundWindow != nullptr)
    {
        foregroundThreadId = GetWindowThreadProcessId(
            foregroundWindow,
            &foregroundProcessId
        );

        PrintWindowTextForLog(
            foregroundWindow,
            foregroundTitle,
            static_cast<int>(sizeof(foregroundTitle) / sizeof(foregroundTitle[0]))
        );
    }

    bool isProgramManager =
        wcscmp(foregroundTitle, L"Program Manager") == 0;

    if (foregroundWindow != nullptr && !isProgramManager)
    {
        g_LastRealForegroundWindow = foregroundWindow;
        g_LastRealForegroundThreadId = foregroundThreadId;
        g_LastRealForegroundProcessId = foregroundProcessId;

        wcscpy_s(
            g_LastRealForegroundTitle,
            foregroundTitle
        );
    }

    GUITHREADINFO guiThreadInfo = {};
    guiThreadInfo.cbSize = sizeof(GUITHREADINFO);

    BOOL guiResult = GetGUIThreadInfo(
        0,
        &guiThreadInfo
    );

    wchar_t activeTitle[256] = {};
    wchar_t focusTitle[256] = {};
    wchar_t captureTitle[256] = {};
    wchar_t caretTitle[256] = {};

    PrintWindowTextForLog(guiThreadInfo.hwndActive, activeTitle, 256);
    PrintWindowTextForLog(guiThreadInfo.hwndFocus, focusTitle, 256);
    PrintWindowTextForLog(guiThreadInfo.hwndCapture, captureTitle, 256);
    PrintWindowTextForLog(guiThreadInfo.hwndCaret, caretTitle, 256);

    PrintQpcPrefix();

    std::printf(
        "%s "
        "ForegroundHwnd=%p ForegroundThreadId=%lu ForegroundProcessId=%lu ForegroundTitle=\"%ls\" "
        "LastRealForegroundHwnd=%p LastRealForegroundThreadId=%lu LastRealForegroundProcessId=%lu LastRealForegroundTitle=\"%ls\" "
        "GetGUIThreadInfoResult=%d GuiFlags=0x%08lX "
        "ActiveHwnd=%p ActiveTitle=\"%ls\" "
        "FocusHwnd=%p FocusTitle=\"%ls\" "
        "CaptureHwnd=%p CaptureTitle=\"%ls\" "
        "CaretHwnd=%p CaretTitle=\"%ls\"\n",
        label,
        foregroundWindow,
        foregroundThreadId,
        foregroundProcessId,
        foregroundTitle,
        g_LastRealForegroundWindow,
        g_LastRealForegroundThreadId,
        g_LastRealForegroundProcessId,
        g_LastRealForegroundTitle,
        guiResult ? 1 : 0,
        guiThreadInfo.flags,
        guiThreadInfo.hwndActive,
        activeTitle,
        guiThreadInfo.hwndFocus,
        focusTitle,
        guiThreadInfo.hwndCapture,
        captureTitle,
        guiThreadInfo.hwndCaret,
        caretTitle
    );
}

static void PrintWindowIdentityForLog(
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

        GetWindowTextW(hwnd, title, 256);
        GetClassNameW(hwnd, className, 256);
    }

    PrintQpcPrefix();

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

static const char* GetWinEventName(
    DWORD event
)
{
    switch (event)
    {
    case EVENT_SYSTEM_FOREGROUND:
        return "EVENT_SYSTEM_FOREGROUND";

    case EVENT_OBJECT_FOCUS:
        return "EVENT_OBJECT_FOCUS";

    default:
        return "UNKNOWN_WIN_EVENT";
    }
}

static void CALLBACK ReTouchWinEventProc(
    HWINEVENTHOOK hook,
    DWORD event,
    HWND hwnd,
    LONG objectId,
    LONG childId,
    DWORD eventThreadId,
    DWORD eventTimeMilliseconds
)
{
    DWORD processId = 0;

    if (hwnd != nullptr)
    {
        GetWindowThreadProcessId(
            hwnd,
            &processId
        );
    }

    HWND currentForegroundWindow =
        GetForegroundWindow();

    CURSORINFO cursorInfo = {};
    cursorInfo.cbSize = sizeof(CURSORINFO);

    SetLastError(0);

    BOOL cursorInfoResult =
        GetCursorInfo(
            &cursorInfo
        );

    DWORD cursorInfoError =
        GetLastError();

    PrintQpcPrefix();

    std::printf(
        "WinEvent "
        "Event=%s "
        "EventValue=0x%08lX "
        "Hook=%p "
        "Hwnd=%p "
        "ObjectId=%ld "
        "ChildId=%ld "
        "EventThreadId=%lu "
        "EventProcessId=%lu "
        "EventTimeMs=%lu "
        "CurrentForegroundHwnd=%p "
        "CursorInfoResult=%d "
        "CursorInfoError=%lu "
        "CursorFlags=0x%08lX "
        "CursorPos=(%ld,%ld) "
        "TouchActive=%d "
        "CooldownActive=%d "
        "RawMoveCount=%llu "
        "RawButtonCount=%llu\n",
        GetWinEventName(event),
        event,
        hook,
        hwnd,
        objectId,
        childId,
        eventThreadId,
        processId,
        eventTimeMilliseconds,
        currentForegroundWindow,
        cursorInfoResult ? 1 : 0,
        cursorInfoError,
        cursorInfo.flags,
        cursorInfo.ptScreenPos.x,
        cursorInfo.ptScreenPos.y,
        g_IsTouchActive ? 1 : 0,
        g_IsTouchRecoveryCooldownActive ? 1 : 0,
        static_cast<unsigned long long>(
            g_RawMouseMoveCount
            ),
        static_cast<unsigned long long>(
            g_RawMouseButtonCount
            )
    );

    PrintWindowIdentityForLog(
        "WinEvent.Target",
        hwnd
    );

    if (currentForegroundWindow != hwnd)
    {
        PrintWindowIdentityForLog(
            "WinEvent.CurrentForeground",
            currentForegroundWindow
        );
    }
}

static bool RegisterWinEventHooks()
{
    constexpr DWORD hookFlags =
        WINEVENT_OUTOFCONTEXT |
        WINEVENT_SKIPOWNPROCESS;

    SetLastError(0);

    g_ForegroundWinEventHook =
        SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND,
            EVENT_SYSTEM_FOREGROUND,
            nullptr,
            ReTouchWinEventProc,
            0,
            0,
            hookFlags
        );

    DWORD foregroundHookError =
        GetLastError();

    PrintQpcPrefix();

    std::printf(
        "SetWinEventHook "
        "Event=EVENT_SYSTEM_FOREGROUND "
        "Result=%p "
        "GetLastError=%lu "
        "Flags=0x%08lX\n",
        g_ForegroundWinEventHook,
        foregroundHookError,
        hookFlags
    );

    SetLastError(0);

    g_FocusWinEventHook =
        SetWinEventHook(
            EVENT_OBJECT_FOCUS,
            EVENT_OBJECT_FOCUS,
            nullptr,
            ReTouchWinEventProc,
            0,
            0,
            hookFlags
        );

    DWORD focusHookError =
        GetLastError();

    PrintQpcPrefix();

    std::printf(
        "SetWinEventHook "
        "Event=EVENT_OBJECT_FOCUS "
        "Result=%p "
        "GetLastError=%lu "
        "Flags=0x%08lX\n",
        g_FocusWinEventHook,
        focusHookError,
        hookFlags
    );

    if (g_ForegroundWinEventHook != nullptr &&
        g_FocusWinEventHook != nullptr)
    {
        return true;
    }

    if (g_ForegroundWinEventHook != nullptr)
    {
        UnhookWinEvent(
            g_ForegroundWinEventHook
        );

        g_ForegroundWinEventHook =
            nullptr;
    }

    if (g_FocusWinEventHook != nullptr)
    {
        UnhookWinEvent(
            g_FocusWinEventHook
        );

        g_FocusWinEventHook =
            nullptr;
    }

    return false;
}

static void UnregisterWinEventHooks()
{
    if (g_FocusWinEventHook != nullptr)
    {
        SetLastError(0);

        BOOL focusUnhookResult =
            UnhookWinEvent(
                g_FocusWinEventHook
            );

        DWORD focusUnhookError =
            GetLastError();

        PrintQpcPrefix();

        std::printf(
            "UnhookWinEvent "
            "Event=EVENT_OBJECT_FOCUS "
            "Result=%d "
            "GetLastError=%lu "
            "Hook=%p\n",
            focusUnhookResult ? 1 : 0,
            focusUnhookError,
            g_FocusWinEventHook
        );

        g_FocusWinEventHook =
            nullptr;
    }

    if (g_ForegroundWinEventHook != nullptr)
    {
        SetLastError(0);

        BOOL foregroundUnhookResult =
            UnhookWinEvent(
                g_ForegroundWinEventHook
            );

        DWORD foregroundUnhookError =
            GetLastError();

        PrintQpcPrefix();

        std::printf(
            "UnhookWinEvent "
            "Event=EVENT_SYSTEM_FOREGROUND "
            "Result=%d "
            "GetLastError=%lu "
            "Hook=%p\n",
            foregroundUnhookResult ? 1 : 0,
            foregroundUnhookError,
            g_ForegroundWinEventHook
        );

        g_ForegroundWinEventHook =
            nullptr;
    }
}

static const char* GetTridentHookEventTypeName(
    TridentHookEventType eventType
)
{
    switch (eventType)
    {
    case TridentHookEventType::CbtActivate:
        return "HCBT_ACTIVATE";

    case TridentHookEventType::CbtSetFocus:
        return "HCBT_SETFOCUS";

    case TridentHookEventType::CallWndProcMessage:
        return "WH_CALLWNDPROC";

    case TridentHookEventType::GetMessage:
        return "WH_GETMESSAGE";

    case TridentHookEventType::None:
        return "None";

    default:
        return "UnknownTridentHookEvent";
    }
}

static const char* GetObservedWindowMessageName(
    UINT message
)
{
    switch (message)
    {
    case WM_MOUSEACTIVATE:
        return "WM_MOUSEACTIVATE";

    case WM_ACTIVATE:
        return "WM_ACTIVATE";

    case WM_ACTIVATEAPP:
        return "WM_ACTIVATEAPP";

    case WM_SETFOCUS:
        return "WM_SETFOCUS";

    case WM_KILLFOCUS:
        return "WM_KILLFOCUS";

    case WM_POINTERACTIVATE:
        return "WM_POINTERACTIVATE";

    case WM_SETCURSOR:
        return "WM_SETCURSOR";

    case WM_POINTERENTER:
        return "WM_POINTERENTER";

    case WM_POINTERDOWN:
        return "WM_POINTERDOWN";

    case WM_POINTERUPDATE:
        return "WM_POINTERUPDATE";

    case WM_POINTERUP:
        return "WM_POINTERUP";

    case WM_POINTERLEAVE:
        return "WM_POINTERLEAVE";

    case WM_POINTERCAPTURECHANGED:
        return "WM_POINTERCAPTURECHANGED";

    default:
        return "UNKNOWN_WINDOW_MESSAGE";
    }
}

static bool BuildTridentInputHookDllPath(
    wchar_t* path,
    size_t pathCharacterCount
)
{
    if (path == nullptr ||
        pathCharacterCount == 0)
    {
        return false;
    }

    path[0] = L'\0';

    DWORD copiedCharacterCount =
        GetModuleFileNameW(
            nullptr,
            path,
            static_cast<DWORD>(
                pathCharacterCount
                )
        );

    if (copiedCharacterCount == 0 ||
        copiedCharacterCount >=
        pathCharacterCount)
    {
        return false;
    }

    wchar_t* finalBackslash =
        wcsrchr(
            path,
            L'\\'
        );

    if (finalBackslash == nullptr)
    {
        return false;
    }

    *(finalBackslash + 1) = L'\0';

    errno_t concatenateResult =
        wcscat_s(
            path,
            pathCharacterCount,
            L"TridentInputHook.dll"
        );

    return concatenateResult == 0;
}

static bool InitializeTridentHookSharedMemory()
{
    if (g_TridentHookSharedMapping != nullptr ||
        g_TridentHookSharedState != nullptr)
    {
        return false;
    }

    using ConvertSecurityDescriptorFunction =
        BOOL(WINAPI*)(
            LPCWSTR,
            DWORD,
            PSECURITY_DESCRIPTOR*,
            PULONG
            );

    HMODULE advapiModule =
        LoadLibraryW(
            L"Advapi32.dll"
        );

    if (advapiModule == nullptr)
    {
        PrintQpcPrefix();

        std::printf(
            "LoadLibraryW Advapi32.dll failed. "
            "GetLastError=%lu\n",
            GetLastError()
        );

        return false;
    }

    FARPROC conversionAddress =
        GetProcAddress(
            advapiModule,
            "ConvertStringSecurityDescriptorToSecurityDescriptorW"
        );

    if (conversionAddress == nullptr)
    {
        PrintQpcPrefix();

        std::printf(
            "GetProcAddress "
            "ConvertStringSecurityDescriptorToSecurityDescriptorW "
            "failed. GetLastError=%lu\n",
            GetLastError()
        );

        FreeLibrary(
            advapiModule
        );

        return false;
    }

    auto convertSecurityDescriptor =
        reinterpret_cast<
        ConvertSecurityDescriptorFunction
        >(
            conversionAddress
            );

    PSECURITY_DESCRIPTOR securityDescriptor =
        nullptr;

    ULONG securityDescriptorSize =
        0;

    constexpr wchar_t securityDescriptorString[] =
        L"D:"
        L"(A;;GA;;;SY)"
        L"(A;;GA;;;BA)"
        L"(A;;GRGW;;;IU)"
        L"S:"
        L"(ML;;NW;;;LW)";

    SetLastError(0);

    BOOL conversionResult =
        convertSecurityDescriptor(
            securityDescriptorString,
            1,
            &securityDescriptor,
            &securityDescriptorSize
        );

    DWORD conversionError =
        GetLastError();

    PrintQpcPrefix();

    std::printf(
        "ConvertSharedMemorySecurityDescriptor "
        "Result=%d "
        "GetLastError=%lu "
        "Size=%lu\n",
        conversionResult ? 1 : 0,
        conversionError,
        securityDescriptorSize
    );

    if (!conversionResult ||
        securityDescriptor == nullptr)
    {
        FreeLibrary(
            advapiModule
        );

        return false;
    }

    SECURITY_ATTRIBUTES securityAttributes = {};
    securityAttributes.nLength =
        sizeof(SECURITY_ATTRIBUTES);
    securityAttributes.lpSecurityDescriptor =
        securityDescriptor;
    securityAttributes.bInheritHandle =
        FALSE;

    SetLastError(0);

    HANDLE mapping =
        CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            &securityAttributes,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(
                sizeof(
                    TridentHookSharedState
                    )
                ),
            TRIDENT_INPUT_HOOK_SHARED_MEMORY_NAME
        );

    DWORD createMappingError =
        GetLastError();

    LocalFree(
        securityDescriptor
    );

    securityDescriptor =
        nullptr;

    FreeLibrary(
        advapiModule
    );

    advapiModule =
        nullptr;

    PrintQpcPrefix();

    std::printf(
        "CreateFileMapping "
        "Name=\"%ls\" "
        "Result=%p "
        "GetLastError=%lu "
        "Size=%zu "
        "ExplicitSecurity=1\n",
        TRIDENT_INPUT_HOOK_SHARED_MEMORY_NAME,
        mapping,
        createMappingError,
        sizeof(
            TridentHookSharedState
            )
    );

    if (mapping == nullptr)
    {
        return false;
    }

    if (createMappingError ==
        ERROR_ALREADY_EXISTS)
    {
        PrintQpcPrefix();

        std::printf(
            "CreateFileMapping rejected. "
            "Another Trident hook shared state "
            "already exists.\n"
        );

        CloseHandle(
            mapping
        );

        return false;
    }

    SetLastError(0);

    void* mappedView =
        MapViewOfFile(
            mapping,
            FILE_MAP_READ |
            FILE_MAP_WRITE,
            0,
            0,
            sizeof(
                TridentHookSharedState
                )
        );

    DWORD mapViewError =
        GetLastError();

    PrintQpcPrefix();

    std::printf(
        "MapViewOfFile "
        "Result=%p "
        "GetLastError=%lu "
        "Size=%zu\n",
        mappedView,
        mapViewError,
        sizeof(
            TridentHookSharedState
            )
    );

    if (mappedView == nullptr)
    {
        CloseHandle(
            mapping
        );

        return false;
    }

    TridentHookSharedState* sharedState =
        static_cast<
        TridentHookSharedState*
        >(
            mappedView
            );

    ZeroMemory(
        sharedState,
        sizeof(
            TridentHookSharedState
            )
    );

    sharedState->Signature =
        TRIDENT_INPUT_HOOK_SHARED_SIGNATURE;

    sharedState->Version =
        TRIDENT_INPUT_HOOK_SHARED_VERSION;

    sharedState->Capacity =
        TRIDENT_INPUT_HOOK_EVENT_CAPACITY;

    sharedState->Reserved =
        0;

    InterlockedExchange64(
        &sharedState->WriteSequence,
        0
    );

    g_TridentHookSharedMapping =
        mapping;

    g_TridentHookSharedState =
        sharedState;

    g_LastConsumedTridentHookSequence =
        0;

    g_TridentHookDroppedByReaderCount =
        0;

    PrintQpcPrefix();

    std::printf(
        "TridentHookSharedMemory initialized "
        "Signature=0x%08X "
        "Version=%u "
        "Capacity=%u\n",
        sharedState->Signature,
        sharedState->Version,
        sharedState->Capacity
    );

    return true;
}

static bool LoadAndInstallTridentObservationHooks()
{
    if (g_TridentHookSharedState == nullptr)
    {
        return false;
    }

    wchar_t dllPath[MAX_PATH] = {};

    if (!BuildTridentInputHookDllPath(
        dllPath,
        sizeof(dllPath) /
        sizeof(dllPath[0])
    ))
    {
        PrintQpcPrefix();

        std::printf(
            "BuildTridentInputHookDllPath failed. "
            "GetLastError=%lu\n",
            GetLastError()
        );

        return false;
    }

    SetLastError(0);

    HMODULE hookModule =
        LoadLibraryW(
            dllPath
        );

    DWORD loadLibraryError =
        GetLastError();

    PrintQpcPrefix();

    std::printf(
        "LoadLibraryW "
        "Path=\"%ls\" "
        "Result=%p "
        "GetLastError=%lu\n",
        dllPath,
        hookModule,
        loadLibraryError
    );

    if (hookModule == nullptr)
    {
        return false;
    }

    SetLastError(0);

    FARPROC installAddress =
        GetProcAddress(
            hookModule,
            "TridentInstallObservationHooks"
        );

    DWORD installAddressError =
        GetLastError();

    SetLastError(0);

    FARPROC uninstallAddress =
        GetProcAddress(
            hookModule,
            "TridentUninstallObservationHooks"
        );

    DWORD uninstallAddressError =
        GetLastError();

    PrintQpcPrefix();

    std::printf(
        "GetProcAddress "
        "InstallObservationHooks=%p "
        "InstallError=%lu "
        "UninstallObservationHooks=%p "
        "UninstallError=%lu\n",
        installAddress,
        installAddressError,
        uninstallAddress,
        uninstallAddressError
    );

    if (installAddress == nullptr ||
        uninstallAddress == nullptr)
    {
        FreeLibrary(
            hookModule
        );

        return false;
    }

    auto installFunction =
        reinterpret_cast<
        TridentInstallObservationHooksFunction
        >(
            installAddress
            );

    auto uninstallFunction =
        reinterpret_cast<
        TridentUninstallObservationHooksFunction
        >(
            uninstallAddress
            );

    SetLastError(0);

    BOOL installResult =
        installFunction();

    DWORD installError =
        GetLastError();

    LONG installAttempted =
        InterlockedCompareExchange(
            &g_TridentHookSharedState->
            InstallAttempted,
            0,
            0
        );

    LONG installSucceeded =
        InterlockedCompareExchange(
            &g_TridentHookSharedState->
            InstallSucceeded,
            0,
            0
        );

    LONG sharedInstallError =
        InterlockedCompareExchange(
            &g_TridentHookSharedState->
            InstallLastError,
            0,
            0
        );

    LONG64 installedCbtHookValue =
        InterlockedCompareExchange64(
            &g_TridentHookSharedState->
            InstalledCbtHookValue,
            0,
            0
        );

    LONG64 installedCallWndProcHookValue =
        InterlockedCompareExchange64(
            &g_TridentHookSharedState->
            InstalledCallWndProcHookValue,
            0,
            0
        );

    LONG64 installedGetMessageHookValue =
        InterlockedCompareExchange64(
            &g_TridentHookSharedState->
            InstalledGetMessageHookValue,
            0,
            0
        );

    PrintQpcPrefix();

    std::printf(
        "TridentInstallObservationHooks "
        "Result=%d "
        "GetLastError=%lu "
        "InstallAttempted=%ld "
        "InstallSucceeded=%ld "
        "SharedInstallLastError=%ld "
        "CbtHook=0x%016llX "
        "CallWndProcHook=0x%016llX "
        "GetMessageHook=0x%016llX\n",
        installResult ? 1 : 0,
        installError,
        installAttempted,
        installSucceeded,
        sharedInstallError,
        static_cast<unsigned long long>(
            installedCbtHookValue
            ),
        static_cast<unsigned long long>(
            installedCallWndProcHookValue
            ),
        static_cast<unsigned long long>(
            installedGetMessageHookValue
            )
    );

    if (!installResult)
    {
        FreeLibrary(
            hookModule
        );

        return false;
    }

    g_TridentInputHookModule =
        hookModule;

    g_TridentInstallObservationHooks =
        installFunction;

    g_TridentUninstallObservationHooks =
        uninstallFunction;

    g_AreTridentObservationHooksInstalled =
        true;

    return true;
}

static void DrainTridentHookEvents()
{
    TridentHookSharedState* sharedState =
        g_TridentHookSharedState;

    if (sharedState == nullptr)
    {
        return;
    }

    LONG64 newestSequence =
        InterlockedCompareExchange64(
            &sharedState->WriteSequence,
            0,
            0
        );

    if (newestSequence <=
        g_LastConsumedTridentHookSequence)
    {
        return;
    }

    LONG64 firstSequence =
        g_LastConsumedTridentHookSequence + 1;

    const LONG64 capacity =
        static_cast<LONG64>(
            TRIDENT_INPUT_HOOK_EVENT_CAPACITY
            );

    const LONG64 oldestAvailableSequence =
        newestSequence - capacity + 1;

    if (firstSequence <
        oldestAvailableSequence)
    {
        const LONG64 droppedCount =
            oldestAvailableSequence -
            firstSequence;

        g_TridentHookDroppedByReaderCount +=
            static_cast<uint64_t>(
                droppedCount
                );

        PrintQpcPrefix();

        std::printf(
            "TridentHookReaderOverrun "
            "DroppedNow=%lld "
            "DroppedTotal=%llu "
            "RequestedFirst=%lld "
            "OldestAvailable=%lld "
            "Newest=%lld\n",
            droppedCount,
            static_cast<unsigned long long>(
                g_TridentHookDroppedByReaderCount
                ),
            firstSequence,
            oldestAvailableSequence,
            newestSequence
        );

        firstSequence =
            oldestAvailableSequence;
    }

    for (LONG64 sequence = firstSequence;
        sequence <= newestSequence;
        ++sequence)
    {
        const std::uint32_t slotIndex =
            static_cast<std::uint32_t>(
                (sequence - 1) %
                TRIDENT_INPUT_HOOK_EVENT_CAPACITY
                );

        TridentHookEvent* sourceEvent =
            &sharedState->Events[
                slotIndex
            ];

        LONG64 committedBefore =
            InterlockedCompareExchange64(
                &sourceEvent->CommittedSequence,
                0,
                0
            );

        if (committedBefore != sequence)
        {
            continue;
        }

        TridentHookEvent eventCopy = {};

        eventCopy.Qpc =
            sourceEvent->Qpc;

        eventCopy.EventType =
            sourceEvent->EventType;

        eventCopy.HookCode =
            sourceEvent->HookCode;

        eventCopy.TargetHwnd =
            sourceEvent->TargetHwnd;

        eventCopy.OtherHwnd =
            sourceEvent->OtherHwnd;

        eventCopy.ThreadId =
            sourceEvent->ThreadId;

        eventCopy.ProcessId =
            sourceEvent->ProcessId;

        eventCopy.MouseActivation =
            sourceEvent->MouseActivation;

        eventCopy.CallWndProcSentByCurrentThread =
            sourceEvent->
            CallWndProcSentByCurrentThread;

        eventCopy.GetMessageRemoved =
            sourceEvent->GetMessageRemoved;

        eventCopy.Message =
            sourceEvent->Message;

        eventCopy.ReservedMessage =
            sourceEvent->ReservedMessage;

        eventCopy.MessageWParam =
            sourceEvent->MessageWParam;

        eventCopy.MessageLParam =
            sourceEvent->MessageLParam;

        eventCopy.CursorInfoValid =
            sourceEvent->CursorInfoValid;

        eventCopy.CursorFlags =
            sourceEvent->CursorFlags;

        eventCopy.CursorX =
            sourceEvent->CursorX;

        eventCopy.CursorY =
            sourceEvent->CursorY;

        MemoryBarrier();

        LONG64 committedAfter =
            InterlockedCompareExchange64(
                &sourceEvent->CommittedSequence,
                0,
                0
            );

        if (committedAfter != sequence)
        {
            continue;
        }

        const TridentHookEventType eventType =
            static_cast<
            TridentHookEventType
            >(
                eventCopy.EventType
                );

        HWND targetWindow =
            reinterpret_cast<HWND>(
                static_cast<ULONG_PTR>(
                    eventCopy.TargetHwnd
                    )
                );

        HWND otherWindow =
            reinterpret_cast<HWND>(
                static_cast<ULONG_PTR>(
                    eventCopy.OtherHwnd
                    )
                );

        if (eventType ==
            TridentHookEventType::
            CallWndProcMessage)
        {
            const UINT message =
                eventCopy.Message;

            std::printf(
                "[QPC=%lld Freq=10000000] "
                "TridentCallWndProc "
                "Sequence=%lld "
                "HookCode=%d "
                "Message=%s "
                "MessageValue=0x%04X "
                "TargetHwnd=%p "
                "ThreadId=%lu "
                "ProcessId=%lu "
                "SentByCurrentThread=%u "
                "MessageWParam=0x%016llX "
                "MessageLParam=0x%016llX "
                "CursorInfoValid=%u "
                "CursorFlags=0x%08X "
                "CursorPos=(%ld,%ld) "
                "ReaderDroppedTotal=%llu\n",
                eventCopy.Qpc,
                sequence,
                eventCopy.HookCode,
                GetObservedWindowMessageName(
                    message
                ),
                message,
                targetWindow,
                eventCopy.ThreadId,
                eventCopy.ProcessId,
                eventCopy.
                CallWndProcSentByCurrentThread,
                static_cast<unsigned long long>(
                    eventCopy.MessageWParam
                    ),
                static_cast<unsigned long long>(
                    eventCopy.MessageLParam
                    ),
                eventCopy.CursorInfoValid,
                eventCopy.CursorFlags,
                eventCopy.CursorX,
                eventCopy.CursorY,
                static_cast<unsigned long long>(
                    g_TridentHookDroppedByReaderCount
                    )
            );

            PrintWindowIdentityForLog(
                "TridentCallWndProc.Target",
                targetWindow
            );
        }
        else if (eventType ==
            TridentHookEventType::
            GetMessage)
        {
            const UINT message =
                eventCopy.Message;

            std::printf(
                "[QPC=%lld Freq=10000000] "
                "TridentGetMessage "
                "Sequence=%lld "
                "HookCode=%d "
                "Message=%s "
                "MessageValue=0x%04X "
                "TargetHwnd=%p "
                "ThreadId=%lu "
                "ProcessId=%lu "
                "Removed=%u "
                "MessageWParam=0x%016llX "
                "MessageLParam=0x%016llX "
                "CursorInfoValid=%u "
                "CursorFlags=0x%08X "
                "CursorPos=(%ld,%ld) "
                "ReaderDroppedTotal=%llu\n",
                eventCopy.Qpc,
                sequence,
                eventCopy.HookCode,
                GetObservedWindowMessageName(
                    message
                ),
                message,
                targetWindow,
                eventCopy.ThreadId,
                eventCopy.ProcessId,
                eventCopy.GetMessageRemoved,
                static_cast<unsigned long long>(
                    eventCopy.MessageWParam
                    ),
                static_cast<unsigned long long>(
                    eventCopy.MessageLParam
                    ),
                eventCopy.CursorInfoValid,
                eventCopy.CursorFlags,
                eventCopy.CursorX,
                eventCopy.CursorY,
                static_cast<unsigned long long>(
                    g_TridentHookDroppedByReaderCount
                    )
            );

            PrintWindowIdentityForLog(
                "TridentGetMessage.Target",
                targetWindow
            );
        }
        else
        {
            std::printf(
                "[QPC=%lld Freq=10000000] "
                "TridentCbt "
                "Sequence=%lld "
                "Event=%s "
                "HookCode=%d "
                "TargetHwnd=%p "
                "OtherHwnd=%p "
                "ThreadId=%lu "
                "ProcessId=%lu "
                "MouseActivation=%u "
                "CursorInfoValid=%u "
                "CursorFlags=0x%08X "
                "CursorPos=(%ld,%ld) "
                "ReaderDroppedTotal=%llu\n",
                eventCopy.Qpc,
                sequence,
                GetTridentHookEventTypeName(
                    eventType
                ),
                eventCopy.HookCode,
                targetWindow,
                otherWindow,
                eventCopy.ThreadId,
                eventCopy.ProcessId,
                eventCopy.MouseActivation,
                eventCopy.CursorInfoValid,
                eventCopy.CursorFlags,
                eventCopy.CursorX,
                eventCopy.CursorY,
                static_cast<unsigned long long>(
                    g_TridentHookDroppedByReaderCount
                    )
            );

            PrintWindowIdentityForLog(
                "TridentCbt.Target",
                targetWindow
            );

            if (otherWindow != nullptr)
            {
                PrintWindowIdentityForLog(
                    "TridentCbt.Other",
                    otherWindow
                );
            }
        }

        g_LastConsumedTridentHookSequence =
            sequence;
    }
}

static void ShutdownTridentObservation()
{
    if (g_AreTridentObservationHooksInstalled &&
        g_TridentUninstallObservationHooks !=
        nullptr)
    {
        SetLastError(0);

        BOOL uninstallResult =
            g_TridentUninstallObservationHooks();

        DWORD uninstallError =
            GetLastError();

        PrintQpcPrefix();

        std::printf(
            "TridentUninstallObservationHooks "
            "Result=%d "
            "GetLastError=%lu\n",
            uninstallResult ? 1 : 0,
            uninstallError
        );

        if (uninstallResult)
        {
            g_AreTridentObservationHooksInstalled =
                false;
        }
    }

    if (g_TridentInputHookModule != nullptr)
    {
        PrintQpcPrefix();

        std::printf(
            "TridentInputHook module retained "
            "until process termination. "
            "Module=%p HooksInstalled=%d\n",
            g_TridentInputHookModule,
            g_AreTridentObservationHooksInstalled ?
            1 :
            0
        );
    }

    if (g_TridentHookSharedState != nullptr)
    {
        SetLastError(0);

        BOOL unmapResult =
            UnmapViewOfFile(
                g_TridentHookSharedState
            );

        DWORD unmapError =
            GetLastError();

        PrintQpcPrefix();

        std::printf(
            "UnmapViewOfFile TridentHook "
            "Result=%d "
            "GetLastError=%lu\n",
            unmapResult ? 1 : 0,
            unmapError
        );

        if (unmapResult)
        {
            g_TridentHookSharedState =
                nullptr;
        }
    }

    if (g_TridentHookSharedMapping != nullptr)
    {
        SetLastError(0);

        BOOL closeMappingResult =
            CloseHandle(
                g_TridentHookSharedMapping
            );

        DWORD closeMappingError =
            GetLastError();

        PrintQpcPrefix();

        std::printf(
            "CloseHandle TridentHookMapping "
            "Result=%d "
            "GetLastError=%lu\n",
            closeMappingResult ? 1 : 0,
            closeMappingError
        );

        if (closeMappingResult)
        {
            g_TridentHookSharedMapping =
                nullptr;
        }
    }
}

static void PrintCursorFlagsChangedDetail()
{
    CURSORINFO cursorInfo = {};
    cursorInfo.cbSize = sizeof(CURSORINFO);

    SetLastError(0);

    BOOL cursorInfoResult = GetCursorInfo(&cursorInfo);
    DWORD cursorInfoError = GetLastError();

    POINT cursorPosition = {};

    SetLastError(0);

    BOOL cursorPosResult = GetCursorPos(&cursorPosition);
    DWORD cursorPosError = GetLastError();

    HWND windowFromCursorPoint = nullptr;

    if (cursorPosResult)
    {
        windowFromCursorPoint = WindowFromPoint(cursorPosition);
    }

    HWND rootWindow = nullptr;
    HWND rootOwnerWindow = nullptr;

    if (windowFromCursorPoint != nullptr)
    {
        rootWindow = GetAncestor(windowFromCursorPoint, GA_ROOT);
        rootOwnerWindow = GetAncestor(windowFromCursorPoint, GA_ROOTOWNER);
    }

    HWND foregroundWindow = GetForegroundWindow();
    HWND desktopWindow = GetDesktopWindow();
    HWND shellWindow = GetShellWindow();

    HWND foregroundRootWindow = nullptr;
    HWND foregroundRootOwnerWindow = nullptr;

    if (foregroundWindow != nullptr)
    {
        foregroundRootWindow = GetAncestor(foregroundWindow, GA_ROOT);
        foregroundRootOwnerWindow = GetAncestor(foregroundWindow, GA_ROOTOWNER);
    }

    GUITHREADINFO guiThreadInfo = {};
    guiThreadInfo.cbSize = sizeof(GUITHREADINFO);

    SetLastError(0);

    BOOL guiResult = GetGUIThreadInfo(
        0,
        &guiThreadInfo
    );

    DWORD guiError = GetLastError();

    HWND focusParentWindow = nullptr;
    HWND focusRootWindow = nullptr;
    HWND focusRootOwnerWindow = nullptr;

    if (guiThreadInfo.hwndFocus != nullptr)
    {
        focusParentWindow = GetParent(guiThreadInfo.hwndFocus);
        focusRootWindow = GetAncestor(guiThreadInfo.hwndFocus, GA_ROOT);
        focusRootOwnerWindow = GetAncestor(guiThreadInfo.hwndFocus, GA_ROOTOWNER);
    }

    PrintQpcPrefix();

    std::printf(
        "CursorFlagsChangedDetail "
        "CursorInfoResult=%d CursorInfoError=%lu "
        "CursorFlags=0x%08lX CursorInfoPos=(%ld,%ld) hCursor=%p "
        "CursorPosResult=%d CursorPosError=%lu CursorPos=(%ld,%ld) "
        "ForegroundHwnd=%p "
        "WindowFromPoint=%p RootWindow=%p RootOwnerWindow=%p "
        "DesktopWindow=%p ShellWindow=%p "
        "ForegroundRootWindow=%p ForegroundRootOwnerWindow=%p "
        "GetGUIThreadInfoResult=%d GetGUIThreadInfoError=%lu "
        "FocusHwnd=%p FocusParent=%p FocusRoot=%p FocusRootOwner=%p\n",
        cursorInfoResult ? 1 : 0,
        cursorInfoError,
        cursorInfo.flags,
        cursorInfo.ptScreenPos.x,
        cursorInfo.ptScreenPos.y,
        cursorInfo.hCursor,
        cursorPosResult ? 1 : 0,
        cursorPosError,
        cursorPosition.x,
        cursorPosition.y,
        foregroundWindow,
        windowFromCursorPoint,
        rootWindow,
        rootOwnerWindow,
        desktopWindow,
        shellWindow,
        foregroundRootWindow,
        foregroundRootOwnerWindow,
        guiResult ? 1 : 0,
        guiError,
        guiThreadInfo.hwndFocus,
        focusParentWindow,
        focusRootWindow,
        focusRootOwnerWindow
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.Foreground",
        foregroundWindow
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.ForegroundRoot",
        foregroundRootWindow
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.ForegroundRootOwner",
        foregroundRootOwnerWindow
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.WindowFromPoint",
        windowFromCursorPoint
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.RootWindow",
        rootWindow
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.RootOwnerWindow",
        rootOwnerWindow
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.DesktopWindow",
        desktopWindow
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.ShellWindow",
        shellWindow
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.Focus",
        guiThreadInfo.hwndFocus
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.FocusParent",
        focusParentWindow
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.FocusRoot",
        focusRootWindow
    );

    PrintWindowIdentityForLog(
        "CursorFlagsChangedDetail.FocusRootOwner",
        focusRootOwnerWindow
    );
}

static const char* GetPointerInputTypeName(POINTER_INPUT_TYPE type)
{
    switch (type)
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

static const char* GetPointerMessageName(UINT message)
{
    switch (message)
    {
    case WM_POINTERENTER:
        return "WM_POINTERENTER";
    case WM_POINTERLEAVE:
        return "WM_POINTERLEAVE";
    case WM_POINTERUPDATE:
        return "WM_POINTERUPDATE";
    case WM_POINTERDOWN:
        return "WM_POINTERDOWN";
    case WM_POINTERUP:
        return "WM_POINTERUP";
    case WM_POINTERCAPTURECHANGED:
        return "WM_POINTERCAPTURECHANGED";
    default:
        return "WM_POINTER_UNKNOWN";
    }
}

static void PrintPointerSnapshot(
    const char* label,
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    UINT32 pointerId = GET_POINTERID_WPARAM(wParam);

    POINTER_INPUT_TYPE pointerType = PT_POINTER;

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

    PrintQpcPrefix();

    std::printf(
        "%s "
        "Message=%s hwnd=%p wParam=0x%p lParam=0x%p "
        "PointerId=%u "
        "PointerTypeResult=%d PointerTypeError=%lu PointerType=%s(%u) "
        "PointerInfoResult=%d PointerInfoError=%lu "
        "PointerFlags=0x%08lX "
        "PointerPixel=(%ld,%ld) "
        "PointerHimetric=(%ld,%ld) "
        "TouchActive=%d CooldownActive=%d "
        "RawMoveCount=%llu RawButtonCount=%llu\n",
        label,
        GetPointerMessageName(message),
        hwnd,
        reinterpret_cast<void*>(wParam),
        reinterpret_cast<void*>(lParam),
        pointerId,
        pointerTypeResult ? 1 : 0,
        pointerTypeError,
        GetPointerInputTypeName(pointerType),
        static_cast<unsigned int>(pointerType),
        pointerInfoResult ? 1 : 0,
        pointerInfoError,
        pointerInfo.pointerFlags,
        pointerInfo.ptPixelLocation.x,
        pointerInfo.ptPixelLocation.y,
        pointerInfo.ptHimetricLocation.x,
        pointerInfo.ptHimetricLocation.y,
        g_IsTouchActive ? 1 : 0,
        g_IsTouchRecoveryCooldownActive ? 1 : 0,
        static_cast<unsigned long long>(g_RawMouseMoveCount),
        static_cast<unsigned long long>(g_RawMouseButtonCount)
    );

    PrintGuiFocusSnapshot(label);
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

static void RegisterPointerInputTargetForObservation(HWND hwnd)
{
    SetLastError(0);

    BOOL touchResult = RegisterPointerInputTarget(
        hwnd,
        PT_TOUCH
    );

    DWORD touchError = GetLastError();

    PrintQpcPrefix();

    std::printf(
        "RegisterPointerInputTarget PT_TOUCH Result=%d GetLastError=%lu hwnd=%p\n",
        touchResult ? 1 : 0,
        touchError,
        hwnd
    );

    SetLastError(0);

    BOOL penResult = RegisterPointerInputTarget(
        hwnd,
        PT_PEN
    );

    DWORD penError = GetLastError();

    PrintQpcPrefix();

    std::printf(
        "RegisterPointerInputTarget PT_PEN Result=%d GetLastError=%lu hwnd=%p\n",
        penResult ? 1 : 0,
        penError,
        hwnd
    );
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

static void HandleRawInput(LPARAM lParam,  LPARAM messageExtraInfo)
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
        PrintRawInputDeviceIdentityOnce(
            raw->header
        );

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

        ULONG_PTR extraInfoValue =
            static_cast<ULONG_PTR>(
                messageExtraInfo
                );

        bool hasMiWpSignature =
            (extraInfoValue & g_MiWpSignatureMask) ==
            g_MiWpSignature;

        bool hasTouchSignature =
            hasMiWpSignature &&
            (extraInfoValue & g_MiWpTouchMask) != 0;

        bool hasPenSignature =
            hasMiWpSignature &&
            !hasTouchSignature;

        RawMouseClassification classification =
            ClassifyRawMouseInput(
                raw->header,
                mouse,
                hasMiWpSignature,
                hasTouchSignature
            );

        const char* classificationName =
            GetRawMouseClassificationName(
                classification
            );

        bool isConfirmedTouchPromotion =
            classification ==
            RawMouseClassification::
            ConfirmedTouchPromotion;

        bool legacyAndNewDetectionMatch =
            isAbsolutePromotedMouse ==
            isConfirmedTouchPromotion;

        PrintQpcPrefix();

        std::printf(
            "RawEnter "
            "DeviceHandle=%p "
            "DeviceHandleNull=%d "
            "HeaderType=%lu "
            "HeaderSize=%lu "
            "InputCode=0x%p "
            "MessageExtraInfo=0x%p "
            "MiWpSignature=%d "
            "MiWpTouch=%d "
            "MiWpPen=%d "
            "LegacyAbsolutePromoted=%d "
            "ConfirmedTouchPromotion=%d "
            "LegacyAndNewMatch=%d "
            "Classification=%s "
            "Flags=0x%04X "
            "ButtonFlags=0x%04X "
            "LastX=%ld LastY=%ld "
            "TouchActive=%d "
            "CooldownActive=%d "
            "ProtectionActive=%d\n",
            raw->header.hDevice,
            raw->header.hDevice == nullptr ? 1 : 0,
            raw->header.dwType,
            raw->header.dwSize,
            reinterpret_cast<void*>(
                raw->header.wParam
                ),
            reinterpret_cast<void*>(
                extraInfoValue
                ),
            hasMiWpSignature ? 1 : 0,
            hasTouchSignature ? 1 : 0,
            hasPenSignature ? 1 : 0,
            isAbsolutePromotedMouse ? 1 : 0,
            isConfirmedTouchPromotion ? 1 : 0,
            legacyAndNewDetectionMatch ? 1 : 0,
            classificationName,
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
            PrintGuiFocusSnapshot("BeforeTouchDown");

            g_IsTouchActive = true;

            PrintQpcPrefix();
            std::printf(
                "TouchActive=1 by AbsolutePromoted LeftButtonDown "
                "LastRealForegroundHwnd=%p LastRealForegroundThreadId=%lu LastRealForegroundProcessId=%lu LastRealForegroundTitle=\"%ls\"\n",
                g_LastRealForegroundWindow,
                g_LastRealForegroundThreadId,
                g_LastRealForegroundProcessId,
                g_LastRealForegroundTitle
            );

            PrintGuiFocusSnapshot("AfterTouchDown");
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
        PrintGuiFocusSnapshot("MARK(F8)");
    }

    if (!g_HasPreviousCursor)
    {
        g_PreviousCursor = current;
        g_HasPreviousCursor = true;

        PrintCursorSnapshot("START", current);
        PrintGuiFocusSnapshot("START");
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

        PrintGuiFocusSnapshot("CursorFlagsChanged");
        PrintCursorFlagsChangedDetail();
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
        if (!RegisterRawMouse(
            hwnd
        ))
        {
            std::printf(
                "RegisterRawInputDevices failed. "
                "GetLastError=%lu\n",
                GetLastError()
            );

            PostQuitMessage(1);
            return -1;
        }

        RegisterPointerInputTargetForObservation(
            hwnd
        );

        if (!RegisterWinEventHooks())
        {
            PrintQpcPrefix();

            std::printf(
                "RegisterWinEventHooks failed. "
                "ForegroundHook=%p "
                "FocusHook=%p\n",
                g_ForegroundWinEventHook,
                g_FocusWinEventHook
            );

            PostQuitMessage(1);
            return -1;
        }

        if (!InitializeTridentHookSharedMemory())
        {
            PrintQpcPrefix();

            std::printf(
                "InitializeTridentHookSharedMemory "
                "failed.\n"
            );

            UnregisterWinEventHooks();

            PostQuitMessage(1);
            return -1;
        }

        if (!LoadAndInstallTridentObservationHooks())
        {
            PrintQpcPrefix();

            std::printf(
                "LoadAndInstallTridentObservationHooks "
                "failed.\n"
            );

            ShutdownTridentObservation();
            UnregisterWinEventHooks();

            PostQuitMessage(1);
            return -1;
        }

        SetTimer(
            hwnd,
            1,
            5,
            nullptr
        );

        return 0;

    case WM_POINTERENTER:
        PrintPointerSnapshot(
            "PointerEnter",
            hwnd,
            message,
            wParam,
            lParam
        );
        break;

    case WM_POINTERLEAVE:
        PrintPointerSnapshot(
            "PointerLeave",
            hwnd,
            message,
            wParam,
            lParam
        );
        break;

    case WM_POINTERDOWN:
        PrintPointerSnapshot(
            "PointerDown",
            hwnd,
            message,
            wParam,
            lParam
        );
        break;

    case WM_POINTERUPDATE:
        PrintPointerSnapshot(
            "PointerUpdate",
            hwnd,
            message,
            wParam,
            lParam
        );
        break;

    case WM_POINTERUP:
        PrintPointerSnapshot(
            "PointerUp",
            hwnd,
            message,
            wParam,
            lParam
        );
        break;

    case WM_POINTERCAPTURECHANGED:
        PrintPointerSnapshot(
            "PointerCaptureChanged",
            hwnd,
            message,
            wParam,
            lParam
        );
        break;

    case WM_INPUT:
    {
        const LPARAM messageExtraInfo =
            GetMessageExtraInfo();

        HandleRawInput(
            lParam,
            messageExtraInfo
        );

        return 0;
    }

    case WM_TIMER:
        DrainTridentHookEvents();
        PollCursorAndKeys();
        return 0;

    case WM_DESTROY:
        KillTimer(
            hwnd,
            1
        );

        DrainTridentHookEvents();

        ShutdownTridentObservation();

        UnregisterWinEventHooks();

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(
        hwnd,
        message,
        wParam,
        lParam
    );
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