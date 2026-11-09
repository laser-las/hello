using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.Runtime.InteropServices;
using System.Text;

namespace Halcyon;

// Borderless, click-through, always-on-top window that is redrawn every frame
// and composited over the game's client area with per-pixel alpha.
public sealed class Overlay : Form
{
    private const int WsExLayered = 0x00080000;
    private const int WsExTransparent = 0x00000020;
    private const int WsExToolWindow = 0x00000080;
    private const int WsExTopmost = 0x00000008;
    private const int WsExNoActivate = 0x08000000;

    private readonly string _windowTitle;
    private readonly string _windowClass;

    private IntPtr _game;
    private IntPtr _memDc;
    private IntPtr _dib;
    private IntPtr _oldBitmap;
    private IntPtr _bits;

    private Bitmap? _surface;
    private Graphics? _gfx;

    private int _width;
    private int _height;
    private Native.POINT _origin;
    private bool _visible;

    private readonly Dictionary<int, Font> _fonts = new();

    public Overlay(string windowTitle, string windowClass)
    {
        _windowTitle = windowTitle;
        _windowClass = windowClass;

        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.Manual;
        ShowInTaskbar = false;
        TopMost = true;
        BackColor = Color.Black;
        DoubleBuffered = false;
        SetStyle(ControlStyles.Opaque | ControlStyles.UserPaint, true);

        CreateHandle();
        Native.ShowWindow(Handle, Native.SW_HIDE);

        _memDc = Native.CreateCompatibleDC(IntPtr.Zero);
        if (_memDc == IntPtr.Zero) Log.Write("overlay: CreateCompatibleDC failed");
        else Log.Info("overlay: window 0x{0:X} ready", Handle.ToInt64());
    }

