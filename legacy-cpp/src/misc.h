#pragma once
#include "common.h"
#include "config.h"
#include "game.h"

#include <deque>
#include <unordered_map>

namespace hx {

// Movement and chat automation that does not depend on the aim snapshot path:
// bunny hop, auto strafe, auto reload, chat spam / kill say and a small knife
// bot. All input goes through SendInput and is only sent while the game owns
// the foreground window.
class Misc {
public:
    void reset();
    void tick(const World& world, const Config& cfg, uint32_t targetPid);

private:
    bool foreground(uint32_t targetPid) const;
    void keyEvent(int vk, bool down);
    void tap(int vk);
    void mouseButton(bool down);
    void typeText(const std::string& text);
    void queueChat(const std::string& text);
    void releaseMovement();
    void updateChat(const Config& cfg, uint32_t targetPid, double now);

    Clock clock_;
    double now_ = 0.0;

    double bhopDownAt_ = 0.0;
    double bhopNextAt_ = 0.0;
    int strafeKey_ = 0;
    double reloadNextAt_ = 0.0;

    bool knifeHeld_ = false;
    double knifeReleaseAt_ = 0.0;
    double knifeNextAt_ = 0.0;

    std::deque<std::string> chatQueue_;
    int chatStage_ = 0;
    double chatStageAt_ = 0.0;
    std::string chatCurrent_;
    double chatNextAt_ = 0.0;

    std::unordered_map<uintptr_t, double> lastSeen_;
};

}  // namespace hx
