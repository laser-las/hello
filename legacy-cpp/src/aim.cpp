#include "aim.h"

#include <limits>

namespace hx {
namespace {

inline bool keyDown(int vk) {
    if (vk <= 0) return false;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool processIsForeground(uint32_t pid) {
    const HWND foreground = GetForegroundWindow();
    if (!foreground) return false;
    DWORD owner = 0;
    GetWindowThreadProcessId(foreground, &owner);
    return owner == pid;
}

inline float screenDistSq(const Vec2& a, float x, float y) {
    const float dx = a.x - x;
    const float dy = a.y - y;
    return dx * dx + dy * dy;
}

bool isSniper(int itemDefinitionIndex) {
    switch (itemDefinitionIndex) {
        case 9:
        case 11:
        case 38:
        case 40:
            return true;
        default:
            return false;
    }
}

bool isPistol(int itemDefinitionIndex) {
    switch (itemDefinitionIndex) {
        case 1:   // deagle
        case 2:   // dual berettas
        case 3:   // five-seven
        case 4:   // glock
        case 30:  // tec-9
        case 32:  // p2000
        case 36:  // p250
        case 61:  // usp-s
        case 63:  // cz75
        case 64:  // r8
            return true;
        default:
            return false;
    }
}

// Closest screen point among the hitboxes the config enables. When `sample` is
// supplied the hit ladder is read from the backtrack history instead of the
// current snapshot.
bool bestHitboxPoint(const World& world, const Player& player, const Config& cfg, int width,
                     int height, Vec2& out, float& outDistSq, const AimSample* sample = nullptr) {
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;

    const bool predict = cfg.aim.prediction && cfg.aim.predictMs > 0.0f;
    const float lead = cfg.aim.predictMs / 1000.0f;

    float best = std::numeric_limits<float>::max();
    Vec2 bestPoint{};
    bool found = false;

    auto test = [&](int hitbox) {
        Vec3 point = sample ? sample->point(hitbox) : player.hitPoint(hitbox);
        if (predict) point = point + player.velocity * lead;

        Vec2 screen{};
        if (!worldToScreen(world.view, point, w, h, screen)) return;

        const float dist = screenDistSq(screen, cx, cy);
        if (dist < best) {
            best = dist;
            bestPoint = screen;
            found = true;
        }
    };

    if (cfg.aim.multiHitbox) {
        if (cfg.aim.hitHead) test(0);
        if (cfg.aim.hitNeck) test(1);
        if (cfg.aim.hitChest) test(2);
        if (cfg.aim.hitStomach) test(3);
    } else {
        test(std::clamp(cfg.aim.hitbox, 0, 3));
    }

    out = bestPoint;
    outDistSq = best;
    return found;
}

}  // namespace

void Aim::reset() { releaseAll(); }

void Aim::releaseAll() {
    if (leftHeld_) mouseButton(false, false);
    if (rightHeld_) mouseButton(false, true);
    releaseMovement();

    leftHeld_ = false;
    rightHeld_ = false;
    releaseAtMs_ = 0.0;
    fireAtMs_ = -1.0;
    rightReleaseAtMs_ = 0.0;
    burstCooldownUntil_ = 0.0;
    burstNextAtMs_ = 0.0;
    burstShots_ = 0;
    residualX_ = 0.0f;
    residualY_ = 0.0f;
    reactionAddress_ = 0;
    reactionUntilMs_ = 0.0;
    lockedTarget_ = 0;
    firstShotTarget_ = 0;
    firstShotAtMs_ = 0.0;
    pistolCooldownUntil_ = 0.0;
    history_.clear();
    lastPunch_ = Vec3{};
    status_ = AimStatus{};
}

void Aim::keyEvent(int vk, bool down) {
    if (vk <= 0) return;
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(vk);
    input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC));
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(input));
}

void Aim::releaseMovement() {
    if (stopKey_ != 0) {
        keyEvent(stopKey_, false);
        stopKey_ = 0;
    }
    if (crouchHeld_) {
        keyEvent(VK_CONTROL, false);
        crouchHeld_ = false;
    }
}

