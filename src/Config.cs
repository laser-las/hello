using System.Globalization;
using System.Text;

namespace Halcyon;

public enum Preset
{
    Legit = 0,
    SemiLegit,
    SemiRage,
    Rage,
    Custom,
}

// One offset the program has to resolve at runtime. Either a module relative
// offset ("0x1A2B3C") or a signature ("sig:48 8B 0D ? ? ? ?") whose RIP-relative
// target is used.
public sealed class OffsetSpec
{
    public string Raw = "0";
    public string PatternText = "";
    public bool Pattern;
    public ulong ModuleOffset;
}

public sealed class AimConfig
{
    public bool Enabled;
    public int Key = Vk.XButton2;
    public bool Toggle;

    // Target selection.
    public int Hitbox = 0;          // 0 head, 1 neck, 2 chest, 3 stomach, 4 nearest to crosshair
    public bool MultiHitbox;
    public bool HitHead = true;
    public bool HitNeck = true;
    public bool HitChest = true;
    public bool HitStomach;
    public int Priority = 2;        // 0 crosshair, 1 distance, 2 fov, 3 health
    public bool TargetSwitch = true;
    public float SwitchDelay = 0.05f;
    public float MaxDistance;
    public bool TeamCheck = true;
    public bool VisibleOnly;
    public bool IgnoreSmoke;
    public bool IgnoreFlashed;
    public bool OnlyScoped;

    // Field of view, in screen pixels from the crosshair.
    public float Fov = 120.0f;
    public bool DynamicFov;
    public float FovMin = 40.0f;
    public float FovMax = 300.0f;
    public float FovScale = 1.0f;

    // Motion.
    public float Smooth = 8.0f;
    public int SmoothMode = 2;
    public float Humanize = 0.15f;
    public float HumanizeJitter = 0.35f;
    public float AimStep;
    public float AimScale = 1.0f;
    public float ReactionTime = 0.06f;
    public float ReactionJitter = 0.03f;
    public float FirstShotDelay;

    // Firing.
    public bool AutoFire;
    public bool AutoScope;
    public bool AutoPistol;
    public bool AutoStop;
    public bool AutoCrouch;

    // Recoil control.
    public bool Rcs = true;
    public bool RcsStandalone;
    public float RcsX = 0.85f;
    public float RcsY = 0.85f;
    public float RcsSmooth = 4.0f;
    public bool RcsScopeOnly;

    // Prediction / backtrack (external approximation).
    public bool Prediction;
    public float PredictMs = 25.0f;
    public bool Backtrack;
    public float BacktrackMs = 80.0f;
    public int ResolverMode;        // 0 off, 1 static, 2 jitter

    public bool DrawFov = true;
    public uint FovColor = 0xFFB0B0B0;
    public float FovAlpha = 0.35f;
    public bool Silent;             // reserved: external client cannot write usercmds
}

public sealed class TriggerConfig
{
    public bool Enabled;
    public int Key;
    public bool Toggle;

    public float Radius = 10.0f;
    public float MaxDistance;
    public bool TeamCheck = true;
    public bool VisibleOnly;
    public bool OnlyScoped;
    public bool WallCheck;

    public bool Randomize = true;
    public int DelayMin = 8;
    public int DelayMax = 26;
    public int HoldMin = 8;
    public int HoldMax = 18;

    public bool Burst;
    public int BurstShots = 3;
    public float BurstDelay = 0.05f;
    public float BurstCooldown = 0.35f;
}

public sealed class EspConfig
{
    public bool Enabled = true;
    public bool TeamCheck = true;
    public float MaxDistance = 4000.0f;
    public bool VisibleCheck;
    public bool OofCheck = true;
    public float FadeStart = 3000.0f;

    public bool Box = true;
    public int BoxStyle = 0;        // 0 full, 1 corner, 2 3D
    public float BoxThickness = 1.6f;
    public bool FilledBox;
    public uint FillColor = 0x22101010;

    public bool HealthBar = true;
    public int HealthBarPos = 0;    // 0 left, 1 right, 2 top
    public bool HealthText;
    public bool ArmorBar;
    public bool Name = true;
    public bool Weapon = true;
    public bool Distance = true;
    public bool Money;
    public bool Ammo;
    public bool HeadDot = true;
    public bool Snapline = true;
    public int SnaplineOrigin = 0;  // 0 bottom, 1 top, 2 center
    public bool Skeleton;
    public float SkeletonThickness = 1.2f;
    public bool Flags;
    public bool Glow;
    public bool OffscreenArrows;
    public bool SoundEsp;
    public float SoundRadius = 800.0f;
    public bool Bomb;
    public bool BombTimer;
    public bool DroppedWeapons;
    public bool Grenades;
    public bool LocalPlayer;
    public int TextSize = 0;
    public bool OutlineText = true;

    public uint ColorVisible = 0xFF35FF6E;
    public uint ColorOccluded = 0xFFFF4040;
    public uint ColorSnapline = 0x9035FF6E;
    public uint ColorText = 0xFFE8E8E8;
    public uint ColorHealth = 0xFF35FF6E;
    public uint ColorArmor = 0xFF4DA6FF;
    public uint ColorSkeleton = 0xFFE8E8E8;
    public uint ColorHeadDot = 0xFFE8E8E8;
    public uint ColorGlow = 0x8035FF6E;
}

public sealed class VisualsConfig
{
    public bool FovChanger;
    public float Fov = 90.0f;
    public bool ThirdPerson;
    public float ThirdPersonDist = 120.0f;
    public bool NoScope;

    public bool Crosshair;
    public int CrosshairStyle = 0;  // 0 cross, 1 dot, 2 circle, 3 t
    public float CrosshairSize = 6.0f;
    public float CrosshairGap = 3.0f;
    public float CrosshairThickness = 1.0f;
    public uint CrosshairColor = 0xFF35FF6E;

    public bool Hitmarker;
    public float HitmarkerSize = 12.0f;
    public uint HitmarkerColor = 0xFFFFFFFF;
    public bool DamageIndicator;

    public bool SpectatorList;
    public bool Watermark = true;
    public bool KeybindList = true;
    public bool FpsCounter;
    public bool PingCounter;
    public bool TimeCounter;

    public bool NoFlash;
    public float FlashAlpha;
    public bool NoSmoke;
    public bool NoDecals;
    public bool NightMode;
    public float NightAlpha = 0.45f;
    public bool RemoveHands;

    public uint Accent = 0xFF7C4DFF;
    public uint AccentDim = 0x667C4DFF;
}

public sealed class RadarConfig
{
    public bool Enabled;
    public float Size = 180.0f;
    public float Zoom = 1.0f;
    public int PosX = 20;
    public int PosY = 20;
    public bool Rotate = true;
    public bool ShowEnemies = true;
    public bool ShowTeam;
    public bool ShowBomb = true;
    public bool ShowDropped;
    public float DotSize = 4.0f;
    public uint ColorEnemy = 0xFFFF4040;
    public uint ColorTeam = 0xFF35FF6E;
    public uint ColorBomb = 0xFFFFAA00;
}

public sealed class MiscConfig
{
    public bool Bhop;
    public int BhopKey = Vk.Space;
    public bool AutoStrafe;
    public float StrafeStrength = 1.0f;
    public bool JumpBug;
    public bool EdgeJump;
    public int EdgeJumpKey;

    public bool AutoPistol;
    public bool AutoReload;
    public bool AutoAccept;
    public bool AutoDefuse;
    public bool AutoBuy;
    public string AutoBuyText = "";

    public bool FakeLatency;
    public int FakeLatencyMs = 80;

    public bool ChatSpam;
    public string ChatSpamText = "hx";
    public float ChatSpamDelay = 2.0f;

    public bool KillSay;
    public string KillSayText = "nice";

    public bool ClanTag;
    public string ClanTagText = "hx";

    public bool AntiAim;
    public int AntiAimMode;
    public bool KnifeBot;
    public bool AutoGG;
    public bool AutoMolotov;
    public bool PreserveKillfeed;
}

