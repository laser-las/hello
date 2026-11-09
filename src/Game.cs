using System.Text;

namespace Halcyon;

public sealed class ResolvedOffsets
{
    public ulong EntityList;
    public ulong LocalPlayer;
    public ulong ViewMatrix;

    public bool Valid => EntityList != 0 && LocalPlayer != 0 && ViewMatrix != 0;
}

public sealed class Player
{
    public ulong Address;
    public int Index;
    public int Health;
    public int Armor;
    public int Team;
    public int Weapon;
    public int Money;
    public int Ammo;
    public bool Spotted;
    public bool Scoped;
    public bool Defusing;
    public bool Flashed;
    public string Name = "";

    public Vec3 Origin;
    public Vec3 Eye;
    public Vec3 Head;
    public Vec3 Neck;
    public Vec3 Chest;
    public Vec3 Stomach;
    public Vec3 Velocity;
    public float Distance;

    // Height ladder used by the aimbot hitbox resolver.
    public Vec3 HitPoint(int hitbox) => hitbox switch
    {
        0 => Head,
        1 => Neck,
        2 => Chest,
        3 => Stomach,
        _ => Head,
    };
}

public sealed class World
{
    public Mat4 View = new();
    public Vec3 LocalOrigin;
    public Vec3 LocalEye;
    public Vec3 LocalVelocity;
    public Vec3 AimPunch;
    public Vec3 ViewAngles;
    public int LocalTeam;
    public int LocalHealth;
    public int LocalAmmo;
    public int LocalWeapon;
    public bool HaveLocal;
    public bool LocalScoped;
    public bool LocalFlashed;
    public readonly List<Player> Players = new();
    public float ScanMs;
}

public static class Weapons
{
    public static string Name(int id) => id switch
    {
        1 => "deagle",
        2 => "elite",
        3 => "five-seven",
        4 => "glock",
        7 => "ak-47",
        8 => "aug",
        9 => "awp",
        10 => "famas",
        11 => "g3sg1",
        13 => "galil",
        14 => "m249",
        16 => "m4a4",
        17 => "mac-10",
        19 => "p90",
        23 => "mp5",
        24 => "ump",
        25 => "xm1014",
        26 => "bizon",
        27 => "mag-7",
        28 => "negev",
        29 => "sawed-off",
        30 => "tec-9",
        31 => "zeus",
        32 => "p2000",
        33 => "mp7",
        34 => "mp9",
        35 => "nova",
        36 => "p250",
        38 => "scar-20",
        39 => "sg553",
        40 => "ssg08",
        60 => "m4a1-s",
        61 => "usp-s",
        63 => "cz75",
        64 => "r8",
        _ => "",
    };

    public static bool IsSniper(int id) => id is 9 or 11 or 38 or 40;

    public static bool IsPistol(int id) => id is 1 or 2 or 3 or 4 or 30 or 32 or 36 or 61 or 63 or 64;

    public static bool IsKnife(int id) => id is 42 or 59;

    public static bool IsGun(int id)
    {
        if (id <= 0) return false;
        if (IsKnife(id)) return false;
        if (id == 31) return false;             // zeus
        if (id is >= 43 and <= 49) return false; // grenades + c4
        return true;
    }
}

// Reads the entity list out of the target's memory and shapes it into a World.
public sealed class Game
{
    private readonly Process _proc;
    private readonly Config _cfg;
    private readonly float[] _bone = new float[12];
    private ResolvedOffsets _resolved = new();

    public Game(Process proc, Config cfg)
    {
        _proc = proc;
        _cfg = cfg;
    }

    public ResolvedOffsets Resolved => _resolved;

    private bool Source1 => string.Equals(_cfg.Layout, "source1", StringComparison.OrdinalIgnoreCase);

    public ResolvedOffsets Resolve()
    {
        _resolved = new ResolvedOffsets
        {
            EntityList = ResolveOne(_cfg.EntityList, "entity_list"),
            LocalPlayer = ResolveOne(_cfg.LocalPlayer, "local_player"),
            ViewMatrix = ResolveOne(_cfg.ViewMatrix, "view_matrix"),
        };
        return _resolved;
    }

