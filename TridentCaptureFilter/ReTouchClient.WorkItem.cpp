#include "ReTouchClient.h"
#include "GlobalContext.h"

typedef struct _RETOUCH_WORKITEM_CONTEXT
{
    ReTouchClient::PCLIENT_CONTEXT Client;
} RETOUCH_WORKITEM_CONTEXT, * PRETOUCH_WORKITEM_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    RETOUCH_WORKITEM_CONTEXT,
    ReTouchWorkItemGetContext
);

namespace ReTouchClient
{
    NTSTATUS CreateWorkItem(
        _Inout_ PCLIENT_CONTEXT Client,
        _In_ WDFDEVICE Device
    )
    {
        if (Client == nullptr || Device == nullptr)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (Client->WorkItem != nullptr)
        {
            return STATUS_SUCCESS;
        }

        WDF_WORKITEM_CONFIG workItemConfig;
        WDF_WORKITEM_CONFIG_INIT(
            &workItemConfig,
            ReTouchClientWorkItem
        );

        WDF_OBJECT_ATTRIBUTES workItemAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
            &workItemAttributes,
            RETOUCH_WORKITEM_CONTEXT
        );

        workItemAttributes.ParentObject = Device;

        WDFWORKITEM workItem = nullptr;

        NTSTATUS status = WdfWorkItemCreate(
            &workItemConfig,
            &workItemAttributes,
            &workItem
        );

        if (!NT_SUCCESS(status))
        {
            Client->WorkItem = nullptr;
            return status;
        }

        PRETOUCH_WORKITEM_CONTEXT workItemContext =
            ReTouchWorkItemGetContext(workItem);

        workItemContext->Client = Client;

        Client->WorkItem = workItem;

        return STATUS_SUCCESS;
    }

    VOID ReTouchClientWorkItem(
        _In_ WDFWORKITEM WorkItem
    )
    {
        PRETOUCH_WORKITEM_CONTEXT workItemContext =
            ReTouchWorkItemGetContext(WorkItem);

        if (workItemContext == nullptr ||
            workItemContext->Client == nullptr)
        {
            return;
        }

        PCLIENT_CONTEXT client = workItemContext->Client;
        PTRIDENT_GLOBAL_STATS globalStats = TridentGetGlobalStats();

        for (;;)
        {
            RETOUCH_FRAME frame = {};

            KIRQL oldIrql;
            KeAcquireSpinLock(
                &client->FrameLock,
                &oldIrql
            );

            RtlCopyMemory(
                &frame,
                &client->LatestFrame,
                sizeof(frame)
            );

            KeReleaseSpinLock(
                &client->FrameLock,
                oldIrql
            );

            InterlockedIncrement(&client->WorkItemRunCount);
            InterlockedIncrement(&globalStats->WorkItemRunCount);

            InterlockedExchange(
                &client->LastWorkItemContactCount,
                frame.ContactCount
            );

            InterlockedExchange(
                &globalStats->LastWorkItemContactCount,
                frame.ContactCount
            );

            NTSTATUS sendStatus =
                SendFrameToDevice(
                    client,
                    &frame
                );

            InterlockedExchange(
                &client->LastSubmitFrameStatus,
                sendStatus
            );

            InterlockedExchange(
                &globalStats->LastSendFrameStatus,
                sendStatus
            );

            InterlockedExchange(
                &globalStats->LastSubmitFrameStatus,
                sendStatus
            );

            LONG oldState =
                InterlockedCompareExchange(
                    &client->WorkItemQueued,
                    0,
                    1
                );

            if (oldState == 1)
            {
                break;
            }

            oldState =
                InterlockedCompareExchange(
                    &client->WorkItemQueued,
                    1,
                    2
                );

            if (oldState == 2)
            {
                continue;
            }

            InterlockedExchange(
                &client->WorkItemQueued,
                0
            );

            break;
        }
    }
}