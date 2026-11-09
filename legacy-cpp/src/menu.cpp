#include "menu.h"

#include <cwchar>
#include <initializer_list>

namespace hx {
namespace {

constexpr float kRowHeight = 23.0f;
constexpr float kRowGap = 14.0f;
constexpr float kHeaderHeight = 46.0f;
constexpr float kSidebarWidth = 178.0f;
constexpr float kFooterHeight = 28.0f;

uint32_t withAlpha(uint32_t argb, float alpha) {
    const uint32_t a = static_cast<uint32_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    return (argb & 0x00FFFFFFu) | (a << 24);
}

uint32_t opaque(uint32_t argb) { return (argb & 0x00FFFFFFu) | 0xFF000000u; }

struct Ctx {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    int id = 0;
    Vec2 mouse{};
    bool down = false;
    bool clicked = false;
};

struct Column {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    int id = 0;
};

std::string clipboardText() {
    std::string out;
    if (!OpenClipboard(nullptr)) return out;
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle) {
        const auto* text = static_cast<const wchar_t*>(GlobalLock(handle));
        if (text) {
            out = narrow(std::wstring(text));
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    return out;
}

void popUtf8(std::string& text) {
    if (text.empty()) return;
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) text.pop_back();
    if (!text.empty()) text.pop_back();
}

}  // namespace

void Menu::render(Overlay& overlay, MenuHost& host) {
    if (!open_ || !host.cfg) return;

    Config& cfg = *host.cfg;

    const float screenW = static_cast<float>(overlay.width());
    const float screenH = static_cast<float>(overlay.height());
    if (screenW < 400.0f || screenH < 300.0f) return;

    const Vec2 mouse = overlay.mousePosition();
    const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool clicked = down && !mouseWasDown_;
    const bool released = !down && mouseWasDown_;
    (void)released;

    for (int vk = 1; vk < 256; ++vk) {
        const bool now = (GetAsyncKeyState(vk) & 0x8000) != 0;
        edge_[vk] = now && !keys_[vk];
        keys_[vk] = now;
    }

    const double now = static_cast<double>(GetTickCount64());

    const bool wasCapturing = keyTarget_ != nullptr;
    if (keyTarget_) {
        for (int vk = 1; vk < 256; ++vk) {
            if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
            if (!edge_[vk]) continue;
            if (vk == VK_ESCAPE) {
                keyTarget_ = nullptr;
            } else {
                *keyTarget_ = vk;
                keyTarget_ = nullptr;
            }
            break;
        }
    }
    const bool inputBlocked = wasCapturing;

    if (textTarget_) {
        std::string& text = *textTarget_;
        if (edge_[VK_ESCAPE] || edge_[VK_RETURN] || edge_[VK_NUMPADENTER]) {
            textTarget_ = nullptr;
        } else {
            if (edge_[VK_BACK]) popUtf8(text);
            const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrl && edge_['V']) text += clipboardText();

            for (int vk = 0x20; vk < 256; ++vk) {
                if (!edge_[vk]) continue;
                if (vk == VK_BACK || vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_NUMPADENTER) continue;
                if (vk >= VK_SHIFT && vk <= VK_MENU) continue;

                BYTE state[256]{};
                GetKeyboardState(state);
                wchar_t buffer[8]{};
                const int count = ToUnicode(vk, MapVirtualKeyW(vk, MAPVK_VK_TO_VSC), state, buffer, 8, 0);
                if (count > 0 && buffer[0] >= 0x20) {
                    text += narrow(std::wstring(buffer, static_cast<size_t>(count)));
                }
            }
        }
    }

    if (clicked && textTarget_) textTarget_ = nullptr;

    const uint32_t accent = opaque(cfg.visuals.accent);
    const uint32_t bg = 0xF21A1A22u;
    const uint32_t headerColor = 0xF5101016u;
    const uint32_t sidebarColor = 0xF0181820u;
    const uint32_t panelColor = 0xF222222Cu;
    const uint32_t textColor = 0xFFE8E8F0u;
    const uint32_t dimColor = 0xFF9A9AA8u;
    const uint32_t disabledColor = 0xFF646472u;
    const uint32_t lineColor = 0xFF2E2E3Au;
    const uint32_t boxColor = 0xFF1C1C26u;

    const float panelW = std::min(1080.0f, screenW - 48.0f);
    const float panelH = std::min(720.0f, screenH - 48.0f);
    const float panelX = (screenW - panelW) * 0.5f;
    const float panelY = (screenH - panelH) * 0.5f;
    const float contentX = panelX + kSidebarWidth + 16.0f;
    const float contentY = panelY + kHeaderHeight + 14.0f;
    const float contentW = panelW - kSidebarWidth - 32.0f;

    overlay.fillRect(0.0f, 0.0f, screenW, screenH, 0x66000000u);
    overlay.fillRect(panelX + 8.0f, panelY + 8.0f, panelW, panelH, 0x50000000u);
    overlay.fillRect(panelX, panelY, panelW, panelH, bg);
    overlay.rect(panelX, panelY, panelW, panelH, lineColor, 1.0f);

    overlay.fillRect(panelX, panelY, panelW, kHeaderHeight, headerColor);
    overlay.fillRect(panelX, panelY + kHeaderHeight - 1.0f, panelW, 1.0f, withAlpha(accent, 0.55f));
    overlay.text(L"HALCYON", panelX + 18.0f, panelY + 11.0f, accent, 21.0f, false);
    overlay.text(L"external client", panelX + 118.0f, panelY + 18.0f, dimColor, 11.0f, false);

    const std::wstring presetLabel = widen(presetName(cfg.preset));
    const float presetWidth = overlay.measure(presetLabel, 13.0f);
    overlay.text(presetLabel, panelX + panelW - presetWidth - 18.0f, panelY + 16.0f, textColor,
                 13.0f, false);

    overlay.fillRect(panelX, panelY + kHeaderHeight, kSidebarWidth,
                     panelH - kHeaderHeight - kFooterHeight, sidebarColor);
    overlay.fillRect(panelX + kSidebarWidth, panelY + kHeaderHeight, 1.0f,
                     panelH - kHeaderHeight - kFooterHeight, lineColor);

