#include "config.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace hx {
namespace {

std::string trim(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

bool startsWith(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::string boolText(bool v) { return v ? "1" : "0"; }

std::string floatText(float v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.4f", v);
    return buf;
}

std::string colorText(uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08X", v);
    return buf;
}

}  // namespace

const char* presetName(Preset p) {
    switch (p) {
        case Preset::Legit: return "Legit";
        case Preset::SemiLegit: return "Semi-Legit";
        case Preset::SemiRage: return "Semi-Rage";
        case Preset::Rage: return "Rage";
        case Preset::Custom: return "Custom";
    }
    return "Custom";
}

const char* presetKey(Preset p) {
    switch (p) {
        case Preset::Legit: return "legit";
        case Preset::SemiLegit: return "semi-legit";
        case Preset::SemiRage: return "semi-rage";
        case Preset::Rage: return "rage";
        case Preset::Custom: return "custom";
    }
    return "custom";
}

int keyNameToVk(const std::string& raw) {
    const std::string name = upper(trim(raw));
    if (name.empty()) return 0;

    if (name.size() >= 2 && name[0] == 'F') {
        const int n = std::atoi(name.c_str() + 1);
        if (n >= 1 && n <= 24) return VK_F1 + n - 1;
    }

    struct Alias {
        const char* name;
        int vk;
    };
    static const Alias aliases[] = {
        {"LBUTTON", VK_LBUTTON},   {"RBUTTON", VK_RBUTTON},   {"MBUTTON", VK_MBUTTON},
        {"XBUTTON1", VK_XBUTTON1}, {"MOUSE4", VK_XBUTTON1},   {"XBUTTON2", VK_XBUTTON2},
        {"MOUSE5", VK_XBUTTON2},   {"SHIFT", VK_SHIFT},       {"LSHIFT", VK_LSHIFT},
        {"RSHIFT", VK_RSHIFT},     {"CTRL", VK_CONTROL},      {"CONTROL", VK_CONTROL},
        {"LCTRL", VK_LCONTROL},    {"RCTRL", VK_RCONTROL},    {"ALT", VK_MENU},
        {"LMENU", VK_LMENU},       {"RMENU", VK_RMENU},       {"SPACE", VK_SPACE},
        {"TAB", VK_TAB},           {"CAPS", VK_CAPITAL},      {"CAPSLOCK", VK_CAPITAL},
        {"ENTER", VK_RETURN},      {"RETURN", VK_RETURN},     {"ESC", VK_ESCAPE},
        {"ESCAPE", VK_ESCAPE},     {"END", VK_END},           {"HOME", VK_HOME},
        {"INSERT", VK_INSERT},     {"DELETE", VK_DELETE},     {"DEL", VK_DELETE},
        {"PGUP", VK_PRIOR},        {"PGDN", VK_NEXT},         {"UP", VK_UP},
        {"DOWN", VK_DOWN},         {"LEFT", VK_LEFT},         {"RIGHT", VK_RIGHT},
        {"NUMLOCK", VK_NUMLOCK},   {"SCROLL", VK_SCROLL},     {"PAUSE", VK_PAUSE},
    };

    for (const auto& a : aliases) {
        if (name == a.name) return a.vk;
    }

    if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z') return name[0];
    if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') return name[0];

    char* end = nullptr;
    const long value = std::strtol(name.c_str(), &end, 0);
    if (end != name.c_str()) return static_cast<int>(value);
    return 0;
}

std::string vkToName(int vk) {
    if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, static_cast<char>(vk));

    switch (vk) {
        case 0: return "NONE";
        case VK_XBUTTON1: return "XBUTTON1";
        case VK_XBUTTON2: return "XBUTTON2";
        case VK_LBUTTON: return "LBUTTON";
        case VK_RBUTTON: return "RBUTTON";
        case VK_MBUTTON: return "MBUTTON";
        case VK_SHIFT: return "SHIFT";
        case VK_CONTROL: return "CTRL";
        case VK_MENU: return "ALT";
        case VK_SPACE: return "SPACE";
        case VK_TAB: return "TAB";
        case VK_END: return "END";
        case VK_INSERT: return "INSERT";
        case VK_DELETE: return "DELETE";
        default: break;
    }
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%02X", vk);
    return buffer;
}

bool parseOffsetSpec(const std::string& text, OffsetSpec& out) {
    out = OffsetSpec{};
    out.raw = trim(text);

    if (out.raw.empty()) return false;

    if (startsWith(out.raw, "sig:")) {
        out.pattern = true;
        out.raw = trim(out.raw.substr(4));
        return !out.raw.empty();
    }

    const char* begin = out.raw.c_str();
    char* end = nullptr;
    const unsigned long long value = std::strtoull(begin, &end, 0);
    if (end == begin) return false;
    out.moduleOffset = static_cast<uintptr_t>(value);
    return true;
}

bool Config::load(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    values_.clear();

    std::string line;
    std::string section;
    bool first = true;

    while (std::getline(file, line)) {
        if (first && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        first = false;

        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.front() == '[') {
            const size_t close = line.find(']');
            if (close != std::string::npos) section = lower(trim(line.substr(1, close - 1)));
            continue;
        }

        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = lower(trim(line.substr(0, eq)));
        std::string value = trim(line.substr(eq + 1));

        const size_t hash = value.find(" #");
        if (hash != std::string::npos) value = trim(value.substr(0, hash));

        if (!key.empty() && !section.empty()) values_[section + "." + key] = value;
    }

    // [game]
    process = widen(get("game", "process", narrow(process)));
    windowTitle = widen(get("game", "window_title", narrow(windowTitle)));
    windowClass = widen(get("game", "window_class", narrow(windowClass)));
    clientModule = widen(get("game", "module", narrow(clientModule)));
    layout = lower(get("game", "layout", layout));

    // [offsets]
    parseOffsetSpec(get("offsets", "entity_list", "0"), entityList);
    parseOffsetSpec(get("offsets", "local_player", "0"), localPlayer);
    parseOffsetSpec(get("offsets", "view_matrix", "0"), viewMatrix);

    entityStride = getInt("offsets", "entity_stride", entityStride);
    maxEntities = getInt("offsets", "max_entities", maxEntities);
    health = getInt("offsets", "m_ihealth", health);
    team = getInt("offsets", "m_iteamnum", team);
    lifeState = getInt("offsets", "m_lifestate", lifeState);
    origin = getInt("offsets", "m_vecorigin", origin);
    eyeOffset = getInt("offsets", "m_vecoffset", eyeOffset);
    boneMatrix = getInt("offsets", "m_bonematrix", boneMatrix);
    boneIndirect = getBool("offsets", "bone_indirect", boneIndirect);
    headBone = getInt("offsets", "head_bone", headBone);
    dormant = getInt("offsets", "m_dormant", dormant);
    spotted = getInt("offsets", "m_bspotted", spotted);
    name = getInt("offsets", "m_szname", name);
    weapon = getInt("offsets", "m_weapon", weapon);
    armor = getInt("offsets", "m_armor", armor);
    scoped = getInt("offsets", "m_scoped", scoped);
    flashed = getInt("offsets", "m_flashed", flashed);
    defusing = getInt("offsets", "m_defusing", defusing);
    money = getInt("offsets", "m_money", money);
    ammo = getInt("offsets", "m_ammo", ammo);
    velocity = getInt("offsets", "m_velocity", velocity);
    aimPunch = getInt("offsets", "m_aimpunch", aimPunch);
    viewAngles = getInt("offsets", "m_viewangles", viewAngles);

    // [aim]
    aim.enabled = getBool("aim", "enabled", aim.enabled);
    aim.key = getKey("aim", "key", aim.key);
    aim.toggle = getBool("aim", "toggle", aim.toggle);
    aim.hitbox = getInt("aim", "hitbox", aim.hitbox);
    aim.multiHitbox = getBool("aim", "multi_hitbox", aim.multiHitbox);
    aim.hitHead = getBool("aim", "hit_head", aim.hitHead);
    aim.hitNeck = getBool("aim", "hit_neck", aim.hitNeck);
    aim.hitChest = getBool("aim", "hit_chest", aim.hitChest);
    aim.hitStomach = getBool("aim", "hit_stomach", aim.hitStomach);
    aim.priority = getInt("aim", "priority", aim.priority);
    aim.targetSwitch = getBool("aim", "target_switch", aim.targetSwitch);
    aim.switchDelay = getFloat("aim", "switch_delay", aim.switchDelay);
    aim.maxDistance = getFloat("aim", "max_distance", aim.maxDistance);
    aim.teamCheck = getBool("aim", "team_check", aim.teamCheck);
    aim.visibleOnly = getBool("aim", "visible_only", aim.visibleOnly);
    aim.ignoreSmoke = getBool("aim", "ignore_smoke", aim.ignoreSmoke);
    aim.ignoreFlashed = getBool("aim", "ignore_flashed", aim.ignoreFlashed);
    aim.onlyScoped = getBool("aim", "only_scoped", aim.onlyScoped);
    aim.fov = getFloat("aim", "fov", aim.fov);
    aim.dynamicFov = getBool("aim", "dynamic_fov", aim.dynamicFov);
    aim.fovMin = getFloat("aim", "fov_min", aim.fovMin);
    aim.fovMax = getFloat("aim", "fov_max", aim.fovMax);
    aim.fovScale = getFloat("aim", "fov_scale", aim.fovScale);
    aim.smooth = getFloat("aim", "smooth", aim.smooth);
    aim.smoothMode = getInt("aim", "smooth_mode", aim.smoothMode);
    aim.humanize = getFloat("aim", "humanize", aim.humanize);
    aim.humanizeJitter = getFloat("aim", "humanize_jitter", aim.humanizeJitter);
    aim.aimStep = getFloat("aim", "aim_step", aim.aimStep);
    aim.aimScale = getFloat("aim", "scale", aim.aimScale);
    aim.reactionTime = getFloat("aim", "reaction_time", aim.reactionTime);
    aim.reactionJitter = getFloat("aim", "reaction_jitter", aim.reactionJitter);
    aim.firstShotDelay = getFloat("aim", "first_shot_delay", aim.firstShotDelay);
    aim.autoFire = getBool("aim", "auto_fire", aim.autoFire);
    aim.autoScope = getBool("aim", "auto_scope", aim.autoScope);
    aim.autoPistol = getBool("aim", "auto_pistol", aim.autoPistol);
    aim.autoStop = getBool("aim", "auto_stop", aim.autoStop);
    aim.autoCrouch = getBool("aim", "auto_crouch", aim.autoCrouch);
    aim.rcs = getBool("aim", "rcs", aim.rcs);
    aim.rcsStandalone = getBool("aim", "rcs_standalone", aim.rcsStandalone);
    aim.rcsX = getFloat("aim", "rcs_x", aim.rcsX);
    aim.rcsY = getFloat("aim", "rcs_y", aim.rcsY);
    aim.rcsSmooth = getFloat("aim", "rcs_smooth", aim.rcsSmooth);
    aim.rcsScopeOnly = getBool("aim", "rcs_scope_only", aim.rcsScopeOnly);
    aim.prediction = getBool("aim", "prediction", aim.prediction);
    aim.predictMs = getFloat("aim", "predict_ms", aim.predictMs);
    aim.backtrack = getBool("aim", "backtrack", aim.backtrack);
    aim.backtrackMs = getFloat("aim", "backtrack_ms", aim.backtrackMs);
    aim.resolverMode = getInt("aim", "resolver", aim.resolverMode);
    aim.drawFov = getBool("aim", "draw_fov", aim.drawFov);
    aim.fovColor = getColor("aim", "fov_color", aim.fovColor);
    aim.fovAlpha = getFloat("aim", "fov_alpha", aim.fovAlpha);
    aim.silent = getBool("aim", "silent", aim.silent);

    // [trigger]
    trigger.enabled = getBool("trigger", "enabled", trigger.enabled);
    trigger.key = getKey("trigger", "key", trigger.key);
    trigger.toggle = getBool("trigger", "toggle", trigger.toggle);
    trigger.radius = getFloat("trigger", "radius", trigger.radius);
    trigger.maxDistance = getFloat("trigger", "max_distance", trigger.maxDistance);
    trigger.teamCheck = getBool("trigger", "team_check", trigger.teamCheck);
    trigger.visibleOnly = getBool("trigger", "visible_only", trigger.visibleOnly);
    trigger.onlyScoped = getBool("trigger", "only_scoped", trigger.onlyScoped);
    trigger.wallCheck = getBool("trigger", "wall_check", trigger.wallCheck);
    trigger.randomize = getBool("trigger", "randomize", trigger.randomize);
    trigger.delayMin = getInt("trigger", "delay_min_ms", trigger.delayMin);
    trigger.delayMax = getInt("trigger", "delay_max_ms", trigger.delayMax);
    trigger.holdMin = getInt("trigger", "hold_min_ms", trigger.holdMin);
    trigger.holdMax = getInt("trigger", "hold_max_ms", trigger.holdMax);
    trigger.burst = getBool("trigger", "burst", trigger.burst);
    trigger.burstShots = getInt("trigger", "burst_shots", trigger.burstShots);
    trigger.burstDelay = getFloat("trigger", "burst_delay", trigger.burstDelay);
    trigger.burstCooldown = getFloat("trigger", "burst_cooldown", trigger.burstCooldown);

    // [esp]
    esp.enabled = getBool("esp", "enabled", esp.enabled);
    esp.teamCheck = getBool("esp", "team_check", esp.teamCheck);
    esp.maxDistance = getFloat("esp", "max_distance", esp.maxDistance);
    esp.visibleCheck = getBool("esp", "visible_check", esp.visibleCheck);
    esp.oofCheck = getBool("esp", "oof_check", esp.oofCheck);
    esp.fadeStart = getFloat("esp", "fade_start", esp.fadeStart);
    esp.box = getBool("esp", "box", esp.box);
    esp.boxStyle = getInt("esp", "box_style", esp.boxStyle);
    esp.boxThickness = getFloat("esp", "thickness", esp.boxThickness);
    esp.filledBox = getBool("esp", "filled_box", esp.filledBox);
    esp.fillColor = getColor("esp", "fill_color", esp.fillColor);
    esp.healthBar = getBool("esp", "health_bar", esp.healthBar);
    esp.healthBarPos = getInt("esp", "health_bar_pos", esp.healthBarPos);
    esp.healthText = getBool("esp", "health_text", esp.healthText);
    esp.armorBar = getBool("esp", "armor_bar", esp.armorBar);
    esp.name = getBool("esp", "name", esp.name);
    esp.weapon = getBool("esp", "weapon", esp.weapon);
    esp.distance = getBool("esp", "distance", esp.distance);
    esp.money = getBool("esp", "money", esp.money);
    esp.ammo = getBool("esp", "ammo", esp.ammo);
    esp.headDot = getBool("esp", "head_dot", esp.headDot);
    esp.snapline = getBool("esp", "snapline", esp.snapline);
    esp.snaplineOrigin = getInt("esp", "snapline_origin", esp.snaplineOrigin);
    esp.skeleton = getBool("esp", "skeleton", esp.skeleton);
    esp.skeletonThickness = getFloat("esp", "skeleton_thickness", esp.skeletonThickness);
    esp.flags = getBool("esp", "flags", esp.flags);
    esp.glow = getBool("esp", "glow", esp.glow);
    esp.offscreenArrows = getBool("esp", "offscreen_arrows", esp.offscreenArrows);
    esp.soundEsp = getBool("esp", "sound_esp", esp.soundEsp);
    esp.soundRadius = getFloat("esp", "sound_radius", esp.soundRadius);
    esp.bomb = getBool("esp", "bomb", esp.bomb);
    esp.bombTimer = getBool("esp", "bomb_timer", esp.bombTimer);
    esp.droppedWeapons = getBool("esp", "dropped_weapons", esp.droppedWeapons);
    esp.grenades = getBool("esp", "grenades", esp.grenades);
    esp.localPlayer = getBool("esp", "local_player", esp.localPlayer);
    esp.textSize = getInt("esp", "text_size", esp.textSize);
    esp.outlineText = getBool("esp", "outline_text", esp.outlineText);
    esp.colorVisible = getColor("esp", "color_visible", esp.colorVisible);
    esp.colorOccluded = getColor("esp", "color_occluded", esp.colorOccluded);
    esp.colorSnapline = getColor("esp", "color_snapline", esp.colorSnapline);
    esp.colorText = getColor("esp", "color_text", esp.colorText);
    esp.colorHealth = getColor("esp", "color_health", esp.colorHealth);
    esp.colorArmor = getColor("esp", "color_armor", esp.colorArmor);
    esp.colorSkeleton = getColor("esp", "color_skeleton", esp.colorSkeleton);
    esp.colorHeadDot = getColor("esp", "color_head_dot", esp.colorHeadDot);
    esp.colorGlow = getColor("esp", "color_glow", esp.colorGlow);

    // [visuals]
    visuals.fovChanger = getBool("visuals", "fov_changer", visuals.fovChanger);
    visuals.fov = getFloat("visuals", "fov", visuals.fov);
    visuals.thirdPerson = getBool("visuals", "third_person", visuals.thirdPerson);
    visuals.thirdPersonDist = getFloat("visuals", "third_person_dist", visuals.thirdPersonDist);
    visuals.noScope = getBool("visuals", "no_scope", visuals.noScope);
    visuals.crosshair = getBool("visuals", "crosshair", visuals.crosshair);
    visuals.crosshairStyle = getInt("visuals", "crosshair_style", visuals.crosshairStyle);
    visuals.crosshairSize = getFloat("visuals", "crosshair_size", visuals.crosshairSize);
    visuals.crosshairGap = getFloat("visuals", "crosshair_gap", visuals.crosshairGap);
    visuals.crosshairThickness = getFloat("visuals", "crosshair_thickness", visuals.crosshairThickness);
    visuals.crosshairColor = getColor("visuals", "crosshair_color", visuals.crosshairColor);
    visuals.hitmarker = getBool("visuals", "hitmarker", visuals.hitmarker);
    visuals.hitmarkerSize = getFloat("visuals", "hitmarker_size", visuals.hitmarkerSize);
    visuals.hitmarkerColor = getColor("visuals", "hitmarker_color", visuals.hitmarkerColor);
    visuals.damageIndicator = getBool("visuals", "damage_indicator", visuals.damageIndicator);
    visuals.spectatorList = getBool("visuals", "spectator_list", visuals.spectatorList);
    visuals.watermark = getBool("visuals", "watermark", visuals.watermark);
    visuals.keybindList = getBool("visuals", "keybind_list", visuals.keybindList);
    visuals.fpsCounter = getBool("visuals", "fps_counter", visuals.fpsCounter);
    visuals.pingCounter = getBool("visuals", "ping_counter", visuals.pingCounter);
    visuals.timeCounter = getBool("visuals", "time_counter", visuals.timeCounter);
    visuals.noFlash = getBool("visuals", "no_flash", visuals.noFlash);
    visuals.flashAlpha = getFloat("visuals", "flash_alpha", visuals.flashAlpha);
    visuals.noSmoke = getBool("visuals", "no_smoke", visuals.noSmoke);
    visuals.noDecals = getBool("visuals", "no_decals", visuals.noDecals);
    visuals.nightMode = getBool("visuals", "night_mode", visuals.nightMode);
    visuals.nightAlpha = getFloat("visuals", "night_alpha", visuals.nightAlpha);
    visuals.removeHands = getBool("visuals", "remove_hands", visuals.removeHands);
    visuals.accent = getColor("visuals", "accent", visuals.accent);
    visuals.accentDim = getColor("visuals", "accent_dim", visuals.accentDim);

    // [radar]
    radar.enabled = getBool("radar", "enabled", radar.enabled);
    radar.size = getFloat("radar", "size", radar.size);
    radar.zoom = getFloat("radar", "zoom", radar.zoom);
    radar.posX = getInt("radar", "pos_x", radar.posX);
    radar.posY = getInt("radar", "pos_y", radar.posY);
    radar.rotate = getBool("radar", "rotate", radar.rotate);
    radar.showEnemies = getBool("radar", "show_enemies", radar.showEnemies);
    radar.showTeam = getBool("radar", "show_team", radar.showTeam);
    radar.showBomb = getBool("radar", "show_bomb", radar.showBomb);
    radar.showDropped = getBool("radar", "show_dropped", radar.showDropped);
    radar.dotSize = getFloat("radar", "dot_size", radar.dotSize);
    radar.colorEnemy = getColor("radar", "color_enemy", radar.colorEnemy);
    radar.colorTeam = getColor("radar", "color_team", radar.colorTeam);
    radar.colorBomb = getColor("radar", "color_bomb", radar.colorBomb);

    // [misc]
    misc.bhop = getBool("misc", "bhop", misc.bhop);
    misc.bhopKey = getKey("misc", "bhop_key", misc.bhopKey);
    misc.autoStrafe = getBool("misc", "auto_strafe", misc.autoStrafe);
    misc.strafeStrength = getFloat("misc", "strafe_strength", misc.strafeStrength);
    misc.jumpBug = getBool("misc", "jump_bug", misc.jumpBug);
    misc.edgeJump = getBool("misc", "edge_jump", misc.edgeJump);
    misc.edgeJumpKey = getKey("misc", "edge_jump_key", misc.edgeJumpKey);
    misc.autoPistol = getBool("misc", "auto_pistol", misc.autoPistol);
    misc.autoReload = getBool("misc", "auto_reload", misc.autoReload);
    misc.autoAccept = getBool("misc", "auto_accept", misc.autoAccept);
    misc.autoDefuse = getBool("misc", "auto_defuse", misc.autoDefuse);
    misc.autoBuy = getBool("misc", "auto_buy", misc.autoBuy);
    misc.autoBuyText = get("misc", "auto_buy_text", misc.autoBuyText);
    misc.fakeLatency = getBool("misc", "fake_latency", misc.fakeLatency);
    misc.fakeLatencyMs = getInt("misc", "fake_latency_ms", misc.fakeLatencyMs);
    misc.chatSpam = getBool("misc", "chat_spam", misc.chatSpam);
    misc.chatSpamText = get("misc", "chat_spam_text", misc.chatSpamText);
    misc.chatSpamDelay = getFloat("misc", "chat_spam_delay", misc.chatSpamDelay);
    misc.killSay = getBool("misc", "kill_say", misc.killSay);
    misc.killSayText = get("misc", "kill_say_text", misc.killSayText);
    misc.clanTag = getBool("misc", "clan_tag", misc.clanTag);
    misc.clanTagText = get("misc", "clan_tag_text", misc.clanTagText);
    misc.antiAim = getBool("misc", "anti_aim", misc.antiAim);
    misc.antiAimMode = getInt("misc", "anti_aim_mode", misc.antiAimMode);
    misc.knifeBot = getBool("misc", "knife_bot", misc.knifeBot);
    misc.autoGG = getBool("misc", "auto_gg", misc.autoGG);
    misc.autoMolotov = getBool("misc", "auto_molotov", misc.autoMolotov);
    misc.preserveKillfeed = getBool("misc", "preserve_killfeed", misc.preserveKillfeed);

    // [keys]
    menuKey = getKey("keys", "menu", menuKey);
    quitKey = getKey("keys", "quit", quitKey);
    espToggleKey = getKey("keys", "esp_toggle", espToggleKey);
    aimToggleKey = getKey("keys", "aim_toggle", aimToggleKey);
    triggerToggleKey = getKey("keys", "trigger_toggle", triggerToggleKey);
    panicKey = getKey("keys", "panic", panicKey);

    // [loop]
    renderFps = getInt("loop", "render_fps", renderFps);
    aimHz = getInt("loop", "aim_hz", aimHz);

    const std::string presetText = lower(get("general", "preset", presetKey(preset)));
    preset = Preset::Custom;
    if (presetText == "legit") preset = Preset::Legit;
    else if (presetText == "semi-legit" || presetText == "semilegit") preset = Preset::SemiLegit;
    else if (presetText == "semi-rage" || presetText == "semirage") preset = Preset::SemiRage;
    else if (presetText == "rage") preset = Preset::Rage;

    // Sanity.
    if (aim.smooth < 1.0f) aim.smooth = 1.0f;
    if (aim.aimScale <= 0.0f) aim.aimScale = 1.0f;
    if (trigger.delayMax < trigger.delayMin) trigger.delayMax = trigger.delayMin;
    if (trigger.holdMax < trigger.holdMin) trigger.holdMax = trigger.holdMin;
    if (aimHz < 30) aimHz = 30;
    if (aimHz > 2000) aimHz = 2000;
    if (esp.maxDistance <= 0.0f) esp.maxDistance = 4000.0f;

    return true;
}

int Config::importOffsets(const char* path) {
    std::ifstream file(path);
    if (!file) {
        logf("import: cannot open %s", path);
        return 0;
    }

    int applied = 0;
    std::string line;

    auto setAddress = [](OffsetSpec& spec, unsigned long long value) {
        spec.pattern = false;
        spec.moduleOffset = static_cast<uintptr_t>(value);
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "0x%llX", value);
        spec.raw = buffer;
    };

