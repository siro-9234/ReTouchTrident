#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <stdio.h>

#pragma comment(lib, "setupapi.lib")

DEFINE_GUID(
    GUID_DEVINTERFACE_RETOUCH,
    0x7b3f8c21, 0x29f4, 0x4b6a,
    0x9c, 0x11, 0x2f, 0x8a, 0x4e, 0x72, 0xd1, 0x90
);

#define FILE_DEVICE_RETOUCH 0x8000

#define IOCTL_RETOUCH_GET_STATS \
    CTL_CODE(FILE_DEVICE_RETOUCH, 0x802, METHOD_BUFFERED, FILE_READ_DATA)

typedef struct _RETOUCH_STATS
{
    LONG DeviceAddCount;
    LONG DeviceCleanupCount;
    LONG QueueInitializeCount;

    LONG VirtualTouchInitializeCount;
    LONG VirtualTouchInitializeStatus;

    LONG VhfCreateStatus;
    LONG VhfStartCount;

    LONG SubmitFrameCount;
    LONG LastSubmitFrameStatus;
    LONG LastContactCount;

    LONG GetFeatureCount;
    LONG LastGetFeatureReportId;

    LONG SetFeatureCount;
    LONG LastSetFeatureReportId;

    LONG WdmDeviceObjectNull;

    LONG LastActiveContactCount;
    LONG LastFirstContactFlags;
    LONG LastFirstContactId;
    LONG LastFirstContactX;
    LONG LastFirstContactY;
    LONG LastReportContactCount;

    LONG ReceivedSubmitFrameIoctlCount;
    LONG ReceivedContactCount;
    LONG ReceivedFirstContactId;
    LONG ReceivedFirstContactIsDown;
    LONG ReceivedFirstContactX;
    LONG ReceivedFirstContactY;

} RETOUCH_STATS, * PRETOUCH_STATS;

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

    RETOUCH_STATS stats = {};
    DWORD bytesReturned = 0;

    BOOL ok = DeviceIoControl(
        device,
        IOCTL_RETOUCH_GET_STATS,
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

    wprintf(L"\n=== ReTouch Stats ===\n");
    wprintf(L"BytesReturned:                   %lu\n", bytesReturned);

    wprintf(L"DeviceAddCount:                  %ld\n", stats.DeviceAddCount);
    wprintf(L"DeviceCleanupCount:              %ld\n", stats.DeviceCleanupCount);
    wprintf(L"QueueInitializeCount:            %ld\n", stats.QueueInitializeCount);

    wprintf(L"VirtualTouchInitializeCount:     %ld\n", stats.VirtualTouchInitializeCount);
    wprintf(L"VirtualTouchInitializeStatus:    0x%08X\n", stats.VirtualTouchInitializeStatus);

    wprintf(L"VhfCreateStatus:                 0x%08X\n", stats.VhfCreateStatus);
    wprintf(L"VhfStartCount:                   %ld\n", stats.VhfStartCount);

    wprintf(L"SubmitFrameCount:                %ld\n", stats.SubmitFrameCount);
    wprintf(L"LastSubmitFrameStatus:           0x%08X\n", stats.LastSubmitFrameStatus);
    wprintf(L"LastContactCount:                %ld\n", stats.LastContactCount);

    wprintf(L"GetFeatureCount:                 %ld\n", stats.GetFeatureCount);
    wprintf(L"LastGetFeatureReportId:          %ld\n", stats.LastGetFeatureReportId);

    wprintf(L"SetFeatureCount:                  %ld\n", stats.SetFeatureCount);
    wprintf(L"LastSetFeatureReportId:           %ld\n", stats.LastSetFeatureReportId);

    wprintf(L"WdmDeviceObjectNull:             %ld\n", stats.WdmDeviceObjectNull);

    wprintf(L"LastActiveContactCount:          %ld\n", stats.LastActiveContactCount);
    wprintf(L"LastFirstContactFlags:           0x%02X\n", stats.LastFirstContactFlags);
    wprintf(L"LastFirstContactId:              %ld\n", stats.LastFirstContactId);
    wprintf(L"LastFirstContactX:               %ld\n", stats.LastFirstContactX);
    wprintf(L"LastFirstContactY:               %ld\n", stats.LastFirstContactY);
    wprintf(L"LastReportContactCount:          %ld\n", stats.LastReportContactCount);

    wprintf(L"ReceivedSubmitFrameIoctlCount:   %ld\n", stats.ReceivedSubmitFrameIoctlCount);
    wprintf(L"ReceivedContactCount:            %ld\n", stats.ReceivedContactCount);
    wprintf(L"ReceivedFirstContactId:          %ld\n", stats.ReceivedFirstContactId);
    wprintf(L"ReceivedFirstContactIsDown:      %ld\n", stats.ReceivedFirstContactIsDown);
    wprintf(L"ReceivedFirstContactX:           %ld\n", stats.ReceivedFirstContactX);
    wprintf(L"ReceivedFirstContactY:           %ld\n", stats.ReceivedFirstContactY);

    wprintf(L"\n");

    CloseHandle(device);
    return TRUE;
}

int wmain()
{
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_RETOUCH,
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
            &GUID_DEVINTERFACE_RETOUCH,
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
            wprintf(L"ReTouch interface #%lu\n", index);
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
        wprintf(L"No ReTouch interface found.\n");
        return 1;
    }

    return 0;
}