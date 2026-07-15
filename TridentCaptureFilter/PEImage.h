#pragma once

#include <ntddk.h>

typedef struct _TRIDENT_PE_SECTION_VIEW
{
    const UCHAR* Base;
    SIZE_T Size;
    ULONG VirtualAddress;
    ULONG Characteristics;
} TRIDENT_PE_SECTION_VIEW, *PTRIDENT_PE_SECTION_VIEW;

namespace TridentPEImage
{
    NTSTATUS ValidateImage(
        _In_reads_bytes_(ImageSize) const UCHAR* ImageBase,
        _In_ SIZE_T ImageSize
    );

    NTSTATUS GetSection(
        _In_reads_bytes_(ImageSize) const UCHAR* ImageBase,
        _In_ SIZE_T ImageSize,
        _In_reads_(8) const CHAR SectionName[8],
        _Out_ PTRIDENT_PE_SECTION_VIEW Section
    );
}