    public int WidthPx => _width;
    public int HeightPx => _height;
    public bool VisibleOverlay => _visible;

    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ExStyle |= WsExLayered | WsExTransparent | WsExToolWindow | WsExTopmost | WsExNoActivate;
            return cp;
        }
    }

    protected override void OnPaintBackground(PaintEventArgs e)
    {
        // Composited with UpdateLayeredWindow instead.
    }

    protected override void OnPaint(PaintEventArgs e)
    {
    }

    protected override void WndProc(ref Message m)
    {
        switch (m.Msg)
        {
            case Native.WM_NCHITTEST:
                m.Result = (IntPtr)Native.HTTRANSPARENT;
                return;
            case Native.WM_MOUSEACTIVATE:
                m.Result = (IntPtr)Native.MA_NOACTIVATE;
                return;
            case Native.WM_ERASEBKGND:
                m.Result = (IntPtr)1;
                return;
        }

        base.WndProc(ref m);
    }

    public static IntPtr FindGameWindow(string windowTitle, string windowClass)
    {
        var found = IntPtr.Zero;

        Native.EnumWindows((hwnd, _) =>
        {
            if (!Native.IsWindowVisible(hwnd)) return true;
            if (Native.GetWindow(hwnd, Native.GW_OWNER) != IntPtr.Zero) return true;

            var className = new StringBuilder(256);
            Native.GetClassNameW(hwnd, className, className.Capacity);
            if (windowClass.Length > 0 &&
                !string.Equals(className.ToString(), windowClass, StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }

            var title = new StringBuilder(512);
            Native.GetWindowTextW(hwnd, title, title.Capacity);
            if (windowTitle.Length > 0 &&
                title.ToString().IndexOf(windowTitle, StringComparison.OrdinalIgnoreCase) < 0)
            {
                return true;
            }

            Native.GetClientRect(hwnd, out var client);
            if (client.Right < 200 || client.Bottom < 200) return true;

            found = hwnd;
            return false;
        }, IntPtr.Zero);

        return found;
    }

    // Locates the game window, resizes the backing store when needed and clears
    // the frame. Returns false while the game window is missing or minimised.
    public bool BeginFrame()
    {
        if (Handle == IntPtr.Zero) return false;

        if (_game != IntPtr.Zero && !Native.IsWindow(_game))
        {
            _game = IntPtr.Zero;
            Native.ShowWindow(Handle, Native.SW_HIDE);
            _visible = false;
        }

        if (_game == IntPtr.Zero)
        {
            _game = FindGameWindow(_windowTitle, _windowClass);
            if (_game == IntPtr.Zero)
            {
                if (_visible)
                {
                    Native.ShowWindow(Handle, Native.SW_HIDE);
                    _visible = false;
                }

                return false;
            }

            Log.Info("overlay: game window 0x{0:X} found", _game.ToInt64());
        }

        Native.GetClientRect(_game, out var client);
        var width = client.Right - client.Left;
        var height = client.Bottom - client.Top;
        if (width <= 0 || height <= 0) return false;

        var origin = new Native.POINT { X = 0, Y = 0 };
        Native.ClientToScreen(_game, ref origin);

        var sizeChanged = width != _width || height != _height;
        if (!EnsureSurface(width, height)) return false;

        if (sizeChanged || origin.X != _origin.X || origin.Y != _origin.Y)
        {
            _origin = origin;
            Native.SetWindowPos(Handle, Native.HWND_TOPMOST, origin.X, origin.Y, width, height,
                Native.SWP_NOACTIVATE);
        }

        var foreground = Native.GetAncestor(Native.GetForegroundWindow(), Native.GA_ROOT);
        var wantsVisible = foreground == _game && !Native.IsIconic(_game);

        if (wantsVisible && !_visible)
        {
            Native.ShowWindow(Handle, Native.SW_SHOWNOACTIVATE);
            Native.SetWindowPos(Handle, Native.HWND_TOPMOST, origin.X, origin.Y, width, height,
                Native.SWP_NOACTIVATE | Native.SWP_SHOWWINDOW);
            _visible = true;
        }
        else if (!wantsVisible && _visible)
        {
            Native.ShowWindow(Handle, Native.SW_HIDE);
            _visible = false;
        }

        if (!_visible) return false;

        unsafe
        {
            new Span<byte>((void*)_bits, width * height * 4).Clear();
        }

        return true;
    }

    private bool EnsureSurface(int width, int height)
    {
        if (width <= 0 || height <= 0) return false;
        if (_surface != null && width == _width && height == _height) return true;

        _gfx?.Dispose();
        _gfx = null;
        _surface?.Dispose();
        _surface = null;

        if (_dib != IntPtr.Zero)
        {
            if (_memDc != IntPtr.Zero && _oldBitmap != IntPtr.Zero) Native.SelectObject(_memDc, _oldBitmap);
            Native.DeleteObject(_dib);
            _dib = IntPtr.Zero;
            _oldBitmap = IntPtr.Zero;
        }

        _bits = IntPtr.Zero;

        var bmi = new Native.BITMAPINFO
        {
            bmiHeader = new Native.BITMAPINFOHEADER
            {
                biSize = (uint)Marshal.SizeOf<Native.BITMAPINFOHEADER>(),
                biWidth = width,
                biHeight = -height, // top-down
                biPlanes = 1,
                biBitCount = 32,
                biCompression = 0,  // BI_RGB
            },
        };

        _dib = Native.CreateDIBSection(_memDc, ref bmi, Native.DIB_RGB_COLORS, out _bits, IntPtr.Zero, 0);
        if (_dib == IntPtr.Zero || _bits == IntPtr.Zero)
        {
            Log.Write("overlay: CreateDIBSection failed");
            return false;
        }

        _oldBitmap = Native.SelectObject(_memDc, _dib);

        _surface = new Bitmap(width, height, width * 4, PixelFormat.Format32bppArgb, _bits);
        _gfx = Graphics.FromImage(_surface);
        _gfx.SmoothingMode = SmoothingMode.AntiAlias;
        _gfx.PixelOffsetMode = PixelOffsetMode.Half;
        _gfx.CompositingMode = CompositingMode.SourceOver;
        _gfx.TextRenderingHint = TextRenderingHint.AntiAlias;

        _width = width;
        _height = height;
        return true;
    }

    // Premultiplies the frame and pushes it to the compositor.
    public unsafe void EndFrame()
    {
        if (_gfx == null || _bits == IntPtr.Zero || !_visible) return;

        _gfx.Flush(FlushIntention.Sync);
        Premultiply((uint*)_bits, _width * _height);

        var screen = Native.GetDC(IntPtr.Zero);
        if (screen == IntPtr.Zero) return;

        var destination = new Native.POINT { X = _origin.X, Y = _origin.Y };
        var source = new Native.POINT { X = 0, Y = 0 };
        var size = new Native.SIZE { CX = _width, CY = _height };
        var blend = new Native.BLENDFUNCTION
        {
            BlendOp = Native.AC_SRC_OVER,
            BlendFlags = 0,
            SourceConstantAlpha = 255,
            AlphaFormat = Native.AC_SRC_ALPHA,
        };

        if (!Native.UpdateLayeredWindow(Handle, screen, ref destination, ref size, _memDc, ref source, 0,
                ref blend, Native.ULW_ALPHA))
        {
            Log.Write("overlay: UpdateLayeredWindow failed");
        }

        Native.ReleaseDC(IntPtr.Zero, screen);
    }

    private static unsafe void Premultiply(uint* pixels, int count)
    {
        for (var i = 0; i < count; ++i)
        {
            var v = pixels[i];
            var a = v >> 24;
            if (a == 0 || a == 255) continue;

            var r = ((((v >> 16) & 0xFF) * a) + 127) / 255;
            var g = ((((v >> 8) & 0xFF) * a) + 127) / 255;
            var b = (((v & 0xFF) * a) + 127) / 255;

            pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    private static Color Argb(uint color)
    {
        return Color.FromArgb((byte)((color >> 24) & 0xFF), (byte)((color >> 16) & 0xFF),
            (byte)((color >> 8) & 0xFF), (byte)(color & 0xFF));
    }

    private Font FontFor(float size)
    {
        var key = (int)(size * 10.0f);
        if (_fonts.TryGetValue(key, out var cached)) return cached;

        Font font;
        try
        {
            font = new Font("Consolas", size, FontStyle.Regular, GraphicsUnit.Pixel);
        }
        catch
        {
            font = new Font(FontFamily.GenericSansSerif, size, FontStyle.Regular, GraphicsUnit.Pixel);
        }

        _fonts[key] = font;
        return font;
    }

    public void Line(float x1, float y1, float x2, float y2, uint color, float thickness)
    {
        if (_gfx == null) return;
        using var pen = new Pen(Argb(color), thickness);
        _gfx.DrawLine(pen, x1, y1, x2, y2);
    }

    public void Rect(float x, float y, float w, float h, uint color, float thickness)
    {
        if (_gfx == null) return;
        using var pen = new Pen(Argb(color), thickness);
        _gfx.DrawRectangle(pen, x, y, w, h);
    }

    public void FillRect(float x, float y, float w, float h, uint color)
    {
        if (_gfx == null) return;
        using var brush = new SolidBrush(Argb(color));
        _gfx.FillRectangle(brush, x, y, w, h);
    }

    public void Circle(float cx, float cy, float radius, uint color, float thickness)
    {
        if (_gfx == null) return;
        using var pen = new Pen(Argb(color), thickness);
        _gfx.DrawEllipse(pen, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    }

    public void Dot(float cx, float cy, float radius, uint color)
    {
        if (_gfx == null) return;
        using var brush = new SolidBrush(Argb(color));
        _gfx.FillEllipse(brush, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    }

    public new void Text(string value, float x, float y, uint color, float size, bool centered)
    {
        if (_gfx == null || string.IsNullOrEmpty(value)) return;

        var font = FontFor(size);
        using var format = new StringFormat
        {
            Alignment = centered ? StringAlignment.Center : StringAlignment.Near,
            LineAlignment = StringAlignment.Near,
        };
        using var shadow = new SolidBrush(Argb(0xC0000000));
        using var brush = new SolidBrush(Argb(color));

        _gfx.DrawString(value, font, shadow, x + 1.0f, y + 1.0f, format);
        _gfx.DrawString(value, font, brush, x, y, format);
    }

    public float Measure(string value, float size)
    {
        if (_gfx == null || string.IsNullOrEmpty(value)) return 0.0f;
        var font = FontFor(size);
        return _gfx.MeasureString(value, font).Width;
    }

    public void RefocusGame()
    {
        if (_game == IntPtr.Zero || !Native.IsWindow(_game)) return;

        var foreground = Native.GetForegroundWindow();
        var foregroundThread = foreground != IntPtr.Zero
            ? Native.GetWindowThreadProcessId(foreground, out _)
            : 0;
        var thisThread = Native.GetCurrentThreadId();

        if (foregroundThread != 0 && foregroundThread != thisThread)
            Native.AttachThreadInput(thisThread, foregroundThread, true);

        Native.SetForegroundWindow(_game);

        if (foregroundThread != 0 && foregroundThread != thisThread)
            Native.AttachThreadInput(thisThread, foregroundThread, false);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _gfx?.Dispose();
            _gfx = null;
            _surface?.Dispose();
            _surface = null;

            foreach (var font in _fonts.Values) font.Dispose();
            _fonts.Clear();

            if (_dib != IntPtr.Zero)
            {
                if (_memDc != IntPtr.Zero && _oldBitmap != IntPtr.Zero) Native.SelectObject(_memDc, _oldBitmap);
                Native.DeleteObject(_dib);
                _dib = IntPtr.Zero;
                _oldBitmap = IntPtr.Zero;
            }

            _bits = IntPtr.Zero;

            if (_memDc != IntPtr.Zero)
            {
                Native.DeleteDC(_memDc);
                _memDc = IntPtr.Zero;
            }
        }

        base.Dispose(disposing);
    }
}
