#include "Win32kManager.h"
#include "HookManager.h"
#include "PatchTargetInspector.h"
#include "PatchTransaction.h"
#include "PatternScanner.h"
#include "PEImage.h"
#include "TridentLog.h"
#include <aux_klib.h>
#include <ntimage.h>

namespace
{
    constexpr ULONG kValidatedBuild = 26200;
    constexpr ULONG kSuppressionPointOffset = 0xEC;
    constexpr ULONG kSkipTargetOffset = 0x164;
    constexpr ULONG kMinimumFunctionSpan = kSkipTargetOffset + 0x28;
    constexpr ULONG kPoolTag = 'kRtT';

    // Windows 11 25H2 build 26200.8737, x64.
    // Wildcards cover stack-frame displacement and allocation size fields.
    constexpr UCHAR kFunctionPattern[] =
    {
        0x48, 0x8B, 0xC4,
        0x48, 0x89, 0x58, 0x10,
        0x48, 0x89, 0x70, 0x18,
        0x48, 0x89, 0x78, 0x20,
        0x55,
        0x48, 0x8D, 0xA8, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x81, 0xEC, 0x00, 0x00, 0x00, 0x00
    };

    constexpr UCHAR kFunctionMask[] =
    {
        0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF,
        0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00
    };

    constexpr UCHAR kSavedArgumentSequence[] =
    {
        0x48, 0x8B, 0xD9, // mov rbx, rcx
        0x48, 0x8B, 0xF2  // mov rsi, rdx
    };

    constexpr UCHAR kSuppressionPointBytes[] =
    {
        0x33, 0xDB // xor ebx, ebx
    };

    constexpr UCHAR kSkipTargetBytes[] =
    {
        0x48, 0x8B, 0x8D, 0x80, 0x00, 0x00, 0x00,
        0x48, 0x33, 0xCC
    };

    TRIDENT_WIN32K_STATUS g_Status;

    class PoolBuffer final
    {
    public:
        explicit PoolBuffer(_In_opt_ PVOID Buffer = nullptr) : buffer_(Buffer) {}
        ~PoolBuffer()
        {
            if (buffer_ != nullptr)
            {
                ExFreePoolWithTag(buffer_, kPoolTag);
            }
        }

        PoolBuffer(const PoolBuffer&) = delete;
        PoolBuffer& operator=(const PoolBuffer&) = delete;

        PVOID Get() const { return buffer_; }
        void Reset(_In_opt_ PVOID Buffer)
        {
            if (buffer_ != nullptr)
            {
                ExFreePoolWithTag(buffer_, kPoolTag);
            }
            buffer_ = Buffer;
        }

    private:
        PVOID buffer_;
    };

    BOOLEAN IsSupportedBuild(_In_ const RTL_OSVERSIONINFOW& Version)
    {
        return Version.dwMajorVersion == 10 &&
            Version.dwMinorVersion == 0 &&
            Version.dwBuildNumber == kValidatedBuild;
    }

    BOOLEAN IsWin32kFullName(_In_z_ const UCHAR* Name)
    {
        if (Name == nullptr)
        {
            return FALSE;
        }

        static constexpr UCHAR kExpected[] = "win32kfull.sys";
        for (SIZE_T index = 0; index < RTL_NUMBER_OF(kExpected); ++index)
        {
            UCHAR value = Name[index];
            if (value >= 'A' && value <= 'Z')
            {
                value = static_cast<UCHAR>(value - 'A' + 'a');
            }

            if (value != kExpected[index])
            {
                return FALSE;
            }

            if (kExpected[index] == '\0')
            {
                return TRUE;
            }
        }

        return FALSE;
    }

