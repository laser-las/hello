#include "overlay.h"

#include <objidl.h>
#include <gdiplus.h>

#include <memory>
#include <unordered_map>

using namespace Gdiplus;

namespace hx {
namespace {

constexpr wchar_t kOverlayClass[] = L"HalcyonOverlay";

std::atomic<bool> g_interactive{false};

LRESULT CALLBACK overlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NCHITTEST:
            return g_interactive.load(std::memory_order_relaxed) ? HTCLIENT : HTTRANSPARENT;
        case WM_MOUSEACTIVATE:
            return g_interactive.load(std::memory_order_relaxed) ? MA_ACTIVATE : MA_NOACTIVATE;
        case WM_ERASEBKGND:
            return 1;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

inline Color argb(uint32_t c) {
    return Color(static_cast<BYTE>((c >> 24) & 0xFF), static_cast<BYTE>((c >> 16) & 0xFF),
                 static_cast<BYTE>((c >> 8) & 0xFF), static_cast<BYTE>(c & 0xFF));
}

void premultiply(uint32_t* pixels, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const uint32_t v = pixels[i];
        const uint32_t a = v >> 24;
        if (a == 0 || a == 255) continue;

        const uint32_t r = ((((v >> 16) & 0xFF) * a) + 127) / 255;
        const uint32_t g = ((((v >> 8) & 0xFF) * a) + 127) / 255;
        const uint32_t b = (((v & 0xFF) * a) + 127) / 255;
        pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

bool containsIgnoreCase(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    auto lower = [](std::wstring s) {
        for (wchar_t& c : s) c = static_cast<wchar_t>(towlower(c));
        return s;
    };
    return lower(haystack).find(lower(needle)) != std::wstring::npos;
}

struct FindContext {
    const std::wstring* title = nullptr;
    const std::wstring* klass = nullptr;
    HWND found = nullptr;
};

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM param) {
    auto* ctx = reinterpret_cast<FindContext*>(param);
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

    wchar_t className[256]{};
    GetClassNameW(hwnd, className, 256);
    if (!ctx->klass->empty() && _wcsicmp(className, ctx->klass->c_str()) != 0) return TRUE;

    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, 512);
    if (!containsIgnoreCase(title, *ctx->title)) return TRUE;

    RECT client{};
    GetClientRect(hwnd, &client);
    if (client.right < 200 || client.bottom < 200) return TRUE;

    ctx->found = hwnd;
    return FALSE;
}

Font* fontFor(float size) {
    static std::unordered_map<int, std::unique_ptr<Font>> cache;
    const int key = static_cast<int>(size * 10.0f);
    auto it = cache.find(key);
    if (it != cache.end() && it->second) return it->second.get();

    static FontFamily familyConsolas(L"Consolas");
    static FontFamily familySegoe(L"Segoe UI");
    FontFamily* family = familyConsolas.IsAvailable() ? &familyConsolas : &familySegoe;

    auto font = std::make_unique<Font>(family, size, FontStyleRegular, UnitPixel);
    Font* raw = font.get();
    cache[key] = std::move(font);
    return raw;
}

}  // namespace

HWND findGameWindow(const std::wstring& windowTitle, const std::wstring& windowClass) {
    FindContext ctx{&windowTitle, &windowClass, nullptr};
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.found;
}

Overlay::~Overlay() { destroy(); }

bool Overlay::create(const std::wstring& windowTitle, const std::wstring& windowClass) {
    windowTitle_ = windowTitle;
    windowClass_ = windowClass;

    GdiplusStartupInput input;
    if (GdiplusStartup(&gdiplusToken_, &input, nullptr) != Ok) {
        logf("overlay: GDI+ startup failed");
        return false;
    }
    gdiplusReady_ = true;

    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = overlayProc;
    wc.hInstance = instance;
    wc.lpszClassName = kOverlayClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        logf("overlay: RegisterClassEx failed (%lu)", GetLastError());
        return false;
    }

    hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
                                WS_EX_NOACTIVATE,
                            kOverlayClass, L"", WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, instance,
                            nullptr);
    if (!hwnd_) {
        logf("overlay: CreateWindowEx failed (%lu)", GetLastError());
        return false;
    }

    memDC_ = CreateCompatibleDC(nullptr);
    if (!memDC_) {
        logf("overlay: CreateCompatibleDC failed");
        return false;
    }

    logf("overlay: window %p ready", reinterpret_cast<void*>(hwnd_));
    return true;
}