    while (std::getline(file, line)) {
        std::string cleaned;
        cleaned.reserve(line.size());
        for (char c : line) {
            if (c == '"' || c == '{' || c == '}' || c == '[' || c == ']' || c == ',') continue;
            cleaned.push_back(c);
        }

        const size_t sep = cleaned.find_first_of("=:");
        if (sep == std::string::npos) continue;

        std::string key = lower(trim(cleaned.substr(0, sep)));
        const std::string value = trim(cleaned.substr(sep + 1));
        if (key.empty() || value.empty()) continue;

        const size_t dot = key.find_last_of('.');
        if (dot != std::string::npos) key = key.substr(dot + 1);

        key.erase(std::remove_if(key.begin(), key.end(),
                                 [](unsigned char c) {
                                     return !(std::isalnum(c) || c == '_');
                                 }),
                  key.end());
        if (key.empty()) continue;

        char* end = nullptr;
        const unsigned long long v = std::strtoull(value.c_str(), &end, 0);
        if (end == value.c_str()) continue;

        if (key == "dwentitylist") {
            setAddress(entityList, v);
        } else if (key == "dwlocalplayer" || key == "dwlocalplayerpawn" ||
                   key == "dwlocalplayercontroller") {
            setAddress(localPlayer, v);
        } else if (key == "dwviewmatrix") {
            setAddress(viewMatrix, v);
        } else if (key == "m_ihealth") {
            health = static_cast<int>(v);
        } else if (key == "m_iteamnum") {
            team = static_cast<int>(v);
        } else if (key == "m_lifestate") {
            lifeState = static_cast<int>(v);
        } else if (key == "m_vecabsorigin" || key == "m_vecorigin") {
            origin = static_cast<int>(v);
        } else if (key == "m_vecoffset" || key == "m_vecviewoffset") {
            eyeOffset = static_cast<int>(v);
        } else if (key == "m_pbonearray" || key == "m_dwbonematrix" || key == "m_modelstate") {
            boneMatrix = static_cast<int>(v);
        } else if (key == "m_bdormant") {
            dormant = static_cast<int>(v);
        } else if (key == "m_bspotted") {
            spotted = static_cast<int>(v);
        } else if (key == "m_szname" || key == "m_iszplayername") {
            name = static_cast<int>(v);
        } else if (key == "m_iitemdefinitionindex") {
            weapon = static_cast<int>(v);
        } else if (key == "m_armorvalue" || key == "m_iarmor") {
            armor = static_cast<int>(v);
        } else if (key == "m_bisscoped") {
            scoped = static_cast<int>(v);
        } else if (key == "m_flflashduration") {
            flashed = static_cast<int>(v);
        } else if (key == "m_vecabsvelocity" || key == "m_vecvelocity") {
            velocity = static_cast<int>(v);
        } else if (key == "m_aimpunchangle") {
            aimPunch = static_cast<int>(v);
        } else if (key == "m_angviewangles" || key == "m_angclientviewangles") {
            viewAngles = static_cast<int>(v);
        } else {
            continue;
        }

        ++applied;
    }