public sealed class Config
{
    // [game]
    public string Process = "cs2.exe";
    public string WindowTitle = "Counter-Strike 2";
    public string WindowClass = "SDL_app";
    public string ClientModule = "client.dll";
    public string Layout = "source2";

    // [offsets]
    public OffsetSpec EntityList = new();
    public OffsetSpec LocalPlayer = new();
    public OffsetSpec ViewMatrix = new();

    public int EntityStride = 0x78;
    public int MaxEntities = 64;
    public int Health = 0x344;
    public int Team = 0x3F4;
    public int LifeState = 0x34C;
    public int Origin = 0x1638;
    public int EyeOffset = -1;
    public int BoneMatrix = 0x1A0;
    public bool BoneIndirect;
    public int HeadBone = 6;
    public int Dormant = -1;
    public int Spotted = -1;

    // Optional extended offsets. A value < 0 disables the element.
    public int Name = -1;
    public int Weapon = -1;
    public int Armor = -1;
    public int Scoped = -1;
    public int Flashed = -1;
    public int Defusing = -1;
    public int Money = -1;
    public int Ammo = -1;
    public int Velocity = -1;
    public int AimPunch = -1;
    public int ViewAngles = -1;

    // [features]
    public AimConfig Aim = new();
    public TriggerConfig Trigger = new();
    public EspConfig Esp = new();
    public VisualsConfig Visuals = new();
    public RadarConfig Radar = new();
    public MiscConfig Misc = new();

    // [keys]
    public int MenuKey = Vk.Insert;
    public int QuitKey = Vk.End;
    public int EspToggleKey = Vk.F1;
    public int AimToggleKey = Vk.F2;
    public int TriggerToggleKey = Vk.F3;
    public int PanicKey = Vk.Delete;

    // [loop]
    public int RenderFps;
    public int AimHz = 500;

    public Preset Preset = Preset.Legit;

    private readonly Dictionary<string, string> _values = new(StringComparer.Ordinal);

    public static string PresetName(Preset p) => p switch
    {
        Preset.Legit => "Legit",
        Preset.SemiLegit => "Semi-Legit",
        Preset.SemiRage => "Semi-Rage",
        Preset.Rage => "Rage",
        _ => "Custom",
    };

    public static string PresetKey(Preset p) => p switch
    {
        Preset.Legit => "legit",
        Preset.SemiLegit => "semi-legit",
        Preset.SemiRage => "semi-rage",
        Preset.Rage => "rage",
        _ => "custom",
    };

    public static Preset PresetFromString(string text)
    {
        return text.Trim().ToLowerInvariant() switch
        {
            "legit" => Preset.Legit,
            "semi-legit" or "semilegit" => Preset.SemiLegit,
            "semi-rage" or "semirage" => Preset.SemiRage,
            "rage" => Preset.Rage,
            _ => Preset.Custom,
        };
    }

