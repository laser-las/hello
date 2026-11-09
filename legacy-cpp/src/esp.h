#pragma once
#include "common.h"
#include "config.h"
#include "game.h"
#include "overlay.h"

namespace hx {

struct EspState {
    bool espOn = true;
    bool aimOn = true;
    bool triggerOn = true;
    bool hasAimTarget = false;
    Vec2 aimTarget{};
    float fps = 0.0f;
    float scanMs = 0.0f;
    size_t players = 0;
    std::wstring preset;
};

// Draws one complete overlay frame: player ESP, radar, crosshair and the HUD
// counters. Nothing here touches the game, it only reads the World snapshot.
void drawEsp(Overlay& overlay, const World& world, const Config& cfg, const EspState& state);

}  // namespace hx
