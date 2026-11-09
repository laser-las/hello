using System.Globalization;

namespace Halcyon;

public sealed class EspState
{
    public bool EspOn = true;
    public bool AimOn = true;
    public bool TriggerOn = true;
    public bool HasAimTarget;
    public Vec2 AimTarget;
    public float Fps;
    public float ScanMs;
    public int Players;
    public string Preset = "";
}

// Draws one complete overlay frame: player ESP, radar, crosshair and the HUD
// counters. Nothing here touches the game, it only reads the World snapshot.
public sealed class EspRenderer
{
    private readonly Dictionary<ulong, int> _trackedHealth = new();
    private double _hitmarkerUntil;
    private double _damageUntil;
    private int _lastDamage;

    private static uint WithAlpha(uint argb, float alpha)
    {
        var a = (uint)(Math.Clamp(alpha, 0.0f, 1.0f) * 255.0f);
        return (argb & 0x00FFFFFFu) | (a << 24);
    }

    private static string Meters(float distance)
    {
        return string.Format(CultureInfo.InvariantCulture, "{0:0}m", MathUtil.ToUnitsMeters(distance));
    }

    private static void TextRight(Overlay overlay, string value, float right, float y, uint color, float size)
    {
        overlay.Text(value, right - overlay.Measure(value, size), y, color, size, false);
    }

    private static void DrawCornerBox(Overlay overlay, float x, float y, float w, float h, uint color,
        float thickness)
    {
        var arm = MathF.Max(3.0f, MathF.Min(w, h) * 0.25f);
        overlay.Line(x, y, x + arm, y, color, thickness);
        overlay.Line(x, y, x, y + arm, color, thickness);

        overlay.Line(x + w - arm, y, x + w, y, color, thickness);
        overlay.Line(x + w, y, x + w, y + arm, color, thickness);

        overlay.Line(x, y + h - arm, x, y + h, color, thickness);
        overlay.Line(x, y + h, x + arm, y + h, color, thickness);

        overlay.Line(x + w - arm, y + h, x + w, y + h, color, thickness);
        overlay.Line(x + w, y + h - arm, x + w, y + h, color, thickness);
    }

    private static void DrawHealthBar(Overlay overlay, Config cfg, float boxX, float boxY, float boxW,
        float boxH, int health, float fade)
    {
        var barW = MathF.Max(2.0f, boxW * 0.11f);
        var frac = Math.Clamp(health / 100.0f, 0.0f, 1.0f);

        var x = boxX - barW - 3.0f;
        var y = boxY;
        var w = barW;
        var h = boxH;

        if (cfg.Esp.HealthBarPos == 1)
        {
            x = boxX + boxW + 3.0f;
        }
        else if (cfg.Esp.HealthBarPos == 2)
        {
            x = boxX;
            y = boxY - barW - 3.0f;
            w = boxW;
            h = barW;
        }

        overlay.FillRect(x, y, w, h, 0xB0000000u);

        if (cfg.Esp.HealthBarPos == 2) overlay.FillRect(x, y, w * frac, h, WithAlpha(cfg.Esp.ColorHealth, fade));
        else overlay.FillRect(x, y + h * (1.0f - frac), w, h * frac, WithAlpha(cfg.Esp.ColorHealth, fade));
    }

    private static void DrawArmorBar(Overlay overlay, Config cfg, float boxX, float boxY, float boxW,
        float boxH, int armor, float fade)
    {
        var barW = MathF.Max(1.5f, boxW * 0.06f);
        var frac = Math.Clamp(armor / 100.0f, 0.0f, 1.0f);
        var x = boxX - barW * 2.0f - 5.0f;

        overlay.FillRect(x, boxY, barW, boxH, 0xB0000000u);
        overlay.FillRect(x, boxY + boxH * (1.0f - frac), barW, boxH * frac, WithAlpha(cfg.Esp.ColorArmor, fade));
    }