    public bool Load(string path)
    {
        if (!File.Exists(path)) return false;

        _values.Clear();
        var section = "";
        var first = true;

        foreach (var rawLine in File.ReadLines(path, Encoding.UTF8))
        {
            var line = rawLine;
            if (first && line.Length > 0 && line[0] == '\uFEFF') line = line[1..];
            first = false;

            if (line.EndsWith('\r')) line = line[..^1];
            line = line.Trim();
            if (line.Length == 0 || line[0] == '#' || line[0] == ';') continue;

            if (line[0] == '[')
            {
                var close = line.IndexOf(']');
                if (close > 0) section = line[1..close].Trim().ToLowerInvariant();
                continue;
            }

            var eq = line.IndexOf('=');
            if (eq < 0) continue;

            var key = line[..eq].Trim().ToLowerInvariant();
            var value = line[(eq + 1)..].Trim();

            var hash = value.IndexOf(" #", StringComparison.Ordinal);
            if (hash >= 0) value = value[..hash].Trim();

            if (key.Length > 0 && section.Length > 0) _values[section + "." + key] = value;
        }

        // [game]
        Process = Get("game", "process", Process);
        WindowTitle = Get("game", "window_title", WindowTitle);
        WindowClass = Get("game", "window_class", WindowClass);
        ClientModule = Get("game", "module", ClientModule);
        Layout = Get("game", "layout", Layout).ToLowerInvariant();

        // [offsets]
        EntityList = ParseOffsetSpec(Get("offsets", "entity_list", "0"));
        LocalPlayer = ParseOffsetSpec(Get("offsets", "local_player", "0"));
        ViewMatrix = ParseOffsetSpec(Get("offsets", "view_matrix", "0"));

        EntityStride = GetInt("offsets", "entity_stride", EntityStride);
        MaxEntities = GetInt("offsets", "max_entities", MaxEntities);
        Health = GetInt("offsets", "m_ihealth", Health);
        Team = GetInt("offsets", "m_iteamnum", Team);
        LifeState = GetInt("offsets", "m_lifestate", LifeState);
        Origin = GetInt("offsets", "m_vecorigin", Origin);
        EyeOffset = GetInt("offsets", "m_vecoffset", EyeOffset);
        BoneMatrix = GetInt("offsets", "m_bonematrix", BoneMatrix);
        BoneIndirect = GetBool("offsets", "bone_indirect", BoneIndirect);
        HeadBone = GetInt("offsets", "head_bone", HeadBone);
        Dormant = GetInt("offsets", "m_dormant", Dormant);
        Spotted = GetInt("offsets", "m_bspotted", Spotted);
        Name = GetInt("offsets", "m_szname", Name);
        Weapon = GetInt("offsets", "m_weapon", Weapon);
        Armor = GetInt("offsets", "m_armor", Armor);
        Scoped = GetInt("offsets", "m_scoped", Scoped);
        Flashed = GetInt("offsets", "m_flashed", Flashed);
        Defusing = GetInt("offsets", "m_defusing", Defusing);
        Money = GetInt("offsets", "m_money", Money);
        Ammo = GetInt("offsets", "m_ammo", Ammo);
        Velocity = GetInt("offsets", "m_velocity", Velocity);
        AimPunch = GetInt("offsets", "m_aimpunch", AimPunch);
        ViewAngles = GetInt("offsets", "m_viewangles", ViewAngles);

        // [aim]
        Aim.Enabled = GetBool("aim", "enabled", Aim.Enabled);
        Aim.Key = GetKey("aim", "key", Aim.Key);
        Aim.Toggle = GetBool("aim", "toggle", Aim.Toggle);
        Aim.Hitbox = GetInt("aim", "hitbox", Aim.Hitbox);
        Aim.MultiHitbox = GetBool("aim", "multi_hitbox", Aim.MultiHitbox);
        Aim.HitHead = GetBool("aim", "hit_head", Aim.HitHead);
        Aim.HitNeck = GetBool("aim", "hit_neck", Aim.HitNeck);
        Aim.HitChest = GetBool("aim", "hit_chest", Aim.HitChest);
        Aim.HitStomach = GetBool("aim", "hit_stomach", Aim.HitStomach);
        Aim.Priority = GetInt("aim", "priority", Aim.Priority);
        Aim.TargetSwitch = GetBool("aim", "target_switch", Aim.TargetSwitch);
        Aim.SwitchDelay = GetFloat("aim", "switch_delay", Aim.SwitchDelay);
        Aim.MaxDistance = GetFloat("aim", "max_distance", Aim.MaxDistance);
        Aim.TeamCheck = GetBool("aim", "team_check", Aim.TeamCheck);
        Aim.VisibleOnly = GetBool("aim", "visible_only", Aim.VisibleOnly);
        Aim.IgnoreSmoke = GetBool("aim", "ignore_smoke", Aim.IgnoreSmoke);
        Aim.IgnoreFlashed = GetBool("aim", "ignore_flashed", Aim.IgnoreFlashed);
        Aim.OnlyScoped = GetBool("aim", "only_scoped", Aim.OnlyScoped);
        Aim.Fov = GetFloat("aim", "fov", Aim.Fov);
        Aim.DynamicFov = GetBool("aim", "dynamic_fov", Aim.DynamicFov);
        Aim.FovMin = GetFloat("aim", "fov_min", Aim.FovMin);
        Aim.FovMax = GetFloat("aim", "fov_max", Aim.FovMax);
        Aim.FovScale = GetFloat("aim", "fov_scale", Aim.FovScale);
        Aim.Smooth = GetFloat("aim", "smooth", Aim.Smooth);
        Aim.SmoothMode = GetInt("aim", "smooth_mode", Aim.SmoothMode);
        Aim.Humanize = GetFloat("aim", "humanize", Aim.Humanize);
        Aim.HumanizeJitter = GetFloat("aim", "humanize_jitter", Aim.HumanizeJitter);
        Aim.AimStep = GetFloat("aim", "aim_step", Aim.AimStep);
        Aim.AimScale = GetFloat("aim", "scale", Aim.AimScale);
        Aim.ReactionTime = GetFloat("aim", "reaction_time", Aim.ReactionTime);
        Aim.ReactionJitter = GetFloat("aim", "reaction_jitter", Aim.ReactionJitter);
        Aim.FirstShotDelay = GetFloat("aim", "first_shot_delay", Aim.FirstShotDelay);
        Aim.AutoFire = GetBool("aim", "auto_fire", Aim.AutoFire);
        Aim.AutoScope = GetBool("aim", "auto_scope", Aim.AutoScope);
        Aim.AutoPistol = GetBool("aim", "auto_pistol", Aim.AutoPistol);
        Aim.AutoStop = GetBool("aim", "auto_stop", Aim.AutoStop);
        Aim.AutoCrouch = GetBool("aim", "auto_crouch", Aim.AutoCrouch);
        Aim.Rcs = GetBool("aim", "rcs", Aim.Rcs);
        Aim.RcsStandalone = GetBool("aim", "rcs_standalone", Aim.RcsStandalone);
        Aim.RcsX = GetFloat("aim", "rcs_x", Aim.RcsX);
        Aim.RcsY = GetFloat("aim", "rcs_y", Aim.RcsY);
        Aim.RcsSmooth = GetFloat("aim", "rcs_smooth", Aim.RcsSmooth);
        Aim.RcsScopeOnly = GetBool("aim", "rcs_scope_only", Aim.RcsScopeOnly);
        Aim.Prediction = GetBool("aim", "prediction", Aim.Prediction);
        Aim.PredictMs = GetFloat("aim", "predict_ms", Aim.PredictMs);
        Aim.Backtrack = GetBool("aim", "backtrack", Aim.Backtrack);
        Aim.BacktrackMs = GetFloat("aim", "backtrack_ms", Aim.BacktrackMs);
        Aim.ResolverMode = GetInt("aim", "resolver", Aim.ResolverMode);
        Aim.DrawFov = GetBool("aim", "draw_fov", Aim.DrawFov);
        Aim.FovColor = GetColor("aim", "fov_color", Aim.FovColor);
        Aim.FovAlpha = GetFloat("aim", "fov_alpha", Aim.FovAlpha);
        Aim.Silent = GetBool("aim", "silent", Aim.Silent);

        // [trigger]
        Trigger.Enabled = GetBool("trigger", "enabled", Trigger.Enabled);
        Trigger.Key = GetKey("trigger", "key", Trigger.Key);
        Trigger.Toggle = GetBool("trigger", "toggle", Trigger.Toggle);
        Trigger.Radius = GetFloat("trigger", "radius", Trigger.Radius);
        Trigger.MaxDistance = GetFloat("trigger", "max_distance", Trigger.MaxDistance);
        Trigger.TeamCheck = GetBool("trigger", "team_check", Trigger.TeamCheck);
        Trigger.VisibleOnly = GetBool("trigger", "visible_only", Trigger.VisibleOnly);
        Trigger.OnlyScoped = GetBool("trigger", "only_scoped", Trigger.OnlyScoped);
        Trigger.WallCheck = GetBool("trigger", "wall_check", Trigger.WallCheck);
        Trigger.Randomize = GetBool("trigger", "randomize", Trigger.Randomize);
        Trigger.DelayMin = GetInt("trigger", "delay_min_ms", Trigger.DelayMin);
        Trigger.DelayMax = GetInt("trigger", "delay_max_ms", Trigger.DelayMax);
        Trigger.HoldMin = GetInt("trigger", "hold_min_ms", Trigger.HoldMin);
        Trigger.HoldMax = GetInt("trigger", "hold_max_ms", Trigger.HoldMax);
        Trigger.Burst = GetBool("trigger", "burst", Trigger.Burst);
        Trigger.BurstShots = GetInt("trigger", "burst_shots", Trigger.BurstShots);
        Trigger.BurstDelay = GetFloat("trigger", "burst_delay", Trigger.BurstDelay);
        Trigger.BurstCooldown = GetFloat("trigger", "burst_cooldown", Trigger.BurstCooldown);

        // [esp]
        Esp.Enabled = GetBool("esp", "enabled", Esp.Enabled);
        Esp.TeamCheck = GetBool("esp", "team_check", Esp.TeamCheck);
        Esp.MaxDistance = GetFloat("esp", "max_distance", Esp.MaxDistance);
        Esp.VisibleCheck = GetBool("esp", "visible_check", Esp.VisibleCheck);
        Esp.OofCheck = GetBool("esp", "oof_check", Esp.OofCheck);
        Esp.FadeStart = GetFloat("esp", "fade_start", Esp.FadeStart);
        Esp.Box = GetBool("esp", "box", Esp.Box);
        Esp.BoxStyle = GetInt("esp", "box_style", Esp.BoxStyle);
        Esp.BoxThickness = GetFloat("esp", "thickness", Esp.BoxThickness);
        Esp.FilledBox = GetBool("esp", "filled_box", Esp.FilledBox);
        Esp.FillColor = GetColor("esp", "fill_color", Esp.FillColor);
        Esp.HealthBar = GetBool("esp", "health_bar", Esp.HealthBar);
        Esp.HealthBarPos = GetInt("esp", "health_bar_pos", Esp.HealthBarPos);
        Esp.HealthText = GetBool("esp", "health_text", Esp.HealthText);
        Esp.ArmorBar = GetBool("esp", "armor_bar", Esp.ArmorBar);
        Esp.Name = GetBool("esp", "name", Esp.Name);
        Esp.Weapon = GetBool("esp", "weapon", Esp.Weapon);
        Esp.Distance = GetBool("esp", "distance", Esp.Distance);
        Esp.Money = GetBool("esp", "money", Esp.Money);
        Esp.Ammo = GetBool("esp", "ammo", Esp.Ammo);
        Esp.HeadDot = GetBool("esp", "head_dot", Esp.HeadDot);
        Esp.Snapline = GetBool("esp", "snapline", Esp.Snapline);
        Esp.SnaplineOrigin = GetInt("esp", "snapline_origin", Esp.SnaplineOrigin);
        Esp.Skeleton = GetBool("esp", "skeleton", Esp.Skeleton);
        Esp.SkeletonThickness = GetFloat("esp", "skeleton_thickness", Esp.SkeletonThickness);
        Esp.Flags = GetBool("esp", "flags", Esp.Flags);
        Esp.Glow = GetBool("esp", "glow", Esp.Glow);
        Esp.OffscreenArrows = GetBool("esp", "offscreen_arrows", Esp.OffscreenArrows);
        Esp.SoundEsp = GetBool("esp", "sound_esp", Esp.SoundEsp);
        Esp.SoundRadius = GetFloat("esp", "sound_radius", Esp.SoundRadius);
        Esp.Bomb = GetBool("esp", "bomb", Esp.Bomb);
        Esp.BombTimer = GetBool("esp", "bomb_timer", Esp.BombTimer);
        Esp.DroppedWeapons = GetBool("esp", "dropped_weapons", Esp.DroppedWeapons);
        Esp.Grenades = GetBool("esp", "grenades", Esp.Grenades);
        Esp.LocalPlayer = GetBool("esp", "local_player", Esp.LocalPlayer);
        Esp.TextSize = GetInt("esp", "text_size", Esp.TextSize);
        Esp.OutlineText = GetBool("esp", "outline_text", Esp.OutlineText);
        Esp.ColorVisible = GetColor("esp", "color_visible", Esp.ColorVisible);
        Esp.ColorOccluded = GetColor("esp", "color_occluded", Esp.ColorOccluded);
        Esp.ColorSnapline = GetColor("esp", "color_snapline", Esp.ColorSnapline);
        Esp.ColorText = GetColor("esp", "color_text", Esp.ColorText);
        Esp.ColorHealth = GetColor("esp", "color_health", Esp.ColorHealth);
        Esp.ColorArmor = GetColor("esp", "color_armor", Esp.ColorArmor);
        Esp.ColorSkeleton = GetColor("esp", "color_skeleton", Esp.ColorSkeleton);
        Esp.ColorHeadDot = GetColor("esp", "color_head_dot", Esp.ColorHeadDot);
        Esp.ColorGlow = GetColor("esp", "color_glow", Esp.ColorGlow);

        // [visuals]
        Visuals.FovChanger = GetBool("visuals", "fov_changer", Visuals.FovChanger);
        Visuals.Fov = GetFloat("visuals", "fov", Visuals.Fov);
        Visuals.ThirdPerson = GetBool("visuals", "third_person", Visuals.ThirdPerson);
        Visuals.ThirdPersonDist = GetFloat("visuals", "third_person_dist", Visuals.ThirdPersonDist);
        Visuals.NoScope = GetBool("visuals", "no_scope", Visuals.NoScope);
        Visuals.Crosshair = GetBool("visuals", "crosshair", Visuals.Crosshair);
        Visuals.CrosshairStyle = GetInt("visuals", "crosshair_style", Visuals.CrosshairStyle);
        Visuals.CrosshairSize = GetFloat("visuals", "crosshair_size", Visuals.CrosshairSize);
        Visuals.CrosshairGap = GetFloat("visuals", "crosshair_gap", Visuals.CrosshairGap);
        Visuals.CrosshairThickness = GetFloat("visuals", "crosshair_thickness", Visuals.CrosshairThickness);
        Visuals.CrosshairColor = GetColor("visuals", "crosshair_color", Visuals.CrosshairColor);
        Visuals.Hitmarker = GetBool("visuals", "hitmarker", Visuals.Hitmarker);
        Visuals.HitmarkerSize = GetFloat("visuals", "hitmarker_size", Visuals.HitmarkerSize);
        Visuals.HitmarkerColor = GetColor("visuals", "hitmarker_color", Visuals.HitmarkerColor);
        Visuals.DamageIndicator = GetBool("visuals", "damage_indicator", Visuals.DamageIndicator);
        Visuals.SpectatorList = GetBool("visuals", "spectator_list", Visuals.SpectatorList);
        Visuals.Watermark = GetBool("visuals", "watermark", Visuals.Watermark);
        Visuals.KeybindList = GetBool("visuals", "keybind_list", Visuals.KeybindList);
        Visuals.FpsCounter = GetBool("visuals", "fps_counter", Visuals.FpsCounter);
        Visuals.PingCounter = GetBool("visuals", "ping_counter", Visuals.PingCounter);
        Visuals.TimeCounter = GetBool("visuals", "time_counter", Visuals.TimeCounter);
        Visuals.NoFlash = GetBool("visuals", "no_flash", Visuals.NoFlash);
        Visuals.FlashAlpha = GetFloat("visuals", "flash_alpha", Visuals.FlashAlpha);
        Visuals.NoSmoke = GetBool("visuals", "no_smoke", Visuals.NoSmoke);
        Visuals.NoDecals = GetBool("visuals", "no_decals", Visuals.NoDecals);
        Visuals.NightMode = GetBool("visuals", "night_mode", Visuals.NightMode);
        Visuals.NightAlpha = GetFloat("visuals", "night_alpha", Visuals.NightAlpha);
        Visuals.RemoveHands = GetBool("visuals", "remove_hands", Visuals.RemoveHands);
        Visuals.Accent = GetColor("visuals", "accent", Visuals.Accent);
        Visuals.AccentDim = GetColor("visuals", "accent_dim", Visuals.AccentDim);

        // [radar]
        Radar.Enabled = GetBool("radar", "enabled", Radar.Enabled);
        Radar.Size = GetFloat("radar", "size", Radar.Size);
        Radar.Zoom = GetFloat("radar", "zoom", Radar.Zoom);
        Radar.PosX = GetInt("radar", "pos_x", Radar.PosX);
        Radar.PosY = GetInt("radar", "pos_y", Radar.PosY);
        Radar.Rotate = GetBool("radar", "rotate", Radar.Rotate);
        Radar.ShowEnemies = GetBool("radar", "show_enemies", Radar.ShowEnemies);
        Radar.ShowTeam = GetBool("radar", "show_team", Radar.ShowTeam);
        Radar.ShowBomb = GetBool("radar", "show_bomb", Radar.ShowBomb);
        Radar.ShowDropped = GetBool("radar", "show_dropped", Radar.ShowDropped);
        Radar.DotSize = GetFloat("radar", "dot_size", Radar.DotSize);
        Radar.ColorEnemy = GetColor("radar", "color_enemy", Radar.ColorEnemy);
        Radar.ColorTeam = GetColor("radar", "color_team", Radar.ColorTeam);
        Radar.ColorBomb = GetColor("radar", "color_bomb", Radar.ColorBomb);

        // [misc]
        Misc.Bhop = GetBool("misc", "bhop", Misc.Bhop);
        Misc.BhopKey = GetKey("misc", "bhop_key", Misc.BhopKey);
        Misc.AutoStrafe = GetBool("misc", "auto_strafe", Misc.AutoStrafe);
        Misc.StrafeStrength = GetFloat("misc", "strafe_strength", Misc.StrafeStrength);
        Misc.JumpBug = GetBool("misc", "jump_bug", Misc.JumpBug);
        Misc.EdgeJump = GetBool("misc", "edge_jump", Misc.EdgeJump);
        Misc.EdgeJumpKey = GetKey("misc", "edge_jump_key", Misc.EdgeJumpKey);
        Misc.AutoPistol = GetBool("misc", "auto_pistol", Misc.AutoPistol);
        Misc.AutoReload = GetBool("misc", "auto_reload", Misc.AutoReload);
        Misc.AutoAccept = GetBool("misc", "auto_accept", Misc.AutoAccept);
        Misc.AutoDefuse = GetBool("misc", "auto_defuse", Misc.AutoDefuse);
        Misc.AutoBuy = GetBool("misc", "auto_buy", Misc.AutoBuy);
        Misc.AutoBuyText = Get("misc", "auto_buy_text", Misc.AutoBuyText);
        Misc.FakeLatency = GetBool("misc", "fake_latency", Misc.FakeLatency);
        Misc.FakeLatencyMs = GetInt("misc", "fake_latency_ms", Misc.FakeLatencyMs);
        Misc.ChatSpam = GetBool("misc", "chat_spam", Misc.ChatSpam);
        Misc.ChatSpamText = Get("misc", "chat_spam_text", Misc.ChatSpamText);
        Misc.ChatSpamDelay = GetFloat("misc", "chat_spam_delay", Misc.ChatSpamDelay);
        Misc.KillSay = GetBool("misc", "kill_say", Misc.KillSay);
        Misc.KillSayText = Get("misc", "kill_say_text", Misc.KillSayText);
        Misc.ClanTag = GetBool("misc", "clan_tag", Misc.ClanTag);
        Misc.ClanTagText = Get("misc", "clan_tag_text", Misc.ClanTagText);
        Misc.AntiAim = GetBool("misc", "anti_aim", Misc.AntiAim);
        Misc.AntiAimMode = GetInt("misc", "anti_aim_mode", Misc.AntiAimMode);
        Misc.KnifeBot = GetBool("misc", "knife_bot", Misc.KnifeBot);
        Misc.AutoGG = GetBool("misc", "auto_gg", Misc.AutoGG);
        Misc.AutoMolotov = GetBool("misc", "auto_molotov", Misc.AutoMolotov);
        Misc.PreserveKillfeed = GetBool("misc", "preserve_killfeed", Misc.PreserveKillfeed);

        // [keys]
        MenuKey = GetKey("keys", "menu", MenuKey);
        QuitKey = GetKey("keys", "quit", QuitKey);
        EspToggleKey = GetKey("keys", "esp_toggle", EspToggleKey);
        AimToggleKey = GetKey("keys", "aim_toggle", AimToggleKey);
        TriggerToggleKey = GetKey("keys", "trigger_toggle", TriggerToggleKey);
        PanicKey = GetKey("keys", "panic", PanicKey);

        // [loop]
        RenderFps = GetInt("loop", "render_fps", RenderFps);
        AimHz = GetInt("loop", "aim_hz", AimHz);

        Preset = PresetFromString(Get("general", "preset", PresetKey(Preset)));

        // Sanity.
        if (Aim.Smooth < 1.0f) Aim.Smooth = 1.0f;
        if (Aim.AimScale <= 0.0f) Aim.AimScale = 1.0f;
        if (Trigger.DelayMax < Trigger.DelayMin) Trigger.DelayMax = Trigger.DelayMin;
        if (Trigger.HoldMax < Trigger.HoldMin) Trigger.HoldMax = Trigger.HoldMin;
        if (AimHz < 30) AimHz = 30;
        if (AimHz > 2000) AimHz = 2000;
        if (Esp.MaxDistance <= 0.0f) Esp.MaxDistance = 4000.0f;

        return true;
    }