void Aim::updateHistory(const World& world, double now) {
    for (const Player& player : world.players) {
        std::deque<AimSample>& samples = history_[player.address];
        AimSample sample;
        sample.time = now;
        sample.head = player.head;
        sample.neck = player.neck;
        sample.chest = player.chest;
        sample.stomach = player.stomach;
        samples.push_back(sample);

        while (!samples.empty() && now - samples.front().time > 600.0) samples.pop_front();
    }

    for (auto it = history_.begin(); it != history_.end();) {
        std::deque<AimSample>& samples = it->second;
        while (!samples.empty() && now - samples.front().time > 600.0) samples.pop_front();

        if (samples.empty()) {
            it = history_.erase(it);
        } else {
            ++it;
        }
    }
}

double Aim::nowMs() const { return clock_.ms(); }

int Aim::randomBetween(int lo, int hi) {
    if (hi <= lo) return lo;
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng_);
}

float Aim::randomFloat(float lo, float hi) {
    if (hi <= lo) return lo;
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng_);
}

void Aim::moveMouse(float dx, float dy) {
    residualX_ += dx;
    residualY_ += dy;

    const int mx = static_cast<int>(residualX_);
    const int my = static_cast<int>(residualY_);
    if (mx == 0 && my == 0) return;

    residualX_ -= static_cast<float>(mx);
    residualY_ -= static_cast<float>(my);

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = mx;
    input.mi.dy = my;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(input));
}

void Aim::mouseButton(bool down, bool right) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = right ? (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP)
                             : (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
    SendInput(1, &input, sizeof(input));
}

float Aim::effectiveFov(const Config& cfg, float distance) const {
    if (!cfg.aim.dynamicFov) return cfg.aim.fov;

    float fov = cfg.aim.fov * (1.0f + cfg.aim.fovScale * (distance / 1000.0f));
    if (cfg.aim.fovMin > 0.0f) fov = std::max(fov, cfg.aim.fovMin);
    if (cfg.aim.fovMax > 0.0f) fov = std::min(fov, cfg.aim.fovMax);
    return fov;
}

// Counteracts view punch by moving the mouse the opposite way. Deltas are
// accumulated because the game reports the total punch, not per tick.
void Aim::applyRecoilControl(const World& world, const Config& cfg, int clientWidth) {
    const Vec3 punch = world.aimPunch;
    if (punch.x == lastPunch_.x && punch.y == lastPunch_.y) return;

    const float degreesPerPixel =
        std::max(1.0f, cfg.visuals.fov) / static_cast<float>(std::max(1, clientWidth));
    const float pixelsPerDegree = 1.0f / std::max(0.001f, degreesPerPixel);

    // +yaw is counter clockwise (left), so a positive punch.y is cancelled by
    // moving the mouse right; +pitch looks down, cancelled by moving up.
    const float dx = (punch.y - lastPunch_.y) * cfg.aim.rcsX * pixelsPerDegree;
    const float dy = -(punch.x - lastPunch_.x) * cfg.aim.rcsY * pixelsPerDegree;

    lastPunch_ = punch;

    const float stepX = dx / std::max(1.0f, cfg.aim.rcsSmooth);
    const float stepY = dy / std::max(1.0f, cfg.aim.rcsSmooth);
    moveMouse(stepX, stepY);
}