    logf("import: %d entries applied from %s", applied, path);
    return applied;
}

bool Config::save(const char* path) const {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    auto o = [&](const char*, const char* key, const std::string& value) {
        f << key << " = " << value << "\n";
    };
    auto ob = [&](const char*, const char* key, bool value) {
        f << key << " = " << boolText(value) << "\n";
    };
    auto oi = [&](const char*, const char* key, int value) {
        f << key << " = " << value << "\n";
    };
    auto of = [&](const char*, const char* key, float value) {
        f << key << " = " << floatText(value) << "\n";
    };
    auto oc = [&](const char*, const char* key, uint32_t value) {
        f << key << " = " << colorText(value) << "\n";
    };

    f << "# hx client configuration\n\n";

    f << "[general]\n";
    o("general", "preset", presetKey(preset));

    f << "\n[game]\n";
    o("game", "process", narrow(process));
    o("game", "window_title", narrow(windowTitle));
    o("game", "window_class", narrow(windowClass));
    o("game", "module", narrow(clientModule));
    o("game", "layout", layout);

    f << "\n[offsets]\n";
    o("offsets", "entity_list", entityList.raw);
    o("offsets", "local_player", localPlayer.raw);
    o("offsets", "view_matrix", viewMatrix.raw);
    oi("offsets", "entity_stride", entityStride);
    oi("offsets", "max_entities", maxEntities);
    oi("offsets", "m_ihealth", health);
    oi("offsets", "m_iteamnum", team);
    oi("offsets", "m_lifestate", lifeState);
    oi("offsets", "m_vecorigin", origin);
    oi("offsets", "m_vecoffset", eyeOffset);
    oi("offsets", "m_bonematrix", boneMatrix);
    ob("offsets", "bone_indirect", boneIndirect);
    oi("offsets", "head_bone", headBone);
    oi("offsets", "m_dormant", dormant);
    oi("offsets", "m_bspotted", spotted);
    oi("offsets", "m_szname", name);
    oi("offsets", "m_weapon", weapon);
    oi("offsets", "m_armor", armor);
    oi("offsets", "m_scoped", scoped);
    oi("offsets", "m_flashed", flashed);
    oi("offsets", "m_defusing", defusing);
    oi("offsets", "m_money", money);
    oi("offsets", "m_ammo", ammo);
    oi("offsets", "m_velocity", velocity);
    oi("offsets", "m_aimpunch", aimPunch);
    oi("offsets", "m_viewangles", viewAngles);

    f << "\n[aim]\n";
    ob("aim", "enabled", aim.enabled);
    oi("aim", "key", aim.key);
    ob("aim", "toggle", aim.toggle);
    oi("aim", "hitbox", aim.hitbox);
    ob("aim", "multi_hitbox", aim.multiHitbox);
    ob("aim", "hit_head", aim.hitHead);
    ob("aim", "hit_neck", aim.hitNeck);
    ob("aim", "hit_chest", aim.hitChest);
    ob("aim", "hit_stomach", aim.hitStomach);
    oi("aim", "priority", aim.priority);
    ob("aim", "target_switch", aim.targetSwitch);
    of("aim", "switch_delay", aim.switchDelay);
    of("aim", "max_distance", aim.maxDistance);
    ob("aim", "team_check", aim.teamCheck);
    ob("aim", "visible_only", aim.visibleOnly);
    ob("aim", "ignore_smoke", aim.ignoreSmoke);
    ob("aim", "ignore_flashed", aim.ignoreFlashed);
    ob("aim", "only_scoped", aim.onlyScoped);
    of("aim", "fov", aim.fov);
    ob("aim", "dynamic_fov", aim.dynamicFov);
    of("aim", "fov_min", aim.fovMin);
    of("aim", "fov_max", aim.fovMax);
    of("aim", "fov_scale", aim.fovScale);
    of("aim", "smooth", aim.smooth);
    oi("aim", "smooth_mode", aim.smoothMode);
    of("aim", "humanize", aim.humanize);
    of("aim", "humanize_jitter", aim.humanizeJitter);
    of("aim", "aim_step", aim.aimStep);
    of("aim", "scale", aim.aimScale);
    of("aim", "reaction_time", aim.reactionTime);
    of("aim", "reaction_jitter", aim.reactionJitter);
    of("aim", "first_shot_delay", aim.firstShotDelay);
    ob("aim", "auto_fire", aim.autoFire);
    ob("aim", "auto_scope", aim.autoScope);
    ob("aim", "auto_pistol", aim.autoPistol);
    ob("aim", "auto_stop", aim.autoStop);
    ob("aim", "auto_crouch", aim.autoCrouch);
    ob("aim", "rcs", aim.rcs);
    ob("aim", "rcs_standalone", aim.rcsStandalone);
    of("aim", "rcs_x", aim.rcsX);
    of("aim", "rcs_y", aim.rcsY);
    of("aim", "rcs_smooth", aim.rcsSmooth);
    ob("aim", "rcs_scope_only", aim.rcsScopeOnly);
    ob("aim", "prediction", aim.prediction);
    of("aim", "predict_ms", aim.predictMs);
    ob("aim", "backtrack", aim.backtrack);
    of("aim", "backtrack_ms", aim.backtrackMs);
    oi("aim", "resolver", aim.resolverMode);
    ob("aim", "draw_fov", aim.drawFov);
    oc("aim", "fov_color", aim.fovColor);
    of("aim", "fov_alpha", aim.fovAlpha);
    ob("aim", "silent", aim.silent);

    f << "\n[trigger]\n";
    ob("trigger", "enabled", trigger.enabled);
    oi("trigger", "key", trigger.key);
    ob("trigger", "toggle", trigger.toggle);
    of("trigger", "radius", trigger.radius);
    of("trigger", "max_distance", trigger.maxDistance);
    ob("trigger", "team_check", trigger.teamCheck);
    ob("trigger", "visible_only", trigger.visibleOnly);
    ob("trigger", "only_scoped", trigger.onlyScoped);
    ob("trigger", "wall_check", trigger.wallCheck);
    ob("trigger", "randomize", trigger.randomize);
    oi("trigger", "delay_min_ms", trigger.delayMin);
    oi("trigger", "delay_max_ms", trigger.delayMax);
    oi("trigger", "hold_min_ms", trigger.holdMin);
    oi("trigger", "hold_max_ms", trigger.holdMax);
    ob("trigger", "burst", trigger.burst);
    oi("trigger", "burst_shots", trigger.burstShots);
    of("trigger", "burst_delay", trigger.burstDelay);
    of("trigger", "burst_cooldown", trigger.burstCooldown);

    f << "\n[esp]\n";
    ob("esp", "enabled", esp.enabled);
    ob("esp", "team_check", esp.teamCheck);
    of("esp", "max_distance", esp.maxDistance);
    ob("esp", "visible_check", esp.visibleCheck);
    ob("esp", "oof_check", esp.oofCheck);
    of("esp", "fade_start", esp.fadeStart);
    ob("esp", "box", esp.box);
    oi("esp", "box_style", esp.boxStyle);
    of("esp", "thickness", esp.boxThickness);
    ob("esp", "filled_box", esp.filledBox);
    oc("esp", "fill_color", esp.fillColor);
    ob("esp", "health_bar", esp.healthBar);
    oi("esp", "health_bar_pos", esp.healthBarPos);
    ob("esp", "health_text", esp.healthText);
    ob("esp", "armor_bar", esp.armorBar);
    ob("esp", "name", esp.name);
    ob("esp", "weapon", esp.weapon);
    ob("esp", "distance", esp.distance);
    ob("esp", "money", esp.money);
    ob("esp", "ammo", esp.ammo);
    ob("esp", "head_dot", esp.headDot);
    ob("esp", "snapline", esp.snapline);
    oi("esp", "snapline_origin", esp.snaplineOrigin);
    ob("esp", "skeleton", esp.skeleton);
    of("esp", "skeleton_thickness", esp.skeletonThickness);
    ob("esp", "flags", esp.flags);
    ob("esp", "glow", esp.glow);
    ob("esp", "offscreen_arrows", esp.offscreenArrows);
    ob("esp", "sound_esp", esp.soundEsp);
    of("esp", "sound_radius", esp.soundRadius);
    ob("esp", "bomb", esp.bomb);
    ob("esp", "bomb_timer", esp.bombTimer);
    ob("esp", "dropped_weapons", esp.droppedWeapons);
    ob("esp", "grenades", esp.grenades);
    ob("esp", "local_player", esp.localPlayer);
    oi("esp", "text_size", esp.textSize);
    ob("esp", "outline_text", esp.outlineText);
    oc("esp", "color_visible", esp.colorVisible);
    oc("esp", "color_occluded", esp.colorOccluded);
    oc("esp", "color_snapline", esp.colorSnapline);
    oc("esp", "color_text", esp.colorText);
    oc("esp", "color_health", esp.colorHealth);
    oc("esp", "color_armor", esp.colorArmor);
    oc("esp", "color_skeleton", esp.colorSkeleton);
    oc("esp", "color_head_dot", esp.colorHeadDot);
    oc("esp", "color_glow", esp.colorGlow);

    f << "\n[visuals]\n";
    ob("visuals", "fov_changer", visuals.fovChanger);
    of("visuals", "fov", visuals.fov);
    ob("visuals", "third_person", visuals.thirdPerson);
    of("visuals", "third_person_dist", visuals.thirdPersonDist);
    ob("visuals", "no_scope", visuals.noScope);
    ob("visuals", "crosshair", visuals.crosshair);
    oi("visuals", "crosshair_style", visuals.crosshairStyle);
    of("visuals", "crosshair_size", visuals.crosshairSize);
    of("visuals", "crosshair_gap", visuals.crosshairGap);
    of("visuals", "crosshair_thickness", visuals.crosshairThickness);
    oc("visuals", "crosshair_color", visuals.crosshairColor);
    ob("visuals", "hitmarker", visuals.hitmarker);
    of("visuals", "hitmarker_size", visuals.hitmarkerSize);
    oc("visuals", "hitmarker_color", visuals.hitmarkerColor);
    ob("visuals", "damage_indicator", visuals.damageIndicator);
    ob("visuals", "spectator_list", visuals.spectatorList);
    ob("visuals", "watermark", visuals.watermark);
    ob("visuals", "keybind_list", visuals.keybindList);
    ob("visuals", "fps_counter", visuals.fpsCounter);
    ob("visuals", "ping_counter", visuals.pingCounter);
    ob("visuals", "time_counter", visuals.timeCounter);
    ob("visuals", "no_flash", visuals.noFlash);
    of("visuals", "flash_alpha", visuals.flashAlpha);
    ob("visuals", "no_smoke", visuals.noSmoke);
    ob("visuals", "no_decals", visuals.noDecals);
    ob("visuals", "night_mode", visuals.nightMode);
    of("visuals", "night_alpha", visuals.nightAlpha);
    ob("visuals", "remove_hands", visuals.removeHands);
    oc("visuals", "accent", visuals.accent);
    oc("visuals", "accent_dim", visuals.accentDim);

    f << "\n[radar]\n";
    ob("radar", "enabled", radar.enabled);
    of("radar", "size", radar.size);
    of("radar", "zoom", radar.zoom);
    oi("radar", "pos_x", radar.posX);
    oi("radar", "pos_y", radar.posY);
    ob("radar", "rotate", radar.rotate);
    ob("radar", "show_enemies", radar.showEnemies);
    ob("radar", "show_team", radar.showTeam);
    ob("radar", "show_bomb", radar.showBomb);
    ob("radar", "show_dropped", radar.showDropped);
    of("radar", "dot_size", radar.dotSize);
    oc("radar", "color_enemy", radar.colorEnemy);
    oc("radar", "color_team", radar.colorTeam);
    oc("radar", "color_bomb", radar.colorBomb);

    f << "\n[misc]\n";
    ob("misc", "bhop", misc.bhop);
    oi("misc", "bhop_key", misc.bhopKey);
    ob("misc", "auto_strafe", misc.autoStrafe);
    of("misc", "strafe_strength", misc.strafeStrength);
    ob("misc", "jump_bug", misc.jumpBug);
    ob("misc", "edge_jump", misc.edgeJump);
    oi("misc", "edge_jump_key", misc.edgeJumpKey);
    ob("misc", "auto_pistol", misc.autoPistol);
    ob("misc", "auto_reload", misc.autoReload);
    ob("misc", "auto_accept", misc.autoAccept);
    ob("misc", "auto_defuse", misc.autoDefuse);
    ob("misc", "auto_buy", misc.autoBuy);
    o("misc", "auto_buy_text", misc.autoBuyText);
    ob("misc", "fake_latency", misc.fakeLatency);
    oi("misc", "fake_latency_ms", misc.fakeLatencyMs);
    ob("misc", "chat_spam", misc.chatSpam);
    o("misc", "chat_spam_text", misc.chatSpamText);
    of("misc", "chat_spam_delay", misc.chatSpamDelay);
    ob("misc", "kill_say", misc.killSay);
    o("misc", "kill_say_text", misc.killSayText);
    ob("misc", "clan_tag", misc.clanTag);
    o("misc", "clan_tag_text", misc.clanTagText);
    ob("misc", "anti_aim", misc.antiAim);
    oi("misc", "anti_aim_mode", misc.antiAimMode);
    ob("misc", "knife_bot", misc.knifeBot);
    ob("misc", "auto_gg", misc.autoGG);
    ob("misc", "auto_molotov", misc.autoMolotov);
    ob("misc", "preserve_killfeed", misc.preserveKillfeed);

    f << "\n[keys]\n";
    oi("keys", "menu", menuKey);
    oi("keys", "quit", quitKey);
    oi("keys", "esp_toggle", espToggleKey);
    oi("keys", "aim_toggle", aimToggleKey);
    oi("keys", "trigger_toggle", triggerToggleKey);
    oi("keys", "panic", panicKey);

    f << "\n[loop]\n";
    oi("loop", "render_fps", renderFps);
    oi("loop", "aim_hz", aimHz);

    return true;
}

