#include "PatternScanner.h"

namespace
{
    constexpr UCHAR kMatchByte = 0xFF;
    constexpr UCHAR kWildcardByte = 0x00;
}

BOOLEAN
TridentPatternScanner::BytesMatch(
    _In_reads_bytes_(BufferSize) const UCHAR* Buffer,
    _In_ SIZE_T BufferSize,
    _In_reads_bytes_(PatternSize) const UCHAR* Pattern,
    _In_reads_bytes_(PatternSize) const UCHAR* Mask,
    _In_ SIZE_T PatternSize
)
{
    if (Buffer == nullptr || Pattern == nullptr || Mask == nullptr ||
        PatternSize == 0 || BufferSize < PatternSize)
    {
        return FALSE;
    }

    for (SIZE_T index = 0; index < PatternSize; ++index)
    {
        if (Mask[index] != kMatchByte && Mask[index] != kWildcardByte)
        {
            return FALSE;
        }

        if (Mask[index] == kMatchByte && Buffer[index] != Pattern[index])
        {
            return FALSE;
        }
    }

    return TRUE;
}

NTSTATUS
TridentPatternScanner::FindUniquePattern(
    _In_reads_bytes_(ImageSize) const UCHAR* ImageBase,
    _In_ SIZE_T ImageSize,
    _In_reads_bytes_(PatternSize) const UCHAR* Pattern,
    _In_reads_bytes_(PatternSize) const UCHAR* Mask,
    _In_ SIZE_T PatternSize,
    _Outptr_result_bytebuffer_(PatternSize) const UCHAR** Match
)
{
    if (Match == nullptr)
    {
        return STATUS_INVALID_PARAMETER;
    }

    *Match = nullptr;

    if (ImageBase == nullptr || Pattern == nullptr || Mask == nullptr ||
        PatternSize == 0 || ImageSize < PatternSize)
    {
        return STATUS_INVALID_PARAMETER;
    }

    const UCHAR* uniqueMatch = nullptr;
    const SIZE_T lastOffset = ImageSize - PatternSize;

    for (SIZE_T offset = 0; offset <= lastOffset; ++offset)
    {
        if (!BytesMatch(ImageBase + offset, ImageSize - offset, Pattern, Mask, PatternSize))
        {
            continue;
        }

        if (uniqueMatch != nullptr)
        {
            return STATUS_OBJECT_NAME_COLLISION;
        }

        uniqueMatch = ImageBase + offset;
    }

    if (uniqueMatch == nullptr)
    {
        return STATUS_NOT_FOUND;
    }

    *Match = uniqueMatch;
    return STATUS_SUCCESS;
}
