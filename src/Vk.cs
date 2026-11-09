using System;
using System.Collections.Generic;
using System.Globalization;

namespace Halcyon;

internal static class Vk
{
    public const int None = 0;
    public const int LButton = 0x01;
    public const int RButton = 0x02;
    public const int MButton = 0x04;
    public const int XButton1 = 0x05;
    public const int XButton2 = 0x06;
    public const int Back = 0x08;
    public const int Tab = 0x09;
    public const int Return = 0x0D;
    public const int Shift = 0x10;
    public const int Control = 0x11;
    public const int Menu = 0x12;
    public const int Pause = 0x13;
    public const int Capital = 0x14;
    public const int Escape = 0x1B;
    public const int Space = 0x20;
    public const int Prior = 0x21;
    public const int Next = 0x22;
    public const int End = 0x23;
    public const int Home = 0x24;
    public const int Left = 0x25;
    public const int Up = 0x26;
    public const int Right = 0x27;
    public const int Down = 0x28;
    public const int Insert = 0x2D;
    public const int Delete = 0x2E;
    public const int NumLock = 0x90;
    public const int ScrollLock = 0x91;
    public const int LShift = 0xA0;
    public const int RShift = 0xA1;
    public const int LControl = 0xA2;
    public const int RControl = 0xA3;
    public const int LMenu = 0xA4;
    public const int RMenu = 0xA5;

    public const int F1 = 0x70;
    public const int F2 = 0x71;
    public const int F3 = 0x72;
    public const int F4 = 0x73;
    public const int F5 = 0x74;
    public const int F6 = 0x75;
    public const int F7 = 0x76;
    public const int F8 = 0x77;
    public const int F9 = 0x78;
    public const int F10 = 0x79;
    public const int F11 = 0x7A;
    public const int F12 = 0x7B;
    public const int F13 = 0x7C;
    public const int F14 = 0x7D;
    public const int F15 = 0x7E;
    public const int F16 = 0x7F;
    public const int F17 = 0x80;
    public const int F18 = 0x81;
    public const int F19 = 0x82;
    public const int F20 = 0x83;
    public const int F21 = 0x84;
    public const int F22 = 0x85;
    public const int F23 = 0x86;
    public const int F24 = 0x87;

    private static readonly Dictionary<string, int> Names = new(StringComparer.OrdinalIgnoreCase)
    {
        ["NONE"] = 0,
        ["LBUTTON"] = LButton,
        ["RBUTTON"] = RButton,
        ["MBUTTON"] = MButton,
        ["XBUTTON1"] = XButton1,
        ["XBUTTON2"] = XButton2,
        ["MOUSE4"] = XButton1,
        ["MOUSE5"] = XButton2,
        ["BACK"] = Back,
        ["TAB"] = Tab,
        ["ENTER"] = Return,
        ["RETURN"] = Return,
        ["SHIFT"] = Shift,
        ["CTRL"] = Control,
        ["CONTROL"] = Control,
        ["ALT"] = Menu,
        ["MENU"] = Menu,
        ["PAUSE"] = Pause,
        ["CAPSLOCK"] = Capital,
        ["ESC"] = Escape,
        ["ESCAPE"] = Escape,
        ["SPACE"] = Space,
        ["PAGEUP"] = Prior,
        ["PRIOR"] = Prior,
        ["PAGEDOWN"] = Next,
        ["NEXT"] = Next,
        ["END"] = End,
        ["HOME"] = Home,
        ["LEFT"] = Left,
        ["UP"] = Up,
        ["RIGHT"] = Right,
        ["DOWN"] = Down,
        ["INSERT"] = Insert,
        ["DELETE"] = Delete,
        ["DEL"] = Delete,
        ["NUMLOCK"] = NumLock,
        ["SCROLLLOCK"] = ScrollLock,
        ["SCROLL"] = ScrollLock,
        ["CAPS"] = Capital,
        ["PGUP"] = Prior,
        ["PGDN"] = Next,
        ["LSHIFT"] = LShift,
        ["RSHIFT"] = RShift,
        ["LCTRL"] = LControl,
        ["RCTRL"] = RControl,
        ["LCONTROL"] = LControl,
        ["RCONTROL"] = RControl,
        ["LMENU"] = LMenu,
        ["RMENU"] = RMenu,
    };

    public static int NameToVk(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return 0;
        name = name.Trim();

        if (Names.TryGetValue(name, out var known)) return known;

        if (name.Length == 1)
        {
            var c = char.ToUpperInvariant(name[0]);
            if (c >= 'A' && c <= 'Z') return c;
            if (c >= '0' && c <= '9') return c;
        }

        if (name.StartsWith("F", StringComparison.OrdinalIgnoreCase) &&
            int.TryParse(name.AsSpan(1), NumberStyles.Integer, CultureInfo.InvariantCulture, out var f) &&
            f >= 1 && f <= 24)
        {
            return 0x6F + f;
        }

        if (name.StartsWith("NUMPAD", StringComparison.OrdinalIgnoreCase))
        {
            var rest = name.Substring(6);
            if (int.TryParse(rest, NumberStyles.Integer, CultureInfo.InvariantCulture, out var n) &&
                n >= 0 && n <= 9)
            {
                return 0x60 + n;
            }
        }

        if (name.StartsWith("0x", StringComparison.OrdinalIgnoreCase) &&
            int.TryParse(name.AsSpan(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var hex))
        {
            return hex & 0xFF;
        }

        if (int.TryParse(name, NumberStyles.Integer, CultureInfo.InvariantCulture, out var numeric))
        {
            return numeric & 0xFF;
        }

        return 0;
    }

    public static string ToName(int vk)
    {
        if (vk <= 0) return "NONE";

        switch (vk)
        {
            case LButton: return "LBUTTON";
            case RButton: return "RBUTTON";
            case MButton: return "MBUTTON";
            case XButton1: return "XBUTTON1";
            case XButton2: return "XBUTTON2";
            case Back: return "BACK";
            case Tab: return "TAB";
            case Return: return "ENTER";
            case Shift: return "SHIFT";
            case Control: return "CTRL";
            case Menu: return "ALT";
            case Pause: return "PAUSE";
            case Capital: return "CAPSLOCK";
            case Escape: return "ESC";
            case Space: return "SPACE";
            case Prior: return "PAGEUP";
            case Next: return "PAGEDOWN";
            case End: return "END";
            case Home: return "HOME";
            case Left: return "LEFT";
            case Up: return "UP";
            case Right: return "RIGHT";
            case Down: return "DOWN";
            case Insert: return "INSERT";
            case Delete: return "DELETE";
            case NumLock: return "NUMLOCK";
            case ScrollLock: return "SCROLLLOCK";
            case LShift: return "LSHIFT";
            case RShift: return "RSHIFT";
            case LControl: return "LCONTROL";
            case RControl: return "RCONTROL";
            case LMenu: return "LMENU";
            case RMenu: return "RMENU";
            default: break;
        }

        if (vk >= 0x70 && vk <= 0x87) return "F" + (vk - 0x6F).ToString(CultureInfo.InvariantCulture);
        if (vk >= 0x60 && vk <= 0x69) return "NUMPAD" + (vk - 0x60).ToString(CultureInfo.InvariantCulture);
        if (vk >= 'A' && vk <= 'Z') return ((char)vk).ToString();
        if (vk >= '0' && vk <= '9') return ((char)vk).ToString();

        return "0x" + vk.ToString("X2", CultureInfo.InvariantCulture);
    }
}
