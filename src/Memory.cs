using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Halcyon;

public sealed class ModuleInfo
{
    public string Name = "";
    public ulong Base;
    public uint Size;
}

// Read-only handle onto the target process plus the few primitives the rest of
// the program needs: typed reads, module lookup and byte-pattern scanning.
public sealed class Process : IDisposable
{
    private const int ChunkSize = 4 * 1024 * 1024;

    [ThreadStatic] private static byte[]? _scratch;

    private IntPtr _handle;
    private uint _pid;
    private List<ModuleInfo> _modules = new();
    private readonly Dictionary<string, int> _moduleIndex = new(StringComparer.OrdinalIgnoreCase);

    public IntPtr Handle => _handle;
    public uint Pid => _pid;
    public bool Running
    {
        get
        {
            if (_handle == IntPtr.Zero) return false;
            return Native.GetExitCodeProcess(_handle, out var code) && code == Native.STILL_ACTIVE;
        }
    }

    public void Dispose() => Detach();

    public void Detach()
    {
        if (_handle != IntPtr.Zero)
        {
            Native.CloseHandle(_handle);
            _handle = IntPtr.Zero;
        }

        _pid = 0;
        _modules = new List<ModuleInfo>();
        _moduleIndex.Clear();
    }

    // Finds `exe` by image name and opens it with VM_READ | QUERY_INFORMATION.
    public bool Attach(string exe)
    {
        Detach();

        var snap = Native.CreateToolhelp32Snapshot(Native.TH32CS_SNAPPROCESS, 0);
        if (snap == Native.INVALID_HANDLE_VALUE) return false;

        var pe = new Native.PROCESSENTRY32W
        {
            dwSize = (uint)Marshal.SizeOf<Native.PROCESSENTRY32W>(),
            szExeFile = "",
        };

        var found = false;
        uint pid = 0;

        if (Native.Process32FirstW(snap, ref pe))
        {
            do
            {
                if (string.Equals(pe.szExeFile, exe, StringComparison.OrdinalIgnoreCase))
                {
                    pid = pe.th32ProcessID;
                    found = true;
                    break;
                }
            } while (Native.Process32NextW(snap, ref pe));
        }

        Native.CloseHandle(snap);
        if (!found || pid == 0) return false;

        var handle = Native.OpenProcess(Native.PROCESS_VM_READ | Native.PROCESS_QUERY_INFORMATION, false,
            (int)pid);
        if (handle == IntPtr.Zero) return false;

        _handle = handle;
        _pid = pid;
        return RefreshModules();
    }

    public bool RefreshModules()
    {
        var modules = new List<ModuleInfo>();
        var index = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);

        var snap = Native.CreateToolhelp32Snapshot(Native.TH32CS_SNAPMODULE | Native.TH32CS_SNAPMODULE32, _pid);
        if (snap == Native.INVALID_HANDLE_VALUE) return false;

        var me = new Native.MODULEENTRY32W
        {
            dwSize = (uint)Marshal.SizeOf<Native.MODULEENTRY32W>(),
            szModule = "",
            szExePath = "",
        };

        if (Native.Module32FirstW(snap, ref me))
        {
            do
            {
                var info = new ModuleInfo
                {
                    Name = me.szModule ?? "",
                    Base = (ulong)me.modBaseAddr.ToInt64(),
                    Size = me.modBaseSize,
                };

                index[info.Name] = modules.Count;
                modules.Add(info);
            } while (Native.Module32NextW(snap, ref me));
        }

        Native.CloseHandle(snap);

