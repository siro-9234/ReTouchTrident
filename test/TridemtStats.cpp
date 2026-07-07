#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <stdio.h>

#pragma comment(lib, "setupapi.lib")

// {7C3A92D8-6C4D-4D8E-9F35-123456789ABC}
DEFINE_GUID(
    GUID_DEVINTERFACE_TRIDENT_CAPTURE,
    0x7c3a92d8, 0x6c4d, 0x4d8e,
    0x9f, 0x35, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc
);

#define IOCTL_TRIDENT_GET_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA)
#define TRIDENT_OBS_IOCTL_HID_GET_DEVICE_ATTRIBUTES 0x000B01A8
#define TRIDENT_OBS_IOCTL_HID_GET_REPORT_DESCRIPTOR 0x000B01BE

typedef struct _TRIDENT_STATS
{
    LONG InternalIoctlCount;
    LONG DeviceIoctlCount;
    LONG GetStatsIoctlCount;
    LONG ReadRequestCount;

    LONG NonStatsDeviceIoctlCount;
    LONG LastNonStatsDeviceIoctlCode;
    LONG NonStatsDeviceIoctlCodes[8];

    LONG HidGetDeviceAttributesCount;
    LONG HidGetReportDescriptorCount;

    LONG LastInternalIoctlCode;
    LONG LastDeviceIoctlCode;

    LONG ReadReportReceived;
    LONG ReadReportCompleted;
    LONG ReadReportSendFailed;
    LONG ReadReportBufferRetrieved;
    LONG ReadReportBufferRetrieveFailed;

    LONG HidGetDeviceAttributesCompleted;
    LONG HidGetDeviceAttributesFailed;
    LONG LastHidGetDeviceAttributesStatus;

    LONG HidGetReportDescriptorCompleted;
    LONG HidGetReportDescriptorFailed;
    LONG LastHidGetReportDescriptorStatus;

    LONG LastCompletedDeviceIoctlCode;
    LONG LastCompletedDeviceIoctlStatus;
    LONG LastCompletedDeviceIoctlInformation;

    LONG ReadCompleted;
    LONG LastReadStatus;
    LONG LastReadInformation;

    LONG LastReadDataLength;
    UCHAR LastReadData[64];

    LONG LastDecodedTouchX;
    LONG LastDecodedTouchY;
    LONG LastDecodeTouchReportSucceeded;
    LONG LastDecodeTouchReportFailed;

    LONG LastDecodedTipSwitch;

    LONG LastFrameContactCount;
    LONG LastFrameX;
    LONG LastFrameY;
    LONG LastFrameIsDown;
} TRIDENT_STATS, * PTRIDENT_STATS;

