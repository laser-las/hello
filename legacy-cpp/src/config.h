#pragma once
#include "common.h"

namespace hx {

// One offset the program has to resolve at runtime. Either a module relative
// offset ("0x1A2B3C") or a signature ("sig:48 8B 0D ? ? ? ? 48 8B 01") whose
// RIP-relative target is used.
struct OffsetSpec {
    std::string raw;
    bool pattern = false;
    uintptr_t moduleOffset = 0;
};

// The overall play style. Every preset writes a coherent set of values into the
// aim / trigger / esp / visual groups so the client can be dragged from "looks
// like a good player" all the way to "does not miss".
enum class Preset {
    Legit = 0,
    SemiLegit,
    SemiRage,
    Rage,
    Custom,
};

const char* presetName(Preset p);
const char* presetKey(Preset p);

// ---------------------------------------------------------------------------
// Aimbot
// ---------------------------------------------------------------------------
struct AimConfig {
    bool enabled = false;
    int key = VK_XBUTTON2;
    bool toggle = false;

    // Target selection.
    int hitbox = 0;  // 0 head, 1 neck, 2 chest, 3 stomach, 4 nearest to crosshair
    bool multiHitbox = false;
    bool hitHead = true;
    bool hitNeck = true;
    bool hitChest = true;
    bool hitStomach = false;
    int priority = 2;  // 0 crosshair, 1 distance, 2 fov, 3 health
    bool targetSwitch = true;
    float switchDelay = 0.05f;
    float maxDistance = 0.0f;
    bool teamCheck = true;
    bool visibleOnly = false;
    bool ignoreSmoke = false;
    bool ignoreFlashed = false;
    bool onlyScoped = false;

    // Field of view, in screen pixels from the crosshair.
    float fov = 120.0f;
    bool dynamicFov = false;
    float fovMin = 40.0f;
    float fovMax = 300.0f;
    float fovScale = 1.0f;  // applied per 1000 units of distance

    // Motion.
    float smooth = 8.0f;      // 1 = snap, higher = slower
    int smoothMode = 2;       // 0 linear, 1 exponential, 2 humanized
    float humanize = 0.15f;   // 0..1 random wobble
    float humanizeJitter = 0.35f;
    float aimStep = 0.0f;     // hard cap in pixels per tick, 0 = off
    float aimScale = 1.0f;    // mouse counts per screen pixel
    float reactionTime = 0.06f;
    float reactionJitter = 0.03f;
    float firstShotDelay = 0.0f;

    // Firing.
    bool autoFire = false;
    bool autoScope = false;
    bool autoPistol = false;
    bool autoStop = false;
    bool autoCrouch = false;

    // Recoil control.
    bool rcs = true;
    bool rcsStandalone = false;
    float rcsX = 0.85f;
    float rcsY = 0.85f;
    float rcsSmooth = 4.0f;
    bool rcsScopeOnly = false;

    // Prediction / backtrack (external approximation).
    bool prediction = false;
    float predictMs = 25.0f;
    bool backtrack = false;
    float backtrackMs = 80.0f;
    int resolverMode = 0;  // 0 off, 1 static, 2 jitter

    bool drawFov = true;
    uint32_t fovColor = 0xFFB0B0B0;
    float fovAlpha = 0.35f;
    bool silent = false;  // reserved: external client cannot write usercmds
};

// ---------------------------------------------------------------------------
// Triggerbot
// ---------------------------------------------------------------------------
struct TriggerConfig {
    bool enabled = false;
    int key = 0;
    bool toggle = false;

    float radius = 10.0f;
    float maxDistance = 0.0f;
    bool teamCheck = true;
    bool visibleOnly = false;
    bool onlyScoped = false;
    bool wallCheck = false;

    bool randomize = true;
    int delayMin = 8;
    int delayMax = 26;
    int holdMin = 8;
    int holdMax = 18;

