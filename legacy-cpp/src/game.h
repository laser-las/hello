#pragma once
#include "common.h"
#include "config.h"
#include "mem.h"

namespace hx {

struct ResolvedOffsets {
    uintptr_t entityList = 0;
    uintptr_t localPlayer = 0;
    uintptr_t viewMatrix = 0;

    bool valid() const { return entityList != 0 && localPlayer != 0 && viewMatrix != 0; }
};

// One drawable enemy as it existed at the moment of the snapshot.
struct Player {
    uintptr_t address = 0;
    int index = 0;
    int health = 0;
    int armor = 0;
    int team = 0;
    int weapon = 0;
    int money = 0;
    int ammo = 0;
    bool spotted = false;
    bool scoped = false;
    bool defusing = false;
    bool flashed = false;
    std::string name;

    Vec3 origin;
    Vec3 eye;
    Vec3 head;
    Vec3 neck;
    Vec3 chest;
    Vec3 stomach;
    Vec3 velocity;
    float distance = 0.0f;

    // Height ladder used by the aimbot hitbox resolver.
    Vec3 hitPoint(int hitbox) const;
};

struct World {
    Mat4 view{};
    Vec3 localOrigin{};
    Vec3 localEye{};
    Vec3 localVelocity{};
    Vec3 aimPunch{};
    Vec3 viewAngles{};
    int localTeam = 0;
    int localHealth = 0;
    int localAmmo = 0;
    int localWeapon = 0;
    bool haveLocal = false;
    bool localScoped = false;
    bool localFlashed = false;
    std::vector<Player> players;
    float scanMs = 0.0f;
};

enum class Layout { Source1, Source2 };

const char* weaponName(int itemDefinitionIndex);

// Reads the entity list out of the target's memory and shapes it into a World.
class Game {
public:
    bool init(const Process& proc, const Config& cfg, Layout layout) {
        proc_ = &proc;
        cfg_ = &cfg;
        layout_ = layout;
        return true;
    }

    // Turns the configured offset specs into absolute addresses.
    ResolvedOffsets resolve();

    // Fills `world` from memory. Returns false when the process is gone.
    bool update(World& world);

private:
    uintptr_t resolveOne(const OffsetSpec& spec, const char* name) const;
    uintptr_t entityAddress(uintptr_t listBase, int index) const;
    bool readBone(uintptr_t entity, int bone, Vec3& out) const;

    const Process* proc_ = nullptr;
    const Config* cfg_ = nullptr;
    Layout layout_ = Layout::Source2;
    ResolvedOffsets resolved_{};
};

}  // namespace hx
