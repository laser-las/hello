#pragma once
#include "common.h"

namespace Gdiplus {
class Graphics;
class Bitmap;
class Font;
}

namespace hx {

// Borderless, click-through, always-on-top window that is redrawn every frame
// and composited over the game's client area with per-pixel alpha.
class Overlay {
public:
    ~Overlay();

    bool create(const std::wstring& windowTitle, const std::wstring& windowClass);
    void destroy();

    // Locates the game window, resizes the backing store when needed and clears
    // the frame. Returns false while the game window is missing or minimised.
    bool beginFrame();

    // Premultiplies the frame and pushes it to the compositor.
    void endFrame();

    int width() const { return width_; }
    int height() const { return height_; }
    bool visible() const { return visible_; }

    // Makes the window accept mouse input (menu open) or pass everything
    // through to the game (menu closed).
    void setInteractive(bool value);
    bool interactive() const { return interactive_; }
    void focus();
    void refocusGame();

    HWND window() const { return hwnd_; }
    Vec2 mousePosition() const;
    float measure(const std::wstring& value, float size);

    void line(float x1, float y1, float x2, float y2, uint32_t argb, float thickness);
    void rect(float x, float y, float w, float h, uint32_t argb, float thickness);
    void fillRect(float x, float y, float w, float h, uint32_t argb);
    void circle(float cx, float cy, float radius, uint32_t argb, float thickness);
    void dot(float cx, float cy, float radius, uint32_t argb);
    void text(const std::wstring& value, float x, float y, uint32_t argb, float size, bool centered);

private:
    bool ensureSurface(int width, int height);
    void pump();

    HWND hwnd_ = nullptr;
    HWND game_ = nullptr;
    HDC memDC_ = nullptr;
    HBITMAP dib_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    void* bits_ = nullptr;

    Gdiplus::Bitmap* surface_ = nullptr;
    Gdiplus::Graphics* gfx_ = nullptr;

    int width_ = 0;
    int height_ = 0;
    POINT origin_{0, 0};
    bool visible_ = false;
    bool interactive_ = false;
    bool gdiplusReady_ = false;
    ULONG_PTR gdiplusToken_ = 0;
    std::wstring windowTitle_;
    std::wstring windowClass_;
};

// Finds the top level window of the game by class and/or title substring.
HWND findGameWindow(const std::wstring& windowTitle, const std::wstring& windowClass);

}  // namespace hx