    NTSTATUS LocateWin32kFull(
        _Outptr_ PVOID* ModuleBase,
        _Out_ PULONG ModuleSize
    )
    {
        if (ModuleBase == nullptr || ModuleSize == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        *ModuleBase = nullptr;
        *ModuleSize = 0;

        NTSTATUS status = AuxKlibInitialize();
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        ULONG bytes = 0;
        status = AuxKlibQueryModuleInformation(&bytes, sizeof(AUX_MODULE_EXTENDED_INFO), nullptr);
        if (!NT_SUCCESS(status) || bytes == 0)
        {
            return NT_SUCCESS(status) ? STATUS_NOT_FOUND : status;
        }

        PoolBuffer modules(ExAllocatePool2(POOL_FLAG_NON_PAGED, bytes, kPoolTag));
        if (modules.Get() == nullptr)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(modules.Get(), bytes);
        status = AuxKlibQueryModuleInformation(
            &bytes,
            sizeof(AUX_MODULE_EXTENDED_INFO),
            modules.Get()
        );
        if (!NT_SUCCESS(status))
        {
            return status;
        }

        const ULONG moduleCount = bytes / sizeof(AUX_MODULE_EXTENDED_INFO);
        const auto entries = static_cast<const AUX_MODULE_EXTENDED_INFO*>(modules.Get());

        for (ULONG index = 0; index < moduleCount; ++index)
        {
            const AUX_MODULE_EXTENDED_INFO& entry = entries[index];
            if (entry.FileNameOffset >= sizeof(entry.FullPathName))
            {
                continue;
            }

            const UCHAR* fileName = entry.FullPathName + entry.FileNameOffset;
            if (!IsWin32kFullName(fileName))
            {
                continue;
            }

            if (entry.BasicInfo.ImageBase == nullptr || entry.ImageSize == 0)
            {
                return STATUS_INVALID_IMAGE_FORMAT;
            }

            *ModuleBase = entry.BasicInfo.ImageBase;
            *ModuleSize = entry.ImageSize;
            return STATUS_SUCCESS;
        }

        return STATUS_NOT_FOUND;
    }

    BOOLEAN ValidateCandidateLayout(
        _In_reads_bytes_(AvailableBytes) const UCHAR* Function,
        _In_ SIZE_T AvailableBytes
    )
    {
        if (Function == nullptr || AvailableBytes < kMinimumFunctionSpan)
        {
            return FALSE;
        }

        if (RtlCompareMemory(Function + 0x2F,
            kSavedArgumentSequence,
            sizeof(kSavedArgumentSequence)) != sizeof(kSavedArgumentSequence))
        {
            return FALSE;
        }

        if (RtlCompareMemory(Function + kSuppressionPointOffset,
            kSuppressionPointBytes,
            sizeof(kSuppressionPointBytes)) != sizeof(kSuppressionPointBytes))
        {
            return FALSE;
        }

        if (RtlCompareMemory(Function + kSkipTargetOffset,
            kSkipTargetBytes,
            sizeof(kSkipTargetBytes)) != sizeof(kSkipTargetBytes))
        {
            return FALSE;
        }

        // The instruction immediately before the suppression point must be a CALL rel32.
        if (Function[kSuppressionPointOffset - 0x15] != 0xE8)
        {
            return FALSE;
        }

        return TRUE;
    }

    NTSTATUS LocateAndValidateCandidate(
        _In_ PVOID ModuleBase,
        _In_ ULONG ModuleSize,
        _Outptr_ PVOID* TextBase,
        _Out_ PSIZE_T TextSize,
        _Outptr_ PVOID* FunctionAddress,
        _Outptr_ PVOID* CandidateAddress,
        _Outptr_ PVOID* SkipTargetAddress
    )
    {
        if (ModuleBase == nullptr || ModuleSize == 0 ||
            TextBase == nullptr || TextSize == nullptr ||
            FunctionAddress == nullptr || CandidateAddress == nullptr ||
            SkipTargetAddress == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        *TextBase = nullptr;
        *TextSize = 0;
        *FunctionAddress = nullptr;
        *CandidateAddress = nullptr;
        *SkipTargetAddress = nullptr;

        const auto imageBase = static_cast<const UCHAR*>(ModuleBase);

        NTSTATUS status =
            TridentPEImage::ValidateImage(imageBase, ModuleSize);

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        static constexpr CHAR kTextName[8] =
        {
            '.', 't', 'e', 'x', 't', 0, 0, 0
        };

        TRIDENT_PE_SECTION_VIEW text = {};

        status = TridentPEImage::GetSection(
            imageBase,
            ModuleSize,
            kTextName,
            &text
        );

        if (!NT_SUCCESS(status))
        {
            return status;
        }

        if ((text.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0 ||
            (text.Characteristics & IMAGE_SCN_MEM_READ) == 0)
        {
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        if (text.Base == nullptr ||
            text.Size < sizeof(kFunctionPattern) ||
            text.Size < kMinimumFunctionSpan)
        {
            return STATUS_INVALID_IMAGE_FORMAT;
        }

        const UCHAR* validatedMatch = nullptr;
        ULONG validatedMatchCount = 0;

        const SIZE_T lastOffset =
            text.Size - sizeof(kFunctionPattern);

        for (SIZE_T offset = 0; offset <= lastOffset; ++offset)
        {
            const UCHAR* current =
                text.Base + offset;

            const SIZE_T availableBytes =
                text.Size - offset;

            if (!TridentPatternScanner::BytesMatch(
                current,
                availableBytes,
                kFunctionPattern,
                kFunctionMask,
                sizeof(kFunctionPattern)))
            {
                continue;
            }

            if (!ValidateCandidateLayout(
                current,
                availableBytes))
            {
                continue;
            }

            validatedMatch = current;
            ++validatedMatchCount;

            if (validatedMatchCount > 1)
            {
                return STATUS_OBJECT_NAME_COLLISION;
            }
        }

        if (validatedMatch == nullptr ||
            validatedMatchCount == 0)
        {
            return STATUS_NOT_FOUND;
        }

        *TextBase =
            const_cast<UCHAR*>(text.Base);

        *TextSize =
            text.Size;

        *FunctionAddress =
            const_cast<UCHAR*>(validatedMatch);

        *CandidateAddress =
            const_cast<UCHAR*>(
                validatedMatch + kSuppressionPointOffset);

        *SkipTargetAddress =
            const_cast<UCHAR*>(
                validatedMatch + kSkipTargetOffset);

        return STATUS_SUCCESS;
    }
}

NTSTATUS
TridentWin32kManager::Initialize()
{
    RtlZeroMemory(&g_Status, sizeof(g_Status));
    g_Status.OsVersion.dwOSVersionInfoSize = sizeof(g_Status.OsVersion);
    g_Status.State = TridentWin32kStateUninitialized;

    TridentHookManager::Initialize();

    NTSTATUS status = RtlGetVersion(&g_Status.OsVersion);
    if (!NT_SUCCESS(status))
    {
        g_Status.State = TridentWin32kStateFailed;
        g_Status.LastStatus = status;
        TridentLogError("RtlGetVersion failed: 0x%08X", status);
        return status;
    }

    TridentLogInfo(
        "OS version %lu.%lu build %lu detected",
        g_Status.OsVersion.dwMajorVersion,
        g_Status.OsVersion.dwMinorVersion,
        g_Status.OsVersion.dwBuildNumber
    );

    if (!IsSupportedBuild(g_Status.OsVersion))
    {
        g_Status.State = TridentWin32kStateUnsupportedBuild;
        g_Status.LastStatus = STATUS_NOT_SUPPORTED;
        TridentLogWarning(
            "touch cursor suppression is disabled: build %lu is not the validated build %lu",
            g_Status.OsVersion.dwBuildNumber,
            kValidatedBuild
        );
        return STATUS_NOT_SUPPORTED;
    }

    status = LocateWin32kFull(&g_Status.ModuleBase, &g_Status.ModuleSize);
    if (!NT_SUCCESS(status))
    {
        g_Status.State = TridentWin32kStateModuleUnavailable;
        g_Status.LastStatus = status;
        TridentLogWarning("win32kfull discovery failed: 0x%08X", status);
        return status;
    }

    TridentLogInfo(
        "win32kfull located: base=%p size=0x%lX",
        g_Status.ModuleBase,
        g_Status.ModuleSize
    );

    status = LocateAndValidateCandidate(
        g_Status.ModuleBase,
        g_Status.ModuleSize,
        &g_Status.TextBase,
        &g_Status.TextSize,
        &g_Status.FunctionAddress,
        &g_Status.CandidateAddress,
        &g_Status.SkipTargetAddress
    );
    if (!NT_SUCCESS(status))
    {
        g_Status.State = TridentWin32kStateSignatureUnavailable;
        g_Status.LastStatus = status;
        TridentLogWarning("candidate signature validation failed: 0x%08X", status);
        return status;
    }

    g_Status.FunctionRva = static_cast<ULONG>(
        static_cast<UCHAR*>(g_Status.FunctionAddress) - static_cast<UCHAR*>(g_Status.ModuleBase));
    g_Status.CandidateRva = static_cast<ULONG>(
        static_cast<UCHAR*>(g_Status.CandidateAddress) - static_cast<UCHAR*>(g_Status.ModuleBase));
    g_Status.SkipTargetRva = static_cast<ULONG>(
        static_cast<UCHAR*>(g_Status.SkipTargetAddress) - static_cast<UCHAR*>(g_Status.ModuleBase));

    TridentLogInfo(
        ".text located: base=%p size=0x%Iu",
        g_Status.TextBase,
        g_Status.TextSize
    );
    TridentLogInfo(
        "DeferPointerCursorOperation candidate: function=%p RVA=0x%lX",
        g_Status.FunctionAddress,
        g_Status.FunctionRva
    );
    TridentLogInfo(
        "validated suppression boundary: hook=%p RVA=0x%lX skip=%p RVA=0x%lX",
        g_Status.CandidateAddress,
        g_Status.CandidateRva,
        g_Status.SkipTargetAddress,
        g_Status.SkipTargetRva
    );

    status = TridentHookManager::Configure(
        g_Status.CandidateAddress,
        g_Status.SkipTargetAddress
    );

    if (!NT_SUCCESS(status))
    {
        g_Status.State = TridentWin32kStateFailed;
        g_Status.LastStatus = status;
        return status;
    }

    status = TridentHookManager::Prepare();

    if (!NT_SUCCESS(status))
    {
        g_Status.State = TridentWin32kStateFailed;
        g_Status.LastStatus = status;

        TridentLogWarning(
            "Hook prepare failed: 0x%08X",
            status
        );

        return status;
    }

    g_Status.State = TridentWin32kStateReady;
    g_Status.LastStatus = STATUS_SUCCESS;
    TridentLogInfo("touch cursor suppression discovery is ready; no patch has been installed");
    return STATUS_SUCCESS;
}

VOID
TridentWin32kManager::Shutdown()
{
    const NTSTATUS status = TridentHookManager::Disable();
    if (!NT_SUCCESS(status) && status != STATUS_NOT_SUPPORTED)
    {
        TridentLogWarning("hook disable returned 0x%08X", status);
    }

    TridentHookManager::Reset();
    RtlZeroMemory(&g_Status, sizeof(g_Status));
}

VOID
TridentWin32kManager::QueryStatus(
    _Out_ PTRIDENT_WIN32K_STATUS Status
)
{
    if (Status == nullptr)
    {
        return;
    }

    RtlCopyMemory(Status, &g_Status, sizeof(*Status));
}