    public int ImportOffsets(string path)
    {
        if (!File.Exists(path))
        {
            Log.Info("import: cannot open {0}", path);
            return 0;
        }

        var applied = 0;

        foreach (var rawLine in File.ReadLines(path))
        {
            var cleaned = new StringBuilder(rawLine.Length);
            foreach (var c in rawLine)
            {
                if (c is '"' or '{' or '}' or '[' or ']' or ',') continue;
                cleaned.Append(c);
            }

            var line = cleaned.ToString();
            var sep = line.IndexOfAny(new[] { '=', ':' });
            if (sep < 0) continue;

            var key = line[..sep].Trim().ToLowerInvariant();
            var value = line[(sep + 1)..].Trim();
            if (key.Length == 0 || value.Length == 0) continue;

            var dot = key.LastIndexOf('.');
            if (dot >= 0) key = key[(dot + 1)..];

            key = new string(key.Where(c => char.IsLetterOrDigit(c) || c == '_').ToArray());
            if (key.Length == 0) continue;

            if (!TryParseULongAuto(value, out var v)) continue;

            switch (key)
            {
                case "dwentitylist":
                    SetAddress(EntityList, v);
                    break;
                case "dwlocalplayer":
                case "dwlocalplayerpawn":
                case "dwlocalplayercontroller":
                    SetAddress(LocalPlayer, v);
                    break;
                case "dwviewmatrix":
                    SetAddress(ViewMatrix, v);
                    break;
                case "m_ihealth":
                    Health = (int)v;
                    break;
                case "m_iteamnum":
                    Team = (int)v;
                    break;
                case "m_lifestate":
                    LifeState = (int)v;
                    break;
                case "m_vecabsorigin":
                case "m_vecorigin":
                    Origin = (int)v;
                    break;
                case "m_vecoffset":
                case "m_vecviewoffset":
                    EyeOffset = (int)v;
                    break;
                case "m_pbonearray":
                case "m_dwbonematrix":
                case "m_modelstate":
                    BoneMatrix = (int)v;
                    break;
                case "m_bdormant":
                    Dormant = (int)v;
                    break;
                case "m_bspotted":
                    Spotted = (int)v;
                    break;
                case "m_szname":
                case "m_iszplayername":
                    Name = (int)v;
                    break;
                case "m_iitemdefinitionindex":
                    Weapon = (int)v;
                    break;
                case "m_armorvalue":
                case "m_iarmor":
                    Armor = (int)v;
                    break;
                case "m_bisscoped":
                    Scoped = (int)v;
                    break;
                case "m_flflashduration":
                    Flashed = (int)v;
                    break;
                case "m_vecabsvelocity":
                case "m_vecvelocity":
                    Velocity = (int)v;
                    break;
                case "m_aimpunchangle":
                    AimPunch = (int)v;
                    break;
                case "m_angviewangles":
                case "m_angclientviewangles":
                    ViewAngles = (int)v;
                    break;
                default:
                    continue;
            }

            ++applied;
        }

        Log.Info("import: {0} entries applied from {1}", applied, path);
        return applied;
    }

