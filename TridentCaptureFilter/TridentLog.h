#pragma once

#include <ntddk.h>

#define TRIDENT_LOG_PREFIX "[TridentCaptureFilter] "

#define TridentLogInfo(Format, ...) \
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, \
        TRIDENT_LOG_PREFIX Format "\n", __VA_ARGS__)

#define TridentLogWarning(Format, ...) \
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, \
        TRIDENT_LOG_PREFIX Format "\n", __VA_ARGS__)

#define TridentLogError(Format, ...) \
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, \
        TRIDENT_LOG_PREFIX Format "\n", __VA_ARGS__)