void Config::applyPreset(Preset p) {
    preset = p;
    if (p == Preset::Custom) return;

    // Shared defaults, then per-preset overrides.
    aim.enabled = true;
    aim.teamCheck = true;
    aim.multiHitbox = false;
    aim.hitHead = true;
    aim.hitNeck = true;
    aim.hitChest = true;
    aim.hitStomach = false;
    aim.autoStop = false;
    aim.autoCrouch = false;
    aim.prediction = false;
    aim.backtrack = false;
    aim.silent = false;
    aim.rcsStandalone = false;
    aim.rcsScopeOnly = false;
    aim.onlyScoped = false;
    aim.ignoreFlashed = false;
    aim.ignoreSmoke = false;
    aim.resolverMode = 0;

    trigger.enabled = true;
    trigger.teamCheck = true;
    trigger.visibleOnly = false;
    trigger.onlyScoped = false;
    trigger.burst = false;

    esp.enabled = true;
    esp.teamCheck = true;
    esp.skeleton = false;
    esp.glow = false;
    esp.offscreenArrows = false;
    esp.filledBox = false;
    esp.armorBar = false;
    esp.money = false;
    esp.ammo = false;
    esp.soundEsp = false;
    esp.localPlayer = false;

    visuals.nightMode = false;
    visuals.removeHands = false;

    misc.bhop = false;
    misc.autoStrafe = false;
    misc.jumpBug = false;
    misc.edgeJump = false;
    misc.autoPistol = false;
    misc.autoReload = false;
    misc.autoAccept = false;
    misc.autoDefuse = false;
    misc.autoBuy = false;
    misc.fakeLatency = false;
    misc.chatSpam = false;
    misc.killSay = false;
    misc.clanTag = false;
    misc.antiAim = false;
    misc.knifeBot = false;
    misc.autoGG = false;
    misc.autoMolotov = false;
    misc.preserveKillfeed = false;

    switch (p) {
        case Preset::Legit:
            aim.key = VK_XBUTTON2;
            aim.fov = 55.0f;
            aim.dynamicFov = false;
            aim.smooth = 16.0f;
            aim.smoothMode = 2;
            aim.humanize = 0.45f;
            aim.humanizeJitter = 0.5f;
            aim.reactionTime = 0.16f;
            aim.reactionJitter = 0.06f;
            aim.aimStep = 30.0f;
            aim.aimScale = 0.9f;
            aim.visibleOnly = true;
            aim.autoFire = false;
            aim.autoScope = false;
            aim.autoPistol = false;
            aim.rcs = true;
            aim.rcsX = 0.55f;
            aim.rcsY = 0.55f;
            aim.rcsSmooth = 6.0f;
            aim.priority = 0;
            aim.hitbox = 0;
            trigger.radius = 5.0f;
            trigger.delayMin = 28;
            trigger.delayMax = 70;
            trigger.holdMin = 20;
            trigger.holdMax = 45;
            trigger.randomize = true;
            esp.box = true;
            esp.boxStyle = 1;
            esp.healthBar = true;
            esp.healthText = false;
            esp.name = true;
            esp.weapon = false;
            esp.distance = false;
            esp.headDot = false;
            esp.snapline = false;
            esp.skeleton = false;
            esp.flags = false;
            esp.glow = false;
            esp.offscreenArrows = false;
            break;

        case Preset::SemiLegit:
            aim.key = VK_XBUTTON2;
            aim.fov = 85.0f;
            aim.dynamicFov = true;
            aim.fovMin = 45.0f;
            aim.fovMax = 130.0f;
            aim.smooth = 9.0f;
            aim.smoothMode = 2;
            aim.humanize = 0.25f;
            aim.reactionTime = 0.10f;
            aim.reactionJitter = 0.04f;
            aim.aimStep = 0.0f;
            aim.aimScale = 1.0f;
            aim.visibleOnly = true;
            aim.autoFire = false;
            aim.autoScope = true;
            aim.autoPistol = true;
            aim.rcs = true;
            aim.rcsX = 0.75f;
            aim.rcsY = 0.75f;
            aim.rcsSmooth = 4.0f;
            aim.priority = 2;
            aim.hitbox = 0;
            trigger.radius = 8.0f;
            trigger.delayMin = 14;
            trigger.delayMax = 38;
            trigger.holdMin = 12;
            trigger.holdMax = 28;
            esp.box = true;
            esp.boxStyle = 0;
            esp.healthBar = true;
            esp.healthText = true;
            esp.name = true;
            esp.weapon = true;
            esp.distance = true;
            esp.headDot = true;
            esp.snapline = true;
            esp.skeleton = false;
            esp.flags = true;
            esp.glow = false;
            break;

        case Preset::SemiRage:
            aim.key = VK_XBUTTON2;
            aim.fov = 170.0f;
            aim.dynamicFov = true;
            aim.fovMin = 90.0f;
            aim.fovMax = 260.0f;
            aim.smooth = 3.5f;
            aim.smoothMode = 1;
            aim.humanize = 0.08f;
            aim.reactionTime = 0.04f;
            aim.reactionJitter = 0.015f;
            aim.aimStep = 0.0f;
            aim.aimScale = 1.1f;
            aim.visibleOnly = false;
            aim.autoFire = true;
            aim.autoScope = true;
            aim.autoPistol = true;
            aim.autoStop = true;
            aim.rcs = true;
            aim.rcsX = 0.92f;
            aim.rcsY = 0.92f;
            aim.rcsSmooth = 2.0f;
            aim.priority = 2;
            aim.hitbox = 1;
            aim.multiHitbox = true;
            trigger.radius = 13.0f;
            trigger.delayMin = 2;
            trigger.delayMax = 10;
            trigger.holdMin = 8;
            trigger.holdMax = 16;
            trigger.burst = true;
            trigger.burstShots = 3;
            esp.box = true;
            esp.boxStyle = 0;
            esp.filledBox = true;
            esp.healthBar = true;
            esp.healthText = true;
            esp.armorBar = true;
            esp.name = true;
            esp.weapon = true;
            esp.distance = true;
            esp.money = true;
            esp.ammo = true;
            esp.headDot = true;
            esp.snapline = true;
            esp.skeleton = true;
            esp.flags = true;
            esp.glow = true;
            esp.offscreenArrows = true;
            misc.autoReload = true;
            misc.autoPistol = true;
            break;

        case Preset::Rage:
            aim.key = VK_XBUTTON2;
            aim.fov = 420.0f;
            aim.dynamicFov = false;
            aim.smooth = 1.0f;
            aim.smoothMode = 0;
            aim.humanize = 0.0f;
            aim.reactionTime = 0.0f;
            aim.reactionJitter = 0.0f;
            aim.aimStep = 0.0f;
            aim.aimScale = 1.35f;
            aim.visibleOnly = false;
            aim.autoFire = true;
            aim.autoScope = true;
            aim.autoPistol = true;
            aim.autoStop = true;
            aim.autoCrouch = true;
            aim.prediction = true;
            aim.predictMs = 35.0f;
            aim.backtrack = true;
            aim.backtrackMs = 120.0f;
            aim.resolverMode = 2;
            aim.rcs = true;
            aim.rcsX = 1.0f;
            aim.rcsY = 1.0f;
            aim.rcsSmooth = 1.0f;
            aim.priority = 1;
            aim.hitbox = 0;
            aim.multiHitbox = true;
            aim.hitStomach = true;
            trigger.radius = 22.0f;
            trigger.delayMin = 0;
            trigger.delayMax = 0;
            trigger.holdMin = 6;
            trigger.holdMax = 10;
            trigger.burst = false;
            trigger.randomize = false;
            esp.box = true;
            esp.boxStyle = 0;
            esp.filledBox = true;
            esp.healthBar = true;
            esp.healthText = true;
            esp.armorBar = true;
            esp.name = true;
            esp.weapon = true;
            esp.distance = true;
            esp.money = true;
            esp.ammo = true;
            esp.headDot = true;
            esp.snapline = true;
            esp.skeleton = true;
            esp.flags = true;
            esp.glow = true;
            esp.offscreenArrows = true;
            esp.soundEsp = true;
            esp.localPlayer = true;
            visuals.nightMode = true;
            visuals.removeHands = true;
            misc.bhop = true;
            misc.autoReload = true;
            misc.autoPistol = true;
            break;

        case Preset::Custom:
            break;
    }
}