        _modules = modules;
        _moduleIndex.Clear();
        foreach (var (name, i) in index) _moduleIndex[name] = i;
        return modules.Count > 0;
    }

    public ModuleInfo? Module(string name)
    {
        if (_moduleIndex.Count == 0 && !RefreshModules()) return null;

        if (_moduleIndex.TryGetValue(name, out var i)) return _modules[i];

        // Case-insensitive fallback, module names are not guaranteed in case.
        foreach (var m in _modules)
        {
            if (string.Equals(m.Name, name, StringComparison.OrdinalIgnoreCase)) return m;
        }

        return null;
    }

    public bool ReadBytes(ulong address, byte[] buffer, int size)
    {
        if (_handle == IntPtr.Zero || address == 0 || size <= 0) return false;

        unsafe
        {
            fixed (byte* p = buffer)
            {
                var ok = Native.ReadProcessMemoryPtr(_handle, (IntPtr)(long)address, (IntPtr)p, (IntPtr)size,
                    out var got);
                return ok && (long)got == size;
            }
        }
    }

    public bool Read<T>(ulong address, out T value) where T : unmanaged
    {
        value = default;
        var size = Unsafe.SizeOf<T>();
        if (size <= 0) return false;

        var buffer = _scratch ??= new byte[512];
        if (buffer.Length < size)
        {
            buffer = new byte[size];
            _scratch = buffer;
        }

        if (!ReadBytes(address, buffer, size)) return false;
        value = Unsafe.ReadUnaligned<T>(ref buffer[0]);
        return true;
    }

    public T ReadOr<T>(ulong address, T fallback = default) where T : unmanaged
    {
        return Read<T>(address, out var value) ? value : fallback;
    }

    public ulong ReadPtr(ulong address)
    {
        return Read<ulong>(address, out var value) ? value : 0;
    }

    public Vec3 ReadVec3(ulong address)
    {
        return Read<Vec3>(address, out var value) ? value : default;
    }

    public bool ReadFloats(ulong address, float[] destination, int count)
    {
        var size = count * 4;
        var bytes = new byte[size];
        if (!ReadBytes(address, bytes, size)) return false;
        Buffer.BlockCopy(bytes, 0, destination, 0, size);
        return true;
    }

    // Scans `module` for an IDA style pattern ("48 8B 05 ? ? ? ?").
    public ulong Scan(string moduleName, string pattern, out int matchCount)
    {
        matchCount = 0;

        var mod = Module(moduleName);
        if (mod == null || mod.Size == 0) return 0;

        if (!ParsePattern(pattern, out var bytes, out var mask)) return 0;
        if (bytes.Length == 0) return 0;

        var image = new byte[mod.Size];
        ReadChunked(mod.Base, image);

        if (image.Length < bytes.Length) return 0;

        ulong first = 0;
        var hits = 0;
        var m = bytes.Length;
        var limit = image.Length - m;
        var offset = 0;

        while (offset <= limit)
        {
            var pos = Array.IndexOf(image, bytes[0], offset, limit - offset + 1);
            if (pos < 0) break;

            var matched = true;
            for (var k = 1; k < m; ++k)
            {
                if (mask[k] != 0 && image[pos + k] != bytes[k])
                {
                    matched = false;
                    break;
                }
            }

            if (matched)
            {
                if (hits == 0) first = mod.Base + (ulong)pos;
                ++hits;
            }

            offset = pos + 1;
        }

        matchCount = hits;
        return first;
    }

    private void ReadChunked(ulong address, byte[] destination)
    {
        var done = 0;
        while (done < destination.Length)
        {
            var step = Math.Min(ChunkSize, destination.Length - done);

            unsafe
            {
                fixed (byte* p = &destination[done])
                {
                    var ok = Native.ReadProcessMemoryPtr(_handle, (IntPtr)(long)(address + (ulong)done),
                        (IntPtr)p, (IntPtr)step, out var got);
                    if (!ok) Array.Clear(destination, done, step);
                }
            }

            done += step;
        }
    }

    public static string ExecutableNameOf(uint pid)
    {
        var snap = Native.CreateToolhelp32Snapshot(Native.TH32CS_SNAPPROCESS, 0);
        if (snap == Native.INVALID_HANDLE_VALUE) return "";

        var pe = new Native.PROCESSENTRY32W
        {
            dwSize = (uint)Marshal.SizeOf<Native.PROCESSENTRY32W>(),
            szExeFile = "",
        };

        var name = "";
        if (Native.Process32FirstW(snap, ref pe))
        {
            do
            {
                if (pe.th32ProcessID == pid)
                {
                    name = pe.szExeFile ?? "";
                    break;
                }
            } while (Native.Process32NextW(snap, ref pe));
        }

        Native.CloseHandle(snap);
        return name;
    }

    // Decodes `mov/lea reg, [rip+disp32]` at `address` and returns the referenced
    // absolute address. Returns 0 if the instruction is not RIP-relative.
    public ulong DecodeRipRelative(ulong address)
    {
        var code = new byte[16];
        if (!ReadBytes(address, code, code.Length)) return 0;

        var i = 0;
        while (i < 4)
        {
            var b = code[i];
            if (b is 0x66 or 0x67 or 0x2E or 0x3E or 0x26 or 0x36 or 0x64 or 0x65 or 0xF0 or 0xF2 or 0xF3)
            {
                ++i;
                continue;
            }

            break;
        }

        if ((code[i] & 0xF0) == 0x40) ++i;

        var opcode = code[i];
        if (opcode is not (0x8B or 0x8D or 0x89 or 0x3B)) return 0;
        ++i;

        var modrm = code[i];
        if ((modrm & 0xC7) != 0x05) return 0; // mod=00, rm=101 -> RIP relative
        ++i;

        var disp = BitConverter.ToInt32(code, i);
        var length = i + 4;
        return address + (ulong)length + (ulong)(long)disp;
    }

    public static bool ParsePattern(string pattern, out byte[] bytes, out byte[] mask)
    {
        var byteList = new List<byte>();
        var maskList = new List<byte>();

        foreach (var token in pattern.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries))
        {
            if (token[0] == '?')
            {
                byteList.Add(0);
                maskList.Add(0);
                continue;
            }

            if (!byte.TryParse(token, System.Globalization.NumberStyles.HexNumber,
                    System.Globalization.CultureInfo.InvariantCulture, out var value))
            {
                bytes = Array.Empty<byte>();
                mask = Array.Empty<byte>();
                return false;
            }

            byteList.Add(value);
            maskList.Add(0xFF);
        }

        bytes = byteList.ToArray();
        mask = maskList.ToArray();
        return bytes.Length > 0;
    }
}
