using System.Runtime.InteropServices;

namespace Halcyon;

// Thin wrapper around SendInput. Everything reaches the game as ordinary raw
// input because the client never touches the target's memory for writing.
public static class Input
{
    private static readonly int InputSize = Marshal.SizeOf<Native.INPUT>();

    public static bool IsKeyDown(int vk)
    {
        if (vk <= 0) return false;
        return (Native.GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    public static bool Foreground(uint pid)
    {
        var foreground = Native.GetForegroundWindow();
        if (foreground == IntPtr.Zero) return false;
        Native.GetWindowThreadProcessId(foreground, out var owner);
        return owner == pid;
    }

    public static void SendKey(int vk, bool down)
    {
        if (vk <= 0) return;

        var input = new Native.INPUT
        {
            type = Native.INPUT_KEYBOARD,
        };
        input.u.ki.wVk = (ushort)vk;
        input.u.ki.wScan = (ushort)Native.MapVirtualKeyW((uint)vk, Native.MAPVK_VK_TO_VSC);
        input.u.ki.dwFlags = down ? 0 : Native.KEYEVENTF_KEYUP;

        Native.SendInput(1, new[] { input }, InputSize);
    }

    public static void Tap(int vk)
    {
        SendKey(vk, true);
        SendKey(vk, false);
    }

    public static void SendMouseButton(bool down, bool right)
    {
        var input = new Native.INPUT
        {
            type = Native.INPUT_MOUSE,
        };

        input.u.mi.dwFlags = right
            ? (down ? Native.MOUSEEVENTF_RIGHTDOWN : Native.MOUSEEVENTF_RIGHTUP)
            : (down ? Native.MOUSEEVENTF_LEFTDOWN : Native.MOUSEEVENTF_LEFTUP);

        Native.SendInput(1, new[] { input }, InputSize);
    }

    public static void SendMouseMove(int dx, int dy)
    {
        if (dx == 0 && dy == 0) return;

        var input = new Native.INPUT
        {
            type = Native.INPUT_MOUSE,
        };
        input.u.mi.dx = dx;
        input.u.mi.dy = dy;
        input.u.mi.dwFlags = Native.MOUSEEVENTF_MOVE;

        Native.SendInput(1, new[] { input }, InputSize);
    }

    public static void SendText(string text)
    {
        if (string.IsNullOrEmpty(text)) return;

        var inputs = new Native.INPUT[text.Length * 2];
        for (var i = 0; i < text.Length; ++i)
        {
            var down = new Native.INPUT
            {
                type = Native.INPUT_KEYBOARD,
            };
            down.u.ki.wVk = 0;
            down.u.ki.wScan = text[i];
            down.u.ki.dwFlags = Native.KEYEVENTF_UNICODE;

            var up = down;
            up.u.ki.dwFlags = Native.KEYEVENTF_UNICODE | Native.KEYEVENTF_KEYUP;

            inputs[i * 2] = down;
            inputs[i * 2 + 1] = up;
        }

        Native.SendInput((uint)inputs.Length, inputs, InputSize);
    }
}