static BOOL ReadStatsFromDevice(const wchar_t* devicePath)
{
    HANDLE device = CreateFileW(
        devicePath,
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

    TRIDENT_STATS stats = {};
    DWORD bytesReturned = 0;

    BOOL ok = DeviceIoControl(
        device,
        IOCTL_TRIDENT_GET_STATS,
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

    wprintf(L"\n=== Trident Stats ===\n");
    wprintf(L"BytesReturned:                     %lu\n", bytesReturned);
    wprintf(L"InternalIoctlCount:                %ld\n", stats.InternalIoctlCount);
    wprintf(L"ReadReportReceived:                %ld\n", stats.ReadReportReceived);
    wprintf(L"ReadReportCompleted:               %ld\n", stats.ReadReportCompleted);
    wprintf(L"ReadReportSendFailed:              %ld\n", stats.ReadReportSendFailed);
    wprintf(L"ReadReportBufferRetrieved:         %ld\n", stats.ReadReportBufferRetrieved);
    wprintf(L"ReadReportBufferRetrieveFailed:    %ld\n", stats.ReadReportBufferRetrieveFailed);
    wprintf(L"DeviceIoctlCount:                  %ld\n", stats.DeviceIoctlCount);
    wprintf(L"GetStatsIoctlCount:                %ld\n", stats.GetStatsIoctlCount);
    wprintf(L"LastInternalIoctlCode:             0x%08X\n", stats.LastInternalIoctlCode);
    wprintf(L"LastDeviceIoctlCode:               0x%08X\n", stats.LastDeviceIoctlCode);
    wprintf(L"ReadRequestCount:                  %ld\n", stats.ReadRequestCount);
    wprintf(L"NonStatsDeviceIoctlCount:          %ld\n", stats.NonStatsDeviceIoctlCount);
    wprintf(L"LastNonStatsDeviceIoctlCode:       0x%08X\n", stats.LastNonStatsDeviceIoctlCode);
    wprintf(L"NonStatsDeviceIoctlCodes:\n");

    for (int i = 0; i < 8; i++)
    {
        wprintf(
            L"  [%d] 0x%08X\n",
            i,
            stats.NonStatsDeviceIoctlCodes[i]
        );
    }

    wprintf(L"HidGetDeviceAttributesCount:       %ld\n", stats.HidGetDeviceAttributesCount);
    wprintf(L"HidGetReportDescriptorCount:       %ld\n", stats.HidGetReportDescriptorCount);
    wprintf(L"HidGetDeviceAttributesCompleted:  %ld\n", stats.HidGetDeviceAttributesCompleted);
    wprintf(L"HidGetDeviceAttributesFailed:     %ld\n", stats.HidGetDeviceAttributesFailed);
    wprintf(L"LastHidGetDeviceAttributesStatus: 0x%08X\n", stats.LastHidGetDeviceAttributesStatus);

    wprintf(L"HidGetReportDescriptorCompleted:  %ld\n", stats.HidGetReportDescriptorCompleted);
    wprintf(L"HidGetReportDescriptorFailed:     %ld\n", stats.HidGetReportDescriptorFailed);
    wprintf(L"LastHidGetReportDescriptorStatus: 0x%08X\n", stats.LastHidGetReportDescriptorStatus);

    wprintf(L"LastCompletedDeviceIoctlCode:        0x%08X\n", stats.LastCompletedDeviceIoctlCode);
    wprintf(L"LastCompletedDeviceIoctlStatus:      0x%08X\n", stats.LastCompletedDeviceIoctlStatus);
    wprintf(L"LastCompletedDeviceIoctlInformation: %ld\n", stats.LastCompletedDeviceIoctlInformation);

    wprintf(L"ReadCompleted:                    %ld\n", stats.ReadCompleted);
    wprintf(L"LastReadStatus:                   0x%08X\n", stats.LastReadStatus);
    wprintf(L"LastReadInformation:              %ld\n", stats.LastReadInformation);

    wprintf(L"LastReadDataLength:               %ld\n", stats.LastReadDataLength);

    wprintf(L"LastDecodedTouchX:                %ld\n", stats.LastDecodedTouchX);
    wprintf(L"LastDecodedTouchY:                %ld\n", stats.LastDecodedTouchY);
    wprintf(L"LastDecodeTouchReportSucceeded:   %ld\n", stats.LastDecodeTouchReportSucceeded);
    wprintf(L"LastDecodeTouchReportFailed:      %ld\n", stats.LastDecodeTouchReportFailed);

    wprintf(L"LastReadData:\n");
    for (int i = 0; i < stats.LastReadDataLength && i < 64; i++)
    {
        if (i % 16 == 0)
        {
            wprintf(L"  ");
        }

        wprintf(L"%02X ", stats.LastReadData[i]);

        if (i % 16 == 15)
        {
            wprintf(L"\n");
        }
    }

    if (stats.LastReadDataLength >= 6)
    {
        USHORT x = static_cast<USHORT>(
            stats.LastReadData[2] |
            (stats.LastReadData[3] << 8)
            );

        USHORT y = static_cast<USHORT>(
            stats.LastReadData[4] |
            (stats.LastReadData[5] << 8)
            );

        wprintf(L"\n");

        wprintf(L"DecodedTouchX:                   %u\n", x);
        wprintf(L"DecodedTouchY:                   %u\n", y);
        wprintf(L"LastDecodedTipSwitch:           %ld\n", stats.LastDecodedTipSwitch);

        wprintf(L"LastFrameContactCount:          %ld\n", stats.LastFrameContactCount);
        wprintf(L"LastFrameX:                     %ld\n", stats.LastFrameX);
        wprintf(L"LastFrameY:                     %ld\n", stats.LastFrameY);
        wprintf(L"LastFrameIsDown:                %ld\n", stats.LastFrameIsDown);
    }

    wprintf(L"\n");

    CloseHandle(device);
    return TRUE;
}

int wmain()
{
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_TRIDENT_CAPTURE,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE)
    {
        wprintf(L"SetupDiGetClassDevsW failed. GetLastError=%lu\n", GetLastError());
        return 1;
    }

    DWORD index = 0;
    BOOL found = FALSE;

    while (true)
    {
        SP_DEVICE_INTERFACE_DATA interfaceData = {};
        interfaceData.cbSize = sizeof(interfaceData);

        BOOL enumOk = SetupDiEnumDeviceInterfaces(
            deviceInfoSet,
            nullptr,
            &GUID_DEVINTERFACE_TRIDENT_CAPTURE,
            index,
            &interfaceData
        );

        if (!enumOk)
        {
            DWORD error = GetLastError();

            if (error == ERROR_NO_MORE_ITEMS)
            {
                break;
            }

            wprintf(L"SetupDiEnumDeviceInterfaces failed. GetLastError=%lu\n", error);
            break;
        }

        DWORD requiredSize = 0;

        SetupDiGetDeviceInterfaceDetailW(
            deviceInfoSet,
            &interfaceData,
            nullptr,
            0,
            &requiredSize,
            nullptr
        );

        PSP_DEVICE_INTERFACE_DETAIL_DATA_W detailData =
            reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(
                malloc(requiredSize)
                );

        if (detailData == nullptr)
        {
            wprintf(L"malloc failed.\n");
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
            return 1;
        }

        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        BOOL detailOk = SetupDiGetDeviceInterfaceDetailW(
            deviceInfoSet,
            &interfaceData,
            detailData,
            requiredSize,
            nullptr,
            nullptr
        );

        if (detailOk)
        {
            found = TRUE;

            wprintf(L"\n============================================================\n");
            wprintf(L"Trident interface #%lu\n", index);
            wprintf(L"DevicePath:\n");
            wprintf(L"%s\n", detailData->DevicePath);

            ReadStatsFromDevice(detailData->DevicePath);
        }
        else
        {
            wprintf(
                L"SetupDiGetDeviceInterfaceDetailW failed. GetLastError=%lu\n",
                GetLastError()
            );
        }

        free(detailData);
        index++;
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    if (!found)
    {
        wprintf(L"No Trident interface found.\n");
        return 1;
    }

    return 0;
}