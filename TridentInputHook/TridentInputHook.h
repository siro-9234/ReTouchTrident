#pragma once

#include <windows.h>
#include <cstdint>
#include <type_traits>

constexpr wchar_t
TRIDENT_INPUT_HOOK_SHARED_MEMORY_NAME[] =
L"Local\\ReTouchTridentInputHook.SharedState.v1";

constexpr std::uint32_t
TRIDENT_INPUT_HOOK_SHARED_SIGNATURE =
0x31494854;
// ASCII little-endian: "THI1"

constexpr std::uint32_t
TRIDENT_INPUT_HOOK_SHARED_VERSION =
1;

constexpr std::uint32_t
TRIDENT_INPUT_HOOK_EVENT_CAPACITY =
256;

enum class TridentHookEventType : std::uint32_t
{
    None = 0,
    CbtActivate = 1,
    CbtSetFocus = 2
};

struct alignas(8) TridentHookEvent
{
    volatile LONG64 CommittedSequence;

    LONG64 Qpc;

    std::uint32_t EventType;
    std::int32_t HookCode;

    std::uint64_t TargetHwnd;
    std::uint64_t OtherHwnd;

    std::uint32_t ThreadId;
    std::uint32_t ProcessId;

    std::uint32_t MouseActivation;

    std::uint32_t CursorInfoValid;
    std::uint32_t CursorFlags;

    std::int32_t CursorX;
    std::int32_t CursorY;
};

struct alignas(8) TridentHookSharedState
{
    std::uint32_t Signature;
    std::uint32_t Version;
    std::uint32_t Capacity;
    std::uint32_t Reserved;

    volatile LONG InstallAttempted;
    volatile LONG InstallSucceeded;
    volatile LONG InstallLastError;
    volatile LONG64 InstalledHookValue;

    volatile LONG64 WriteSequence;

    TridentHookEvent
        Events[TRIDENT_INPUT_HOOK_EVENT_CAPACITY];
};

static_assert(
    std::is_standard_layout_v<
    TridentHookEvent
    >,
    "TridentHookEvent must use a stable shared-memory layout."
    );

static_assert(
    std::is_standard_layout_v<
    TridentHookSharedState
    >,
    "TridentHookSharedState must use a stable shared-memory layout."
    );

extern "C"
{
    __declspec(dllexport)
        BOOL WINAPI TridentInstallCbtHook();

    __declspec(dllexport)
        BOOL WINAPI TridentUninstallCbtHook();
}