    private ulong ResolveOne(OffsetSpec spec, string name)
    {
        var mod = _proc.Module(_cfg.ClientModule);
        if (mod == null)
        {
            Log.Info("offsets: {0} missing - module {1} not found", name, _cfg.ClientModule);
            return 0;
        }

        if (spec.Pattern)
        {
            var hit = _proc.Scan(_cfg.ClientModule, spec.PatternText, out var matches);
            if (hit == 0)
            {
                Log.Info("offsets: {0} signature found 0 matches", name);
                return 0;
            }

            var target = _proc.DecodeRipRelative(hit);
            if (target == 0) target = hit;

            if (matches > 1)
            {
                Log.Info("offsets: {0} signature matched {1} times, using first at 0x{2:X} (resolved 0x{3:X})",
                    name, matches, hit, target);
            }
            else
            {
                Log.Info("offsets: {0} signature resolved to 0x{1:X}", name, target);
            }

            return target;
        }

        if (spec.ModuleOffset == 0)
        {
            Log.Info("offsets: {0} not configured", name);
            return 0;
        }

        var absolute = mod.Base + spec.ModuleOffset;
        Log.Info("offsets: {0} = 0x{1:X} (module+0x{2:X})", name, absolute, spec.ModuleOffset);
        return absolute;
    }

    private ulong EntityAddress(ulong listBase, int index)
    {
        if (Source1)
            return _proc.ReadPtr(listBase + 0x10UL * (ulong)index);

        // Source 2: chunk table of 512 entries, each chunk a fixed stride array.
        var chunk = _proc.ReadPtr(listBase + 0x10UL + 8UL * (ulong)(index >> 9));
        if (chunk == 0) return 0;
        return _proc.ReadPtr(chunk + (ulong)_cfg.EntityStride * (ulong)(index & 0x1FF));
    }

    private bool ReadBone(ulong entity, int bone, out Vec3 result)
    {
        result = default;

        var baseAddress = entity + (ulong)_cfg.BoneMatrix;
        if (_cfg.BoneIndirect)
        {
            baseAddress = _proc.ReadPtr(baseAddress);
            if (baseAddress == 0) return false;
        }

        // 3x4 bone matrix, translation column is the 12th/8th/4th float.
        var matrix = _bone;
        var addr = baseAddress + (ulong)bone * 32UL;
        if (!_proc.ReadFloats(addr, matrix, 12)) return false;

        result = new Vec3(matrix[3], matrix[7], matrix[11]);
        return MathUtil.Finite3(result);
    }