    private static void DrawRadar(Overlay overlay, World world, Config cfg)
    {
        if (!world.HaveLocal) return;

        var radius = MathF.Max(40.0f, cfg.Radar.Size * 0.5f);
        var cx = cfg.Radar.PosX + radius;
        var cy = cfg.Radar.PosY + radius;

        overlay.Dot(cx, cy, radius, 0x50000000u);
        overlay.Circle(cx, cy, radius, WithAlpha(cfg.Visuals.Accent, 0.65f), 1.5f);
        overlay.Line(cx - 5.0f, cy, cx + 5.0f, cy, 0x60FFFFFF, 1.0f);
        overlay.Line(cx, cy - 5.0f, cx, cy + 5.0f, 0x60FFFFFF, 1.0f);

        var yaw = cfg.Radar.Rotate ? world.ViewAngles.Y : 0.0f;
        var radians = -yaw * MathUtil.Pi / 180.0f;
        var c = MathF.Cos(radians);
        var s = MathF.Sin(radians);
        var scale = radius / 1400.0f * MathF.Max(0.1f, cfg.Radar.Zoom);

        foreach (var p in world.Players)
        {
            var sameTeam = world.LocalTeam != 0 && p.Team == world.LocalTeam;
            if (sameTeam ? !cfg.Radar.ShowTeam : !cfg.Radar.ShowEnemies) continue;

            var dx = p.Origin.X - world.LocalOrigin.X;
            var dy = p.Origin.Y - world.LocalOrigin.Y;

            var rx = (dx * c - dy * s) * scale;
            var ry = (dx * s + dy * c) * scale;

            var length = MathF.Sqrt(rx * rx + ry * ry);
            var limit = radius - 4.0f;
            if (length > limit)
            {
                var k = limit / length;
                rx *= k;
                ry *= k;
            }

            var color = sameTeam ? cfg.Radar.ColorTeam : cfg.Radar.ColorEnemy;
            overlay.Dot(cx + rx, cy - ry, cfg.Radar.DotSize, color);
        }
    }

    private static void DrawCrosshair(Overlay overlay, Config cfg, float width, float height)
    {
        var cx = width * 0.5f;
        var cy = height * 0.5f;
        var size = cfg.Visuals.CrosshairSize;
        var gap = cfg.Visuals.CrosshairGap;
        var thickness = cfg.Visuals.CrosshairThickness;
        var color = cfg.Visuals.CrosshairColor;

        switch (cfg.Visuals.CrosshairStyle)
        {
            case 1:
                overlay.Dot(cx, cy, MathF.Max(1.0f, thickness), color);
                break;
            case 2:
                overlay.Circle(cx, cy, MathF.Max(2.0f, size), color, thickness);
                break;
            case 3:
                overlay.Line(cx - size - gap, cy, cx + size + gap, cy, color, thickness);
                overlay.Line(cx, cy - gap, cx, cy + gap - size, color, thickness);
                break;
            default:
                overlay.Line(cx - size - gap, cy, cx - gap, cy, color, thickness);
                overlay.Line(cx + gap, cy, cx + size + gap, cy, color, thickness);
                overlay.Line(cx, cy - size - gap, cx, cy - gap, color, thickness);
                overlay.Line(cx, cy + gap, cx, cy + size + gap, color, thickness);
                break;
        }
    }