void Overlay::destroy() {
    delete gfx_;
    gfx_ = nullptr;
    delete surface_;
    surface_ = nullptr;

    if (dib_) {
        if (memDC_ && oldBitmap_) SelectObject(memDC_, oldBitmap_);
        DeleteObject(dib_);
        dib_ = nullptr;
        oldBitmap_ = nullptr;
    }
    bits_ = nullptr;

    if (memDC_) {
        DeleteDC(memDC_);
        memDC_ = nullptr;
    }

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    if (gdiplusReady_) {
        GdiplusShutdown(gdiplusToken_);
        gdiplusReady_ = false;
    }

    game_ = nullptr;
    width_ = height_ = 0;
    visible_ = false;
}

bool Overlay::ensureSurface(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (surface_ && width == width_ && height == height_) return true;

    delete gfx_;
    gfx_ = nullptr;
    delete surface_;
    surface_ = nullptr;

    if (dib_) {
        SelectObject(memDC_, oldBitmap_);
        DeleteObject(dib_);
        dib_ = nullptr;
        oldBitmap_ = nullptr;
    }
    bits_ = nullptr;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    dib_ = CreateDIBSection(memDC_, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib_ || !bits) {
        logf("overlay: CreateDIBSection failed (%lu)", GetLastError());
        return false;
    }
    oldBitmap_ = SelectObject(memDC_, dib_);
    bits_ = bits;

    surface_ = new Bitmap(width, height, width * 4, PixelFormat32bppARGB,
                          static_cast<BYTE*>(bits_));
    if (surface_->GetLastStatus() != Ok) {
        logf("overlay: GDI+ surface wrap failed");
        return false;
    }

    gfx_ = new Graphics(surface_);
    gfx_->SetSmoothingMode(SmoothingModeAntiAlias);
    gfx_->SetPixelOffsetMode(PixelOffsetModeHalf);
    gfx_->SetCompositingMode(CompositingModeSourceOver);
    gfx_->SetTextRenderingHint(TextRenderingHintAntiAlias);

    width_ = width;
    height_ = height;
    return true;
}

void Overlay::pump() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