void Config::logSummary() const {
    logf("config: preset=%s process=%s layout=%s module=%s", presetName(preset),
         narrow(process).c_str(), layout.c_str(), narrow(clientModule).c_str());
    logf("config: offsets entity_list=%s local_player=%s view_matrix=%s", entityList.raw.c_str(),
         localPlayer.raw.c_str(), viewMatrix.raw.c_str());
    logf("config: stride=0x%X max_entities=%d health=0x%X team=0x%X origin=0x%X bone=0x%X head_bone=%d",
         entityStride, maxEntities, health, team, origin, boneMatrix, headBone);
    logf("config: aim=%d key=%s fov=%.0f smooth=%.1f mode=%d fire=%d rcs=%d",
         aim.enabled, vkToName(aim.key).c_str(), aim.fov, aim.smooth, aim.smoothMode, aim.autoFire,
         aim.rcs);
    logf("config: trigger=%d key=%s radius=%.0f delay=%d-%dms", trigger.enabled,
         vkToName(trigger.key).c_str(), trigger.radius, trigger.delayMin, trigger.delayMax);
    logf("config: esp=%d box=%d health=%d name=%d skeleton=%d glow=%d", esp.enabled, esp.box,
         esp.healthBar, esp.name, esp.skeleton, esp.glow);
}