    bool burst = false;
    int burstShots = 3;
    float burstDelay = 0.05f;
    float burstCooldown = 0.35f;
};

// ---------------------------------------------------------------------------
// Player ESP
// ---------------------------------------------------------------------------
struct EspConfig {
    bool enabled = true;
    bool teamCheck = true;
    float maxDistance = 4000.0f;
    bool visibleCheck = false;
    bool oofCheck = true;  // only draw when on screen
    float fadeStart = 3000.0f;

    bool box = true;
    int boxStyle = 0;  // 0 full, 1 corner, 2 3D
    float boxThickness = 1.6f;
    bool filledBox = false;
    uint32_t fillColor = 0x22101010;

    bool healthBar = true;
    int healthBarPos = 0;  // 0 left, 1 right, 2 top
    bool healthText = false;
    bool armorBar = false;
    bool name = true;
    bool weapon = true;
    bool distance = true;
    bool money = false;
    bool ammo = false;
    bool headDot = true;
    bool snapline = true;
    int snaplineOrigin = 0;  // 0 bottom, 1 top, 2 center
    bool skeleton = false;
    float skeletonThickness = 1.2f;
    bool flags = false;
    bool glow = false;
    bool offscreenArrows = false;
    bool soundEsp = false;
    float soundRadius = 800.0f;
    bool bomb = false;
    bool bombTimer = false;
    bool droppedWeapons = false;
    bool grenades = false;
    bool localPlayer = false;
    int textSize = 0;
    bool outlineText = true;

    uint32_t colorVisible = 0xFF35FF6E;
    uint32_t colorOccluded = 0xFFFF4040;
    uint32_t colorSnapline = 0x9035FF6E;
    uint32_t colorText = 0xFFE8E8E8;
    uint32_t colorHealth = 0xFF35FF6E;
    uint32_t colorArmor = 0xFF4DA6FF;
    uint32_t colorSkeleton = 0xFFE8E8E8;
    uint32_t colorHeadDot = 0xFFE8E8E8;
    uint32_t colorGlow = 0x8035FF6E;
};

// ---------------------------------------------------------------------------
// World / screen visuals
// ---------------------------------------------------------------------------
struct VisualsConfig {
    bool fovChanger = false;
    float fov = 90.0f;
    bool thirdPerson = false;
    float thirdPersonDist = 120.0f;
    bool noScope = false;

    bool crosshair = false;
    int crosshairStyle = 0;  // 0 cross, 1 dot, 2 circle, 3 t
    float crosshairSize = 6.0f;
    float crosshairGap = 3.0f;
    float crosshairThickness = 1.0f;
    uint32_t crosshairColor = 0xFF35FF6E;

    bool hitmarker = false;
    float hitmarkerSize = 12.0f;
    uint32_t hitmarkerColor = 0xFFFFFFFF;
    bool damageIndicator = false;

    bool spectatorList = false;
    bool watermark = true;
    bool keybindList = true;
    bool fpsCounter = false;
    bool pingCounter = false;
    bool timeCounter = false;

    bool noFlash = false;
    float flashAlpha = 0.0f;
    bool noSmoke = false;
    bool noDecals = false;
    bool nightMode = false;
    float nightAlpha = 0.45f;
    bool removeHands = false;

    uint32_t accent = 0xFF7C4DFF;
    uint32_t accentDim = 0x667C4DFF;
};

// ---------------------------------------------------------------------------
// Radar
// ---------------------------------------------------------------------------
struct RadarConfig {
    bool enabled = false;
    float size = 180.0f;
    float zoom = 1.0f;
    int posX = 20;
    int posY = 20;
    bool rotate = true;
    bool showEnemies = true;
    bool showTeam = false;
    bool showBomb = true;
    bool showDropped = false;
    float dotSize = 4.0f;
    uint32_t colorEnemy = 0xFFFF4040;
    uint32_t colorTeam = 0xFF35FF6E;
    uint32_t colorBomb = 0xFFFFAA00;
};

// ---------------------------------------------------------------------------
// Misc / movement
// ---------------------------------------------------------------------------
struct MiscConfig {
    bool bhop = false;
    int bhopKey = VK_SPACE;
    bool autoStrafe = false;
    float strafeStrength = 1.0f;
    bool jumpBug = false;
    bool edgeJump = false;
    int edgeJumpKey = 0;

