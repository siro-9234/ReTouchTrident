#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

static void PrintLastError(const wchar_t* where)
{
    DWORD err = GetLastError();
    std::wcout << L"  " << where << L" failed. GetLastError=" << err << L"\n";
}

static void AnalyzeHidDevice(const wchar_t* devicePath)
{
    std::wcout << L"\n============================================================\n";
    std::wcout << L"DevicePath:\n" << devicePath << L"\n";

    HANDLE h = CreateFileW(
        devicePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (h == INVALID_HANDLE_VALUE)
    {
        PrintLastError(L"CreateFile GENERIC_READ|GENERIC_WRITE");

        h = CreateFileW(
            devicePath,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (h == INVALID_HANDLE_VALUE)
        {
            PrintLastError(L"CreateFile GENERIC_READ");
            return;
        }
    }

    std::wcout << L"OpenStatus: SUCCESS\n";

    HIDD_ATTRIBUTES attrs = {};
    attrs.Size = sizeof(attrs);

    if (HidD_GetAttributes(h, &attrs))
    {
        std::wcout << L"VendorID:  0x" << std::hex << std::setw(4) << std::setfill(L'0') << attrs.VendorID << L"\n";
        std::wcout << L"ProductID: 0x" << std::hex << std::setw(4) << std::setfill(L'0') << attrs.ProductID << L"\n";
        std::wcout << L"Version:   0x" << std::hex << std::setw(4) << std::setfill(L'0') << attrs.VersionNumber << L"\n";
        std::wcout << std::dec;
    }
    else
    {
        PrintLastError(L"HidD_GetAttributes");
    }

    PHIDP_PREPARSED_DATA preparsed = nullptr;

    if (!HidD_GetPreparsedData(h, &preparsed))
    {
        PrintLastError(L"HidD_GetPreparsedData");
        CloseHandle(h);
        return;
    }

    HIDP_CAPS caps = {};
    NTSTATUS capsStatus = HidP_GetCaps(preparsed, &caps);

    std::wcout << L"HidP_GetCaps Status: 0x"
        << std::hex << capsStatus << std::dec << L"\n";

    if (capsStatus == HIDP_STATUS_SUCCESS)
    {
        std::wcout << L"UsagePage:               0x" << std::hex << caps.UsagePage << std::dec << L"\n";
        std::wcout << L"Usage:                   0x" << std::hex << caps.Usage << std::dec << L"\n";
        std::wcout << L"InputReportByteLength:   " << caps.InputReportByteLength << L"\n";
        std::wcout << L"OutputReportByteLength:  " << caps.OutputReportByteLength << L"\n";
        std::wcout << L"FeatureReportByteLength: " << caps.FeatureReportByteLength << L"\n";

        if (caps.InputReportByteLength > 0)
        {
            std::vector<BYTE> buffer(caps.InputReportByteLength);
            DWORD bytesRead = 0;

            BOOL ok = ReadFile(
                h,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead,
                nullptr
            );

            std::wcout << L"ReadFile Result: " << (ok ? L"SUCCESS" : L"FAILED") << L"\n";
            std::wcout << L"ReadFile LastError: " << GetLastError() << L"\n";
            std::wcout << L"Read BytesReturned: " << bytesRead << L"\n";
        }
    }

    HidD_FreePreparsedData(preparsed);
    CloseHandle(h);
}

int wmain()
{
    GUID hidGuid = {};
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevsW(
        &hidGuid,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (devInfo == INVALID_HANDLE_VALUE)
    {
        PrintLastError(L"SetupDiGetClassDevsW");
        return 1;
    }

    DWORD index = 0;

    while (true)
    {
        SP_DEVICE_INTERFACE_DATA ifData = {};
        ifData.cbSize = sizeof(ifData);

        if (!SetupDiEnumDeviceInterfaces(
            devInfo,
            nullptr,
            &hidGuid,
            index,
            &ifData))
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
                break;

            PrintLastError(L"SetupDiEnumDeviceInterfaces");
            break;
        }

        DWORD requiredSize = 0;

        SetupDiGetDeviceInterfaceDetailW(
            devInfo,
            &ifData,
            nullptr,
            0,
            &requiredSize,
            nullptr
        );

        std::vector<BYTE> detailBuffer(requiredSize);
        auto detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(
            devInfo,
            &ifData,
            detail,
            requiredSize,
            nullptr,
            nullptr))
        {
            PrintLastError(L"SetupDiGetDeviceInterfaceDetailW");
            ++index;
            continue;
        }

        AnalyzeHidDevice(detail->DevicePath);

        ++index;
    }

    SetupDiDestroyDeviceInfoList(devInfo);

    std::wcout << L"\nDone.\n";
    return 0;
}