    public bool Save(string path)
    {
        try
        {
            using var f = new StreamWriter(path, false, new UTF8Encoding(false));
            f.NewLine = "\n";

            void O(string key, string value) => f.Write(key + " = " + value + "\n");
            void Ob(string key, bool value) => O(key, value ? "1" : "0");
            void Oi(string key, int value) => O(key, value.ToString(CultureInfo.InvariantCulture));
            void Of(string key, float value) => O(key, value.ToString("0.0###", CultureInfo.InvariantCulture));
            void Oc(string key, uint value) => O(key, "0x" + value.ToString("X8", CultureInfo.InvariantCulture));

            f.Write("# halcyon configuration\n\n");

            f.Write("[general]\n");
            O("preset", PresetKey(Preset));

            f.Write("\n[game]\n");
            O("process", Process);
            O("window_title", WindowTitle);
            O("window_class", WindowClass);
            O("module", ClientModule);
            O("layout", Layout);

            f.Write("\n[offsets]\n");
            O("entity_list", EntityList.Raw);
            O("local_player", LocalPlayer.Raw);
            O("view_matrix", ViewMatrix.Raw);
            Oi("entity_stride", EntityStride);
            Oi("max_entities", MaxEntities);
            Oi("m_ihealth", Health);
            Oi("m_iteamnum", Team);
            Oi("m_lifestate", LifeState);
            Oi("m_vecorigin", Origin);
            Oi("m_vecoffset", EyeOffset);
            Oi("m_bonematrix", BoneMatrix);
            Ob("bone_indirect", BoneIndirect);
            Oi("head_bone", HeadBone);
            Oi("m_dormant", Dormant);
            Oi("m_bspotted", Spotted);
            Oi("m_szname", Name);
            Oi("m_weapon", Weapon);
            Oi("m_armor", Armor);
            Oi("m_scoped", Scoped);
            Oi("m_flashed", Flashed);
            Oi("m_defusing", Defusing);
            Oi("m_money", Money);
            Oi("m_ammo", Ammo);
            Oi("m_velocity", Velocity);
            Oi("m_aimpunch", AimPunch);
            Oi("m_viewangles", ViewAngles);

            f.Write("\n[aim]\n");
            Ob("enabled", Aim.Enabled);
            Oi("key", Aim.Key);
            Ob("toggle", Aim.Toggle);
            Oi("hitbox", Aim.Hitbox);
            Ob("multi_hitbox", Aim.MultiHitbox);
            Ob("hit_head", Aim.HitHead);
            Ob("hit_neck", Aim.HitNeck);
            Ob("hit_chest", Aim.HitChest);
            Ob("hit_stomach", Aim.HitStomach);
            Oi("priority", Aim.Priority);
            Ob("target_switch", Aim.TargetSwitch);
            Of("switch_delay", Aim.SwitchDelay);
            Of("max_distance", Aim.MaxDistance);
            Ob("team_check", Aim.TeamCheck);
            Ob("visible_only", Aim.VisibleOnly);
            Ob("ignore_smoke", Aim.IgnoreSmoke);
            Ob("ignore_flashed", Aim.IgnoreFlashed);
            Ob("only_scoped", Aim.OnlyScoped);
            Of("fov", Aim.Fov);
            Ob("dynamic_fov", Aim.DynamicFov);
            Of("fov_min", Aim.FovMin);
            Of("fov_max", Aim.FovMax);
            Of("fov_scale", Aim.FovScale);
            Of("smooth", Aim.Smooth);
            Oi("smooth_mode", Aim.SmoothMode);
            Of("humanize", Aim.Humanize);
            Of("humanize_jitter", Aim.HumanizeJitter);
            Of("aim_step", Aim.AimStep);
            Of("scale", Aim.AimScale);
            Of("reaction_time", Aim.ReactionTime);
            Of("reaction_jitter", Aim.ReactionJitter);
            Of("first_shot_delay", Aim.FirstShotDelay);
            Ob("auto_fire", Aim.AutoFire);
            Ob("auto_scope", Aim.AutoScope);
            Ob("auto_pistol", Aim.AutoPistol);
            Ob("auto_stop", Aim.AutoStop);
            Ob("auto_crouch", Aim.AutoCrouch);
            Ob("rcs", Aim.Rcs);
            Ob("rcs_standalone", Aim.RcsStandalone);
            Of("rcs_x", Aim.RcsX);
            Of("rcs_y", Aim.RcsY);
            Of("rcs_smooth", Aim.RcsSmooth);
            Ob("rcs_scope_only", Aim.RcsScopeOnly);
            Ob("prediction", Aim.Prediction);
            Of("predict_ms", Aim.PredictMs);
            Ob("backtrack", Aim.Backtrack);
            Of("backtrack_ms", Aim.BacktrackMs);
            Oi("resolver", Aim.ResolverMode);
            Ob("draw_fov", Aim.DrawFov);
            Oc("fov_color", Aim.FovColor);
            Of("fov_alpha", Aim.FovAlpha);
            Ob("silent", Aim.Silent);

            f.Write("\n[trigger]\n");
            Ob("enabled", Trigger.Enabled);
            Oi("key", Trigger.Key);
            Ob("toggle", Trigger.Toggle);
            Of("radius", Trigger.Radius);
            Of("max_distance", Trigger.MaxDistance);
            Ob("team_check", Trigger.TeamCheck);
            Ob("visible_only", Trigger.VisibleOnly);
            Ob("only_scoped", Trigger.OnlyScoped);
            Ob("wall_check", Trigger.WallCheck);
            Ob("randomize", Trigger.Randomize);
            Oi("delay_min_ms", Trigger.DelayMin);
            Oi("delay_max_ms", Trigger.DelayMax);
            Oi("hold_min_ms", Trigger.HoldMin);
            Oi("hold_max_ms", Trigger.HoldMax);
            Ob("burst", Trigger.Burst);
            Oi("burst_shots", Trigger.BurstShots);
            Of("burst_delay", Trigger.BurstDelay);
            Of("burst_cooldown", Trigger.BurstCooldown);

            f.Write("\n[esp]\n");
            Ob("enabled", Esp.Enabled);
            Ob("team_check", Esp.TeamCheck);
            Of("max_distance", Esp.MaxDistance);
            Ob("visible_check", Esp.VisibleCheck);
            Ob("oof_check", Esp.OofCheck);
            Of("fade_start", Esp.FadeStart);
            Ob("box", Esp.Box);
            Oi("box_style", Esp.BoxStyle);
            Of("thickness", Esp.BoxThickness);
            Ob("filled_box", Esp.FilledBox);
            Oc("fill_color", Esp.FillColor);
            Ob("health_bar", Esp.HealthBar);
            Oi("health_bar_pos", Esp.HealthBarPos);
            Ob("health_text", Esp.HealthText);
            Ob("armor_bar", Esp.ArmorBar);
            Ob("name", Esp.Name);
            Ob("weapon", Esp.Weapon);
            Ob("distance", Esp.Distance);
            Ob("money", Esp.Money);
            Ob("ammo", Esp.Ammo);
            Ob("head_dot", Esp.HeadDot);
            Ob("snapline", Esp.Snapline);
            Oi("snapline_origin", Esp.SnaplineOrigin);
            Ob("skeleton", Esp.Skeleton);
            Of("skeleton_thickness", Esp.SkeletonThickness);
            Ob("flags", Esp.Flags);
            Ob("glow", Esp.Glow);
            Ob("offscreen_arrows", Esp.OffscreenArrows);
            Ob("sound_esp", Esp.SoundEsp);
            Of("sound_radius", Esp.SoundRadius);
            Ob("bomb", Esp.Bomb);
            Ob("bomb_timer", Esp.BombTimer);
            Ob("dropped_weapons", Esp.DroppedWeapons);
            Ob("grenades", Esp.Grenades);
            Ob("local_player", Esp.LocalPlayer);
            Oi("text_size", Esp.TextSize);
            Ob("outline_text", Esp.OutlineText);
            Oc("color_visible", Esp.ColorVisible);
            Oc("color_occluded", Esp.ColorOccluded);
            Oc("color_snapline", Esp.ColorSnapline);
            Oc("color_text", Esp.ColorText);
            Oc("color_health", Esp.ColorHealth);
            Oc("color_armor", Esp.ColorArmor);
            Oc("color_skeleton", Esp.ColorSkeleton);
            Oc("color_head_dot", Esp.ColorHeadDot);
            Oc("color_glow", Esp.ColorGlow);

            f.Write("\n[visuals]\n");
            Ob("fov_changer", Visuals.FovChanger);
            Of("fov", Visuals.Fov);
            Ob("third_person", Visuals.ThirdPerson);
            Of("third_person_dist", Visuals.ThirdPersonDist);
            Ob("no_scope", Visuals.NoScope);
            Ob("crosshair", Visuals.Crosshair);
            Oi("crosshair_style", Visuals.CrosshairStyle);
            Of("crosshair_size", Visuals.CrosshairSize);
            Of("crosshair_gap", Visuals.CrosshairGap);
            Of("crosshair_thickness", Visuals.CrosshairThickness);
            Oc("crosshair_color", Visuals.CrosshairColor);
            Ob("hitmarker", Visuals.Hitmarker);
            Of("hitmarker_size", Visuals.HitmarkerSize);
            Oc("hitmarker_color", Visuals.HitmarkerColor);
            Ob("damage_indicator", Visuals.DamageIndicator);
            Ob("spectator_list", Visuals.SpectatorList);
            Ob("watermark", Visuals.Watermark);
            Ob("keybind_list", Visuals.KeybindList);
            Ob("fps_counter", Visuals.FpsCounter);
            Ob("ping_counter", Visuals.PingCounter);
            Ob("time_counter", Visuals.TimeCounter);
            Ob("no_flash", Visuals.NoFlash);
            Of("flash_alpha", Visuals.FlashAlpha);
            Ob("no_smoke", Visuals.NoSmoke);
            Ob("no_decals", Visuals.NoDecals);
            Ob("night_mode", Visuals.NightMode);
            Of("night_alpha", Visuals.NightAlpha);
            Ob("remove_hands", Visuals.RemoveHands);
            Oc("accent", Visuals.Accent);
            Oc("accent_dim", Visuals.AccentDim);

            f.Write("\n[radar]\n");
            Ob("enabled", Radar.Enabled);
            Of("size", Radar.Size);
            Of("zoom", Radar.Zoom);
            Oi("pos_x", Radar.PosX);
            Oi("pos_y", Radar.PosY);
            Ob("rotate", Radar.Rotate);
            Ob("show_enemies", Radar.ShowEnemies);
            Ob("show_team", Radar.ShowTeam);
            Ob("show_bomb", Radar.ShowBomb);
            Ob("show_dropped", Radar.ShowDropped);
            Of("dot_size", Radar.DotSize);
            Oc("color_enemy", Radar.ColorEnemy);
            Oc("color_team", Radar.ColorTeam);
            Oc("color_bomb", Radar.ColorBomb);

            f.Write("\n[misc]\n");
            Ob("bhop", Misc.Bhop);
            Oi("bhop_key", Misc.BhopKey);
            Ob("auto_strafe", Misc.AutoStrafe);
            Of("strafe_strength", Misc.StrafeStrength);
            Ob("jump_bug", Misc.JumpBug);
            Ob("edge_jump", Misc.EdgeJump);
            Oi("edge_jump_key", Misc.EdgeJumpKey);
            Ob("auto_pistol", Misc.AutoPistol);
            Ob("auto_reload", Misc.AutoReload);
            Ob("auto_accept", Misc.AutoAccept);
            Ob("auto_defuse", Misc.AutoDefuse);
            Ob("auto_buy", Misc.AutoBuy);
            O("auto_buy_text", Misc.AutoBuyText);
            Ob("fake_latency", Misc.FakeLatency);
            Oi("fake_latency_ms", Misc.FakeLatencyMs);
            Ob("chat_spam", Misc.ChatSpam);
            O("chat_spam_text", Misc.ChatSpamText);
            Of("chat_spam_delay", Misc.ChatSpamDelay);
            Ob("kill_say", Misc.KillSay);
            O("kill_say_text", Misc.KillSayText);
            Ob("clan_tag", Misc.ClanTag);
            O("clan_tag_text", Misc.ClanTagText);
            Ob("anti_aim", Misc.AntiAim);
            Oi("anti_aim_mode", Misc.AntiAimMode);
            Ob("knife_bot", Misc.KnifeBot);
            Ob("auto_gg", Misc.AutoGG);
            Ob("auto_molotov", Misc.AutoMolotov);
            Ob("preserve_killfeed", Misc.PreserveKillfeed);

            f.Write("\n[keys]\n");
            Oi("menu", MenuKey);
            Oi("quit", QuitKey);
            Oi("esp_toggle", EspToggleKey);
            Oi("aim_toggle", AimToggleKey);
            Oi("trigger_toggle", TriggerToggleKey);
            Oi("panic", PanicKey);

            f.Write("\n[loop]\n");
            Oi("render_fps", RenderFps);
            Oi("aim_hz", AimHz);

            return true;
        }
        catch (Exception ex)
        {
            Log.Info("config: could not write {0}: {1}", path, ex.Message);
            return false;
        }
    }

