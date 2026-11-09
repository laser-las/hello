#include "esp.h"

#include <cstdarg>
#include <cstring>
#include <unordered_map>

namespace hx {
namespace {

constexpr float kUnitsToMeters = 0.01905f;

uint32_t withAlpha(uint32_t argb, float alpha) {
    const uint32_t a = static_cast<uint32_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    return (argb & 0x00FFFFFFu) | (a << 24);
}

std::wstring fmtw(const char* format, ...) {
    char buffer[256];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);
    return std::wstring(buffer, buffer + std::strlen(buffer));
}

std::wstring widenUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int need = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (need <= 0) return std::wstring();
    std::wstring out(static_cast<size_t>(need), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), need);
    return out;
}

void drawCornerBox(Overlay& overlay, float x, float y, float w, float h, uint32_t color,
                   float thickness) {
    const float arm = std::max(3.0f, std::min(w, h) * 0.25f);
    overlay.line(x, y, x + arm, y, color, thickness);
    overlay.line(x, y, x, y + arm, color, thickness);

    overlay.line(x + w - arm, y, x + w, y, color, thickness);
    overlay.line(x + w, y, x + w, y + arm, color, thickness);

    overlay.line(x, y + h - arm, x, y + h, color, thickness);
    overlay.line(x, y + h, x + arm, y + h, color, thickness);

    overlay.line(x + w - arm, y + h, x + w, y + h, color, thickness);
    overlay.line(x + w, y + h - arm, x + w, y + h, color, thickness);
}

void drawHealthBar(Overlay& overlay, const Config& cfg, float boxX, float boxY, float boxW,
                   float boxH, int health) {
    const float barW = std::max(2.0f, boxW * 0.11f);
    const float frac = std::clamp(static_cast<float>(health) / 100.0f, 0.0f, 1.0f);

    float x = boxX - barW - 3.0f;
    float y = boxY;
    float w = barW;
    float h = boxH;

    if (cfg.esp.healthBarPos == 1) {
        x = boxX + boxW + 3.0f;
    } else if (cfg.esp.healthBarPos == 2) {
        x = boxX;
        y = boxY - barW - 3.0f;
        w = boxW;
        h = barW;
    }

    overlay.fillRect(x, y, w, h, 0xB0000000u);

    if (cfg.esp.healthBarPos == 2) {
        overlay.fillRect(x, y, w * frac, h, cfg.esp.colorHealth);
    } else {
        overlay.fillRect(x, y + h * (1.0f - frac), w, h * frac, cfg.esp.colorHealth);
    }
}

void drawArmorBar(Overlay& overlay, const Config& cfg, float boxX, float boxY, float boxW,
                  float boxH, int armor) {
    const float barW = std::max(1.5f, boxW * 0.06f);
    const float frac = std::clamp(static_cast<float>(armor) / 100.0f, 0.0f, 1.0f);
    const float x = boxX - barW * 2.0f - 5.0f;

    overlay.fillRect(x, boxY, barW, boxH, 0xB0000000u);
    overlay.fillRect(x, boxY + boxH * (1.0f - frac), barW, boxH * frac, cfg.esp.colorArmor);
}

void drawRadar(Overlay& overlay, const World& world, const Config& cfg) {
    if (!world.haveLocal) return;

    const float radius = std::max(40.0f, cfg.radar.size * 0.5f);
    const float cx = static_cast<float>(cfg.radar.posX) + radius;
    const float cy = static_cast<float>(cfg.radar.posY) + radius;

    overlay.dot(cx, cy, radius, 0x50000000u);
    overlay.circle(cx, cy, radius, withAlpha(cfg.visuals.accent, 0.65f), 1.5f);
    overlay.line(cx - 5.0f, cy, cx + 5.0f, cy, 0x60FFFFFF, 1.0f);
    overlay.line(cx, cy - 5.0f, cx, cy + 5.0f, 0x60FFFFFF, 1.0f);

    const float yaw = cfg.radar.rotate ? world.viewAngles.y : 0.0f;
    const float radians = -yaw * kPi / 180.0f;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    const float scale = (radius / 1400.0f) * std::max(0.1f, cfg.radar.zoom);

    for (const Player& p : world.players) {
        const bool sameTeam = world.localTeam != 0 && p.team == world.localTeam;
        if (sameTeam ? !cfg.radar.showTeam : !cfg.radar.showEnemies) continue;

        const float dx = p.origin.x - world.localOrigin.x;
        const float dy = p.origin.y - world.localOrigin.y;

        float rx = (dx * c - dy * s) * scale;
        float ry = (dx * s + dy * c) * scale;

        const float length = std::sqrt(rx * rx + ry * ry);
        const float limit = radius - 4.0f;
        if (length > limit) {
            const float k = limit / length;
            rx *= k;
            ry *= k;
        }

        const uint32_t color = sameTeam ? cfg.radar.colorTeam : cfg.radar.colorEnemy;
        overlay.dot(cx + rx, cy - ry, cfg.radar.dotSize, color);
    }
}

