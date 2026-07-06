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

typedef struct _TRIDENT_STATS
{
    LONG InternalIoctlCount;
    LONG ReadReportReceived;
    LONG ReadReportCompleted;
    LONG ReadReportSendFailed;
    LONG ReadReportBufferRetrieved;
    LONG ReadReportBufferRetrieveFailed;
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

            wprintf(L"\nFound Trident interface:\n");
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