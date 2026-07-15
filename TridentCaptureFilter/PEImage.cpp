#include "PEImage.h"
#include <ntimage.h>

namespace
{
    BOOLEAN IsRangeValid(
        _In_ SIZE_T ImageSize,
        _In_ SIZE_T Offset,
        _In_ SIZE_T Length
    )
    {
        return Offset <= ImageSize && Length <= (ImageSize - Offset);
    }

    BOOLEAN SectionNameEquals(
        _In_reads_(8) const UCHAR Name[8],
        _In_reads_(8) const CHAR Expected[8]
    )
    {
        for (SIZE_T index = 0; index < IMAGE_SIZEOF_SHORT_NAME; ++index)
        {
            if (Name[index] != static_cast<UCHAR>(Expected[index]))
            {
                return FALSE;
            }
        }

        return TRUE;
    }
}

NTSTATUS
TridentPEImage::ValidateImage(
    _In_reads_bytes_(ImageSize) const UCHAR* ImageBase,
    _In_ SIZE_T ImageSize
)
{
    if (ImageBase == nullptr || ImageSize < sizeof(IMAGE_DOS_HEADER))
    {
        return STATUS_INVALID_PARAMETER;
    }

    const auto dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(ImageBase);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE || dosHeader->e_lfanew <= 0)
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    const SIZE_T ntOffset = static_cast<SIZE_T>(dosHeader->e_lfanew);
    if (!IsRangeValid(ImageSize, ntOffset, sizeof(IMAGE_NT_HEADERS64)))
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    const auto ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(ImageBase + ntOffset);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE ||
        ntHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    if (ntHeaders->OptionalHeader.SizeOfImage == 0 ||
        ntHeaders->OptionalHeader.SizeOfImage > ImageSize ||
        ntHeaders->FileHeader.NumberOfSections == 0)
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    const SIZE_T sectionTableOffset = ntOffset +
        FIELD_OFFSET(IMAGE_NT_HEADERS64, OptionalHeader) +
        ntHeaders->FileHeader.SizeOfOptionalHeader;
    const SIZE_T sectionTableSize =
        static_cast<SIZE_T>(ntHeaders->FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);

    if (!IsRangeValid(ImageSize, sectionTableOffset, sectionTableSize))
    {
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
TridentPEImage::GetSection(
    _In_reads_bytes_(ImageSize) const UCHAR* ImageBase,
    _In_ SIZE_T ImageSize,
    _In_reads_(8) const CHAR SectionName[8],
    _Out_ PTRIDENT_PE_SECTION_VIEW Section
)
{
    if (Section == nullptr || SectionName == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Section, sizeof(*Section));

    NTSTATUS status = ValidateImage(ImageBase, ImageSize);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    const auto dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(ImageBase);
    const auto ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        ImageBase + static_cast<SIZE_T>(dosHeader->e_lfanew));
    const auto sectionHeaders = IMAGE_FIRST_SECTION(ntHeaders);

    for (USHORT index = 0; index < ntHeaders->FileHeader.NumberOfSections; ++index)
    {
        const IMAGE_SECTION_HEADER& header = sectionHeaders[index];
        if (!SectionNameEquals(header.Name, SectionName))
        {
            continue;
        }

        const SIZE_T virtualAddress = header.VirtualAddress;
        SIZE_T virtualSize = header.Misc.VirtualSize;
        if (virtualSize == 0)
        {
            virtualSize = header.SizeOfRawData;
        }

        if (virtualSize == 0 || !IsRangeValid(ImageSize, virtualAddress, virtualSize))
        {
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        Section->Base = ImageBase + virtualAddress;
        Section->Size = virtualSize;
        Section->VirtualAddress = header.VirtualAddress;
        Section->Characteristics = header.Characteristics;
        return STATUS_SUCCESS;
    }

    return STATUS_NOT_FOUND;
}