    public void ApplyPreset(Preset p)
    {
        Preset = p;
        if (p == Preset.Custom) return;

        // Shared defaults, then per-preset overrides.
        Aim.Enabled = true;
        Aim.TeamCheck = true;
        Aim.MultiHitbox = false;
        Aim.HitHead = true;
        Aim.HitNeck = true;
        Aim.HitChest = true;
        Aim.HitStomach = false;
        Aim.AutoStop = false;
        Aim.AutoCrouch = false;
        Aim.Prediction = false;
        Aim.Backtrack = false;
        Aim.Silent = false;
        Aim.RcsStandalone = false;
        Aim.RcsScopeOnly = false;
        Aim.OnlyScoped = false;
        Aim.IgnoreFlashed = false;
        Aim.IgnoreSmoke = false;
        Aim.ResolverMode = 0;

        Trigger.Enabled = true;
        Trigger.TeamCheck = true;
        Trigger.VisibleOnly = false;
        Trigger.OnlyScoped = false;
        Trigger.Burst = false;

        Esp.Enabled = true;
        Esp.TeamCheck = true;
        Esp.Skeleton = false;
        Esp.Glow = false;
        Esp.OffscreenArrows = false;
        Esp.FilledBox = false;
        Esp.ArmorBar = false;
        Esp.Money = false;
        Esp.Ammo = false;
        Esp.SoundEsp = false;
        Esp.LocalPlayer = false;

        Visuals.NightMode = false;
        Visuals.RemoveHands = false;

        Misc.Bhop = false;
        Misc.AutoStrafe = false;
        Misc.JumpBug = false;
        Misc.EdgeJump = false;
        Misc.AutoPistol = false;
        Misc.AutoReload = false;
        Misc.AutoAccept = false;
        Misc.AutoDefuse = false;
        Misc.AutoBuy = false;
        Misc.FakeLatency = false;
        Misc.ChatSpam = false;
        Misc.KillSay = false;
        Misc.ClanTag = false;
        Misc.AntiAim = false;
        Misc.KnifeBot = false;
        Misc.AutoGG = false;
        Misc.AutoMolotov = false;
        Misc.PreserveKillfeed = false;

        switch (p)
        {
            case Preset.Legit:
                Aim.Key = Vk.XButton2;
                Aim.Fov = 55.0f;
                Aim.DynamicFov = false;
                Aim.Smooth = 16.0f;
                Aim.SmoothMode = 2;
                Aim.Humanize = 0.45f;
                Aim.HumanizeJitter = 0.5f;
                Aim.ReactionTime = 0.16f;
                Aim.ReactionJitter = 0.06f;
                Aim.AimStep = 30.0f;
                Aim.AimScale = 0.9f;
                Aim.VisibleOnly = true;
                Aim.AutoFire = false;
                Aim.AutoScope = false;
                Aim.AutoPistol = false;
                Aim.Rcs = true;
                Aim.RcsX = 0.55f;
                Aim.RcsY = 0.55f;
                Aim.RcsSmooth = 6.0f;
                Aim.Priority = 0;
                Aim.Hitbox = 0;
                Trigger.Radius = 5.0f;
                Trigger.DelayMin = 28;
                Trigger.DelayMax = 70;
                Trigger.HoldMin = 20;
                Trigger.HoldMax = 45;
                Trigger.Randomize = true;
                Esp.Box = true;
                Esp.BoxStyle = 1;
                Esp.HealthBar = true;
                Esp.HealthText = false;
                Esp.Name = true;
                Esp.Weapon = false;
                Esp.Distance = false;
                Esp.HeadDot = false;
                Esp.Snapline = false;
                Esp.Skeleton = false;
                Esp.Flags = false;
                Esp.Glow = false;
                Esp.OffscreenArrows = false;
                break;

            case Preset.SemiLegit:
                Aim.Key = Vk.XButton2;
                Aim.Fov = 85.0f;
                Aim.DynamicFov = true;
                Aim.FovMin = 45.0f;
                Aim.FovMax = 130.0f;
                Aim.Smooth = 9.0f;
                Aim.SmoothMode = 2;
                Aim.Humanize = 0.25f;
                Aim.ReactionTime = 0.10f;
                Aim.ReactionJitter = 0.04f;
                Aim.AimStep = 0.0f;
                Aim.AimScale = 1.0f;
                Aim.VisibleOnly = true;
                Aim.AutoFire = false;
                Aim.AutoScope = true;
                Aim.AutoPistol = true;
                Aim.Rcs = true;
                Aim.RcsX = 0.75f;
                Aim.RcsY = 0.75f;
                Aim.RcsSmooth = 4.0f;
                Aim.Priority = 2;
                Aim.Hitbox = 0;
                Trigger.Radius = 8.0f;
                Trigger.DelayMin = 14;
                Trigger.DelayMax = 38;
                Trigger.HoldMin = 12;
                Trigger.HoldMax = 28;
                Esp.Box = true;
                Esp.BoxStyle = 0;
                Esp.HealthBar = true;
                Esp.HealthText = true;
                Esp.Name = true;
                Esp.Weapon = true;
                Esp.Distance = true;
                Esp.HeadDot = true;
                Esp.Snapline = true;
                Esp.Skeleton = false;
                Esp.Flags = true;
                Esp.Glow = false;
                break;

            case Preset.SemiRage:
                Aim.Key = Vk.XButton2;
                Aim.Fov = 170.0f;
                Aim.DynamicFov = true;
                Aim.FovMin = 90.0f;
                Aim.FovMax = 260.0f;
                Aim.Smooth = 3.5f;
                Aim.SmoothMode = 1;
                Aim.Humanize = 0.08f;
                Aim.ReactionTime = 0.04f;
                Aim.ReactionJitter = 0.015f;
                Aim.AimStep = 0.0f;
                Aim.AimScale = 1.1f;
                Aim.VisibleOnly = false;
                Aim.AutoFire = true;
                Aim.AutoScope = true;
                Aim.AutoPistol = true;
                Aim.AutoStop = true;
                Aim.Rcs = true;
                Aim.RcsX = 0.92f;
                Aim.RcsY = 0.92f;
                Aim.RcsSmooth = 2.0f;
                Aim.Priority = 2;
                Aim.Hitbox = 1;
                Aim.MultiHitbox = true;
                Trigger.Radius = 13.0f;
                Trigger.DelayMin = 2;
                Trigger.DelayMax = 10;
                Trigger.HoldMin = 8;
                Trigger.HoldMax = 16;
                Trigger.Burst = true;
                Trigger.BurstShots = 3;
                Esp.Box = true;
                Esp.BoxStyle = 0;
                Esp.FilledBox = true;
                Esp.HealthBar = true;
                Esp.HealthText = true;
                Esp.ArmorBar = true;
                Esp.Name = true;
                Esp.Weapon = true;
                Esp.Distance = true;
                Esp.Money = true;
                Esp.Ammo = true;
                Esp.HeadDot = true;
                Esp.Snapline = true;
                Esp.Skeleton = true;
                Esp.Flags = true;
                Esp.Glow = true;
                Esp.OffscreenArrows = true;
                Misc.AutoReload = true;
                Misc.AutoPistol = true;
                break;

            case Preset.Rage:
                Aim.Key = Vk.XButton2;
                Aim.Fov = 420.0f;
                Aim.DynamicFov = false;
                Aim.Smooth = 1.0f;
                Aim.SmoothMode = 0;
                Aim.Humanize = 0.0f;
                Aim.ReactionTime = 0.0f;
                Aim.ReactionJitter = 0.0f;
                Aim.AimStep = 0.0f;
                Aim.AimScale = 1.35f;
                Aim.VisibleOnly = false;
                Aim.AutoFire = true;
                Aim.AutoScope = true;
                Aim.AutoPistol = true;
                Aim.AutoStop = true;
                Aim.AutoCrouch = true;
                Aim.Prediction = true;
                Aim.PredictMs = 35.0f;
                Aim.Backtrack = true;
                Aim.BacktrackMs = 120.0f;
                Aim.ResolverMode = 2;
                Aim.Rcs = true;
                Aim.RcsX = 1.0f;
                Aim.RcsY = 1.0f;
                Aim.RcsSmooth = 1.0f;
                Aim.Priority = 1;
                Aim.Hitbox = 0;
                Aim.MultiHitbox = true;
                Aim.HitStomach = true;
                Trigger.Radius = 22.0f;
                Trigger.DelayMin = 0;
                Trigger.DelayMax = 0;
                Trigger.HoldMin = 6;
                Trigger.HoldMax = 10;
                Trigger.Burst = false;
                Trigger.Randomize = false;
                Esp.Box = true;
                Esp.BoxStyle = 0;
                Esp.FilledBox = true;
                Esp.HealthBar = true;
                Esp.HealthText = true;
                Esp.ArmorBar = true;
                Esp.Name = true;
                Esp.Weapon = true;
                Esp.Distance = true;
                Esp.Money = true;
                Esp.Ammo = true;
                Esp.HeadDot = true;
                Esp.Snapline = true;
                Esp.Skeleton = true;
                Esp.Flags = true;
                Esp.Glow = true;
                Esp.OffscreenArrows = true;
                Esp.SoundEsp = true;
                Esp.LocalPlayer = true;
                Visuals.NightMode = true;
                Visuals.RemoveHands = true;
                Misc.Bhop = true;
                Misc.AutoReload = true;
                Misc.AutoPistol = true;
                break;

            case Preset.Custom:
                break;
        }
    }

