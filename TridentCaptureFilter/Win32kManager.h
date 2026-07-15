#pragma once

#include <ntddk.h>

typedef enum _TRIDENT_WIN32K_STATE
{
    TridentWin32kStateUninitialized = 0,
    TridentWin32kStateUnsupportedBuild,
    TridentWin32kStateModuleUnavailable,
    TridentWin32kStateSignatureUnavailable,
    TridentWin32kStateReady,
    TridentWin32kStateFailed
} TRIDENT_WIN32K_STATE;

typedef struct _TRIDENT_WIN32K_STATUS
{
    RTL_OSVERSIONINFOW OsVersion;
    TRIDENT_WIN32K_STATE State;
    NTSTATUS LastStatus;
    PVOID ModuleBase;
    ULONG ModuleSize;
    PVOID TextBase;
    SIZE_T TextSize;
    PVOID FunctionAddress;
    PVOID CandidateAddress;
    PVOID SkipTargetAddress;
    ULONG FunctionRva;
    ULONG CandidateRva;
    ULONG SkipTargetRva;
} TRIDENT_WIN32K_STATUS, *PTRIDENT_WIN32K_STATUS;

namespace TridentWin32kManager
{
    NTSTATUS Initialize();
    VOID Shutdown();
    VOID QueryStatus(_Out_ PTRIDENT_WIN32K_STATUS Status);
}
