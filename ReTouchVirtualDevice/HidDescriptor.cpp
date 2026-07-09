#include "HidDescriptor.h"

#define RETOUCH_FINGER_COLLECTION \
    0x05, 0x0D,        /* Usage Page (Digitizers) */ \
    0x09, 0x22,        /* Usage (Finger) */ \
    0xA1, 0x02,        /* Collection (Logical) */ \
    0x09, 0x42,        /*   Usage (Tip Switch) */ \
    0x09, 0x32,        /*   Usage (In Range) */ \
    0x09, 0x47,        /*   Usage (Confidence) */ \
    0x15, 0x00,        /*   Logical Minimum (0) */ \
    0x25, 0x01,        /*   Logical Maximum (1) */ \
    0x75, 0x01,        /*   Report Size (1) */ \
    0x95, 0x03,        /*   Report Count (3) */ \
    0x81, 0x02,        /*   Input (Data,Var,Abs) */ \
    0x75, 0x05,        /*   Report Size (5) */ \
    0x95, 0x01,        /*   Report Count (1) */ \
    0x81, 0x03,        /*   Input (Const,Var,Abs) */ \
    0x05, 0x0D,        /*   Usage Page (Digitizers) */ \
    0x09, 0x51,        /*   Usage (Contact Identifier) */ \
    0x15, 0x00,        /*   Logical Minimum (0) */ \
    0x25, 0x0A,        /*   Logical Maximum (10) */ \
    0x75, 0x08,        /*   Report Size (8) */ \
    0x95, 0x01,        /*   Report Count (1) */ \
    0x81, 0x02,        /*   Input (Data,Var,Abs) */ \
    0x05, 0x01,        /*   Usage Page (Generic Desktop) */ \
    0x09, 0x30,        /*   Usage (X) */ \
    0x15, 0x00,        /*   Logical Minimum (0) */ \
    0x27, 0xFF, 0xFF, 0x00, 0x00,  /* Logical Maximum (65535) */ \
    0x35, 0x00,        /*   Physical Minimum (0) */ \
    0x47, 0xFF, 0xFF, 0x00, 0x00,  /* Physical Maximum (65535) */ \
    0x55, 0x00,        /*   Unit Exponent (0) */ \
    0x65, 0x00,        /*   Unit (None) */ \
    0x75, 0x10,        /*   Report Size (16) */ \
    0x95, 0x01,        /*   Report Count (1) */ \
    0x81, 0x02,        /*   Input (Data,Var,Abs) */ \
    0x09, 0x31,        /*   Usage (Y) */ \
    0x15, 0x00,        /*   Logical Minimum (0) */ \
    0x27, 0xFF, 0xFF, 0x00, 0x00,  /* Logical Maximum (65535) */ \
    0x35, 0x00,        /*   Physical Minimum (0) */ \
    0x47, 0xFF, 0xFF, 0x00, 0x00,  /* Physical Maximum (65535) */ \
    0x55, 0x00,        /*   Unit Exponent (0) */ \
    0x65, 0x00,        /*   Unit (None) */ \
    0x75, 0x10,        /*   Report Size (16) */ \
    0x95, 0x01,        /*   Report Count (1) */ \
    0x81, 0x02,        /*   Input (Data,Var,Abs) */ \
    0xC0               /* End Collection */

const UCHAR g_ReportDescriptor[] =
{
    0x05, 0x0D,        // Usage Page (Digitizers)
    0x09, 0x04,        // Usage (Touch Screen)
    0xA1, 0x01,        // Collection (Application)

    0x85, 0x01,        //   Report ID (1)

    RETOUCH_FINGER_COLLECTION,
    RETOUCH_FINGER_COLLECTION,
    RETOUCH_FINGER_COLLECTION,
    RETOUCH_FINGER_COLLECTION,
    RETOUCH_FINGER_COLLECTION,
    RETOUCH_FINGER_COLLECTION,
    RETOUCH_FINGER_COLLECTION,
    RETOUCH_FINGER_COLLECTION,
    RETOUCH_FINGER_COLLECTION,
    RETOUCH_FINGER_COLLECTION,

    0x05, 0x0D,        //   Usage Page (Digitizers)
    0x09, 0x54,        //   Usage (Contact Count)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x0A,        //   Logical Maximum (10)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    0x09, 0x56,        //   Usage (Scan Time)
    0x15, 0x00,        //   Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  // Logical Maximum (65535)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs)

    0x85, 0x02,        //   Report ID (2)
    0x09, 0x55,        //   Usage (Contact Count Maximum)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x0A,        //   Logical Maximum (10)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0xB1, 0x02,        //   Feature (Data,Var,Abs)

    0xC0               // End Collection
};

const ULONG g_ReportDescriptorSize = sizeof(g_ReportDescriptor);