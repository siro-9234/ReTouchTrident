#pragma once

#include <ntddk.h>

namespace TridentPatternScanner
{
    NTSTATUS FindUniquePattern(
        _In_reads_bytes_(ImageSize) const UCHAR* ImageBase,
        _In_ SIZE_T ImageSize,
        _In_reads_bytes_(PatternSize) const UCHAR* Pattern,
        _In_reads_bytes_(PatternSize) const UCHAR* Mask,
        _In_ SIZE_T PatternSize,
        _Outptr_result_bytebuffer_(PatternSize) const UCHAR** Match
    );

    BOOLEAN BytesMatch(
        _In_reads_bytes_(BufferSize) const UCHAR* Buffer,
        _In_ SIZE_T BufferSize,
        _In_reads_bytes_(PatternSize) const UCHAR* Pattern,
        _In_reads_bytes_(PatternSize) const UCHAR* Mask,
        _In_ SIZE_T PatternSize
    );
}