void drawCrosshair(Overlay& overlay, const Config& cfg, float width, float height) {
    const float cx = width * 0.5f;
    const float cy = height * 0.5f;
    const float size = cfg.visuals.crosshairSize;
    const float gap = cfg.visuals.crosshairGap;
    const float thickness = cfg.visuals.crosshairThickness;
    const uint32_t color = cfg.visuals.crosshairColor;

    switch (cfg.visuals.crosshairStyle) {
        case 1:
            overlay.dot(cx, cy, std::max(1.0f, thickness), color);
            break;
        case 2:
            overlay.circle(cx, cy, std::max(2.0f, size), color, thickness);
            break;
        case 3:
            overlay.line(cx - size - gap, cy, cx + size + gap, cy, color, thickness);
            overlay.line(cx, cy - gap, cx, cy + gap - size, color, thickness);
            break;
        default:
            overlay.line(cx - size - gap, cy, cx - gap, cy, color, thickness);
            overlay.line(cx + gap, cy, cx + size + gap, cy, color, thickness);
            overlay.line(cx, cy - size - gap, cx, cy - gap, color, thickness);
            overlay.line(cx, cy + gap, cx, cy + size + gap, color, thickness);
            break;
    }
}

void drawSkeleton(Overlay& overlay, const World& world, const Config& cfg, const Player& player,
                  float boxWidth, float fade) {
    Vec2 head{};
    Vec2 neck{};
    Vec2 chest{};
    Vec2 stomach{};
    Vec2 feet{};
    if (!worldToScreen(world.view, player.head, static_cast<float>(overlay.width()),
                       static_cast<float>(overlay.height()), head)) {
        return;
    }
    if (!worldToScreen(world.view, player.neck, static_cast<float>(overlay.width()),
                       static_cast<float>(overlay.height()), neck)) {
        return;
    }
    if (!worldToScreen(world.view, player.chest, static_cast<float>(overlay.width()),
                       static_cast<float>(overlay.height()), chest)) {
        return;
    }
    if (!worldToScreen(world.view, player.stomach, static_cast<float>(overlay.width()),
                       static_cast<float>(overlay.height()), stomach)) {
        return;
    }
    if (!worldToScreen(world.view, player.origin, static_cast<float>(overlay.width()),
                       static_cast<float>(overlay.height()), feet)) {
        return;
    }

    const uint32_t color = withAlpha(cfg.esp.colorSkeleton, fade);
    const float thickness = std::max(0.5f, cfg.esp.skeletonThickness);

    overlay.line(head.x, head.y, neck.x, neck.y, color, thickness);
    overlay.line(neck.x, neck.y, chest.x, chest.y, color, thickness);
    overlay.line(chest.x, chest.y, stomach.x, stomach.y, color, thickness);
    overlay.line(stomach.x, stomach.y, feet.x, feet.y, color, thickness);

    const float halfShoulder = boxWidth * 0.34f;
    const float halfHip = boxWidth * 0.18f;
    const float shoulderY = chest.y + (stomach.y - chest.y) * 0.25f;
    const float hipY = stomach.y + (feet.y - stomach.y) * 0.30f;
    const float elbowY = shoulderY + (hipY - shoulderY) * 0.55f;

    const Vec2 leftShoulder{chest.x - halfShoulder, shoulderY};
    const Vec2 rightShoulder{chest.x + halfShoulder, shoulderY};
    const Vec2 leftElbow{chest.x - halfShoulder * 1.25f, elbowY};
    const Vec2 rightElbow{chest.x + halfShoulder * 1.25f, elbowY};
    const Vec2 leftHip{stomach.x - halfHip, hipY};
    const Vec2 rightHip{stomach.x + halfHip, hipY};

    overlay.line(chest.x, chest.y, leftShoulder.x, leftShoulder.y, color, thickness);
    overlay.line(chest.x, chest.y, rightShoulder.x, rightShoulder.y, color, thickness);
    overlay.line(leftShoulder.x, leftShoulder.y, leftElbow.x, leftElbow.y, color, thickness);
    overlay.line(rightShoulder.x, rightShoulder.y, rightElbow.x, rightElbow.y, color, thickness);

    overlay.line(stomach.x, stomach.y, leftHip.x, leftHip.y, color, thickness);
    overlay.line(stomach.x, stomach.y, rightHip.x, rightHip.y, color, thickness);
    overlay.line(leftHip.x, leftHip.y, feet.x - boxWidth * 0.18f, feet.y, color, thickness);
    overlay.line(rightHip.x, rightHip.y, feet.x + boxWidth * 0.18f, feet.y, color, thickness);
}

