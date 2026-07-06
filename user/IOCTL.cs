using System;
using System.Runtime.InteropServices;

namespace user
{
    internal static class IOCTL
    {
        private const uint FILE_DEVICE_RETOUCH = 0x8000;
        private const uint METHOD_BUFFERED = 0;
        private const uint FILE_WRITE_DATA = 0x0002;

        private static uint CTL_CODE(uint deviceType, uint function, uint method, uint access)
        {
            return (deviceType << 16) | (access << 14) | (function << 2) | method;
        }

        public static readonly uint IOCTL_RETOUCH_SUBMIT_FRAME =
            CTL_CODE(FILE_DEVICE_RETOUCH, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA);

        public const int RETOUCH_MAX_CONTACTS = 10;

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct RETOUCH_CONTACT
        {
            public byte Id;
            public byte IsDown;
            public ushort X;
            public ushort Y;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct RETOUCH_FRAME
        {
            public byte ContactCount;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = RETOUCH_MAX_CONTACTS)]
            public RETOUCH_CONTACT[] Contacts;
        }
    }
}