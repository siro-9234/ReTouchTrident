#include "ReTouchClient.h"
#include "GlobalContext.h"

#include <initguid.h>

DEFINE_GUID(
    GUID_DEVINTERFACE_RETOUCH,
    0x7b3f8c21, 0x29f4, 0x4b6a,
    0x9c, 0x11, 0x2f, 0x8a, 0x4e, 0x72, 0xd1, 0x90
);

#define FILE_DEVICE_RETOUCH 0x8000

#define IOCTL_RETOUCH_SUBMIT_FRAME \
    CTL_CODE(FILE_DEVICE_RETOUCH, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)

namespace ReTouchClient
{
    static NTSTATUS QueryInterface(
        _Inout_ PCLIENT_CONTEXT Client,
        _Outptr_result_maybenull_ PWSTR* SymbolicLinkList
    );

    static LONG ExtractRootSystemNumber(
        _In_z_ PWSTR SymbolicLink
    )
    {
        if (SymbolicLink == nullptr)
        {
            return 0;
        }

        for (PWSTR p = SymbolicLink; *p != UNICODE_NULL; p++)
        {
            if ((p[0] == L'0') &&
                (p[1] == L'0') &&
                (p[2] == L'0') &&
                (p[3] >= L'0') &&
                (p[3] <= L'9'))
            {
                return static_cast<LONG>(p[3] - L'0');
            }
        }

        return 0;
    }

    static NTSTATUS OpenFirstInterface(
        _Inout_ PCLIENT_CONTEXT Client,
        _In_z_ PWSTR SymbolicLink
    );

    static volatile LONG g_NextClientInstanceId = 0;

