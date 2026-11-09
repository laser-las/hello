using System.Globalization;

namespace Halcyon;

internal static class MathUtil
{
    public const float Pi = 3.14159265358979323846f;
    public const float Rad2Deg = 180.0f / Pi;

    public static bool Finite(float v) => float.IsFinite(v);

    public static bool Finite3(Vec3 v) =>
        float.IsFinite(v.X) && float.IsFinite(v.Y) && float.IsFinite(v.Z) &&
        MathF.Abs(v.X) < 1.0e7f && MathF.Abs(v.Y) < 1.0e7f && MathF.Abs(v.Z) < 1.0e7f;

    // Source style view matrix: 16 floats, row major, as stored in the engine.
    public static bool WorldToScreen(Mat4 vm, Vec3 p, float width, float height, out Vec2 screen)
    {
        screen = default;

        var m = vm.M;
        var w = m[3] * p.X + m[7] * p.Y + m[11] * p.Z + m[15];
        if (w < 0.001f) return false;

        var x = m[0] * p.X + m[4] * p.Y + m[8] * p.Z + m[12];
        var y = m[1] * p.X + m[5] * p.Y + m[9] * p.Z + m[13];

        screen.X = (x / w * 0.5f + 0.5f) * width;
        screen.Y = (-y / w * 0.5f + 0.5f) * height;

        return screen.X > -width && screen.X < width * 2.0f &&
               screen.Y > -height && screen.Y < height * 2.0f;
    }

    public static float ToUnitsMeters(float units) => units * 0.01905f;
}

public struct Vec2
{
    public float X;
    public float Y;

    public Vec2(float x, float y)
    {
        X = x;
        Y = y;
    }
}

public struct Vec3
{
    public float X;
    public float Y;
    public float Z;

    public Vec3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public static Vec3 operator +(Vec3 a, Vec3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
    public static Vec3 operator -(Vec3 a, Vec3 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
    public static Vec3 operator *(Vec3 a, float s) => new(a.X * s, a.Y * s, a.Z * s);

    public float Length() => MathF.Sqrt(X * X + Y * Y + Z * Z);
    public float Length2D() => MathF.Sqrt(X * X + Y * Y);
}

// A managed array keeps the interop read trivial while still exposing the exact
// index layout the engine stores.
public sealed class Mat4
{
    public readonly float[] M = new float[16];
}

public sealed class Clock
{
    private readonly System.Diagnostics.Stopwatch _watch = System.Diagnostics.Stopwatch.StartNew();

    public void Reset() => _watch.Restart();

    public double Ms() => _watch.Elapsed.TotalMilliseconds;
}

// Minimal timestamped logger. Writes to stdout and to a file when one is open.
public static class Log
{
    private static readonly object Gate = new();
    private static StreamWriter? _file;

    public static void OpenFile(string path)
    {
        lock (Gate)
        {
            _file?.Dispose();
            try
            {
                _file = new StreamWriter(path, false)
                {
                    AutoFlush = true
                };
            }
            catch
            {
                _file = null;
            }
        }
    }

    public static void CloseFile()
    {
        lock (Gate)
        {
            _file?.Dispose();
            _file = null;
        }
    }

    public static void Write(string message)
    {
        var stamp = DateTime.Now.ToString("HH:mm:ss.fff", CultureInfo.InvariantCulture);
        var line = "[" + stamp + "] " + message;

        lock (Gate)
        {
            try
            {
                Console.WriteLine(line);
            }
            catch
            {
                // WinExe has no console.
            }

            try
            {
                _file?.WriteLine(line);
            }
            catch
            {
                // Logging must never take the client down.
            }
        }
    }

    public static void Info(string format, params object[] args)
    {
        Write(string.Format(CultureInfo.InvariantCulture, format, args));
    }
}