    public bool Update(World world)
    {
        var clock = new Clock();
        world.Players.Clear();
        world.HaveLocal = false;

        if (!_resolved.Valid) return true;

        if (!_proc.ReadFloats(_resolved.ViewMatrix, world.View.M, 16)) return false;

        var localPawn = _proc.ReadPtr(_resolved.LocalPlayer);
        if (localPawn != 0)
        {
            world.LocalOrigin = _proc.ReadVec3(localPawn + (ulong)_cfg.Origin);
            world.LocalTeam = _proc.ReadOr(localPawn + (ulong)_cfg.Team, 0);
            world.LocalHealth = _proc.ReadOr(localPawn + (ulong)_cfg.Health, 0);
            world.HaveLocal = MathUtil.Finite3(world.LocalOrigin);

            world.LocalEye = world.LocalOrigin;
            if (_cfg.EyeOffset >= 0)
            {
                var offset = _proc.ReadVec3(localPawn + (ulong)_cfg.EyeOffset);
                if (MathUtil.Finite3(offset)) world.LocalEye = world.LocalOrigin + offset;
            }

            if (_cfg.Velocity >= 0) world.LocalVelocity = _proc.ReadVec3(localPawn + (ulong)_cfg.Velocity);
            if (_cfg.Ammo >= 0) world.LocalAmmo = _proc.ReadOr(localPawn + (ulong)_cfg.Ammo, 0);
            if (_cfg.Weapon >= 0) world.LocalWeapon = _proc.ReadOr(localPawn + (ulong)_cfg.Weapon, 0);
            if (_cfg.AimPunch >= 0) world.AimPunch = _proc.ReadVec3(localPawn + (ulong)_cfg.AimPunch);
            if (_cfg.ViewAngles >= 0) world.ViewAngles = _proc.ReadVec3(localPawn + (ulong)_cfg.ViewAngles);
            if (_cfg.Scoped >= 0)
                world.LocalScoped = _proc.ReadOr<byte>(localPawn + (ulong)_cfg.Scoped, 0) != 0;
            if (_cfg.Flashed >= 0)
                world.LocalFlashed = _proc.ReadOr<float>(localPawn + (ulong)_cfg.Flashed, 0.0f) > 0.0f;
        }

        var listBase = _proc.ReadPtr(_resolved.EntityList);
        if (listBase == 0)
        {
            world.ScanMs = (float)clock.Ms();
            return true;
        }

        for (var i = 1; i <= _cfg.MaxEntities; ++i)
        {
            var entity = EntityAddress(listBase, i);
            if (entity == 0 || entity == localPawn) continue;

            var health = _proc.ReadOr(entity + (ulong)_cfg.Health, 0);
            if (health <= 0 || health > 1000) continue;

            var lifeState = _proc.ReadOr<byte>(entity + (ulong)_cfg.LifeState, 0);
            if (lifeState != 0) continue;

            if (_cfg.Dormant > 0 && _proc.ReadOr(entity + (ulong)_cfg.Dormant, 1) != 0) continue;

            var p = new Player
            {
                Address = entity,
                Index = i,
                Health = health,
                Team = _proc.ReadOr(entity + (ulong)_cfg.Team, 0),
                Spotted = _cfg.Spotted > 0 && _proc.ReadOr<byte>(entity + (ulong)_cfg.Spotted, 0) != 0,
                Origin = _proc.ReadVec3(entity + (ulong)_cfg.Origin),
            };

            if (!MathUtil.Finite3(p.Origin)) continue;

            if (_cfg.Armor >= 0) p.Armor = _proc.ReadOr(entity + (ulong)_cfg.Armor, 0);
            if (_cfg.Weapon >= 0) p.Weapon = _proc.ReadOr(entity + (ulong)_cfg.Weapon, 0);
            if (_cfg.Money >= 0) p.Money = _proc.ReadOr(entity + (ulong)_cfg.Money, 0);
            if (_cfg.Ammo >= 0) p.Ammo = _proc.ReadOr(entity + (ulong)_cfg.Ammo, 0);
            if (_cfg.Velocity >= 0) p.Velocity = _proc.ReadVec3(entity + (ulong)_cfg.Velocity);
            if (_cfg.Scoped >= 0) p.Scoped = _proc.ReadOr<byte>(entity + (ulong)_cfg.Scoped, 0) != 0;
            if (_cfg.Flashed >= 0)
                p.Flashed = _proc.ReadOr<float>(entity + (ulong)_cfg.Flashed, 0.0f) > 0.0f;
            if (_cfg.Defusing >= 0) p.Defusing = _proc.ReadOr<byte>(entity + (ulong)_cfg.Defusing, 0) != 0;

            if (_cfg.Name >= 0)
            {
                var buffer = new byte[64];
                if (_proc.ReadBytes(entity + (ulong)_cfg.Name, buffer, 63))
                {
                    var zero = Array.IndexOf(buffer, (byte)0);
                    var length = zero >= 0 ? zero : buffer.Length;
                    var name = Encoding.UTF8.GetString(buffer, 0, length);
                    var sb = new StringBuilder(name.Length);
                    foreach (var c in name)
                    {
                        if (c < 0x20 || c == '\x7f') sb.Append(' ');
                        else sb.Append(c);
                    }

                    p.Name = sb.ToString();
                }
            }

            p.Eye = p.Origin;
            if (_cfg.EyeOffset >= 0)
            {
                var offset = _proc.ReadVec3(entity + (ulong)_cfg.EyeOffset);
                if (MathUtil.Finite3(offset)) p.Eye = p.Origin + offset;
            }

            if (!ReadBone(entity, _cfg.HeadBone, out p.Head))
                p.Head = p.Origin + new Vec3(0.0f, 0.0f, 72.0f);

            // Derive the rest of the hit ladder from the measured head height so
            // the values stay sane for crouched and jumping players.
            var headHeight = p.Head.Z - p.Origin.Z;
            if (headHeight < 30.0f || headHeight > 100.0f) headHeight = 72.0f;
            p.Neck = p.Origin + new Vec3(0.0f, 0.0f, headHeight * 0.90f);
            p.Chest = p.Origin + new Vec3(0.0f, 0.0f, headHeight * 0.76f);
            p.Stomach = p.Origin + new Vec3(0.0f, 0.0f, headHeight * 0.60f);

            if (world.HaveLocal)
            {
                var delta = p.Origin - world.LocalOrigin;
                p.Distance = delta.Length();
            }

            world.Players.Add(p);
        }

        world.ScanMs = (float)clock.Ms();
        return true;
    }
}