bool Overlay::beginFrame() {
    pump();

    if (!hwnd_) return false;

    if (game_ && !IsWindow(game_)) {
        game_ = nullptr;
        visible_ = false;
    }

    if (!game_) {
        game_ = findGameWindow(windowTitle_, windowClass_);
        if (!game_) {
            static bool warned = false;
            if (!warned) {
                logf("overlay: no window matches title=\"%s\" class=\"%s\" yet",
                     narrow(windowTitle_).c_str(), narrow(windowClass_).c_str());
                warned = true;
            }
            if (visible_) {
                ShowWindow(hwnd_, SW_HIDE);
                visible_ = false;
            }
            return false;
        }
        logf("overlay: game window %p found", reinterpret_cast<void*>(game_));
    }

    RECT client{};
    GetClientRect(game_, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return false;

    POINT origin{0, 0};
    ClientToScreen(game_, &origin);

    const bool sizeChanged = width != width_ || height != height_;
    if (!ensureSurface(width, height)) return false;

    if (sizeChanged || origin.x != origin_.x || origin.y != origin_.y) {
        origin_ = origin;
        SetWindowPos(hwnd_, HWND_TOPMOST, origin.x, origin.y, width, height,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    const HWND foreground = GetAncestor(GetForegroundWindow(), GA_ROOT);
    const bool wantsVisible = foreground == game_ || interactive_;
    if (wantsVisible && !visible_) {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd_, HWND_TOPMOST, origin.x, origin.y, width, height,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        visible_ = true;
    } else if (!wantsVisible && visible_) {
        ShowWindow(hwnd_, SW_HIDE);
        visible_ = false;
    }

    if (!visible_) return false;

    std::memset(bits_, 0, static_cast<size_t>(width) * height * 4);
    return true;
}

void Overlay::endFrame() {
    if (!gfx_ || !bits_ || !visible_) return;

    gfx_->Flush(FlushIntentionSync);
    premultiply(static_cast<uint32_t*>(bits_), static_cast<size_t>(width_) * height_);

    HDC screen = GetDC(nullptr);
    if (!screen) return;

    POINT dst{origin_.x, origin_.y};
    POINT src{0, 0};
    SIZE size{width_, height_};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    if (!UpdateLayeredWindow(hwnd_, screen, &dst, &size, memDC_, &src, 0, &blend, ULW_ALPHA)) {
        static bool warned = false;
        if (!warned) {
            logf("overlay: UpdateLayeredWindow failed (%lu)", GetLastError());
            warned = true;
        }
    }

    ReleaseDC(nullptr, screen);
}

void Overlay::line(float x1, float y1, float x2, float y2, uint32_t color, float thickness) {
    if (!gfx_) return;
    Pen pen(argb(color), thickness);
    gfx_->DrawLine(&pen, x1, y1, x2, y2);
}

void Overlay::rect(float x, float y, float w, float h, uint32_t color, float thickness) {
    if (!gfx_) return;
    Pen pen(argb(color), thickness);
    gfx_->DrawRectangle(&pen, x, y, w, h);
}

void Overlay::fillRect(float x, float y, float w, float h, uint32_t color) {
    if (!gfx_) return;
    SolidBrush brush(argb(color));
    gfx_->FillRectangle(&brush, x, y, w, h);
}

void Overlay::circle(float cx, float cy, float radius, uint32_t color, float thickness) {
    if (!gfx_) return;
    Pen pen(argb(color), thickness);
    gfx_->DrawEllipse(&pen, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
}

void Overlay::dot(float cx, float cy, float radius, uint32_t color) {
    if (!gfx_) return;
    SolidBrush brush(argb(color));
    gfx_->FillEllipse(&brush, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
}

void Overlay::text(const std::wstring& value, float x, float y, uint32_t color, float size,
                   bool centered) {
    if (!gfx_ || value.empty()) return;

    Font* font = fontFor(size);
    if (!font) return;

    StringFormat format;
    format.SetAlignment(centered ? StringAlignmentCenter : StringAlignmentNear);
    format.SetLineAlignment(StringAlignmentNear);

    SolidBrush shadow(argb(0xC0000000));
    SolidBrush brush(argb(color));
    const PointF shadowPos(x + 1.0f, y + 1.0f);
    const PointF pos(x, y);
    gfx_->DrawString(value.c_str(), -1, font, shadowPos, &format, &shadow);
    gfx_->DrawString(value.c_str(), -1, font, pos, &format, &brush);
}

void Overlay::setInteractive(bool value) {
    interactive_ = value;
    g_interactive.store(value, std::memory_order_relaxed);
    if (!hwnd_) return;

    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (value) {
        style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    } else {
        style |= static_cast<LONG_PTR>(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, style);
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | (value ? 0 : SWP_NOACTIVATE));

    if (value) focus();
}

void Overlay::focus() {
    if (!hwnd_) return;

    const HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread =
        foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    const DWORD thisThread = GetCurrentThreadId();

    if (foregroundThread && foregroundThread != thisThread) {
        AttachThreadInput(thisThread, foregroundThread, TRUE);
    }
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
    if (foregroundThread && foregroundThread != thisThread) {
        AttachThreadInput(thisThread, foregroundThread, FALSE);
    }
}

void Overlay::refocusGame() {
    if (!game_ || !IsWindow(game_)) return;

    const HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread =
        foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    const DWORD thisThread = GetCurrentThreadId();

    if (foregroundThread && foregroundThread != thisThread) {
        AttachThreadInput(thisThread, foregroundThread, TRUE);
    }
    SetForegroundWindow(game_);
    if (foregroundThread && foregroundThread != thisThread) {
        AttachThreadInput(thisThread, foregroundThread, FALSE);
    }
}

Vec2 Overlay::mousePosition() const {
    POINT point{};
    GetCursorPos(&point);
    if (hwnd_) ScreenToClient(hwnd_, &point);
    return Vec2{static_cast<float>(point.x), static_cast<float>(point.y)};
}

float Overlay::measure(const std::wstring& value, float size) {
    if (!gfx_ || value.empty()) return 0.0f;
    Font* font = fontFor(size);
    if (!font) return 0.0f;
    RectF bounds;
    gfx_->MeasureString(value.c_str(), -1, font, PointF(0.0f, 0.0f), &bounds);
    return bounds.Width;
}

}  // namespace hx
