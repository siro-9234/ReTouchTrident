#include <windows.h>
#include <stdio.h>

#define IOCTL_TRIDENT_GET_GLOBAL_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_READ_DATA)

typedef struct _TRIDENT_GLOBAL_STATS_SNAPSHOT
{
    LONG ControlDeviceCreateAttempt;
    LONG ControlDeviceCreateSucceeded;
    LONG ControlDeviceIoctlStatsCount;

    LONG LastDeviceInstanceId;
    LONG LastClientInstanceId;

    LONG ReadCompletionCount;
    LONG DecodeSuccessCount;
    LONG SubmitAttemptCount;
    LONG SubmitCompletedCount;
    LONG SubmitFrameCount;

    LONG WorkItemEnqueueCount;
    LONG WorkItemRunCount;
    LONG LastWorkItemContactCount;
    LONG LastSubmitFrameStatus;
    LONG LastSendFrameStatus;

    LONG OpenInterfaceAttemptCount;
    LONG OpenInterfaceSucceededCount;
    LONG LastOpenInterfaceStatus;
    LONG LastOpenedRootSystemNumber;

    LONG LastReadDataLength;
    LONG LastRawByte0;
    LONG LastRawByte1;
    LONG LastRawByte2;
    LONG LastRawByte3;
    LONG LastRawByte4;
    LONG LastRawByte5;
    LONG LastRawByte6;
    LONG LastRawByte7;
    LONG LastRawByte8;
    LONG LastRawByte9;
    LONG LastRawByte10;
    LONG LastRawByte11;
    LONG LastRawByte12;
    LONG LastRawByte13;
    LONG LastRawByte14;
    LONG LastRawByte15;

    LONG TipCandidateByte0Bit0;
    LONG TipCandidateByte1Bit0;
    LONG TipCandidateByte2Bit0;
    LONG TipCandidateByte6Bit0;
    LONG TipCandidateByte7Bit0;
    LONG LastDecodedTipSwitch;

    ULONG_PTR LastDeviceContextPointer;
    ULONG_PTR LastClientPointer;
    ULONG_PTR LastPhysicalDeviceObject;
    ULONG_PTR LastWdmDeviceObject;
} TRIDENT_GLOBAL_STATS_SNAPSHOT, * PTRIDENT_GLOBAL_STATS_SNAPSHOT;