    private static void DrawSkeleton(Overlay overlay, World world, Config cfg, Player player, float boxWidth,
        float fade)
    {
        var w = overlay.WidthPx;
        var h = overlay.HeightPx;

        if (!MathUtil.WorldToScreen(world.View, player.Head, w, h, out var head)) return;
        if (!MathUtil.WorldToScreen(world.View, player.Neck, w, h, out var neck)) return;
        if (!MathUtil.WorldToScreen(world.View, player.Chest, w, h, out var chest)) return;
        if (!MathUtil.WorldToScreen(world.View, player.Stomach, w, h, out var stomach)) return;
        if (!MathUtil.WorldToScreen(world.View, player.Origin, w, h, out var feet)) return;

        var color = WithAlpha(cfg.Esp.ColorSkeleton, fade);
        var thickness = MathF.Max(0.5f, cfg.Esp.SkeletonThickness);

        overlay.Line(head.X, head.Y, neck.X, neck.Y, color, thickness);
        overlay.Line(neck.X, neck.Y, chest.X, chest.Y, color, thickness);
        overlay.Line(chest.X, chest.Y, stomach.X, stomach.Y, color, thickness);
        overlay.Line(stomach.X, stomach.Y, feet.X, feet.Y, color, thickness);

        var halfShoulder = boxWidth * 0.34f;
        var halfHip = boxWidth * 0.18f;
        var shoulderY = chest.Y + (stomach.Y - chest.Y) * 0.25f;
        var hipY = stomach.Y + (feet.Y - stomach.Y) * 0.30f;
        var elbowY = shoulderY + (hipY - shoulderY) * 0.55f;

        var leftShoulder = new Vec2(chest.X - halfShoulder, shoulderY);
        var rightShoulder = new Vec2(chest.X + halfShoulder, shoulderY);
        var leftElbow = new Vec2(chest.X - halfShoulder * 1.25f, elbowY);
        var rightElbow = new Vec2(chest.X + halfShoulder * 1.25f, elbowY);
        var leftHip = new Vec2(stomach.X - halfHip, hipY);
        var rightHip = new Vec2(stomach.X + halfHip, hipY);

        overlay.Line(chest.X, chest.Y, leftShoulder.X, leftShoulder.Y, color, thickness);
        overlay.Line(chest.X, chest.Y, rightShoulder.X, rightShoulder.Y, color, thickness);
        overlay.Line(leftShoulder.X, leftShoulder.Y, leftElbow.X, leftElbow.Y, color, thickness);
        overlay.Line(rightShoulder.X, rightShoulder.Y, rightElbow.X, rightElbow.Y, color, thickness);

        overlay.Line(stomach.X, stomach.Y, leftHip.X, leftHip.Y, color, thickness);
        overlay.Line(stomach.X, stomach.Y, rightHip.X, rightHip.Y, color, thickness);
        overlay.Line(leftHip.X, leftHip.Y, feet.X - boxWidth * 0.18f, feet.Y, color, thickness);
        overlay.Line(rightHip.X, rightHip.Y, feet.X + boxWidth * 0.18f, feet.Y, color, thickness);
    }

    private static void DrawOffscreenArrow(Overlay overlay, World world, Player player, uint color)
    {
        if (!world.HaveLocal) return;

        var delta = player.Origin - world.LocalEye;
        var targetYaw = MathF.Atan2(delta.Y, delta.X) * MathUtil.Rad2Deg;

        var forwardX = world.View.M[3];
        var forwardY = world.View.M[7];
        var cameraYaw = world.ViewAngles.Y;
        if (MathF.Abs(forwardX) + MathF.Abs(forwardY) > 1.0e-4f)
            cameraYaw = MathF.Atan2(forwardY, forwardX) * MathUtil.Rad2Deg;

        var relative = targetYaw - cameraYaw;
        while (relative > 180.0f) relative -= 360.0f;
        while (relative < -180.0f) relative += 360.0f;

        var radians = relative * MathUtil.Pi / 180.0f;
        var dirX = -MathF.Sin(radians);
        var dirY = -MathF.Cos(radians);

        var width = overlay.WidthPx;
        var height = overlay.HeightPx;
        var cx = width * 0.5f;
        var cy = height * 0.5f;
        var radius = MathF.Min(width, height) * 0.40f;

        var tipX = cx + dirX * radius;
        var tipY = cy + dirY * radius;

        overlay.Line(cx + dirX * (radius - 18.0f), cy + dirY * (radius - 18.0f), tipX, tipY, color, 2.5f);
        overlay.Dot(tipX, tipY, 4.5f, color);

        overlay.Text(Meters(player.Distance), tipX - dirX * 16.0f, tipY - dirY * 16.0f, color, 11.0f, true);
    }