    public void LogSummary()
    {
        Log.Info("config: preset={0} process={1} layout={2} module={3}", PresetName(Preset), Process,
            Layout, ClientModule);
        Log.Info("config: offsets entity_list={0} local_player={1} view_matrix={2}", EntityList.Raw,
            LocalPlayer.Raw, ViewMatrix.Raw);
        Log.Info("config: stride=0x{0:X} max_entities={1} health=0x{2:X} team=0x{3:X} origin=0x{4:X} bone=0x{5:X} head_bone={6}",
            EntityStride, MaxEntities, Health, Team, Origin, BoneMatrix, HeadBone);
        Log.Info("config: aim={0} key={1} fov={2:0} smooth={3:0.0} mode={4} fire={5} rcs={6}",
            Aim.Enabled, Vk.ToName(Aim.Key), Aim.Fov, Aim.Smooth, Aim.SmoothMode, Aim.AutoFire, Aim.Rcs);
        Log.Info("config: trigger={0} key={1} radius={2:0} delay={3}-{4}ms", Trigger.Enabled,
            Vk.ToName(Trigger.Key), Trigger.Radius, Trigger.DelayMin, Trigger.DelayMax);
        Log.Info("config: esp={0} box={1} health={2} name={3} skeleton={4} glow={5}", Esp.Enabled, Esp.Box,
            Esp.HealthBar, Esp.Name, Esp.Skeleton, Esp.Glow);
    }