    NTSTATUS SendFrameToDevice(
        _Inout_ PCLIENT_CONTEXT Client,
        _In_ PRETOUCH_FRAME Frame
    )
    {
        if (Client == nullptr || Frame == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (KeGetCurrentIrql() != PASSIVE_LEVEL)
        {
            return STATUS_INVALID_DEVICE_STATE;
        }

        if (Client->DeviceHandle == nullptr)
        {
            PWSTR symbolicLinkList = nullptr;

            NTSTATUS reopenStatus =
                QueryInterface(
                    Client,
                    &symbolicLinkList
                );

            if (NT_SUCCESS(reopenStatus) &&
                symbolicLinkList != nullptr &&
                symbolicLinkList[0] != UNICODE_NULL)
            {
                reopenStatus =
                    OpenFirstInterface(
                        Client,
                        symbolicLinkList
                    );
            }

            if (symbolicLinkList != nullptr)
            {
                ExFreePool(symbolicLinkList);
            }

            if (!NT_SUCCESS(reopenStatus))
            {
                return reopenStatus;
            }
        }

        if (Client->DeviceHandle == nullptr)
        {
            return STATUS_DEVICE_NOT_READY;
        }

        IO_STATUS_BLOCK ioStatus = {};

        const ULONG ioctlCode =
            static_cast<ULONG>(IOCTL_RETOUCH_SUBMIT_FRAME);

        const ULONG inputLength =
            static_cast<ULONG>(sizeof(RETOUCH_FRAME));

        NTSTATUS status =
            ZwDeviceIoControlFile(
                Client->DeviceHandle,
                nullptr,
                nullptr,
                nullptr,
                &ioStatus,
                ioctlCode,
                Frame,
                inputLength,
                nullptr,
                0UL
            );

        if (!NT_SUCCESS(status))
        {
            if (status == STATUS_INVALID_HANDLE ||
                status == STATUS_OBJECT_TYPE_MISMATCH ||
                status == STATUS_FILE_CLOSED)
            {
                ZwClose(Client->DeviceHandle);
                Client->DeviceHandle = nullptr;
                InterlockedExchange(&Client->OpenSucceeded, 0);
            }
        }

        return status;
    }

    static NTSTATUS QueryInterface(
        _Inout_ PCLIENT_CONTEXT Client,
        _Outptr_result_maybenull_ PWSTR* SymbolicLinkList
    )
    {
        if (Client == nullptr || SymbolicLinkList == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        *SymbolicLinkList = nullptr;

        InterlockedIncrement(&Client->QueryInterfaceCount);

        NTSTATUS status = IoGetDeviceInterfaces(
            &GUID_DEVINTERFACE_RETOUCH,
            nullptr,
            DEVICE_INTERFACE_INCLUDE_NONACTIVE,
            SymbolicLinkList
        );

        InterlockedExchange(&Client->LastQueryInterfaceStatus, status);

        if (NT_SUCCESS(status) &&
            *SymbolicLinkList != nullptr &&
            (*SymbolicLinkList)[0] != UNICODE_NULL)
        {
            InterlockedExchange(&Client->InterfaceFound, 1);
        }
        else
        {
            InterlockedExchange(&Client->InterfaceFound, 0);
        }

        return status;
    }

    static NTSTATUS OpenFirstInterface(
        _Inout_ PCLIENT_CONTEXT Client,
        _In_z_ PWSTR SymbolicLink
    )
    {
        PTRIDENT_GLOBAL_STATS globalStats = TridentGetGlobalStats();

        if (Client == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        InterlockedIncrement(&Client->OpenCount);
        InterlockedIncrement(&globalStats->OpenInterfaceAttemptCount);

        if (SymbolicLink == nullptr || SymbolicLink[0] == UNICODE_NULL)
        {
            InterlockedExchange(&Client->OpenSucceeded, 0);
            InterlockedExchange(&Client->LastOpenStatus, STATUS_OBJECT_NAME_NOT_FOUND);
            InterlockedExchange(&globalStats->LastOpenInterfaceStatus, STATUS_OBJECT_NAME_NOT_FOUND);
            InterlockedExchange(&globalStats->LastOpenedRootSystemNumber, 0);
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        InterlockedExchange(
            &globalStats->LastOpenedRootSystemNumber,
            ExtractRootSystemNumber(SymbolicLink)
        );

        if (Client->DeviceHandle != nullptr)
        {
            ZwClose(Client->DeviceHandle);
            Client->DeviceHandle = nullptr;
        }

        UNICODE_STRING objectName;
        RtlInitUnicodeString(&objectName, SymbolicLink);

        OBJECT_ATTRIBUTES objectAttributes;
        InitializeObjectAttributes(
            &objectAttributes,
            &objectName,
            OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
            nullptr,
            nullptr
        );

        IO_STATUS_BLOCK ioStatus = {};
        HANDLE handle = nullptr;

        NTSTATUS status = ZwCreateFile(
            &handle,
            GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
            &objectAttributes,
            &ioStatus,
            nullptr,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_OPEN,
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
            nullptr,
            0
        );

        InterlockedExchange(&Client->LastOpenStatus, status);
        InterlockedExchange(&globalStats->LastOpenInterfaceStatus, status);

        if (NT_SUCCESS(status))
        {
            Client->DeviceHandle = handle;
            InterlockedExchange(&Client->OpenSucceeded, 1);
            InterlockedExchange(&globalStats->OpenInterfaceSucceededCount, 1);
        }
        else
        {
            InterlockedExchange(&Client->OpenSucceeded, 0);

            if (handle != nullptr)
            {
                ZwClose(handle);
            }
        }

        return status;
    }

    static NTSTATUS SubmitOneTestFrame(
        _Inout_ PCLIENT_CONTEXT Client
    )
    {
        if (Client == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        InterlockedIncrement(&Client->TestSubmitCount);

        RETOUCH_FRAME frame = {};
        frame.ContactCount = 0;

        NTSTATUS status = SendFrameToDevice(Client, &frame);

        InterlockedExchange(&Client->LastTestSubmitStatus, status);
        InterlockedExchange(&Client->TestSubmitSucceeded, NT_SUCCESS(status) ? 1 : 0);

        return status;
    }

    NTSTATUS Initialize(
        _Inout_ PCLIENT_CONTEXT Client
    )
    {
        if (Client == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (Client->ClientInstanceId == 0)
        {
            InterlockedExchange(
                &Client->ClientInstanceId,
                InterlockedIncrement(&g_NextClientInstanceId)
            );
        }

        KeInitializeSpinLock(&Client->FrameLock);

        InterlockedExchange(&Client->WorkItemQueued, 0);

        InterlockedIncrement(&Client->InitializeCount);

        PWSTR symbolicLinkList = nullptr;

        NTSTATUS status = QueryInterface(Client, &symbolicLinkList);

        if (NT_SUCCESS(status) &&
            symbolicLinkList != nullptr &&
            symbolicLinkList[0] != UNICODE_NULL)
        {
            status = OpenFirstInterface(Client, symbolicLinkList);
        }

        if (symbolicLinkList != nullptr)
        {
            ExFreePool(symbolicLinkList);
        }

        if (NT_SUCCESS(status))
        {
            SubmitOneTestFrame(Client);
        }

        return status;
    }

    VOID Shutdown(
        _Inout_ PCLIENT_CONTEXT Client
    )
    {
        if (Client == nullptr)
        {
            return;
        }

        InterlockedIncrement(&Client->ShutdownCount);

        if (Client->WorkItem != nullptr)
        {
            WdfWorkItemFlush(Client->WorkItem);
        }

        if (Client->DeviceHandle != nullptr)
        {
            ZwClose(Client->DeviceHandle);
            Client->DeviceHandle = nullptr;
        }

        InterlockedExchange(&Client->OpenSucceeded, 0);
    }

    NTSTATUS SubmitFrame(
        _Inout_ PCLIENT_CONTEXT Client,
        _In_ PRETOUCH_FRAME Frame
    )
    {
        PTRIDENT_GLOBAL_STATS globalStats = TridentGetGlobalStats();

        if (Client == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        InterlockedExchange(
            &Client->DebugClientPointerLow,
            static_cast<LONG>(PtrToUlong(Client))
        );

        InterlockedIncrement(&Client->SubmitFrameCount);
        InterlockedIncrement(&globalStats->SubmitFrameCount);

        if (Frame == nullptr)
        {
            InterlockedExchange(&Client->LastSubmitFrameStatus, STATUS_INVALID_PARAMETER);
            InterlockedExchange(&Client->LastSubmitFrameContactCount, 0);
            InterlockedExchange(&globalStats->LastSubmitFrameStatus, STATUS_INVALID_PARAMETER);
            return STATUS_INVALID_PARAMETER;
        }

        KIRQL oldIrql;
        KeAcquireSpinLock(
            &Client->FrameLock,
            &oldIrql
        );

        RtlCopyMemory(
            &Client->LatestFrame,
            Frame,
            sizeof(RETOUCH_FRAME)
        );

        KeReleaseSpinLock(
            &Client->FrameLock,
            oldIrql
        );

        InterlockedExchange(&Client->LastSubmitFrameContactCount, Frame->ContactCount);

        if (Client->WorkItem == nullptr)
        {
            InterlockedExchange(&Client->LastSubmitFrameStatus, STATUS_DEVICE_NOT_READY);
            InterlockedExchange(&globalStats->LastSubmitFrameStatus, STATUS_DEVICE_NOT_READY);
            return STATUS_DEVICE_NOT_READY;
        }

        LONG oldState =
            InterlockedCompareExchange(
                &Client->WorkItemQueued,
                1,
                0
            );

        if (oldState == 0)
        {
            InterlockedIncrement(&Client->WorkItemEnqueueCount);
            InterlockedIncrement(&globalStats->WorkItemEnqueueCount);

            WdfWorkItemEnqueue(
                Client->WorkItem
            );
        }
        else
        {
            InterlockedExchange(
                &Client->WorkItemQueued,
                2
            );
        }

        InterlockedExchange(&Client->LastSubmitFrameStatus, STATUS_PENDING);
        InterlockedExchange(&globalStats->LastSubmitFrameStatus, STATUS_PENDING);

        return STATUS_PENDING;
    }

    LONG GetInitializeCount(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->InitializeCount, 0, 0); }
    LONG GetShutdownCount(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->ShutdownCount, 0, 0); }

    LONG GetSubmitFrameCount(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->SubmitFrameCount, 0, 0); }
    LONG GetLastSubmitFrameStatus(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->LastSubmitFrameStatus, 0, 0); }
    LONG GetLastSubmitFrameContactCount(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->LastSubmitFrameContactCount, 0, 0); }

    LONG GetQueryInterfaceCount(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->QueryInterfaceCount, 0, 0); }
    LONG GetInterfaceFound(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->InterfaceFound, 0, 0); }
    LONG GetLastQueryInterfaceStatus(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->LastQueryInterfaceStatus, 0, 0); }

    LONG GetOpenCount(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->OpenCount, 0, 0); }
    LONG GetOpenSucceeded(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->OpenSucceeded, 0, 0); }
    LONG GetLastOpenStatus(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->LastOpenStatus, 0, 0); }

    LONG GetTestSubmitCount(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->TestSubmitCount, 0, 0); }
    LONG GetTestSubmitSucceeded(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->TestSubmitSucceeded, 0, 0); }
    LONG GetLastTestSubmitStatus(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->LastTestSubmitStatus, 0, 0); }

    LONG GetWorkItemEnqueueCount(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->WorkItemEnqueueCount, 0, 0); }
    LONG GetWorkItemRunCount(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->WorkItemRunCount, 0, 0); }
    LONG GetLastWorkItemContactCount(_In_ PCLIENT_CONTEXT Client) { return InterlockedCompareExchange(&Client->LastWorkItemContactCount, 0, 0); }
}