using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace user
{
    public sealed class DriverClient : IDisposable
    {
        private static readonly Guid DeviceInterfaceGuid =
            new Guid("7b3f8c21-29f4-4b6a-9c11-2f8a4e72d190");

        private const uint GENERIC_READ = 0x80000000;
        private const uint GENERIC_WRITE = 0x40000000;
        private const uint OPEN_EXISTING = 3;
        private const uint FILE_ATTRIBUTE_NORMAL = 0x80;

        private const int DIGCF_PRESENT = 0x00000002;
        private const int DIGCF_DEVICEINTERFACE = 0x00000010;

        private readonly SafeFileHandle _handle;
        private IOCTL.RETOUCH_FRAME _frame;

        public DriverClient()
        {
            string path = FindDevicePath();

            _handle = CreateFile(
                path,
                GENERIC_READ | GENERIC_WRITE,
                0,
                IntPtr.Zero,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                IntPtr.Zero);

            if (_handle.IsInvalid)
                throw new Win32Exception(Marshal.GetLastWin32Error(), $"CreateFile failed: {path}");

            _frame = new IOCTL.RETOUCH_FRAME
            {
                ContactCount = 0,
                Contacts = new IOCTL.RETOUCH_CONTACT[IOCTL.RETOUCH_MAX_CONTACTS]
            };
        }

        public void TouchDown(byte id, ushort x, ushort y) => SetContact(id, true, x, y);
        public void TouchMove(byte id, ushort x, ushort y) => SetContact(id, true, x, y);
        public void TouchUp(byte id) => SetContact(id, false, 0, 0);

        public void Commit()
        {
            int size = Marshal.SizeOf<IOCTL.RETOUCH_FRAME>();
            IntPtr buffer = Marshal.AllocHGlobal(size);

            try
            {
                Marshal.StructureToPtr(_frame, buffer, false);

                bool ok = DeviceIoControl(
                    _handle,
                    IOCTL.IOCTL_RETOUCH_SUBMIT_FRAME,
                    buffer,
                    size,
                    IntPtr.Zero,
                    0,
                    out _,
                    IntPtr.Zero);

                if (!ok)
                    throw new Win32Exception(Marshal.GetLastWin32Error());
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

        private void SetContact(byte id, bool isDown, ushort x, ushort y)
        {
            if (id >= IOCTL.RETOUCH_MAX_CONTACTS)
                throw new ArgumentOutOfRangeException(nameof(id));

            _frame.Contacts[id] = new IOCTL.RETOUCH_CONTACT
            {
                Id = id,
                IsDown = isDown ? (byte)1 : (byte)0,
                X = x,
                Y = y
            };

            byte max = 0;
            for (byte i = 0; i < IOCTL.RETOUCH_MAX_CONTACTS; i++)
            {
                if (_frame.Contacts[i].IsDown != 0)
                    max = (byte)(i + 1);
            }

            _frame.ContactCount = max;
        }

        private static string FindDevicePath()
        {
            Guid guid = DeviceInterfaceGuid;

            IntPtr infoSet = SetupDiGetClassDevs(
                ref guid,
                IntPtr.Zero,
                IntPtr.Zero,
                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

            if (infoSet == IntPtr.Zero || infoSet.ToInt64() == -1)
                throw new Win32Exception(Marshal.GetLastWin32Error(), "SetupDiGetClassDevs failed");

            try
            {
                SP_DEVICE_INTERFACE_DATA data = new SP_DEVICE_INTERFACE_DATA();
                data.cbSize = Marshal.SizeOf<SP_DEVICE_INTERFACE_DATA>();

                bool ok = SetupDiEnumDeviceInterfaces(
                    infoSet,
                    IntPtr.Zero,
                    ref guid,
                    0,
                    ref data);

                if (!ok)
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Device interface not found");

                SetupDiGetDeviceInterfaceDetail(
                    infoSet,
                    ref data,
                    IntPtr.Zero,
                    0,
                    out int requiredSize,
                    IntPtr.Zero);

                IntPtr detailBuffer = Marshal.AllocHGlobal(requiredSize);

                try
                {
                    if (IntPtr.Size == 8)
                        Marshal.WriteInt32(detailBuffer, 8);
                    else
                        Marshal.WriteInt32(detailBuffer, 6);

                    ok = SetupDiGetDeviceInterfaceDetail(
                        infoSet,
                        ref data,
                        detailBuffer,
                        requiredSize,
                        out _,
                        IntPtr.Zero);

                    if (!ok)
                        throw new Win32Exception(Marshal.GetLastWin32Error(), "SetupDiGetDeviceInterfaceDetail failed");

                    IntPtr pathPtr = IntPtr.Add(detailBuffer, 4);
                    if (IntPtr.Size == 8)
                        pathPtr = IntPtr.Add(detailBuffer, 8);

                    return Marshal.PtrToStringUni(pathPtr)
                        ?? throw new InvalidOperationException("Device path is null");
                }
                finally
                {
                    Marshal.FreeHGlobal(detailBuffer);
                }
            }
            finally
            {
                SetupDiDestroyDeviceInfoList(infoSet);
            }
        }

        public void Dispose()
        {
            _handle?.Dispose();
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct SP_DEVICE_INTERFACE_DATA
        {
            public int cbSize;
            public Guid InterfaceClassGuid;
            public int Flags;
            public IntPtr Reserved;
        }

        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern IntPtr SetupDiGetClassDevs(
            ref Guid ClassGuid,
            IntPtr Enumerator,
            IntPtr hwndParent,
            int Flags);

        [DllImport("setupapi.dll", SetLastError = true)]
        private static extern bool SetupDiEnumDeviceInterfaces(
            IntPtr DeviceInfoSet,
            IntPtr DeviceInfoData,
            ref Guid InterfaceClassGuid,
            int MemberIndex,
            ref SP_DEVICE_INTERFACE_DATA DeviceInterfaceData);

        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern bool SetupDiGetDeviceInterfaceDetail(
            IntPtr DeviceInfoSet,
            ref SP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
            IntPtr DeviceInterfaceDetailData,
            int DeviceInterfaceDetailDataSize,
            out int RequiredSize,
            IntPtr DeviceInfoData);

        [DllImport("setupapi.dll", SetLastError = true)]
        private static extern bool SetupDiDestroyDeviceInfoList(IntPtr DeviceInfoSet);

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern SafeFileHandle CreateFile(
            string lpFileName,
            uint dwDesiredAccess,
            uint dwShareMode,
            IntPtr lpSecurityAttributes,
            uint dwCreationDisposition,
            uint dwFlagsAndAttributes,
            IntPtr hTemplateFile);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DeviceIoControl(
            SafeFileHandle hDevice,
            uint dwIoControlCode,
            IntPtr lpInBuffer,
            int nInBufferSize,
            IntPtr lpOutBuffer,
            int nOutBufferSize,
            out int lpBytesReturned,
            IntPtr lpOverlapped);
    }
}