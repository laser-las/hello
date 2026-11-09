#pragma once
#include <atomic>

#include "config.h"
#include "overlay.h"

namespace hx {

// Handles the menu needs from main without exposing its private Shared type.
struct MenuHost {
    Config* cfg = nullptr;
    std::atomic<bool>* esp = nullptr;
    std::atomic<bool>* aim = nullptr;
    std::atomic<bool>* trigger = nullptr;
    std::atomic<bool>* stop = nullptr;
    std::string configPath;
};

// In-game configuration menu drawn with the same GDI+ surface as the ESP.
// Click/drag with the left mouse button, cycle combos with a click, click a
// keybind and press the new key. Toggled by the configured menu key.
class Menu {
public:
    void render(Overlay& overlay, MenuHost& host);

    bool open() const { return open_; }
    void setOpen(bool value) { open_ = value; }
    bool capturing() const { return keyTarget_ != nullptr || textTarget_ != nullptr; }

private:
    bool open_ = false;
    int tab_ = 0;

    int idCounter_ = 0;
    int activeSlider_ = -1;
    int* keyTarget_ = nullptr;
    double captureStart_ = 0.0;
    std::string* textTarget_ = nullptr;
    uint32_t* colorTarget_ = nullptr;
    Vec2 pickerPos_{};
    double pickerOpened_ = 0.0;

    bool mouseWasDown_ = false;
    bool keys_[256]{};
    bool edge_[256]{};
};

}  // namespace hx