static BOOL ReadGlobalStatsFromControlDevice()
{
    HANDLE device = CreateFileW(
        L"\\\\.\\TridentCaptureControl",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (device == INVALID_HANDLE_VALUE)
    {
        wprintf(L"CreateFileW failed. GetLastError=%lu\n", GetLastError());
        return FALSE;
    }

    TRIDENT_GLOBAL_STATS_SNAPSHOT stats = {};
    DWORD bytesReturned = 0;

    BOOL ok = DeviceIoControl(
        device,
        IOCTL_TRIDENT_GET_GLOBAL_STATS,
        nullptr,
        0,
        &stats,
        sizeof(stats),
        &bytesReturned,
        nullptr
    );

    if (!ok)
    {
        wprintf(L"DeviceIoControl failed. GetLastError=%lu\n", GetLastError());
        CloseHandle(device);
        return FALSE;
    }

    wprintf(L"\n=== Trident Global Stats ===\n");
    wprintf(L"BytesReturned:                   %lu\n", bytesReturned);

    wprintf(L"\n[Control Device]\n");
    wprintf(L"ControlDeviceCreateAttempt:      %ld\n", stats.ControlDeviceCreateAttempt);
    wprintf(L"ControlDeviceCreateSucceeded:    %ld\n", stats.ControlDeviceCreateSucceeded);
    wprintf(L"ControlDeviceIoctlStatsCount:    %ld\n", stats.ControlDeviceIoctlStatsCount);

    wprintf(L"\n[Last Active Filter]\n");
    wprintf(L"LastDeviceInstanceId:            %ld\n", stats.LastDeviceInstanceId);
    wprintf(L"LastClientInstanceId:            %ld\n", stats.LastClientInstanceId);

    wprintf(L"\n[Counts]\n");
    wprintf(L"ReadCompletionCount:             %ld\n", stats.ReadCompletionCount);
    wprintf(L"DecodeSuccessCount:              %ld\n", stats.DecodeSuccessCount);
    wprintf(L"SubmitAttemptCount:              %ld\n", stats.SubmitAttemptCount);
    wprintf(L"SubmitCompletedCount:            %ld\n", stats.SubmitCompletedCount);
    wprintf(L"SubmitFrameCount:                %ld\n", stats.SubmitFrameCount);

    wprintf(L"\n[WorkItem]\n");
    wprintf(L"WorkItemEnqueueCount:            %ld\n", stats.WorkItemEnqueueCount);
    wprintf(L"WorkItemRunCount:                %ld\n", stats.WorkItemRunCount);
    wprintf(L"LastWorkItemContactCount:        %ld\n", stats.LastWorkItemContactCount);
    wprintf(L"LastSubmitFrameStatus:           0x%08X\n", static_cast<ULONG>(stats.LastSubmitFrameStatus));
    wprintf(L"LastSendFrameStatus:             0x%08X\n", static_cast<ULONG>(stats.LastSendFrameStatus));

    wprintf(L"\n[Open ReTouch]\n");
    wprintf(L"OpenInterfaceAttemptCount:       %ld\n", stats.OpenInterfaceAttemptCount);
    wprintf(L"OpenInterfaceSucceededCount:     %ld\n", stats.OpenInterfaceSucceededCount);
    wprintf(L"LastOpenInterfaceStatus:         0x%08X\n", static_cast<ULONG>(stats.LastOpenInterfaceStatus));
    wprintf(L"LastOpenedRootSystemNumber:      %ld\n", stats.LastOpenedRootSystemNumber);

    wprintf(L"\n[Raw HID]\n");
    wprintf(L"LastReadDataLength:              %ld\n", stats.LastReadDataLength);
    wprintf(L"Bytes[00..15]:                   %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
        stats.LastRawByte0,
        stats.LastRawByte1,
        stats.LastRawByte2,
        stats.LastRawByte3,
        stats.LastRawByte4,
        stats.LastRawByte5,
        stats.LastRawByte6,
        stats.LastRawByte7,
        stats.LastRawByte8,
        stats.LastRawByte9,
        stats.LastRawByte10,
        stats.LastRawByte11,
        stats.LastRawByte12,
        stats.LastRawByte13,
        stats.LastRawByte14,
        stats.LastRawByte15
    );

    wprintf(L"\n[Tip Candidates]\n");
    wprintf(L"TipCandidateByte0Bit0:           %ld\n", stats.TipCandidateByte0Bit0);
    wprintf(L"TipCandidateByte1Bit0:           %ld\n", stats.TipCandidateByte1Bit0);
    wprintf(L"TipCandidateByte2Bit0:           %ld\n", stats.TipCandidateByte2Bit0);
    wprintf(L"TipCandidateByte6Bit0:           %ld\n", stats.TipCandidateByte6Bit0);
    wprintf(L"TipCandidateByte7Bit0:           %ld\n", stats.TipCandidateByte7Bit0);
    wprintf(L"LastDecodedTipSwitch:            %ld\n", stats.LastDecodedTipSwitch);

    wprintf(L"\n[Pointers]\n");
    wprintf(L"LastDeviceContextPointer:        0x%p\n", reinterpret_cast<void*>(stats.LastDeviceContextPointer));
    wprintf(L"LastClientPointer:               0x%p\n", reinterpret_cast<void*>(stats.LastClientPointer));
    wprintf(L"LastPhysicalDeviceObject:        0x%p\n", reinterpret_cast<void*>(stats.LastPhysicalDeviceObject));
    wprintf(L"LastWdmDeviceObject:             0x%p\n", reinterpret_cast<void*>(stats.LastWdmDeviceObject));

    wprintf(L"\n");

    CloseHandle(device);
    return TRUE;
}

int wmain()
{
    BOOL ok = ReadGlobalStatsFromControlDevice();

    if (!ok)
    {
        return 1;
    }

    return 0;
}