    const wchar_t* tabs[] = {L"Aimbot", L"Trigger", L"ESP", L"Visuals", L"Radar", L"Misc", L"Config"};
    const int tabCount = static_cast<int>(sizeof(tabs) / sizeof(tabs[0]));

    auto hit = [&](float x, float y, float w, float h) {
        return mouse.x >= x && mouse.x <= x + w && mouse.y >= y && mouse.y <= y + h;
    };

    for (int i = 0; i < tabCount; ++i) {
        const float ty = panelY + kHeaderHeight + 14.0f + static_cast<float>(i) * 34.0f;
        const bool selected = i == tab_;
        const bool over = hit(panelX + 8.0f, ty - 4.0f, kSidebarWidth - 16.0f, 30.0f);

        if (selected) {
            overlay.fillRect(panelX + 8.0f, ty - 4.0f, kSidebarWidth - 16.0f, 30.0f,
                             withAlpha(accent, 0.20f));
            overlay.fillRect(panelX + 8.0f, ty - 4.0f, 3.0f, 30.0f, accent);
        } else if (over) {
            overlay.fillRect(panelX + 8.0f, ty - 4.0f, kSidebarWidth - 16.0f, 30.0f, 0x18FFFFFFu);
        }

        overlay.text(tabs[i], panelX + 24.0f, ty, selected ? accent : textColor, 14.0f, false);
        if (over && clicked && !inputBlocked) tab_ = i;
    }

    const float footerY = panelY + panelH - kFooterHeight;
    overlay.fillRect(panelX, footerY, panelW, kFooterHeight, headerColor);
    overlay.fillRect(panelX, footerY, panelW, 1.0f, lineColor);
    overlay.text(L"changes apply live", panelX + 18.0f, footerY + 8.0f, dimColor, 11.0f, false);
    const wchar_t* footer = L"INSERT close    DEL panic    END quit";
    const float footerWidth = overlay.measure(footer, 11.0f);
    overlay.text(footer, panelX + panelW - footerWidth - 18.0f, footerY + 8.0f, dimColor, 11.0f,
                 false);

    if (tab_ < 0 || tab_ >= tabCount) tab_ = 0;

    std::vector<Column> columns;