void drawOffscreenArrow(Overlay& overlay, const World& world, const Config& cfg,
                        const Player& player, uint32_t color) {
    if (!world.haveLocal) return;

    const Vec3 delta = player.origin - world.localEye;
    const float targetYaw = std::atan2(delta.y, delta.x) * kRad2Deg;

    const float forwardX = world.view.m[3];
    const float forwardY = world.view.m[7];
    float cameraYaw = world.viewAngles.y;
    if (std::fabs(forwardX) + std::fabs(forwardY) > 1.0e-4f) {
        cameraYaw = std::atan2(forwardY, forwardX) * kRad2Deg;
    }

    float relative = targetYaw - cameraYaw;
    while (relative > 180.0f) relative -= 360.0f;
    while (relative < -180.0f) relative += 360.0f;

    const float radians = relative * kPi / 180.0f;
    const float dirX = -std::sin(radians);
    const float dirY = -std::cos(radians);

    const float width = static_cast<float>(overlay.width());
    const float height = static_cast<float>(overlay.height());
    const float cx = width * 0.5f;
    const float cy = height * 0.5f;
    const float radius = std::min(width, height) * 0.40f;

    const float tipX = cx + dirX * radius;
    const float tipY = cy + dirY * radius;

    overlay.line(cx + dirX * (radius - 18.0f), cy + dirY * (radius - 18.0f), tipX, tipY, color,
                 2.5f);
    overlay.dot(tipX, tipY, 4.5f, color);

    const std::wstring label = fmtw("%.0fm", player.distance * kUnitsToMeters);
    overlay.text(label, tipX - dirX * 16.0f, tipY - dirY * 16.0f, color, 11.0f, true);
}

}  // namespace