void Aim::tick(const World& world, const Config& cfg, int clientWidth, int clientHeight,
               uint32_t targetPid, bool aimAllowed, bool triggerAllowed) {
    status_.hasTarget = false;
    status_.firing = leftHeld_;
    status_.scoped = rightHeld_;

    const double now = nowMs();
    const double dt = std::max(0.0001, (now - lastTickMs_) / 1000.0);
    lastTickMs_ = now;

    if (leftHeld_ && now >= releaseAtMs_) {
        mouseButton(false, false);
        leftHeld_ = false;
    }
    if (rightHeld_ && now >= rightReleaseAtMs_) {
        mouseButton(false, true);
        rightHeld_ = false;
    }

    const bool focused = processIsForeground(targetPid);
    if (!focused || !world.haveLocal || clientWidth <= 0 || clientHeight <= 0) {
        releaseAll();
        return;
    }

    updateHistory(world, now);

    // Key handling: hold mode reads the key directly, toggle mode flips a latched
    // state on each press.
    auto resolveKey = [&](bool toggleMode, int vk, bool& wasDown, bool& state) -> bool {
        if (vk <= 0) return true;
        const bool down = keyDown(vk);
        if (toggleMode) {
            if (down && !wasDown) state = !state;
            wasDown = down;
            return state;
        }
        wasDown = down;
        return down;
    };

    bool aimActive = aimAllowed && cfg.aim.enabled &&
                     resolveKey(cfg.aim.toggle, cfg.aim.key, aimKeyWasDown_, aimToggleState_);
    bool triggerActive =
        triggerAllowed && cfg.trigger.enabled &&
        resolveKey(cfg.trigger.toggle, cfg.trigger.key, triggerKeyWasDown_, triggerToggleState_);

    if (aimActive && cfg.aim.onlyScoped && cfg.scoped >= 0 && !world.localScoped) aimActive = false;
    if (triggerActive && cfg.trigger.onlyScoped && cfg.scoped >= 0 && !world.localScoped) {
        triggerActive = false;
    }

    const float cx = static_cast<float>(clientWidth) * 0.5f;
    const float cy = static_cast<float>(clientHeight) * 0.5f;

    struct Candidate {
        uintptr_t address = 0;
        int index = 0;
        Vec2 point{};
        float fov = 0.0f;
        float score = 0.0f;
    };

    std::vector<Candidate> candidates;

    int triggerIndex = -1;
    float triggerDist = std::numeric_limits<float>::max();
    Vec2 triggerPoint{};

    for (size_t i = 0; i < world.players.size(); ++i) {
        const Player& p = world.players[i];
        const bool sameTeam = world.localTeam != 0 && p.team == world.localTeam;

        if (aimActive && !(cfg.aim.teamCheck && sameTeam)) {
            const bool distanceOk = cfg.aim.maxDistance <= 0.0f || p.distance <= cfg.aim.maxDistance;
            const bool visibleOk = !cfg.aim.visibleOnly || p.spotted;
            const bool flashOk = !cfg.aim.ignoreFlashed || !p.flashed;

            if (distanceOk && visibleOk && flashOk) {
                const AimSample* sample = nullptr;
                if (cfg.aim.backtrack && cfg.aim.backtrackMs > 0.0f) {
                    auto history = history_.find(p.address);
                    if (history != history_.end() && !history->second.empty()) {
                        const double wanted = now - static_cast<double>(cfg.aim.backtrackMs);
                        double bestDiff = 1.0e9;
                        for (const AimSample& entry : history->second) {
                            const double diff = std::fabs(entry.time - wanted);
                            if (diff < bestDiff) {
                                bestDiff = diff;
                                sample = &entry;
                            }
                        }
                    }
                }

                Vec2 point{};
                float distSq = std::numeric_limits<float>::max();
                if (bestHitboxPoint(world, p, cfg, clientWidth, clientHeight, point, distSq, sample)) {
                    const float fov = effectiveFov(cfg, p.distance);
                    if (distSq <= fov * fov) {
                        float score = std::sqrt(distSq);
                        if (cfg.aim.priority == 1) score = p.distance;
                        if (cfg.aim.priority == 3) score = static_cast<float>(p.health);

                        Candidate candidate;
                        candidate.address = p.address;
                        candidate.index = static_cast<int>(i);
                        candidate.point = point;
                        candidate.fov = fov;
                        candidate.score = score;
                        candidates.push_back(candidate);
                    }
                }
            }
        }

        if (triggerActive && !(cfg.trigger.teamCheck && sameTeam)) {
            const bool distanceOk =
                cfg.trigger.maxDistance <= 0.0f || p.distance <= cfg.trigger.maxDistance;
            const bool visibleOk =
                (!cfg.trigger.visibleOnly && !cfg.trigger.wallCheck) || p.spotted;

            if (distanceOk && visibleOk) {
                Vec2 point{};
                float distSq = std::numeric_limits<float>::max();
                if (bestHitboxPoint(world, p, cfg, clientWidth, clientHeight, point, distSq)) {
                    if (distSq <= cfg.trigger.radius * cfg.trigger.radius && distSq < triggerDist) {
                        triggerDist = distSq;
                        triggerIndex = static_cast<int>(i);
                        triggerPoint = point;
                    }
                }
            }
        }
    }

    int chosen = -1;
    if (aimActive && !cfg.aim.targetSwitch && lockedTarget_ != 0) {
        for (size_t i = 0; i < candidates.size(); ++i) {
            if (candidates[i].address == lockedTarget_) {
                chosen = static_cast<int>(i);
                break;
            }
        }
    }
    if (chosen < 0 && !candidates.empty()) {
        float best = std::numeric_limits<float>::max();
        for (size_t i = 0; i < candidates.size(); ++i) {
            if (candidates[i].score < best) {
                best = candidates[i].score;
                chosen = static_cast<int>(i);
            }
        }
    }

    int aimIndex = -1;
    Vec2 aimPoint{};
    float aimFov = cfg.aim.fov;
    uintptr_t aimAddress = 0;

    if (aimActive && chosen >= 0) {
        const Candidate& candidate = candidates[static_cast<size_t>(chosen)];
        aimIndex = candidate.index;
        aimPoint = candidate.point;
        aimFov = candidate.fov;
        aimAddress = candidate.address;

        // External resolver approximation: a fixed or jittered lateral offset.
        if (cfg.aim.resolverMode == 1) {
            aimPoint.x += (aimAddress & 1u) ? 4.0f : -4.0f;
        } else if (cfg.aim.resolverMode == 2) {
            aimPoint.x += randomFloat(-5.0f, 5.0f);
            aimPoint.y += randomFloat(-3.0f, 3.0f);
        }
    }

    if (aimActive && aimIndex >= 0) {
        status_.hasTarget = true;
        status_.targetIndex = aimIndex;
        status_.targetScreen = aimPoint;
        status_.fovPixels = aimFov;

        if (reactionAddress_ != aimAddress) {
            const bool hadTarget = lockedTarget_ != 0;
            reactionAddress_ = aimAddress;
            lockedTarget_ = aimAddress;

            double delay = static_cast<double>(cfg.aim.reactionTime) +
                           static_cast<double>(randomFloat(0.0f, cfg.aim.reactionJitter));
            if (hadTarget && cfg.aim.targetSwitch) {
                delay += static_cast<double>(cfg.aim.switchDelay);
            }
            reactionUntilMs_ = now + delay * 1000.0;

            firstShotTarget_ = aimAddress;
            firstShotAtMs_ = now + static_cast<double>(cfg.aim.firstShotDelay) * 1000.0;
        }

        if (now >= reactionUntilMs_) {
            float dx = aimPoint.x - cx;
            float dy = aimPoint.y - cy;

            const float smooth = std::max(1.0f, cfg.aim.smooth);
            if (cfg.aim.smoothMode == 1) {
                const float tau = 0.02f * smooth;
                const float alpha = 1.0f - std::exp(-static_cast<float>(dt) / tau);
                dx *= alpha;
                dy *= alpha;
            } else {
                dx /= smooth;
                dy /= smooth;
            }

            if (cfg.aim.humanize > 0.0f) {
                const float amount = cfg.aim.humanize * std::max(0.0f, cfg.aim.humanizeJitter);
                dx += randomFloat(-amount, amount) * (std::fabs(dx) + 1.0f);
                dy += randomFloat(-amount, amount) * (std::fabs(dy) + 1.0f);
            }

            float length = std::sqrt(dx * dx + dy * dy);
            if (cfg.aim.aimStep > 0.0f && length > cfg.aim.aimStep) {
                const float k = cfg.aim.aimStep / length;
                dx *= k;
                dy *= k;
                length = cfg.aim.aimStep;
            }

            if (length > 0.3f) {
                moveMouse(dx * cfg.aim.aimScale, dy * cfg.aim.aimScale);
            }
        }
    } else {
        reactionAddress_ = 0;
        if (cfg.aim.targetSwitch) lockedTarget_ = 0;
    }

    // ------------------------------------------------------------------
    // Movement assists.
    // ------------------------------------------------------------------
    const bool wantStop = cfg.aim.autoStop && aimActive && status_.hasTarget &&
                          now >= reactionUntilMs_;

    if (wantStop) {
        const float yaw = world.viewAngles.y * kPi / 180.0f;
        const float rightX = std::sin(yaw);
        const float rightY = -std::cos(yaw);
        const float dot = world.localVelocity.x * rightX + world.localVelocity.y * rightY;

        int key = 0;
        if (dot > 15.0f) {
            key = 'A';
        } else if (dot < -15.0f) {
            key = 'D';
        }

        if (key != stopKey_) {
            if (stopKey_ != 0) keyEvent(stopKey_, false);
            if (key != 0) keyEvent(key, true);
            stopKey_ = key;
        }
    } else if (stopKey_ != 0) {
        keyEvent(stopKey_, false);
        stopKey_ = 0;
    }

    const bool wantCrouch = cfg.aim.autoCrouch && aimActive && status_.hasTarget;
    if (wantCrouch != crouchHeld_) {
        keyEvent(VK_CONTROL, wantCrouch);
        crouchHeld_ = wantCrouch;
    }

    const bool autoPistol =
        (cfg.aim.autoPistol || cfg.misc.autoPistol) && isPistol(world.localWeapon);

    const bool rcsActive =
        cfg.aim.rcs && (leftHeld_ || cfg.aim.rcsStandalone) &&
        (!cfg.aim.rcsScopeOnly || world.localScoped) && world.aimPunch.x != 0.0f;
    if (rcsActive) {
        applyRecoilControl(world, cfg, clientWidth);
    } else {
        lastPunch_ = world.aimPunch;
    }

    if (cfg.aim.autoScope && cfg.scoped >= 0 && world.haveLocal && !world.localScoped &&
        isSniper(world.localWeapon) && status_.hasTarget && !rightHeld_ &&
        now >= scopeCooldownUntil_) {
        mouseButton(true, true);
        rightHeld_ = true;
        rightReleaseAtMs_ = now + 40.0;
        scopeCooldownUntil_ = now + 350.0;
    }

    bool wantFire = false;
    if (triggerActive && triggerIndex >= 0 && now >= burstCooldownUntil_ &&
        now >= burstNextAtMs_) {
        wantFire = true;
        status_.targetIndex = triggerIndex;
        status_.targetScreen = triggerPoint;
    }
    if (cfg.aim.autoFire && aimActive && status_.hasTarget && now >= reactionUntilMs_ &&
        now >= firstShotAtMs_) {
        const float distSq = screenDistSq(aimPoint, cx, cy);
        if (distSq <= 9.0f) wantFire = true;
    }

    if (autoPistol && now < pistolCooldownUntil_) wantFire = false;

    if (wantFire) {
        if (!leftHeld_) {
            if (fireAtMs_ < 0.0) {
                if (cfg.trigger.randomize) {
                    fireAtMs_ =
                        now + static_cast<double>(randomBetween(cfg.trigger.delayMin, cfg.trigger.delayMax));
                } else {
                    fireAtMs_ = now;
                }
            }

            if (now >= fireAtMs_) {
                const int hold = cfg.trigger.randomize
                                     ? randomBetween(cfg.trigger.holdMin, cfg.trigger.holdMax)
                                     : cfg.trigger.holdMin;

                mouseButton(true, false);
                leftHeld_ = true;
                releaseAtMs_ = now + static_cast<double>(std::max(1, hold));
                fireAtMs_ = -1.0;
                ++burstShots_;

                if (autoPistol) {
                    pistolCooldownUntil_ =
                        now + static_cast<double>(std::max(80, hold)) + 50.0;
                }

                if (cfg.trigger.burst && burstShots_ >= std::max(1, cfg.trigger.burstShots)) {
                    burstShots_ = 0;
                    burstCooldownUntil_ =
                        now + static_cast<double>(cfg.trigger.burstCooldown) * 1000.0;
                    burstNextAtMs_ = 0.0;
                } else if (cfg.trigger.burst) {
                    burstNextAtMs_ =
                        now + static_cast<double>(cfg.trigger.burstDelay) * 1000.0;
                }
            }
        }
    } else if (!leftHeld_) {
        fireAtMs_ = -1.0;
        burstShots_ = 0;
        burstNextAtMs_ = 0.0;
    }

    status_.firing = leftHeld_;
    status_.scoped = rightHeld_;
}

}  // namespace hx
