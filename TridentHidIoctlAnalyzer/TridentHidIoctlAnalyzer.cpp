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

static void PrintValueCaps(
    PHIDP_PREPARSED_DATA preparsed,
    HIDP_REPORT_TYPE reportType,
    USHORT count,
    const wchar_t* title)
{
    std::wcout << title << L": " << count << L"\n";

    if (count == 0)
        return;

    std::vector<HIDP_VALUE_CAPS> caps(count);
    USHORT length = count;

    NTSTATUS status = HidP_GetValueCaps(
        reportType,
        caps.data(),
        &length,
        preparsed
    );

    std::wcout << L"  HidP_GetValueCaps Status: 0x"
        << std::hex << status << std::dec << L"\n";

    if (status != HIDP_STATUS_SUCCESS)
        return;

    for (USHORT i = 0; i < length; ++i)
    {
        const auto& c = caps[i];

        std::wcout << L"  [" << i << L"]\n";
        std::wcout << L"      UsagePage:      0x" << std::hex << c.UsagePage << std::dec << L"\n";
        std::wcout << L"      ReportID:       0x" << std::hex << static_cast<int>(c.ReportID) << std::dec << L"\n";
        std::wcout << L"      LinkCollection: " << c.LinkCollection << L"\n";
        std::wcout << L"      LinkUsagePage:  0x" << std::hex << c.LinkUsagePage << std::dec << L"\n";
        std::wcout << L"      LinkUsage:      0x" << std::hex << c.LinkUsage << std::dec << L"\n";

        std::wcout << L"      BitSize:        " << c.BitSize << L"\n";
        std::wcout << L"      ReportCount:    " << c.ReportCount << L"\n";
        std::wcout << L"      BitField:       0x" << std::hex << c.BitField << std::dec << L"\n";

        std::wcout << L"      LogicalMin:     " << c.LogicalMin << L"\n";
        std::wcout << L"      LogicalMax:     " << c.LogicalMax << L"\n";
        std::wcout << L"      PhysicalMin:    " << c.PhysicalMin << L"\n";
        std::wcout << L"      PhysicalMax:    " << c.PhysicalMax << L"\n";

        std::wcout << L"      Units:          0x" << std::hex << c.Units << std::dec << L"\n";
        std::wcout << L"      UnitsExp:       " << c.UnitsExp << L"\n";

        std::wcout << L"      IsAlias:        " << c.IsAlias << L"\n";
        std::wcout << L"      HasNull:        " << c.HasNull << L"\n";
        std::wcout << L"      IsAbsolute:     " << c.IsAbsolute << L"\n";
        std::wcout << L"      IsRange:        " << c.IsRange << L"\n";

        if (c.IsRange)
        {
            std::wcout << L"      UsageRange:     0x"
                << std::hex << c.Range.UsageMin
                << L" - 0x" << c.Range.UsageMax
                << std::dec << L"\n";

            std::wcout << L"      DataIndexRange: "
                << c.Range.DataIndexMin
                << L" - " << c.Range.DataIndexMax << L"\n";
        }
        else
        {
            std::wcout << L"      Usage:          0x"
                << std::hex << c.NotRange.Usage
                << std::dec << L"\n";

            std::wcout << L"      DataIndex:      "
                << c.NotRange.DataIndex << L"\n";
        }
    }
}

static void PrintButtonCaps(
    PHIDP_PREPARSED_DATA preparsed,
    HIDP_REPORT_TYPE reportType,
    USHORT count,
    const wchar_t* title)
{
    std::wcout << title << L": " << count << L"\n";

    if (count == 0)
        return;

    std::vector<HIDP_BUTTON_CAPS> caps(count);
    USHORT length = count;

    NTSTATUS status = HidP_GetButtonCaps(
        reportType,
        caps.data(),
        &length,
        preparsed
    );

    std::wcout << L"  HidP_GetButtonCaps Status: 0x"
        << std::hex << status << std::dec << L"\n";

    if (status != HIDP_STATUS_SUCCESS)
        return;

    for (USHORT i = 0; i < length; ++i)
    {
        const auto& c = caps[i];

        std::wcout << L"  [" << i << L"]\n";
        std::wcout << L"      UsagePage:      0x" << std::hex << c.UsagePage << std::dec << L"\n";
        std::wcout << L"      ReportID:       0x" << std::hex << static_cast<int>(c.ReportID) << std::dec << L"\n";
        std::wcout << L"      LinkCollection: " << c.LinkCollection << L"\n";
        std::wcout << L"      LinkUsagePage:  0x" << std::hex << c.LinkUsagePage << std::dec << L"\n";
        std::wcout << L"      LinkUsage:      0x" << std::hex << c.LinkUsage << std::dec << L"\n";

        std::wcout << L"      ReportCount:    " << c.ReportCount << L"\n";
        std::wcout << L"      BitField:       0x" << std::hex << c.BitField << std::dec << L"\n";

        std::wcout << L"      IsAlias:        " << c.IsAlias << L"\n";
        std::wcout << L"      IsRange:        " << c.IsRange << L"\n";

        if (c.IsRange)
        {
            std::wcout << L"      UsageRange:     0x"
                << std::hex << c.Range.UsageMin
                << L" - 0x" << c.Range.UsageMax
                << std::dec << L"\n";

            std::wcout << L"      DataIndexRange: "
                << c.Range.DataIndexMin
                << L" - " << c.Range.DataIndexMax << L"\n";
        }
        else
        {
            std::wcout << L"      Usage:          0x"
                << std::hex << c.NotRange.Usage
                << std::dec << L"\n";

            std::wcout << L"      DataIndex:      "
                << c.NotRange.DataIndex << L"\n";
        }
    }
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

    auto PrintHidString = [](HANDLE h, const wchar_t* label, auto func)
        {
            wchar_t buffer[256] = {};

            if (func(h, buffer, sizeof(buffer)))
            {
                std::wcout << label << L": " << buffer << L"\n";
            }
            else
            {
                std::wcout << label << L": <failed> GetLastError=" << GetLastError() << L"\n";
            }
        };

    PrintHidString(h, L"Manufacturer", HidD_GetManufacturerString);
    PrintHidString(h, L"Product", HidD_GetProductString);
    PrintHidString(h, L"SerialNumber", HidD_GetSerialNumberString);

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
        PrintButtonCaps(preparsed, HidP_Input, caps.NumberInputButtonCaps, L"Input Button Caps");
        PrintValueCaps(preparsed, HidP_Input, caps.NumberInputValueCaps, L"Input Value Caps");

        PrintButtonCaps(preparsed, HidP_Feature, caps.NumberFeatureButtonCaps, L"Feature Button Caps");
        PrintValueCaps(preparsed, HidP_Feature, caps.NumberFeatureValueCaps, L"Feature Value Caps");

        if (caps.InputReportByteLength > 0)
        {
            std::vector<BYTE> buffer(caps.InputReportByteLength);
            DWORD bytesRead = 0;

            /*BOOL ok = ReadFile(
                h,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead,
                nullptr
            );*/

            if (caps.InputReportByteLength > 0)
            {
                std::wcout << L"ReadFile: SKIPPED\n";
            }
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