    private string Get(string section, string key, string fallback)
    {
        return _values.TryGetValue(section + "." + key, out var value) ? value : fallback;
    }

    private int GetInt(string section, string key, int fallback)
    {
        var text = Get(section, key, "");
        return text.Length > 0 && TryParseLongAuto(text, out var value) ? (int)value : fallback;
    }

    private bool GetBool(string section, string key, bool fallback)
    {
        var text = Get(section, key, "").ToLowerInvariant();
        return text switch
        {
            "" => fallback,
            "1" or "true" or "yes" or "on" => true,
            "0" or "false" or "no" or "off" => false,
            _ => fallback,
        };
    }

    private float GetFloat(string section, string key, float fallback)
    {
        var text = Get(section, key, "");
        return text.Length > 0 && float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var value)
            ? value
            : fallback;
    }

    private uint GetColor(string section, string key, uint fallback)
    {
        var text = Get(section, key, "");
        if (text.Length == 0) return fallback;
        if (text.Length > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text = text[2..];

        if (!ulong.TryParse(text, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var value))
            return fallback;

        return text.Length <= 6 ? 0xFF000000u | (uint)(value & 0xFFFFFFu) : (uint)value;
    }

    private int GetKey(string section, string key, int fallback)
    {
        var text = Get(section, key, "");
        if (text.Length == 0) return fallback;
        var vk = Vk.NameToVk(text);
        return vk != 0 ? vk : fallback;
    }

    public static OffsetSpec ParseOffsetSpec(string text)
    {
        var spec = new OffsetSpec { Raw = text.Trim() };
        if (spec.Raw.Length == 0) return spec;

        if (spec.Raw.StartsWith("sig:", StringComparison.OrdinalIgnoreCase))
        {
            spec.Pattern = true;
            spec.PatternText = spec.Raw[4..].Trim();
            return spec;
        }

        if (TryParseULongAuto(spec.Raw, out var value)) spec.ModuleOffset = value;
        return spec;
    }

    private static void SetAddress(OffsetSpec spec, ulong value)
    {
        spec.Pattern = false;
        spec.PatternText = "";
        spec.ModuleOffset = value;
        spec.Raw = "0x" + value.ToString("X", CultureInfo.InvariantCulture);
    }

    private static bool TryParseLongAuto(string text, out long value)
    {
        text = text.Trim();
        if (text.Length > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
            return long.TryParse(text.AsSpan(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out value);

        return long.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out value);
    }

    private static bool TryParseULongAuto(string text, out ulong value)
    {
        text = text.Trim();
        if (text.Length > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
            return ulong.TryParse(text.AsSpan(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out value);

        return ulong.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out value);
    }
}