std::string Config::get(const std::string& section, const std::string& key,
                        const std::string& fallback) const {
    auto it = values_.find(section + "." + key);
    return it == values_.end() ? fallback : it->second;
}

int Config::getInt(const std::string& section, const std::string& key, int fallback) const {
    const std::string text = get(section, key, "");
    if (text.empty()) return fallback;
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 0);
    return end == text.c_str() ? fallback : static_cast<int>(value);
}

bool Config::getBool(const std::string& section, const std::string& key, bool fallback) const {
    const std::string text = lower(get(section, key, ""));
    if (text.empty()) return fallback;
    if (text == "1" || text == "true" || text == "yes" || text == "on") return true;
    if (text == "0" || text == "false" || text == "no" || text == "off") return false;
    return fallback;
}

float Config::getFloat(const std::string& section, const std::string& key, float fallback) const {
    const std::string text = get(section, key, "");
    if (text.empty()) return fallback;
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    return end == text.c_str() ? fallback : value;
}

uint32_t Config::getColor(const std::string& section, const std::string& key, uint32_t fallback) const {
    std::string text = get(section, key, "");
    if (text.empty()) return fallback;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text = text.substr(2);

    char* end = nullptr;
    const unsigned long long value = std::strtoull(text.c_str(), &end, 16);
    if (end == text.c_str()) return fallback;
    if (text.size() <= 6) return 0xFF000000u | static_cast<uint32_t>(value & 0xFFFFFFu);
    return static_cast<uint32_t>(value);
}

OffsetSpec Config::getOffset(const std::string& key) const {
    OffsetSpec spec;
    parseOffsetSpec(get("offsets", key, "0"), spec);
    return spec;
}

int Config::getKey(const std::string& section, const std::string& key, int fallback) const {
    const std::string text = get(section, key, "");
    if (text.empty()) return fallback;
    const int vk = keyNameToVk(text);
    return vk ? vk : fallback;
}

}  // namespace hx
