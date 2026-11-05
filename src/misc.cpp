#include "misc.h"

#include <cctype>
#include <cstring>

namespace hx {
namespace {

bool keyDown(int vk) {
    if (vk <= 0) return false;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool isKnife(int weapon) { return weapon == 42 || weapon == 59; }

bool isGun(int weapon) {
    if (weapon <= 0) return false;
    if (isKnife(weapon)) return false;
    if (weapon == 31) return false;              // zeus
    if (weapon >= 43 && weapon <= 49) return false;  // grenades + c4
    return true;
}

}  // namespace

bool Misc::foreground(uint32_t targetPid) const {
    const HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) return false;
    DWORD owner = 0;
    GetWindowThreadProcessId(foregroundWindow, &owner);
    return owner == targetPid;
}

void Misc::keyEvent(int vk, bool down) {
    if (vk <= 0) return;
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(vk);
    input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC));
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(input));
}

void Misc::tap(int vk) {
    keyEvent(vk, true);
    keyEvent(vk, false);
}

void Misc::mouseButton(bool down) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(input));
}

void Misc::typeText(const std::string& text) {
    const std::wstring wide = widen(text);
    if (wide.empty()) return;

    std::vector<INPUT> inputs;
    inputs.reserve(wide.size() * 2);

    for (wchar_t c : wide) {
        INPUT down{};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = 0;
        down.ki.wScan = static_cast<WORD>(c);
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(down);

        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }

    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

void Misc::queueChat(const std::string& text) {
    if (text.empty()) return;
    if (chatQueue_.size() >= 8) return;
    chatQueue_.push_back(text);
}

void Misc::updateChat(const Config& cfg, uint32_t targetPid, double now) {
    (void)cfg;

    if (chatStage_ > 0 && !foreground(targetPid)) {
        chatStage_ = 0;
        chatCurrent_.clear();
    }

    if (chatStage_ == 0 && !chatQueue_.empty() && foreground(targetPid)) {
        chatCurrent_ = chatQueue_.front();
        chatQueue_.pop_front();
        chatStage_ = 1;
        chatStageAt_ = now;
    }

    if (chatStage_ == 0) return;
    if (now < chatStageAt_) return;

    switch (chatStage_) {
        case 1:
            tap('y');
            chatStage_ = 2;
            chatStageAt_ = now + 70.0;
            break;
        case 2:
            typeText(chatCurrent_);
            chatStage_ = 3;
            chatStageAt_ = now + 60.0;
            break;
        case 3:
            tap(VK_RETURN);
            chatStage_ = 0;
            chatCurrent_.clear();
            chatStageAt_ = now + 250.0;
            break;
        default:
            chatStage_ = 0;
            break;
    }
}

void Misc::releaseMovement() {
    if (bhopDownAt_ > 0.0) {
        keyEvent(VK_SPACE, false);
        bhopDownAt_ = 0.0;
    }
    if (strafeKey_ != 0) {
        keyEvent(strafeKey_, false);
        strafeKey_ = 0;
    }
    if (knifeHeld_) {
        mouseButton(false);
        knifeHeld_ = false;
    }
}

void Misc::reset() {
    releaseMovement();
    chatQueue_.clear();
    chatCurrent_.clear();
    chatStage_ = 0;
    lastSeen_.clear();
}

void Misc::tick(const World& world, const Config& cfg, uint32_t targetPid) {
    const double now = clock_.ms();
    now_ = now;

    const bool focused = foreground(targetPid);

    if (!focused) {
        releaseMovement();
        return;
    }

    // ------------------------------------------------------------------
    // Bunny hop: pulse the jump key while the bind is held so every landing
    // receives a fresh key-down edge.
    // ------------------------------------------------------------------
    const bool wantBhop = cfg.misc.bhop && cfg.misc.bhopKey > 0 && keyDown(cfg.misc.bhopKey);
    if (wantBhop) {
        if (bhopDownAt_ == 0.0 && now >= bhopNextAt_) {
            keyEvent(VK_SPACE, true);
            bhopDownAt_ = now;
        } else if (bhopDownAt_ > 0.0 && now - bhopDownAt_ >= 10.0) {
            keyEvent(VK_SPACE, false);
            bhopDownAt_ = 0.0;
            bhopNextAt_ = now + 12.0;
        }
    } else if (bhopDownAt_ > 0.0) {
        keyEvent(VK_SPACE, false);
        bhopDownAt_ = 0.0;
    }

    // ------------------------------------------------------------------
    // Auto strafe: while the local player is airborne, hold the strafe key
    // that curves the velocity in the direction it is already travelling.
    // ------------------------------------------------------------------
    const float speed2d = std::sqrt(world.localVelocity.x * world.localVelocity.x +
                                    world.localVelocity.y * world.localVelocity.y);
    const bool airborne = std::fabs(world.localVelocity.z) > 1.0f;
    int desiredStrafe = 0;

    if (cfg.misc.autoStrafe && world.haveLocal && airborne && speed2d > 30.0f) {
        const float yaw = world.viewAngles.y * kPi / 180.0f;
        const float rightX = std::sin(yaw);
        const float rightY = -std::cos(yaw);
        const float dot = world.localVelocity.x * rightX + world.localVelocity.y * rightY;

        desiredStrafe = dot > 0.0f ? 'A' : 'D';

        if (cfg.misc.strafeStrength < 0.5f) {
            const int phase = static_cast<int>(now / 40.0);
            if ((phase & 1) != 0) desiredStrafe = 0;
        }
    }

    if (desiredStrafe != strafeKey_) {
        if (strafeKey_ != 0) keyEvent(strafeKey_, false);
        if (desiredStrafe != 0) keyEvent(desiredStrafe, true);
        strafeKey_ = desiredStrafe;
    }

    // ------------------------------------------------------------------
    // Auto reload: m_ammo has to be configured for this to be meaningful.
    // ------------------------------------------------------------------
    if (cfg.misc.autoReload && cfg.ammo >= 0 && world.haveLocal && world.localAmmo == 0 &&
        isGun(world.localWeapon) && now >= reloadNextAt_) {
        tap('R');
        reloadNextAt_ = now + 450.0;
    }

    // ------------------------------------------------------------------
    // Knife bot: tap attack when an enemy is inside knife range.
    // ------------------------------------------------------------------
    if (knifeHeld_ && now >= knifeReleaseAt_) {
        mouseButton(false);
        knifeHeld_ = false;
    }

    if (cfg.misc.knifeBot && isKnife(world.localWeapon) && now >= knifeNextAt_) {
        bool inRange = false;
        for (const Player& player : world.players) {
            if (world.localTeam != 0 && player.team == world.localTeam) continue;
            if (player.distance <= 75.0f) {
                inRange = true;
                break;
            }
        }

        if (inRange) {
            mouseButton(true);
            knifeHeld_ = true;
            knifeReleaseAt_ = now + 110.0;
            knifeNextAt_ = now + 260.0;
        }
    }

    // ------------------------------------------------------------------
    // Chat spam and kill say.
    // ------------------------------------------------------------------
    if (cfg.misc.chatSpam && !cfg.misc.chatSpamText.empty() && now >= chatNextAt_ &&
        chatQueue_.empty() && chatStage_ == 0) {
        queueChat(cfg.misc.chatSpamText);
        chatNextAt_ = now + static_cast<double>(std::max(0.5f, cfg.misc.chatSpamDelay)) * 1000.0;
    }

    // Approximate kill detection: an entity that was alive, in range, and has
    // now been gone for more than a second is treated as a kill. Dormant
    // players can produce a false positive, which is why the feature is off
    // by default.
    if (cfg.misc.killSay && !cfg.misc.killSayText.empty()) {
        for (const Player& player : world.players) {
            lastSeen_[player.address] = now;
        }

        for (auto it = lastSeen_.begin(); it != lastSeen_.end();) {
            if (now - it->second > 1100.0) {
                queueChat(cfg.misc.killSayText);
                it = lastSeen_.erase(it);
            } else {
                ++it;
            }
        }
    } else {
        lastSeen_.clear();
    }

    updateChat(cfg, targetPid, now);
}

}  // namespace hx