    bool autoPistol = false;
    bool autoReload = false;
    bool autoAccept = false;
    bool autoDefuse = false;
    bool autoBuy = false;
    std::string autoBuyText;

    bool fakeLatency = false;
    int fakeLatencyMs = 80;

    bool chatSpam = false;
    std::string chatSpamText = "hx";
    float chatSpamDelay = 2.0f;

    bool killSay = false;
    std::string killSayText = "nice";

    bool clanTag = false;
    std::string clanTagText = "hx";

    bool antiAim = false;
    int antiAimMode = 0;
    bool knifeBot = false;
    bool autoGG = false;
    bool autoMolotov = false;
    bool preserveKillfeed = false;
};

// ---------------------------------------------------------------------------
// Root configuration
// ---------------------------------------------------------------------------
struct Config {
    // [game]
    std::wstring process = L"cs2.exe";
    std::wstring windowTitle = L"Counter-Strike 2";
    std::wstring windowClass = L"SDL_app";
    std::wstring clientModule = L"client.dll";
    std::string layout = "source2";

    // [offsets]
    OffsetSpec entityList;
    OffsetSpec localPlayer;
    OffsetSpec viewMatrix;

    int entityStride = 0x78;
    int maxEntities = 64;
    int health = 0x344;
    int team = 0x3F4;
    int lifeState = 0x34C;
    int origin = 0x1638;
    int eyeOffset = -1;
    int boneMatrix = 0x1A0;
    bool boneIndirect = false;
    int headBone = 6;
    int dormant = -1;
    int spotted = -1;

    // Optional extended offsets used by the richer ESP elements. A value < 0
    // disables the element instead of guessing.
    int name = -1;
    int weapon = -1;
    int armor = -1;
    int scoped = -1;
    int flashed = -1;
    int defusing = -1;
    int money = -1;
    int ammo = -1;
    int velocity = -1;
    int aimPunch = -1;
    int viewAngles = -1;

    // [features]
    AimConfig aim;
    TriggerConfig trigger;
    EspConfig esp;
    VisualsConfig visuals;
    RadarConfig radar;
    MiscConfig misc;

    // [keys]
    int menuKey = VK_INSERT;
    int quitKey = VK_END;
    int espToggleKey = VK_F1;
    int aimToggleKey = VK_F2;
    int triggerToggleKey = VK_F3;
    int panicKey = VK_DELETE;

    // [loop]
    int renderFps = 0;  // 0 = uncapped
    int aimHz = 500;

    Preset preset = Preset::Legit;

    bool load(const char* path);
    bool save(const char* path) const;

    // Applies a flat symbol dump produced by an offset dumper. Accepts
    // "dwEntityList = 0x1F2A3B0" and JSON-ish "m_iHealth": 344 lines and maps the
    // known names onto the fields above. Returns the number of applied entries.
    int importOffsets(const char* path);

    void applyPreset(Preset p);
    void logSummary() const;

private:
    std::unordered_map<std::string, std::string> values_;

    std::string get(const std::string& section, const std::string& key,
                    const std::string& fallback) const;
    int getInt(const std::string& section, const std::string& key, int fallback) const;
    bool getBool(const std::string& section, const std::string& key, bool fallback) const;
    float getFloat(const std::string& section, const std::string& key, float fallback) const;
    uint32_t getColor(const std::string& section, const std::string& key, uint32_t fallback) const;
    OffsetSpec getOffset(const std::string& key) const;
    int getKey(const std::string& section, const std::string& key, int fallback) const;
};

// "F2", "XBUTTON2", "0x12", "18"
int keyNameToVk(const std::string& name);
std::string vkToName(int vk);

bool parseOffsetSpec(const std::string& text, OffsetSpec& out);

}  // namespace hx