void drawEsp(Overlay& overlay, const World& world, const Config& cfg, const EspState& state) {
    const float width = static_cast<float>(overlay.width());
    const float height = static_cast<float>(overlay.height());
    if (width <= 0.0f || height <= 0.0f) return;

    const float cx = width * 0.5f;
    const float cy = height * 0.5f;
    const float textSize = cfg.esp.textSize > 0 ? static_cast<float>(cfg.esp.textSize) : 13.0f;

    if (cfg.visuals.nightMode) {
        overlay.fillRect(0.0f, 0.0f, width, height,
                         withAlpha(0xFF06101Eu, cfg.visuals.nightAlpha));
    }

    if (cfg.aim.drawFov && state.aimOn && cfg.aim.fov > 1.0f) {
        overlay.circle(cx, cy, cfg.aim.fov, withAlpha(cfg.aim.fovColor, cfg.aim.fovAlpha), 1.0f);
    }

    static std::unordered_map<uintptr_t, int> trackedHealth;
    static double hitmarkerUntil = 0.0;
    static double damageUntil = 0.0;
    static int lastDamage = 0;
    const double now = static_cast<double>(GetTickCount64());

    for (const Player& p : world.players) {
        auto& previous = trackedHealth[p.address];
        if (previous > p.health && p.health > 0) {
            lastDamage = previous - p.health;
            hitmarkerUntil = now + 380.0;
            damageUntil = now + 1100.0;
        }
        previous = p.health;
    }

    if (state.espOn && cfg.esp.enabled) {
        for (const Player& p : world.players) {
            if (cfg.esp.teamCheck && world.localTeam != 0 && p.team == world.localTeam) continue;
            if (cfg.esp.maxDistance > 0.0f && p.distance > cfg.esp.maxDistance) continue;

            const uint32_t arrowColor =
                withAlpha(p.spotted ? cfg.esp.colorVisible : cfg.esp.colorOccluded, 0.85f);

            Vec2 feet{};
            Vec2 head{};
            const bool feetOk = worldToScreen(world.view, p.origin, width, height, feet);
            const bool headOk = worldToScreen(world.view, p.head, width, height, head);
            if (!feetOk || !headOk) {
                if (cfg.esp.offscreenArrows) {
                    drawOffscreenArrow(overlay, world, cfg, p, arrowColor);
                }
                continue;
            }

            float boxHeight = feet.y - head.y;
            if (boxHeight < 4.0f || boxHeight > height * 1.5f) continue;

            const float boxWidth = boxHeight * 0.45f;
            const float left = head.x - boxWidth * 0.5f;
            const float top = head.y;
            const uint32_t color = p.spotted ? cfg.esp.colorVisible : cfg.esp.colorOccluded;

            float fade = 1.0f;
            if (cfg.esp.fadeStart > 0.0f && p.distance > cfg.esp.fadeStart) {
                const float span = std::max(1.0f, cfg.esp.maxDistance - cfg.esp.fadeStart);
                fade = std::clamp(1.0f - (p.distance - cfg.esp.fadeStart) / span, 0.15f, 1.0f);
            }

            if (cfg.esp.glow) {
                for (int ring = 3; ring >= 1; --ring) {
                    const float expand = static_cast<float>(ring) * 2.0f;
                    overlay.rect(left - expand, top - expand, boxWidth + expand * 2.0f,
                                 boxHeight + expand * 2.0f,
                                 withAlpha(cfg.esp.colorGlow, fade * 0.10f * ring), 2.0f);
                }
            }

            if (cfg.esp.skeleton) {
                drawSkeleton(overlay, world, cfg, p, boxWidth, fade);
            }

            if (cfg.esp.filledBox) {
                overlay.fillRect(left, top, boxWidth, boxHeight, withAlpha(cfg.esp.fillColor, fade * 0.35f));
            }

            if (cfg.esp.box) {
                const uint32_t boxColor = withAlpha(color, fade);
                if (cfg.esp.boxStyle == 1) {
                    drawCornerBox(overlay, left, top, boxWidth, boxHeight, boxColor,
                                  cfg.esp.boxThickness);
                } else {
                    overlay.rect(left, top, boxWidth, boxHeight, boxColor, cfg.esp.boxThickness);
                }
            }

            if (cfg.esp.healthBar) {
                drawHealthBar(overlay, cfg, left, top, boxWidth, boxHeight, p.health);
            }
            if (cfg.esp.armorBar && p.armor > 0) {
                drawArmorBar(overlay, cfg, left, top, boxWidth, boxHeight, p.armor);
            }

            if (cfg.esp.headDot) {
                overlay.dot(head.x, head.y, std::max(2.0f, boxWidth * 0.09f), cfg.esp.colorHeadDot);
            }

            if (cfg.esp.snapline) {
                float originX = cx;
                float originY = height;
                if (cfg.esp.snaplineOrigin == 1) originY = 0.0f;
                if (cfg.esp.snaplineOrigin == 2) originY = cy;
                overlay.line(originX, originY, head.x, feet.y, cfg.esp.colorSnapline, 1.0f);
            }

            float textY = top - textSize - 4.0f;
            const uint32_t textColor = withAlpha(cfg.esp.colorText, fade);

            if (cfg.esp.name && !p.name.empty()) {
                overlay.text(widenUtf8(p.name), head.x, textY, textColor, textSize, true);
                textY -= textSize + 2.0f;
            }

            if (cfg.esp.weapon && p.weapon > 0) {
                const std::wstring weapon = widenUtf8(weaponName(p.weapon));
                if (!weapon.empty()) {
                    overlay.text(weapon, head.x, textY, textColor, textSize, true);
                    textY -= textSize + 2.0f;
                }
            }

            std::wstring info;
            if (cfg.esp.healthText) info += fmtw("%d hp", p.health);
            if (cfg.esp.distance) {
                if (!info.empty()) info += L"  ";
                info += fmtw("%.0fm", p.distance * kUnitsToMeters);
            }
            if (cfg.esp.money && p.money > 0) {
                if (!info.empty()) info += L"  ";
                info += fmtw("$%d", p.money);
            }
            if (cfg.esp.ammo && p.ammo >= 0) {
                if (!info.empty()) info += L"  ";
                info += fmtw("%d rnd", p.ammo);
            }
            if (!info.empty()) {
                overlay.text(info, head.x, textY, textColor, textSize, true);
            }

            if (cfg.esp.flags) {
                float flagY = top + 2.0f;
                if (p.scoped) {
                    overlay.text(L"scope", left + boxWidth + 5.0f, flagY, 0xFF4DA6FF, textSize - 1.0f,
                                 false);
                    flagY += textSize;
                }
                if (p.defusing) {
                    overlay.text(L"defuse", left + boxWidth + 5.0f, flagY, 0xFFFFAA00, textSize - 1.0f,
                                 false);
                    flagY += textSize;
                }
                if (p.flashed) {
                    overlay.text(L"flash", left + boxWidth + 5.0f, flagY, 0xFFFFFF66, textSize - 1.0f,
                                 false);
                }
            }
        }
    }

    if (cfg.visuals.hitmarker && now < hitmarkerUntil) {
        const float remaining = static_cast<float>((hitmarkerUntil - now) / 380.0);
        const uint32_t hitColor = withAlpha(cfg.visuals.hitmarkerColor, std::max(0.25f, remaining));
        const float size = std::max(4.0f, cfg.visuals.hitmarkerSize);
        const float inner = size * 0.45f;

        overlay.line(cx - size, cy - size, cx - inner, cy - inner, hitColor, 2.0f);
        overlay.line(cx + inner, cy - inner, cx + size, cy - size, hitColor, 2.0f);
        overlay.line(cx - size, cy + size, cx - inner, cy + inner, hitColor, 2.0f);
        overlay.line(cx + inner, cy + inner, cx + size, cy + size, hitColor, 2.0f);
    }

    if (cfg.visuals.damageIndicator && now < damageUntil && lastDamage > 0) {
        const float remaining = static_cast<float>((damageUntil - now) / 1100.0);
        overlay.text(fmtw("-%d", lastDamage), cx + 16.0f, cy + 10.0f,
                     withAlpha(0xFFFF5050u, std::max(0.2f, remaining)), 14.0f, false);
    }

    if (state.hasAimTarget) {
        overlay.dot(state.aimTarget.x, state.aimTarget.y, 3.0f, 0xFF35FF6Eu);
    }

    if (cfg.visuals.crosshair) {
        drawCrosshair(overlay, cfg, width, height);
    }

    if (cfg.radar.enabled) {
        drawRadar(overlay, world, cfg);
    }

    if (cfg.visuals.watermark) {
        overlay.text(L"halcyon", width - 14.0f, 10.0f, cfg.visuals.accent, 16.0f, false);
        if (!state.preset.empty()) {
            overlay.text(state.preset, width - 14.0f, 28.0f, withAlpha(cfg.visuals.accent, 0.7f),
                         12.0f, false);
        }
    }

    if (cfg.visuals.keybindList) {
        float y = height - 5.0f * 16.0f - 10.0f;
        overlay.text(L"F1  esp", 12.0f, y, withAlpha(cfg.visuals.accent, 0.85f), 12.0f, false);
        y += 16.0f;
        overlay.text(L"F2  aim", 12.0f, y, withAlpha(cfg.visuals.accent, 0.85f), 12.0f, false);
        y += 16.0f;
        overlay.text(L"F3  trigger", 12.0f, y, withAlpha(cfg.visuals.accent, 0.85f), 12.0f, false);
        y += 16.0f;
        overlay.text(L"DEL panic", 12.0f, y, withAlpha(cfg.visuals.accent, 0.85f), 12.0f, false);
        y += 16.0f;
        overlay.text(L"END quit", 12.0f, y, withAlpha(cfg.visuals.accent, 0.85f), 12.0f, false);
    }

    if (cfg.visuals.fpsCounter) {
        overlay.text(fmtw("%.0f fps", state.fps), width - 14.0f, height - 26.0f, cfg.esp.colorText,
                     12.0f, false);
    }

    if (cfg.visuals.timeCounter) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        overlay.text(fmtw("%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond), width - 14.0f,
                     height - 44.0f, cfg.esp.colorText, 12.0f, false);
    }

    overlay.text(fmtw("esp %s   aim %s   trigger %s   %zu targets   scan %.2f ms",
                      state.espOn ? "on" : "off", state.aimOn ? "on" : "off",
                      state.triggerOn ? "on" : "off", state.players, state.scanMs),
                 12.0f, 10.0f, withAlpha(cfg.esp.colorText, 0.9f), 12.0f, false);
}

}  // namespace hx