    public void Draw(Overlay overlay, World world, Config cfg, EspState state)
    {
        var width = overlay.WidthPx;
        var height = overlay.HeightPx;
        if (width <= 0 || height <= 0) return;

        var cx = width * 0.5f;
        var cy = height * 0.5f;
        var textSize = cfg.Esp.TextSize > 0 ? cfg.Esp.TextSize : 13.0f;

        if (cfg.Visuals.NightMode)
            overlay.FillRect(0.0f, 0.0f, width, height, WithAlpha(0xFF06101Eu, cfg.Visuals.NightAlpha));

        if (cfg.Aim.DrawFov && state.AimOn && cfg.Aim.Fov > 1.0f)
            overlay.Circle(cx, cy, cfg.Aim.Fov, WithAlpha(cfg.Aim.FovColor, cfg.Aim.FovAlpha), 1.0f);

        var now = (double)Environment.TickCount64;

        foreach (var p in world.Players)
        {
            var previous = _trackedHealth.TryGetValue(p.Address, out var value) ? value : 0;
            if (previous > p.Health && p.Health > 0)
            {
                _lastDamage = previous - p.Health;
                _hitmarkerUntil = now + 380.0;
                _damageUntil = now + 1100.0;
            }

            _trackedHealth[p.Address] = p.Health;
        }

        if (state.EspOn && cfg.Esp.Enabled)
        {
            foreach (var p in world.Players)
            {
                if (cfg.Esp.TeamCheck && world.LocalTeam != 0 && p.Team == world.LocalTeam) continue;
                if (cfg.Esp.VisibleCheck && !p.Spotted) continue;
                if (cfg.Esp.MaxDistance > 0.0f && p.Distance > cfg.Esp.MaxDistance) continue;

                var arrowColor = WithAlpha(p.Spotted ? cfg.Esp.ColorVisible : cfg.Esp.ColorOccluded, 0.85f);

                var feetOk = MathUtil.WorldToScreen(world.View, p.Origin, width, height, out var feet);
                var headOk = MathUtil.WorldToScreen(world.View, p.Head, width, height, out var head);
                if (!feetOk || !headOk)
                {
                    if (cfg.Esp.OffscreenArrows && !cfg.Esp.OofCheck) DrawOffscreenArrow(overlay, world, p, arrowColor);
                    continue;
                }

                var boxHeight = feet.Y - head.Y;
                if (boxHeight < 4.0f || boxHeight > height * 1.5f) continue;

                var boxWidth = boxHeight * 0.45f;
                var left = head.X - boxWidth * 0.5f;
                var top = head.Y;
                var color = p.Spotted ? cfg.Esp.ColorVisible : cfg.Esp.ColorOccluded;

                var fade = 1.0f;
                if (cfg.Esp.FadeStart > 0.0f && cfg.Esp.MaxDistance > cfg.Esp.FadeStart &&
                    p.Distance > cfg.Esp.FadeStart)
                {
                    var span = MathF.Max(1.0f, cfg.Esp.MaxDistance - cfg.Esp.FadeStart);
                    fade = Math.Clamp(1.0f - (p.Distance - cfg.Esp.FadeStart) / span, 0.15f, 1.0f);
                }

                if (cfg.Esp.Glow)
                {
                    for (var ring = 3; ring >= 1; --ring)
                    {
                        var expand = ring * 2.0f;
                        overlay.Rect(left - expand, top - expand, boxWidth + expand * 2.0f,
                            boxHeight + expand * 2.0f, WithAlpha(cfg.Esp.ColorGlow, fade * 0.10f * ring), 2.0f);
                    }
                }

                if (cfg.Esp.Skeleton) DrawSkeleton(overlay, world, cfg, p, boxWidth, fade);

                if (cfg.Esp.FilledBox)
                    overlay.FillRect(left, top, boxWidth, boxHeight, WithAlpha(cfg.Esp.FillColor, fade * 0.35f));

                if (cfg.Esp.Box)
                {
                    var boxColor = WithAlpha(color, fade);
                    if (cfg.Esp.BoxStyle == 1)
                        DrawCornerBox(overlay, left, top, boxWidth, boxHeight, boxColor, cfg.Esp.BoxThickness);
                    else
                        overlay.Rect(left, top, boxWidth, boxHeight, boxColor, cfg.Esp.BoxThickness);
                }

                if (cfg.Esp.HealthBar) DrawHealthBar(overlay, cfg, left, top, boxWidth, boxHeight, p.Health, fade);
                if (cfg.Esp.ArmorBar && p.Armor > 0)
                    DrawArmorBar(overlay, cfg, left, top, boxWidth, boxHeight, p.Armor, fade);

                if (cfg.Esp.HeadDot)
                    overlay.Dot(head.X, head.Y, MathF.Max(2.0f, boxWidth * 0.09f), cfg.Esp.ColorHeadDot);

                if (cfg.Esp.Snapline)
                {
                    var originX = cx;
                    var originY = (float)height;
                    if (cfg.Esp.SnaplineOrigin == 1) originY = 0.0f;
                    if (cfg.Esp.SnaplineOrigin == 2) originY = cy;
                    overlay.Line(originX, originY, head.X, feet.Y, cfg.Esp.ColorSnapline, 1.0f);
                }

                var textY = top - textSize - 4.0f;
                var textColor = WithAlpha(cfg.Esp.ColorText, fade);

                if (cfg.Esp.Name && p.Name.Length > 0)
                {
                    overlay.Text(p.Name, head.X, textY, textColor, textSize, true);
                    textY -= textSize + 2.0f;
                }

                if (cfg.Esp.Weapon && p.Weapon > 0)
                {
                    var weapon = Weapons.Name(p.Weapon);
                    if (weapon.Length > 0)
                    {
                        overlay.Text(weapon, head.X, textY, textColor, textSize, true);
                        textY -= textSize + 2.0f;
                    }
                }

                var info = "";
                if (cfg.Esp.HealthText) info += p.Health + " hp";
                if (cfg.Esp.Distance)
                {
                    if (info.Length > 0) info += "  ";
                    info += Meters(p.Distance);
                }

                if (cfg.Esp.Money && p.Money > 0)
                {
                    if (info.Length > 0) info += "  ";
                    info += "$" + p.Money.ToString(CultureInfo.InvariantCulture);
                }

                if (cfg.Esp.Ammo && p.Ammo >= 0)
                {
                    if (info.Length > 0) info += "  ";
                    info += p.Ammo.ToString(CultureInfo.InvariantCulture) + " rnd";
                }

                if (info.Length > 0) overlay.Text(info, head.X, textY, textColor, textSize, true);

                if (cfg.Esp.Flags)
                {
                    var flagY = top + 2.0f;
                    if (p.Scoped)
                    {
                        overlay.Text("scope", left + boxWidth + 5.0f, flagY, 0xFF4DA6FF, textSize - 1.0f, false);
                        flagY += textSize;
                    }

                    if (p.Defusing)
                    {
                        overlay.Text("defuse", left + boxWidth + 5.0f, flagY, 0xFFFFAA00, textSize - 1.0f, false);
                        flagY += textSize;
                    }

                    if (p.Flashed)
                        overlay.Text("flash", left + boxWidth + 5.0f, flagY, 0xFFFFFF66, textSize - 1.0f, false);
                }
            }
        }

        if (cfg.Visuals.Hitmarker && now < _hitmarkerUntil)
        {
            var remaining = (float)((_hitmarkerUntil - now) / 380.0);
            var hitColor = WithAlpha(cfg.Visuals.HitmarkerColor, MathF.Max(0.25f, remaining));
            var size = MathF.Max(4.0f, cfg.Visuals.HitmarkerSize);
            var inner = size * 0.45f;

            overlay.Line(cx - size, cy - size, cx - inner, cy - inner, hitColor, 2.0f);
            overlay.Line(cx + inner, cy - inner, cx + size, cy - size, hitColor, 2.0f);
            overlay.Line(cx - size, cy + size, cx - inner, cy + inner, hitColor, 2.0f);
            overlay.Line(cx + inner, cy + inner, cx + size, cy + size, hitColor, 2.0f);
        }

        if (cfg.Visuals.DamageIndicator && now < _damageUntil && _lastDamage > 0)
        {
            var remaining = (float)((_damageUntil - now) / 1100.0);
            overlay.Text("-" + _lastDamage.ToString(CultureInfo.InvariantCulture), cx + 16.0f, cy + 10.0f,
                WithAlpha(0xFFFF5050u, MathF.Max(0.2f, remaining)), 14.0f, false);
        }

        if (state.HasAimTarget) overlay.Dot(state.AimTarget.X, state.AimTarget.Y, 3.0f, 0xFF35FF6Eu);

        if (cfg.Visuals.Crosshair) DrawCrosshair(overlay, cfg, width, height);
        if (cfg.Radar.Enabled) DrawRadar(overlay, world, cfg);

        if (cfg.Visuals.Watermark)
        {
            TextRight(overlay, "halcyon", width - 14.0f, 10.0f, cfg.Visuals.Accent, 16.0f);
            if (state.Preset.Length > 0)
                TextRight(overlay, state.Preset, width - 14.0f, 28.0f,
                    WithAlpha(cfg.Visuals.Accent, 0.7f), 12.0f);
        }

        if (cfg.Visuals.KeybindList)
        {
            var y = height - 6.0f * 16.0f - 10.0f;
            var color = WithAlpha(cfg.Visuals.Accent, 0.85f);

            overlay.Text(Vk.ToName(cfg.MenuKey) + "  menu", 12.0f, y, color, 12.0f, false);
            y += 16.0f;
            overlay.Text(Vk.ToName(cfg.EspToggleKey) + "  esp", 12.0f, y, color, 12.0f, false);
            y += 16.0f;
            overlay.Text(Vk.ToName(cfg.AimToggleKey) + "  aim", 12.0f, y, color, 12.0f, false);
            y += 16.0f;
            overlay.Text(Vk.ToName(cfg.TriggerToggleKey) + "  trigger", 12.0f, y, color, 12.0f, false);
            y += 16.0f;
            overlay.Text(Vk.ToName(cfg.PanicKey) + "  panic", 12.0f, y, color, 12.0f, false);
            y += 16.0f;
            overlay.Text(Vk.ToName(cfg.QuitKey) + "  quit", 12.0f, y, color, 12.0f, false);
        }

        if (cfg.Visuals.FpsCounter)
            TextRight(overlay, string.Format(CultureInfo.InvariantCulture, "{0:0} fps", state.Fps),
                width - 14.0f, height - 26.0f, cfg.Esp.ColorText, 12.0f);

        if (cfg.Visuals.TimeCounter)
            TextRight(overlay, DateTime.Now.ToString("HH:mm:ss", CultureInfo.InvariantCulture),
                width - 14.0f, height - 44.0f, cfg.Esp.ColorText, 12.0f);

        overlay.Text(
            string.Format(CultureInfo.InvariantCulture,
                "esp {0}   aim {1}   trigger {2}   {3} targets   scan {4:0.00} ms",
                state.EspOn ? "on" : "off", state.AimOn ? "on" : "off", state.TriggerOn ? "on" : "off",
                state.Players, state.ScanMs),
            12.0f, 10.0f, WithAlpha(cfg.Esp.ColorText, 0.9f), 12.0f, false);
    }
}