    auto beginColumns = [&](int count) {
        columns.clear();
        const float gap = 16.0f;
        const float width = (contentW - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
        for (int i = 0; i < count; ++i) {
            Column column;
            column.x = contentX + static_cast<float>(i) * (width + gap);
            column.y = contentY;
            column.w = width;
            column.id = i * 2000;
            columns.push_back(column);
        }
    };

    auto makeCtx = [&](int index) {
        Ctx c;
        c.x = columns[index].x;
        c.y = columns[index].y;
        c.w = columns[index].w;
        c.id = columns[index].id;
        c.mouse = mouse;
        c.down = down;
        c.clicked = clicked;
        return c;
    };

    auto storeCtx = [&](int index, const Ctx& c) {
        columns[index].y = c.y;
        columns[index].id = c.id;
    };

    auto section = [&](Ctx& c, const wchar_t* title) {
        overlay.text(title, c.x, c.y + 3.0f, accent, 12.0f, false);
        overlay.fillRect(c.x, c.y + 19.0f, c.w, 1.0f, withAlpha(accent, 0.28f));
        c.y += 27.0f;
    };

    auto checkbox = [&](Ctx& c, const wchar_t* label, bool& value, bool enabled = true) {
        const float y = c.y;
        const bool over = enabled && hit(c.x, y - 1.0f, c.w, kRowHeight);
        if (over) overlay.fillRect(c.x - 5.0f, y - 1.0f, c.w + 10.0f, kRowHeight, 0x14FFFFFFu);

        const float size = 13.0f;
        const float bx = c.x + c.w - size;
        const float by = y + 4.0f;

        if (value) {
            overlay.fillRect(bx, by, size, size, enabled ? accent : 0xFF505060u);
            overlay.line(bx + 3.0f, by + 7.0f, bx + 5.5f, by + 9.5f, 0xFF14141Cu, 1.6f);
            overlay.line(bx + 5.5f, by + 9.5f, bx + 10.0f, by + 4.0f, 0xFF14141Cu, 1.6f);
        } else {
            overlay.fillRect(bx, by, size, size, 0xFF181820u);
            overlay.rect(bx, by, size, size, enabled ? 0xFF50505Eu : 0xFF3A3A46u, 1.0f);
        }

        overlay.text(label, c.x, y, enabled ? textColor : disabledColor, 13.0f, false);
        if (over && c.clicked && !inputBlocked) value = !value;

        c.y += kRowHeight;
        ++c.id;
    };

    auto combo = [&](Ctx& c, const wchar_t* label, int& value,
                     std::initializer_list<const wchar_t*> items, bool enabled = true) {
        const float y = c.y;
        const int count = static_cast<int>(items.size());
        if (count > 0) {
            if (value < 0) value = 0;
            if (value >= count) value = count - 1;
        }

        const float width = std::min(158.0f, c.w * 0.56f);
        const float bx = c.x + c.w - width;
        const bool over = enabled && hit(bx, y - 1.0f, width, kRowHeight);

        overlay.text(label, c.x, y, enabled ? textColor : disabledColor, 13.0f, false);
        overlay.fillRect(bx, y + 2.0f, width, kRowHeight - 6.0f, boxColor);
        overlay.rect(bx, y + 2.0f, width, kRowHeight - 6.0f,
                     over ? withAlpha(accent, 0.8f) : 0xFF3A3A48u, 1.0f);

        if (count > 0) {
            const wchar_t* current = *(items.begin() + value);
            const float textWidth = overlay.measure(current, 12.0f);
            overlay.text(current, bx + width - textWidth - 8.0f, y + 3.0f,
                         enabled ? textColor : disabledColor, 12.0f, false);
        }

        if (over && c.clicked && !inputBlocked && count > 0) value = (value + 1) % count;

        c.y += kRowHeight;
        ++c.id;
    };

    auto sliderImpl = [&](Ctx& c, const wchar_t* label, float& value, float lo, float hi,
                          float step, bool enabled, int decimals) -> bool {
        const float y = c.y;
        const float labelW = c.w * 0.42f;
        const float trackX = c.x + labelW;
        const float trackW = c.w - labelW - 6.0f;
        const float trackY = y + 18.0f;

        const bool over = enabled && hit(c.x, y - 1.0f, c.w, kRowHeight + 4.0f);
        if (over && c.clicked && !inputBlocked) activeSlider_ = c.id;

        if (activeSlider_ == c.id) {
            if (c.down && enabled) {
                const float t = std::clamp((c.mouse.x - trackX) / std::max(1.0f, trackW), 0.0f, 1.0f);
                float next = lo + t * (hi - lo);
                if (step > 0.0f) next = std::round(next / step) * step;
                value = std::clamp(next, lo, hi);
            } else {
                activeSlider_ = -1;
            }
        }

        value = std::clamp(value, lo, hi);

        overlay.text(label, c.x, y, enabled ? textColor : disabledColor, 13.0f, false);

        wchar_t buffer[64]{};
        std::swprintf(buffer, 64, decimals > 0 ? L"%.2f" : L"%.0f", static_cast<double>(value));
        const std::wstring valueText(buffer);
        const float valueWidth = overlay.measure(valueText, 12.0f);
        overlay.text(valueText, c.x + c.w - valueWidth, y + 1.0f,
                     enabled ? accent : disabledColor, 12.0f, false);

        overlay.fillRect(trackX, trackY, trackW, 3.0f, 0xFF30303Cu);
        const float t = (hi > lo) ? (value - lo) / (hi - lo) : 0.0f;
        overlay.fillRect(trackX, trackY, trackW * t, 3.0f, enabled ? accent : 0xFF505060u);
        overlay.dot(trackX + trackW * t, trackY + 1.5f, 4.0f, enabled ? accent : 0xFF505060u);

        c.y += kRowHeight + 4.0f;
        ++c.id;
        return over;
    };

    auto sliderInt = [&](Ctx& c, const wchar_t* label, int& value, int lo, int hi, int step,
                         bool enabled) -> bool {
        float asFloat = static_cast<float>(value);
        const bool result =
            sliderImpl(c, label, asFloat, static_cast<float>(lo), static_cast<float>(hi),
                       static_cast<float>(step), enabled, 0);
        value = static_cast<int>(std::lround(asFloat));
        return result;
    };

    auto keybind = [&](Ctx& c, const wchar_t* label, int& key) {
        const float y = c.y;
        const float width = 116.0f;
        const float bx = c.x + c.w - width;
        const bool over = hit(bx, y - 1.0f, width, kRowHeight);
        const bool capturing = keyTarget_ == &key;

        overlay.text(label, c.x, y, textColor, 13.0f, false);
        overlay.fillRect(bx, y + 2.0f, width, kRowHeight - 6.0f, boxColor);
        overlay.rect(bx, y + 2.0f, width, kRowHeight - 6.0f,
                     capturing ? accent : (over ? withAlpha(accent, 0.8f) : 0xFF3A3A48u), 1.0f);

        const std::wstring keyName = capturing ? L"press a key" : widen(vkToName(key));
        const float textWidth = overlay.measure(keyName, 12.0f);
        overlay.text(keyName, bx + width - textWidth - 8.0f, y + 3.0f,
                     capturing ? accent : textColor, 12.0f, false);

        if (over && c.clicked && !inputBlocked) {
            keyTarget_ = &key;
            captureStart_ = now;
        }

        c.y += kRowHeight;
        ++c.id;
    };

    auto colorRow = [&](Ctx& c, const wchar_t* label, uint32_t& color) {
        const float y = c.y;
        const float width = 46.0f;
        const float bx = c.x + c.w - width;
        const bool over = hit(bx, y - 1.0f, width, kRowHeight);

        overlay.text(label, c.x, y, textColor, 13.0f, false);
        overlay.fillRect(bx, y + 3.0f, width, kRowHeight - 9.0f, color);
        overlay.rect(bx, y + 3.0f, width, kRowHeight - 9.0f,
                     over ? withAlpha(accent, 0.9f) : 0xFF50505Eu, 1.0f);

        if (over && c.clicked && !inputBlocked) {
            colorTarget_ = &color;
            pickerOpened_ = now;
            pickerPos_ = Vec2{bx - 174.0f, y + 26.0f};
        }

        c.y += kRowHeight;
        ++c.id;
    };

    auto textRow = [&](Ctx& c, const wchar_t* label, std::string& text) {
        const float y = c.y;
        const float width = std::min(210.0f, c.w * 0.56f);
        const float bx = c.x + c.w - width;
        const bool over = hit(bx, y - 1.0f, width, kRowHeight);
        const bool editing = textTarget_ == &text;

        overlay.text(label, c.x, y, textColor, 13.0f, false);
        overlay.fillRect(bx, y + 2.0f, width, kRowHeight - 6.0f, boxColor);
        overlay.rect(bx, y + 2.0f, width, kRowHeight - 6.0f,
                     editing ? accent : (over ? withAlpha(accent, 0.8f) : 0xFF3A3A48u), 1.0f);

        std::wstring value = widen(text);
        if (editing) value += L"|";
        while (!value.empty() && overlay.measure(value, 12.0f) > width - 14.0f) value.pop_back();
        overlay.text(value, bx + 7.0f, y + 3.0f, textColor, 12.0f, false);

        if (over && c.clicked && !inputBlocked) {
            textTarget_ = &text;
            captureStart_ = now;
        }

        c.y += kRowHeight;
        ++c.id;
    };

    auto button = [&](Ctx& c, const wchar_t* label, uint32_t fill) {
        const float y = c.y;
        const float h = kRowHeight - 2.0f;
        const bool over = hit(c.x, y, c.w, h);

        overlay.fillRect(c.x, y, c.w, h, withAlpha(fill, over ? 0.85f : 0.55f));
        overlay.rect(c.x, y, c.w, h, withAlpha(fill, 0.95f), 1.0f);

        const float textWidth = overlay.measure(label, 12.0f);
        overlay.text(label, c.x + (c.w - textWidth) * 0.5f, y + 3.0f, 0xFFF2F2F8u, 12.0f, false);

        c.y += kRowHeight + 2.0f;
        ++c.id;
        return over && c.clicked && !inputBlocked;
    };

    auto note = [&](Ctx& c, const wchar_t* text) {
        overlay.text(text, c.x, c.y + 2.0f, dimColor, 11.0f, false);
        c.y += 18.0f;
    };

    auto syncRuntime = [&]() {
        if (host.esp) *host.esp = cfg.esp.enabled;
        if (host.aim) *host.aim = cfg.aim.enabled;
        if (host.trigger) *host.trigger = cfg.trigger.enabled;
    };

    switch (tab_) {
        case 0: {  // Aimbot
            beginColumns(3);

            Ctx a = makeCtx(0);
            section(a, L"TARGET");
            {
                const bool before = cfg.aim.enabled;
                checkbox(a, L"Enabled", cfg.aim.enabled);
                if (before != cfg.aim.enabled && host.aim) *host.aim = cfg.aim.enabled;
            }
            keybind(a, L"Aim key", cfg.aim.key);
            checkbox(a, L"Toggle mode", cfg.aim.toggle);
            combo(a, L"Hitbox", cfg.aim.hitbox, {L"Head", L"Neck", L"Chest", L"Stomach"});
            checkbox(a, L"Multi hitbox", cfg.aim.multiHitbox);
            checkbox(a, L"  Head", cfg.aim.hitHead, cfg.aim.multiHitbox);
            checkbox(a, L"  Neck", cfg.aim.hitNeck, cfg.aim.multiHitbox);
            checkbox(a, L"  Chest", cfg.aim.hitChest, cfg.aim.multiHitbox);
            checkbox(a, L"  Stomach", cfg.aim.hitStomach, cfg.aim.multiHitbox);
            combo(a, L"Priority", cfg.aim.priority,
                  {L"Crosshair", L"Distance", L"FOV", L"Health"});
            checkbox(a, L"Switch targets", cfg.aim.targetSwitch);
            sliderImpl(a, L"Switch delay", cfg.aim.switchDelay, 0.0f, 0.5f, 0.01f,
                       cfg.aim.targetSwitch, 2);
            sliderImpl(a, L"Max distance", cfg.aim.maxDistance, 0.0f, 4000.0f, 50.0f, true, 0);
            checkbox(a, L"Team check", cfg.aim.teamCheck);
            checkbox(a, L"Visible only", cfg.aim.visibleOnly);
            checkbox(a, L"Ignore flashed", cfg.aim.ignoreFlashed);
            checkbox(a, L"Only when scoped", cfg.aim.onlyScoped);
            {
                bool reserved = cfg.aim.ignoreSmoke;
                checkbox(a, L"Ignore smoke (ext.)", reserved, false);
                reserved = cfg.aim.silent;
                checkbox(a, L"Silent aim (ext.)", reserved, false);
            }
            storeCtx(0, a);

            Ctx b = makeCtx(1);
            section(b, L"FIELD OF VIEW");
            sliderImpl(b, L"FOV", cfg.aim.fov, 5.0f, 720.0f, 1.0f, true, 0);
            checkbox(b, L"Dynamic FOV", cfg.aim.dynamicFov);
            sliderImpl(b, L"  Minimum", cfg.aim.fovMin, 0.0f, 720.0f, 1.0f, cfg.aim.dynamicFov, 0);
            sliderImpl(b, L"  Maximum", cfg.aim.fovMax, 0.0f, 900.0f, 1.0f, cfg.aim.dynamicFov, 0);
            sliderImpl(b, L"  Scale", cfg.aim.fovScale, 0.0f, 3.0f, 0.05f, cfg.aim.dynamicFov, 2);
            section(b, L"MOTION");
            sliderImpl(b, L"Smooth", cfg.aim.smooth, 1.0f, 30.0f, 0.5f, true, 1);
            combo(b, L"Smooth mode", cfg.aim.smoothMode, {L"Linear", L"Exponential", L"Humanized"});
            sliderImpl(b, L"Humanize", cfg.aim.humanize, 0.0f, 1.0f, 0.01f, true, 2);
            sliderImpl(b, L"Humanize jitter", cfg.aim.humanizeJitter, 0.0f, 1.0f, 0.01f, true, 2);
            sliderImpl(b, L"Max step (px)", cfg.aim.aimStep, 0.0f, 100.0f, 0.5f, true, 1);
            sliderImpl(b, L"Mouse scale", cfg.aim.aimScale, 0.1f, 3.0f, 0.05f, true, 2);
            sliderImpl(b, L"Reaction time", cfg.aim.reactionTime, 0.0f, 0.5f, 0.01f, true, 2);
            sliderImpl(b, L"Reaction jitter", cfg.aim.reactionJitter, 0.0f, 0.2f, 0.01f, true, 2);
            sliderImpl(b, L"First shot delay", cfg.aim.firstShotDelay, 0.0f, 0.5f, 0.01f, true, 2);
            section(b, L"FOV RING");
            checkbox(b, L"Draw FOV", cfg.aim.drawFov);
            colorRow(b, L"FOV color", cfg.aim.fovColor);
            sliderImpl(b, L"FOV alpha", cfg.aim.fovAlpha, 0.0f, 1.0f, 0.01f, cfg.aim.drawFov, 2);
            storeCtx(1, b);

            Ctx d = makeCtx(2);
            section(d, L"FIRE");
            checkbox(d, L"Auto fire", cfg.aim.autoFire);
            checkbox(d, L"Auto scope", cfg.aim.autoScope);
            checkbox(d, L"Auto pistol", cfg.aim.autoPistol);
            checkbox(d, L"Auto stop", cfg.aim.autoStop);
            checkbox(d, L"Auto crouch", cfg.aim.autoCrouch);
            section(d, L"RECOIL");
            checkbox(d, L"RCS", cfg.aim.rcs);
            checkbox(d, L"RCS standalone", cfg.aim.rcsStandalone);
            sliderImpl(d, L"RCS X", cfg.aim.rcsX, 0.0f, 2.0f, 0.01f, cfg.aim.rcs, 2);
            sliderImpl(d, L"RCS Y", cfg.aim.rcsY, 0.0f, 2.0f, 0.01f, cfg.aim.rcs, 2);
            sliderImpl(d, L"RCS smooth", cfg.aim.rcsSmooth, 1.0f, 12.0f, 0.5f, cfg.aim.rcs, 1);
            checkbox(d, L"RCS scoped only", cfg.aim.rcsScopeOnly, cfg.aim.rcs);
            section(d, L"PREDICTION");
            checkbox(d, L"Velocity prediction", cfg.aim.prediction);
            sliderImpl(d, L"Predict ms", cfg.aim.predictMs, 0.0f, 200.0f, 1.0f, cfg.aim.prediction, 0);
            checkbox(d, L"Backtrack (approx)", cfg.aim.backtrack);
            sliderImpl(d, L"Backtrack ms", cfg.aim.backtrackMs, 0.0f, 250.0f, 5.0f, cfg.aim.backtrack, 0);
            combo(d, L"Resolver", cfg.aim.resolverMode, {L"Off", L"Static", L"Jitter"});
            storeCtx(2, d);
            break;
        }

        case 1: {  // Trigger
            beginColumns(2);

            Ctx a = makeCtx(0);
            section(a, L"TRIGGER");
            {
                const bool before = cfg.trigger.enabled;
                checkbox(a, L"Enabled", cfg.trigger.enabled);
                if (before != cfg.trigger.enabled && host.trigger) *host.trigger = cfg.trigger.enabled;
            }
            keybind(a, L"Trigger key", cfg.trigger.key);
            checkbox(a, L"Toggle mode", cfg.trigger.toggle);
            sliderImpl(a, L"Radius", cfg.trigger.radius, 1.0f, 60.0f, 1.0f, true, 0);
            sliderImpl(a, L"Max distance", cfg.trigger.maxDistance, 0.0f, 4000.0f, 50.0f, true, 0);
            checkbox(a, L"Team check", cfg.trigger.teamCheck);
            checkbox(a, L"Visible only", cfg.trigger.visibleOnly);
            checkbox(a, L"Wall check", cfg.trigger.wallCheck);
            checkbox(a, L"Only when scoped", cfg.trigger.onlyScoped);
            note(a, L"Wall check needs m_bspotted in config.ini.");
            storeCtx(0, a);

            Ctx b = makeCtx(1);
            section(b, L"TIMING");
            checkbox(b, L"Randomize", cfg.trigger.randomize);
            sliderInt(b, L"Delay min (ms)", cfg.trigger.delayMin, 0, 500, 1, true);
            sliderInt(b, L"Delay max (ms)", cfg.trigger.delayMax, 0, 500, 1, true);
            sliderInt(b, L"Hold min (ms)", cfg.trigger.holdMin, 1, 250, 1, true);
            sliderInt(b, L"Hold max (ms)", cfg.trigger.holdMax, 1, 250, 1, true);
            section(b, L"BURST");
            checkbox(b, L"Burst mode", cfg.trigger.burst);
            sliderInt(b, L"Burst shots", cfg.trigger.burstShots, 1, 20, 1, cfg.trigger.burst);
            sliderImpl(b, L"Burst delay", cfg.trigger.burstDelay, 0.0f, 0.5f, 0.01f,
                       cfg.trigger.burst, 2);
            sliderImpl(b, L"Burst cooldown", cfg.trigger.burstCooldown, 0.0f, 2.0f, 0.01f,
                       cfg.trigger.burst, 2);
            note(b, L"Burst delay is applied between shots; cooldown after the burst.");
            storeCtx(1, b);
            break;
        }

        case 2: {  // ESP
            beginColumns(3);

            Ctx a = makeCtx(0);
            section(a, L"MASTER");
            {
                const bool before = cfg.esp.enabled;
                checkbox(a, L"Enabled", cfg.esp.enabled);
                if (before != cfg.esp.enabled && host.esp) *host.esp = cfg.esp.enabled;
            }
            checkbox(a, L"Team check", cfg.esp.teamCheck);
            sliderImpl(a, L"Max distance", cfg.esp.maxDistance, 500.0f, 8000.0f, 100.0f, true, 0);
            sliderImpl(a, L"Fade start", cfg.esp.fadeStart, 0.0f, 8000.0f, 100.0f, true, 0);
            {
                bool reserved = cfg.esp.visibleCheck;
                checkbox(a, L"Visible check (ext.)", reserved, false);
            }
            checkbox(a, L"On-screen only", cfg.esp.oofCheck);
            section(a, L"BOX");
            checkbox(a, L"Box", cfg.esp.box);
            combo(a, L"Box style", cfg.esp.boxStyle, {L"Full", L"Corner", L"3D"});
            sliderImpl(a, L"Thickness", cfg.esp.boxThickness, 0.5f, 5.0f, 0.1f, cfg.esp.box, 2);
            checkbox(a, L"Filled box", cfg.esp.filledBox, cfg.esp.box);
            colorRow(a, L"Fill color", cfg.esp.fillColor);
            checkbox(a, L"Health bar", cfg.esp.healthBar);
            combo(a, L"Health position", cfg.esp.healthBarPos, {L"Left", L"Right", L"Top"});
            checkbox(a, L"Health text", cfg.esp.healthText);
            checkbox(a, L"Armor bar", cfg.esp.armorBar);
            storeCtx(0, a);

            Ctx b = makeCtx(1);
            section(b, L"ELEMENTS");
            checkbox(b, L"Name", cfg.esp.name);
            checkbox(b, L"Weapon", cfg.esp.weapon);
            checkbox(b, L"Distance", cfg.esp.distance);
            checkbox(b, L"Money", cfg.esp.money);
            checkbox(b, L"Ammo", cfg.esp.ammo);
            checkbox(b, L"Head dot", cfg.esp.headDot);
            checkbox(b, L"Snapline", cfg.esp.snapline);
            combo(b, L"Snapline origin", cfg.esp.snaplineOrigin, {L"Bottom", L"Top", L"Center"});
            checkbox(b, L"Skeleton (approx)", cfg.esp.skeleton);
            sliderImpl(b, L"Skeleton width", cfg.esp.skeletonThickness, 0.5f, 4.0f, 0.1f,
                       cfg.esp.skeleton, 2);
            checkbox(b, L"Flags", cfg.esp.flags);
            checkbox(b, L"Glow", cfg.esp.glow);
            checkbox(b, L"Offscreen arrows", cfg.esp.offscreenArrows);
            sliderInt(b, L"Text size", cfg.esp.textSize, 8, 24, 1, true);
            checkbox(b, L"Text outline", cfg.esp.outlineText);
            note(b, L"Skeleton and glow are drawn from the sampled hit ladder.");
            storeCtx(1, b);

            Ctx d = makeCtx(2);
            section(d, L"MORE (EXTERNAL LIMITS)");
            {
                bool reserved = cfg.esp.soundEsp;
                checkbox(d, L"Sound ESP (ext.)", reserved, false);
                reserved = cfg.esp.bomb;
                checkbox(d, L"Bomb ESP (ext.)", reserved, false);
                reserved = cfg.esp.bombTimer;
                checkbox(d, L"Bomb timer (ext.)", reserved, false);
                reserved = cfg.esp.droppedWeapons;
                checkbox(d, L"Dropped weapons (ext.)", reserved, false);
                reserved = cfg.esp.grenades;
                checkbox(d, L"Grenades (ext.)", reserved, false);
                reserved = cfg.esp.localPlayer;
                checkbox(d, L"Local player (ext.)", reserved, false);
            }
            sliderImpl(d, L"Sound radius", cfg.esp.soundRadius, 100.0f, 3000.0f, 50.0f, false, 0);
            section(d, L"COLORS");
            colorRow(d, L"Visible", cfg.esp.colorVisible);
            colorRow(d, L"Occluded", cfg.esp.colorOccluded);
            colorRow(d, L"Snapline", cfg.esp.colorSnapline);
            colorRow(d, L"Text", cfg.esp.colorText);
            colorRow(d, L"Health", cfg.esp.colorHealth);
            colorRow(d, L"Armor", cfg.esp.colorArmor);
            colorRow(d, L"Skeleton", cfg.esp.colorSkeleton);
            colorRow(d, L"Head dot", cfg.esp.colorHeadDot);
            colorRow(d, L"Glow", cfg.esp.colorGlow);
            storeCtx(2, d);
            break;
        }

        case 3: {  // Visuals
            beginColumns(3);

            Ctx a = makeCtx(0);
            section(a, L"CROSSHAIR");
            checkbox(a, L"Custom crosshair", cfg.visuals.crosshair);
            combo(a, L"Style", cfg.visuals.crosshairStyle, {L"Cross", L"Dot", L"Circle", L"T"});
            sliderImpl(a, L"Size", cfg.visuals.crosshairSize, 1.0f, 30.0f, 0.5f,
                       cfg.visuals.crosshair, 1);
            sliderImpl(a, L"Gap", cfg.visuals.crosshairGap, 0.0f, 20.0f, 0.5f,
                       cfg.visuals.crosshair, 1);
            sliderImpl(a, L"Thickness", cfg.visuals.crosshairThickness, 0.5f, 5.0f, 0.1f,
                       cfg.visuals.crosshair, 1);
            colorRow(a, L"Color", cfg.visuals.crosshairColor);
            section(a, L"HIT FEEDBACK");
            checkbox(a, L"Hitmarker", cfg.visuals.hitmarker);
            sliderImpl(a, L"Hitmarker size", cfg.visuals.hitmarkerSize, 4.0f, 30.0f, 1.0f,
                       cfg.visuals.hitmarker, 0);
            colorRow(a, L"Hitmarker color", cfg.visuals.hitmarkerColor);
            checkbox(a, L"Damage indicator", cfg.visuals.damageIndicator);
            storeCtx(0, a);

            Ctx b = makeCtx(1);
            section(b, L"HUD");
            checkbox(b, L"Watermark", cfg.visuals.watermark);
            checkbox(b, L"Keybind list", cfg.visuals.keybindList);
            checkbox(b, L"FPS counter", cfg.visuals.fpsCounter);
            checkbox(b, L"Time counter", cfg.visuals.timeCounter);
            {
                bool reserved = cfg.visuals.pingCounter;
                checkbox(b, L"Ping counter (ext.)", reserved, false);
                reserved = cfg.visuals.spectatorList;
                checkbox(b, L"Spectator list (ext.)", reserved, false);
            }
            colorRow(b, L"Accent", cfg.visuals.accent);
            colorRow(b, L"Accent dim", cfg.visuals.accentDim);
            note(b, L"Watermark and keybind list use the accent color.");
            storeCtx(1, b);

            Ctx d = makeCtx(2);
            section(d, L"SCREEN");
            checkbox(d, L"Night mode", cfg.visuals.nightMode);
            sliderImpl(d, L"Night alpha", cfg.visuals.nightAlpha, 0.0f, 0.9f, 0.01f,
                       cfg.visuals.nightMode, 2);
            {
                bool reserved = cfg.visuals.noFlash;
                checkbox(d, L"No flash (ext.)", reserved, false);
                reserved = cfg.visuals.noSmoke;
                checkbox(d, L"No smoke (ext.)", reserved, false);
                reserved = cfg.visuals.noDecals;
                checkbox(d, L"No decals (ext.)", reserved, false);
                reserved = cfg.visuals.fovChanger;
                checkbox(d, L"FOV changer (ext.)", reserved, false);
                reserved = cfg.visuals.thirdPerson;
                checkbox(d, L"Third person (ext.)", reserved, false);
                reserved = cfg.visuals.noScope;
                checkbox(d, L"No scope overlay (ext.)", reserved, false);
                reserved = cfg.visuals.removeHands;
                checkbox(d, L"Remove hands (ext.)", reserved, false);
            }
            sliderImpl(d, L"Game FOV", cfg.visuals.fov, 60.0f, 140.0f, 1.0f, true, 0);
            sliderImpl(d, L"Third person dist", cfg.visuals.thirdPersonDist, 40.0f, 250.0f, 5.0f,
                       false, 0);
            sliderImpl(d, L"Flash alpha", cfg.visuals.flashAlpha, 0.0f, 1.0f, 0.01f, false, 2);
            note(d, L"Night mode tints the overlay; it cannot change the scene.");
            storeCtx(2, d);
            break;
        }

        case 4: {  // Radar
            beginColumns(2);

            Ctx a = makeCtx(0);
            section(a, L"RADAR");
            checkbox(a, L"Enabled", cfg.radar.enabled);
            sliderImpl(a, L"Size", cfg.radar.size, 80.0f, 500.0f, 10.0f, cfg.radar.enabled, 0);
            sliderImpl(a, L"Zoom", cfg.radar.zoom, 0.2f, 4.0f, 0.1f, cfg.radar.enabled, 1);
            sliderInt(a, L"Position X", cfg.radar.posX, 0, 3000, 5, cfg.radar.enabled);
            sliderInt(a, L"Position Y", cfg.radar.posY, 0, 3000, 5, cfg.radar.enabled);
            checkbox(a, L"Rotate with view", cfg.radar.rotate, cfg.radar.enabled);
            storeCtx(0, a);

            Ctx b = makeCtx(1);
            section(b, L"ENTITIES");
            checkbox(b, L"Show enemies", cfg.radar.showEnemies, cfg.radar.enabled);
            checkbox(b, L"Show team", cfg.radar.showTeam, cfg.radar.enabled);
            checkbox(b, L"Show bomb", cfg.radar.showBomb, cfg.radar.enabled);
            checkbox(b, L"Show dropped", cfg.radar.showDropped, cfg.radar.enabled);
            sliderImpl(b, L"Dot size", cfg.radar.dotSize, 1.0f, 10.0f, 0.5f, cfg.radar.enabled, 1);
            section(b, L"COLORS");
            colorRow(b, L"Enemy", cfg.radar.colorEnemy);
            colorRow(b, L"Team", cfg.radar.colorTeam);
            colorRow(b, L"Bomb", cfg.radar.colorBomb);
            storeCtx(1, b);
            break;
        }

        case 5: {  // Misc
            beginColumns(3);

            Ctx a = makeCtx(0);
            section(a, L"MOVEMENT");
            checkbox(a, L"Bunny hop", cfg.misc.bhop);
            keybind(a, L"Bhop key", cfg.misc.bhopKey);
            checkbox(a, L"Auto strafe", cfg.misc.autoStrafe);
            sliderImpl(a, L"Strafe strength", cfg.misc.strafeStrength, 0.1f, 2.0f, 0.05f,
                       cfg.misc.autoStrafe, 2);
            {
                bool reserved = cfg.misc.jumpBug;
                checkbox(a, L"Jump bug (ext.)", reserved, false);
                reserved = cfg.misc.edgeJump;
                checkbox(a, L"Edge jump (ext.)", reserved, false);
            }
            keybind(a, L"Edge jump key", cfg.misc.edgeJumpKey);
            note(a, L"Movement keys are sent with SendInput while the game is focused.");
            storeCtx(0, a);

            Ctx b = makeCtx(1);
            section(b, L"AUTOMATION");
            checkbox(b, L"Auto pistol (misc)", cfg.misc.autoPistol);
            checkbox(b, L"Auto reload", cfg.misc.autoReload);
            {
                bool reserved = cfg.misc.autoAccept;
                checkbox(b, L"Auto accept (ext.)", reserved, false);
                reserved = cfg.misc.autoDefuse;
                checkbox(b, L"Auto defuse (ext.)", reserved, false);
                reserved = cfg.misc.autoBuy;
                checkbox(b, L"Auto buy (ext.)", reserved, false);
                reserved = cfg.misc.fakeLatency;
                checkbox(b, L"Fake latency (ext.)", reserved, false);
            }
            textRow(b, L"Buy text", cfg.misc.autoBuyText);
            sliderInt(b, L"Fake latency ms", cfg.misc.fakeLatencyMs, 0, 500, 5, false);
            note(b, L"Auto reload reads m_ammo; buy/accept need game UI control.");
            storeCtx(1, b);

            Ctx d = makeCtx(2);
            section(d, L"CHAT / SOCIAL");
            checkbox(d, L"Chat spam", cfg.misc.chatSpam);
            textRow(d, L"Spam text", cfg.misc.chatSpamText);
            sliderImpl(d, L"Spam delay (s)", cfg.misc.chatSpamDelay, 0.5f, 30.0f, 0.5f,
                       cfg.misc.chatSpam, 1);
            checkbox(d, L"Kill say", cfg.misc.killSay);
            textRow(d, L"Kill text", cfg.misc.killSayText);
            {
                bool reserved = cfg.misc.clanTag;
                checkbox(d, L"Clan tag (ext.)", reserved, false);
                reserved = cfg.misc.antiAim;
                checkbox(d, L"Anti aim (ext.)", reserved, false);
                reserved = cfg.misc.autoGG;
                checkbox(d, L"Auto GG (ext.)", reserved, false);
                reserved = cfg.misc.autoMolotov;
                checkbox(d, L"Auto molotov (ext.)", reserved, false);
                reserved = cfg.misc.preserveKillfeed;
                checkbox(d, L"Preserve killfeed (ext.)", reserved, false);
            }
            textRow(d, L"Clan tag text", cfg.misc.clanTagText);
            combo(d, L"Anti aim mode", cfg.misc.antiAimMode,
                  {L"Off", L"Spin", L"Jitter", L"Static"});
            checkbox(d, L"Knife bot", cfg.misc.knifeBot);
            storeCtx(2, d);
            break;
        }

        case 6: {  // Config
            beginColumns(2);

            Ctx a = makeCtx(0);
            section(a, L"PRESETS");
            note(a, L"Applies a coherent legit -> rage configuration.");
            if (button(a, L"Legit", accent)) {
                cfg.applyPreset(Preset::Legit);
                syncRuntime();
            }
            if (button(a, L"Semi-Legit", accent)) {
                cfg.applyPreset(Preset::SemiLegit);
                syncRuntime();
            }
            if (button(a, L"Semi-Rage", accent)) {
                cfg.applyPreset(Preset::SemiRage);
                syncRuntime();
            }
            if (button(a, L"Rage", accent)) {
                cfg.applyPreset(Preset::Rage);
                syncRuntime();
            }
            if (button(a, L"Mark as custom", withAlpha(accent, 0.6f))) cfg.preset = Preset::Custom;

            section(a, L"FILE");
            textRow(a, L"Config path", host.configPath);
            if (button(a, L"Save config", 0xFF35FF6Eu)) {
                if (cfg.save(host.configPath.c_str())) {
                    logf("menu: config saved to %s", host.configPath.c_str());
                } else {
                    logf("menu: could not save %s", host.configPath.c_str());
                }
            }
            if (button(a, L"Reload config", 0xFF4DA6FFu)) {
                if (cfg.load(host.configPath.c_str())) {
                    syncRuntime();
                    logf("menu: config reloaded from %s", host.configPath.c_str());
                }
            }
            if (button(a, L"Panic (disable all)", 0xFFFFAA00u)) {
                cfg.esp.enabled = false;
                cfg.aim.enabled = false;
                cfg.trigger.enabled = false;
                syncRuntime();
                logf("menu: panic");
            }
            if (button(a, L"Unload and quit", 0xFFFF4040u)) {
                if (host.stop) *host.stop = true;
            }
            storeCtx(0, a);

            Ctx b = makeCtx(1);
            section(b, L"HOTKEYS");
            keybind(b, L"Menu", cfg.menuKey);
            keybind(b, L"Quit", cfg.quitKey);
            keybind(b, L"ESP toggle", cfg.espToggleKey);
            keybind(b, L"Aim toggle", cfg.aimToggleKey);
            keybind(b, L"Trigger toggle", cfg.triggerToggleKey);
            keybind(b, L"Panic", cfg.panicKey);
            section(b, L"PERFORMANCE");
            sliderInt(b, L"Render FPS", cfg.renderFps, 0, 360, 5, true);
            sliderInt(b, L"Scan Hz", cfg.aimHz, 30, 2000, 10, true);
            note(b, L"Render FPS 0 = uncapped. Scan Hz is re-read every tick.");
            note(b, L"Offsets live in config.ini; use --import after a patch.");
            storeCtx(1, b);
            break;
        }

        default:
            break;
    }

    (void)now;
    mouseWasDown_ = down;
    (void)captureStart_;

    if (colorTarget_) {
        const float pickerW = 214.0f;
        const float pickerH = 232.0f;
        pickerPos_.x = std::clamp(pickerPos_.x, 8.0f, screenW - pickerW - 8.0f);
        pickerPos_.y = std::clamp(pickerPos_.y, 8.0f, screenH - pickerH - 8.0f);

        const float px = pickerPos_.x;
        const float py = pickerPos_.y;
        const bool overPicker = hit(px, py, pickerW, pickerH);

        if (clicked && now - pickerOpened_ > 120.0 && !overPicker) {
            colorTarget_ = nullptr;
        } else {
            overlay.fillRect(px + 5.0f, py + 5.0f, pickerW, pickerH, 0x50000000u);
            overlay.fillRect(px, py, pickerW, pickerH, panelColor);
            overlay.rect(px, py, pickerW, pickerH, withAlpha(accent, 0.7f), 1.0f);
            overlay.text(L"COLOR", px + 14.0f, py + 10.0f, accent, 13.0f, false);

            uint32_t color = *colorTarget_;
            float r = static_cast<float>((color >> 16) & 0xFF);
            float g = static_cast<float>((color >> 8) & 0xFF);
            float b = static_cast<float>(color & 0xFF);
            float a = static_cast<float>((color >> 24) & 0xFF);

            overlay.fillRect(px + 14.0f, py + 32.0f, pickerW - 28.0f, 26.0f, color);
            overlay.rect(px + 14.0f, py + 32.0f, pickerW - 28.0f, 26.0f, 0xFF50505Eu, 1.0f);

            Ctx picker;
            picker.x = px + 14.0f;
            picker.y = py + 68.0f;
            picker.w = pickerW - 28.0f;
            picker.id = 10000;
            picker.mouse = mouse;
            picker.down = down;
            picker.clicked = clicked;
            picker.id = 10000;

            sliderImpl(picker, L"R", r, 0.0f, 255.0f, 1.0f, true, 0);
            sliderImpl(picker, L"G", g, 0.0f, 255.0f, 1.0f, true, 0);
            sliderImpl(picker, L"B", b, 0.0f, 255.0f, 1.0f, true, 0);
            sliderImpl(picker, L"A", a, 0.0f, 255.0f, 1.0f, true, 0);

            *colorTarget_ = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) |
                            (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);

            const float closeW = 72.0f;
            const float closeX = px + pickerW - closeW - 14.0f;
            const float closeY = py + pickerH - 30.0f;
            const bool closeOver = hit(closeX, closeY, closeW, 20.0f);
            overlay.fillRect(closeX, closeY, closeW, 20.0f,
                             closeOver ? withAlpha(accent, 0.8f) : withAlpha(accent, 0.35f));
            const float closeText = overlay.measure(L"Done", 12.0f);
            overlay.text(L"Done", closeX + (closeW - closeText) * 0.5f, closeY + 3.0f,
                         0xFFF2F2F8u, 12.0f, false);
            if (closeOver && clicked) colorTarget_ = nullptr;
        }
    }

    if (keyTarget_) {
        overlay.fillRect(0.0f, 0.0f, screenW, screenH, 0x50000000u);
        const wchar_t* prompt = L"Press a key to bind it  (ESC cancels)";
        const float width = overlay.measure(prompt, 17.0f);
        overlay.fillRect((screenW - width) * 0.5f - 18.0f, screenH * 0.5f - 20.0f, width + 36.0f,
                         46.0f, 0xF014141Cu);
        overlay.rect((screenW - width) * 0.5f - 18.0f, screenH * 0.5f - 20.0f, width + 36.0f,
                     46.0f, accent, 1.0f);
        overlay.text(prompt, screenW * 0.5f, screenH * 0.5f - 6.0f, textColor, 17.0f, true);
    }
}

}  // namespace hx
