#include "game.h"

namespace hx {

const char* weaponName(int id) {
    switch (id) {
        case 1: return "deagle";
        case 2: return "elite";
        case 3: return "five-seven";
        case 4: return "glock";
        case 7: return "ak-47";
        case 8: return "aug";
        case 9: return "awp";
        case 10: return "famas";
        case 11: return "g3sg1";
        case 13: return "galil";
        case 14: return "m249";
        case 16: return "m4a4";
        case 17: return "mac-10";
        case 19: return "p90";
        case 23: return "mp5";
        case 24: return "ump";
        case 25: return "xm1014";
        case 26: return "bizon";
        case 27: return "mag-7";
        case 28: return "negev";
        case 29: return "sawed-off";
        case 30: return "tec-9";
        case 31: return "zeus";
        case 32: return "p2000";
        case 33: return "mp7";
        case 34: return "mp9";
        case 35: return "nova";
        case 36: return "p250";
        case 38: return "scar-20";
        case 39: return "sg553";
        case 40: return "ssg08";
        case 60: return "m4a1-s";
        case 61: return "usp-s";
        case 63: return "cz75";
        case 64: return "r8";
        default: break;
    }
    return "";
}

Vec3 Player::hitPoint(int hitbox) const {
    switch (hitbox) {
        case 0: return head;
        case 1: return neck;
        case 2: return chest;
        case 3: return stomach;
        default: break;
    }
    return head;
}

uintptr_t Game::resolveOne(const OffsetSpec& spec, const char* name) const {
    const ModuleInfo* mod = proc_->module(cfg_->clientModule);
    if (!mod) {
        logf("offsets: %s missing - module %s not found", name, narrow(cfg_->clientModule).c_str());
        return 0;
    }

    if (spec.pattern) {
        int matches = 0;
        const uintptr_t hit = const_cast<Process*>(proc_)->scan(cfg_->clientModule, spec.raw, &matches);
        if (!hit) {
            logf("offsets: %s signature found 0 matches", name);
            return 0;
        }

        uintptr_t target = decodeRipRelative(*proc_, hit);
        if (!target) target = hit;

        if (matches > 1) {
            logf("offsets: %s signature matched %d times, using first at %p (resolved %p)", name, matches,
                 reinterpret_cast<void*>(hit), reinterpret_cast<void*>(target));
        } else {
            logf("offsets: %s signature resolved to %p", name, reinterpret_cast<void*>(target));
        }
        return target;
    }

    if (spec.moduleOffset == 0) {
        logf("offsets: %s not configured", name);
        return 0;
    }

    const uintptr_t absolute = mod->base + spec.moduleOffset;
    logf("offsets: %s = %p (module+0x%llX)", name, reinterpret_cast<void*>(absolute),
         static_cast<unsigned long long>(spec.moduleOffset));
    return absolute;
}

ResolvedOffsets Game::resolve() {
    resolved_ = ResolvedOffsets{};
    resolved_.entityList = resolveOne(cfg_->entityList, "entity_list");
    resolved_.localPlayer = resolveOne(cfg_->localPlayer, "local_player");
    resolved_.viewMatrix = resolveOne(cfg_->viewMatrix, "view_matrix");
    return resolved_;
}

uintptr_t Game::entityAddress(uintptr_t listBase, int index) const {
    if (layout_ == Layout::Source1) {
        return proc_->readPtr(listBase + 0x10 * static_cast<uintptr_t>(index));
    }

    // Source 2: chunk table of 512 entries, each chunk a fixed stride array.
    const uintptr_t chunk = proc_->readPtr(listBase + 0x10 + 8 * static_cast<uintptr_t>(index >> 9));
    if (!chunk) return 0;
    return proc_->readPtr(chunk + static_cast<uintptr_t>(cfg_->entityStride) * (index & 0x1FF));
}

bool Game::readBone(uintptr_t entity, int bone, Vec3& out) const {
    uintptr_t base = entity + static_cast<uintptr_t>(cfg_->boneMatrix);
    if (cfg_->boneIndirect) {
        base = proc_->readPtr(base);
        if (!base) return false;
    }

    // 3x4 bone matrix, translation column is the 12th/8th/4th float.
    float matrix[12]{};
    const uintptr_t addr = base + static_cast<uintptr_t>(bone) * 32u;
    if (!proc_->read(addr, matrix, sizeof(matrix))) return false;

    out = Vec3{matrix[3], matrix[7], matrix[11]};
    return finite3(out);
}

bool Game::update(World& world) {
    Clock clock;
    world.players.clear();
    world.haveLocal = false;

    if (!resolved_.valid()) return true;

    if (!proc_->read(resolved_.viewMatrix, world.view)) return false;

    const uintptr_t localPawn = proc_->readPtr(resolved_.localPlayer);
    if (localPawn) {
        world.localOrigin = proc_->readVec3(localPawn + static_cast<uintptr_t>(cfg_->origin));
        world.localTeam = proc_->readOr<int>(localPawn + static_cast<uintptr_t>(cfg_->team), 0);
        world.localHealth = proc_->readOr<int>(localPawn + static_cast<uintptr_t>(cfg_->health), 0);
        world.haveLocal = finite3(world.localOrigin);

        world.localEye = world.localOrigin;
        if (cfg_->eyeOffset >= 0) {
            const Vec3 offset = proc_->readVec3(localPawn + static_cast<uintptr_t>(cfg_->eyeOffset));
            if (finite3(offset)) world.localEye = world.localOrigin + offset;
        }
        if (cfg_->velocity >= 0) {
            world.localVelocity = proc_->readVec3(localPawn + static_cast<uintptr_t>(cfg_->velocity));
        }
        if (cfg_->ammo >= 0) {
            world.localAmmo = proc_->readOr<int>(localPawn + static_cast<uintptr_t>(cfg_->ammo), 0);
        }
        if (cfg_->weapon >= 0) {
            world.localWeapon = proc_->readOr<int>(localPawn + static_cast<uintptr_t>(cfg_->weapon), 0);
        }
        if (cfg_->aimPunch >= 0) {
            world.aimPunch = proc_->readVec3(localPawn + static_cast<uintptr_t>(cfg_->aimPunch));
        }
        if (cfg_->viewAngles >= 0) {
            world.viewAngles = proc_->readVec3(localPawn + static_cast<uintptr_t>(cfg_->viewAngles));
        }
        if (cfg_->scoped >= 0) {
            world.localScoped =
                proc_->readOr<uint8_t>(localPawn + static_cast<uintptr_t>(cfg_->scoped), 0) != 0;
        }
        if (cfg_->flashed >= 0) {
            world.localFlashed =
                proc_->readOr<float>(localPawn + static_cast<uintptr_t>(cfg_->flashed), 0.0f) > 0.0f;
        }
    }

    const uintptr_t listBase = proc_->readPtr(resolved_.entityList);
    if (!listBase) {
        world.scanMs = static_cast<float>(clock.ms());
        return true;
    }

    for (int i = 1; i <= cfg_->maxEntities; ++i) {
        const uintptr_t entity = entityAddress(listBase, i);
        if (!entity || entity == localPawn) continue;

        const int health = proc_->readOr<int>(entity + static_cast<uintptr_t>(cfg_->health), 0);
        if (health <= 0 || health > 1000) continue;

        const uint8_t lifeState =
            proc_->readOr<uint8_t>(entity + static_cast<uintptr_t>(cfg_->lifeState), 0);
        if (lifeState != 0) continue;

        if (cfg_->dormant > 0 && proc_->readOr<int>(entity + static_cast<uintptr_t>(cfg_->dormant), 1)) {
            continue;
        }

        Player p;
        p.address = entity;
        p.index = i;
        p.health = health;
        p.team = proc_->readOr<int>(entity + static_cast<uintptr_t>(cfg_->team), 0);
        p.spotted = cfg_->spotted > 0 &&
                    proc_->readOr<uint8_t>(entity + static_cast<uintptr_t>(cfg_->spotted), 0) != 0;
        p.origin = proc_->readVec3(entity + static_cast<uintptr_t>(cfg_->origin));
        if (!finite3(p.origin)) continue;

        if (cfg_->armor >= 0) {
            p.armor = proc_->readOr<int>(entity + static_cast<uintptr_t>(cfg_->armor), 0);
        }
        if (cfg_->weapon >= 0) {
            p.weapon = proc_->readOr<int>(entity + static_cast<uintptr_t>(cfg_->weapon), 0);
        }
        if (cfg_->money >= 0) {
            p.money = proc_->readOr<int>(entity + static_cast<uintptr_t>(cfg_->money), 0);
        }
        if (cfg_->ammo >= 0) {
            p.ammo = proc_->readOr<int>(entity + static_cast<uintptr_t>(cfg_->ammo), 0);
        }
        if (cfg_->velocity >= 0) {
            p.velocity = proc_->readVec3(entity + static_cast<uintptr_t>(cfg_->velocity));
        }
        if (cfg_->scoped >= 0) {
            p.scoped = proc_->readOr<uint8_t>(entity + static_cast<uintptr_t>(cfg_->scoped), 0) != 0;
        }
        if (cfg_->flashed >= 0) {
            p.flashed = proc_->readOr<float>(entity + static_cast<uintptr_t>(cfg_->flashed), 0.0f) > 0.0f;
        }
        if (cfg_->defusing >= 0) {
            p.defusing = proc_->readOr<uint8_t>(entity + static_cast<uintptr_t>(cfg_->defusing), 0) != 0;
        }
        if (cfg_->name >= 0) {
            char buffer[64]{};
            if (proc_->read(entity + static_cast<uintptr_t>(cfg_->name), buffer, sizeof(buffer) - 1)) {
                buffer[sizeof(buffer) - 1] = '\0';
                p.name.assign(buffer);
                for (char& c : p.name) {
                    if (static_cast<unsigned char>(c) < 0x20 || c == '\x7f') c = ' ';
                }
            }
        }

        p.eye = p.origin;
        if (cfg_->eyeOffset >= 0) {
            const Vec3 offset = proc_->readVec3(entity + static_cast<uintptr_t>(cfg_->eyeOffset));
            if (finite3(offset)) p.eye = p.origin + offset;
        }

        if (!readBone(entity, cfg_->headBone, p.head)) {
            p.head = p.origin + Vec3{0.0f, 0.0f, 72.0f};
        }

        // Derive the rest of the hit ladder from the measured head height so the
        // values stay sane for crouched and jumping players.
        float headHeight = p.head.z - p.origin.z;
        if (headHeight < 30.0f || headHeight > 100.0f) headHeight = 72.0f;
        p.neck = p.origin + Vec3{0.0f, 0.0f, headHeight * 0.90f};
        p.chest = p.origin + Vec3{0.0f, 0.0f, headHeight * 0.76f};
        p.stomach = p.origin + Vec3{0.0f, 0.0f, headHeight * 0.60f};

        if (world.haveLocal) {
            const Vec3 delta = p.origin - world.localOrigin;
            p.distance = delta.length();
        }

        world.players.push_back(std::move(p));
    }

    world.scanMs = static_cast<float>(clock.ms());
    return true;
}

}  // namespace hx
