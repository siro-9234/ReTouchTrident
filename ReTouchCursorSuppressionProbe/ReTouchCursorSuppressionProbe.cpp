#include <windows.h>
#include <conio.h>
#include <iostream>

using EnableMouseInputForCursorSuppressionFn =
BOOL(WINAPI*)(BOOL enable);

static void PrintCursorState()
{
    CURSORINFO info = {};
    info.cbSize = sizeof(info);

    if (!GetCursorInfo(&info))
    {
        std::cout
            << "GetCursorInfo failed. Error="
            << GetLastError()
            << '\n';

        return;
    }

    std::cout
        << "CursorFlags=0x"
        << std::hex
        << info.flags
        << std::dec
        << " Showing="
        << ((info.flags & CURSOR_SHOWING) != 0)
        << " Suppressed="
        << ((info.flags & CURSOR_SUPPRESSED) != 0)
        << '\n';
}

int main()
{
    HMODULE win32u =
        LoadLibraryW(
            L"win32u.dll"
        );

    if (win32u == nullptr)
    {
        std::cout
            << "LoadLibraryW(win32u.dll) failed. Error="
            << GetLastError()
            << '\n';

        return 1;
    }

    SetLastError(0);

    auto enableMouseInputForCursorSuppression =
        reinterpret_cast<
        EnableMouseInputForCursorSuppressionFn
        >(
            GetProcAddress(
                win32u,
                "NtUserEnableMouseInputForCursorSuppression"
            )
            );

    if (enableMouseInputForCursorSuppression == nullptr)
    {
        std::cout
            << "Export not found in win32u.dll. Error="
            << GetLastError()
            << '\n';

        FreeLibrary(
            win32u
        );

        return 1;
    }

    std::cout
        << "win32u!NtUserEnableMouseInputForCursorSuppression="
        << reinterpret_cast<void*>(
            enableMouseInputForCursorSuppression
            )
        << '\n';

    std::cout
        << "Initial state:\n";

    PrintCursorState();

    std::cout
        << "\nPress 1: call FALSE\n"
        << "Press 2: call TRUE\n"
        << "Press S: show cursor state\n"
        << "Press ESC: exit\n";

    for (;;)
    {
        const int key =
            _getwch();

        if (key == 27)
        {
            break;
        }

        if (key == L'1')
        {
            SetLastError(0);

            const BOOL result =
                enableMouseInputForCursorSuppression(
                    FALSE
                );

            const DWORD error =
                GetLastError();

            std::cout
                << "\nCall(FALSE) Result="
                << result
                << " Error="
                << error
                << '\n';

            PrintCursorState();
        }
        else if (key == L'2')
        {
            SetLastError(0);

            const BOOL result =
                enableMouseInputForCursorSuppression(
                    TRUE
                );

            const DWORD error =
                GetLastError();

            std::cout
                << "\nCall(TRUE) Result="
                << result
                << " Error="
                << error
                << '\n';

            PrintCursorState();
        }
        else if (key == L's' ||
            key == L'S')
        {
            std::cout << '\n';

            PrintCursorState();
        }
    }

    FreeLibrary(
        win32u
    );

    return 0;
}