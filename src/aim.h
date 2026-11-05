#pragma once
#include "common.h"
#include "config.h"
#include "game.h"

#include <deque>
#include <random>
#include <unordered_map>

namespace hx {

struct AimStatus {
    bool hasTarget = false;
    bool firing = false;
    bool scoped = false;
    int targetIndex = 0;
    Vec2 targetScreen{};
    float fovPixels = 0.0f;
};

// One sampled past position per player, used by the external backtrack
// approximation.
struct AimSample {
    double time = 0.0;
    Vec3 head{};
    Vec3 neck{};
    Vec3 chest{};
    Vec3 stomach{};

    Vec3 point(int hitbox) const {
        switch (hitbox) {
            case 0: return head;
            case 1: return neck;
            case 2: return chest;
            case 3: return stomach;
            default: break;
        }
        return head;
    }
};

// Turns the current World snapshot into mouse input: movement for the aimbot,
// clicks for the triggerbot and a scope click for auto scope. Everything goes
// through SendInput so the game receives it as ordinary raw input.
class Aim {
public:
    void reset();
    void tick(const World& world, const Config& cfg, int clientWidth, int clientHeight,
              uint32_t targetPid, bool aimAllowed, bool triggerAllowed);

    const AimStatus& status() const { return status_; }

private:
    void releaseAll();
    void moveMouse(float dx, float dy);
    void mouseButton(bool down, bool right);
    void keyEvent(int vk, bool down);
    void releaseMovement();
    void applyRecoilControl(const World& world, const Config& cfg, int clientWidth);
    void updateHistory(const World& world, double now);
    float effectiveFov(const Config& cfg, float distance) const;
    double nowMs() const;
    int randomBetween(int lo, int hi);
    float randomFloat(float lo, float hi);

    AimStatus status_{};
    float residualX_ = 0.0f;
    float residualY_ = 0.0f;

    bool leftHeld_ = false;
    bool rightHeld_ = false;
    double releaseAtMs_ = 0.0;
    double fireAtMs_ = -1.0;
    double rightReleaseAtMs_ = 0.0;
    double burstCooldownUntil_ = 0.0;
    double burstNextAtMs_ = 0.0;
    double scopeCooldownUntil_ = 0.0;
    int burstShots_ = 0;

    double reactionUntilMs_ = 0.0;
    uintptr_t reactionAddress_ = 0;
    double lastTickMs_ = 0.0;

    bool aimToggleState_ = false;
    bool triggerToggleState_ = false;
    bool aimKeyWasDown_ = false;
    bool triggerKeyWasDown_ = false;

    // Target locking / first shot.
    uintptr_t lockedTarget_ = 0;
    uintptr_t firstShotTarget_ = 0;
    double firstShotAtMs_ = 0.0;

    // Movement assists.
    int stopKey_ = 0;
    bool crouchHeld_ = false;
    double pistolCooldownUntil_ = 0.0;

    std::unordered_map<uintptr_t, std::deque<AimSample>> history_;

    Vec3 lastPunch_{};
    Clock clock_;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace hx
