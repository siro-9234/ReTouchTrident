#include <windows.h>
#include <cstdio>

struct CursorSnapshot
{
    LARGE_INTEGER Counter;
    LARGE_INTEGER Frequency;
    double TimeMs;
    POINT Position;
    DWORD Flags;
};

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

    LARGE_INTEGER counter = {};
    LARGE_INTEGER frequency = {};

    if (!QueryPerformanceFrequency(&frequency))
    {
        return false;
    }

    if (!QueryPerformanceCounter(&counter))
    {
        return false;
    }

    static LARGE_INTEGER startCounter = counter;

    snapshot->Counter = counter;
    snapshot->Frequency = frequency;

    snapshot->TimeMs =
        static_cast<double>(counter.QuadPart - startCounter.QuadPart) * 1000.0 /
        static_cast<double>(frequency.QuadPart);

    snapshot->Position = cursorInfo.ptScreenPos;
    snapshot->Flags = cursorInfo.flags;

    return true;
}

static bool IsSamePosition(const POINT& a, const POINT& b)
{
    return a.x == b.x && a.y == b.y;
}

static void PrintSnapshotHeader()
{
    std::printf("ReTouchCursorProbe Event Mode QPC\n");
    std::printf("Press F8 to mark before touch.\n");
    std::printf("Press ESC to exit.\n\n");
}

static void PrintMarkEvent(const CursorSnapshot& snapshot)
{
    std::printf("[QPC=%lld Freq=%lld Elapsed=%.3f] MARK(F8) Pos=(%ld,%ld) Flags=0x%08lX\n",
        snapshot.Counter.QuadPart,
        snapshot.Frequency.QuadPart,
        snapshot.TimeMs,
        snapshot.Position.x,
        snapshot.Position.y,
        snapshot.Flags);
}

static void PrintStartEvent(const CursorSnapshot& snapshot)
{
    std::printf("[QPC=%lld Freq=%lld Elapsed=%.3f] START Pos=(%ld,%ld) Flags=0x%08lX\n",
        snapshot.Counter.QuadPart,
        snapshot.Frequency.QuadPart,
        snapshot.TimeMs,
        snapshot.Position.x,
        snapshot.Position.y,
        snapshot.Flags);
}

static void PrintFlagsChangedEvent(
    const CursorSnapshot& previous,
    const CursorSnapshot& current)
{
    UNREFERENCED_PARAMETER(previous);

    std::printf("[QPC=%lld Freq=%lld Elapsed=%.3f] CursorFlags : 0x%08lX -> 0x%08lX Pos=(%ld,%ld)\n",
        current.Counter.QuadPart,
        current.Frequency.QuadPart,
        current.TimeMs,
        previous.Flags,
        current.Flags,
        current.Position.x,
        current.Position.y);
}

static void PrintPositionChangedEvent(
    const CursorSnapshot& previous,
    const CursorSnapshot& current)
{
    std::printf("[QPC=%lld Freq=%lld Elapsed=%.3f] CursorPos   : (%ld,%ld) -> (%ld,%ld) Flags=0x%08lX\n",
        current.Counter.QuadPart,
        current.Frequency.QuadPart,
        current.TimeMs,
        previous.Position.x,
        previous.Position.y,
        current.Position.x,
        current.Position.y,
        current.Flags);
}

static bool WasKeyPressedOnce(int virtualKey)
{
    return (GetAsyncKeyState(virtualKey) & 0x0001) != 0;
}

int main()
{
    PrintSnapshotHeader();

    CursorSnapshot previous = {};

    if (!GetCursorSnapshot(&previous))
    {
        std::printf("Initial GetCursorSnapshot failed. GetLastError=%lu\n", GetLastError());
        return 1;
    }

    PrintStartEvent(previous);

    while (true)
    {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            break;
        }

        CursorSnapshot current = {};

        if (!GetCursorSnapshot(&current))
        {
            std::printf("GetCursorSnapshot failed. GetLastError=%lu\n", GetLastError());
            return 1;
        }

        if (WasKeyPressedOnce(VK_F8))
        {
            PrintMarkEvent(current);
        }

        if (previous.Flags != current.Flags)
        {
            PrintFlagsChangedEvent(previous, current);
        }

        if (!IsSamePosition(previous.Position, current.Position))
        {
            PrintPositionChangedEvent(previous, current);
        }

        previous = current;

        Sleep(5);
    }

    std::printf("ReTouchCursorProbe stopped.\n");
    return 0;
}