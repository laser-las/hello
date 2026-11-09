namespace Halcyon;

public sealed class AimStatus
{
    public bool HasTarget;
    public bool Firing;
    public bool Scoped;
    public int TargetIndex;
    public Vec2 TargetScreen;
    public float FovPixels;
}

// One sampled past position per player, used by the external backtrack
// approximation.
public sealed class AimSample
{
    public double Time;
    public Vec3 Head;
    public Vec3 Neck;
    public Vec3 Chest;
    public Vec3 Stomach;

    public Vec3 Point(int hitbox) => hitbox switch
    {
        0 => Head,
        1 => Neck,
        2 => Chest,
        3 => Stomach,
        _ => Head,
    };
}

// Turns the current World snapshot into mouse input: movement for the aimbot,
// clicks for the triggerbot and a scope click for auto scope. Everything goes
// through SendInput so the game receives it as ordinary raw input.
public sealed class Aim
{
    private readonly AimStatus _status = new();
    private float _residualX;
    private float _residualY;

    private bool _leftHeld;
    private bool _rightHeld;
    private double _releaseAtMs;
    private double _fireAtMs = -1.0;
    private double _rightReleaseAtMs;
    private double _burstCooldownUntil;
    private double _burstNextAtMs;
    private double _scopeCooldownUntil;
    private int _burstShots;

    private double _reactionUntilMs;
    private ulong _reactionAddress;
    private double _lastTickMs;

    private bool _aimToggleState;
    private bool _triggerToggleState;
    private bool _aimKeyWasDown;
    private bool _triggerKeyWasDown;

    // Target locking / first shot.
    private ulong _lockedTarget;
    private ulong _firstShotTarget;
    private double _firstShotAtMs;

    // Movement assists.
    private int _stopKey;
    private bool _crouchHeld;
    private double _pistolCooldownUntil;

    private readonly Dictionary<ulong, List<AimSample>> _history = new();
    private readonly Random _rng = new();

    private Vec3 _lastPunch;
    private readonly Clock _clock = new();

    public AimStatus Status => _status;

    public void Reset() => ReleaseAll();

    private void ReleaseAll()
    {
        if (_leftHeld) Input.SendMouseButton(false, false);
        if (_rightHeld) Input.SendMouseButton(false, true);
        ReleaseMovement();

        _leftHeld = false;
        _rightHeld = false;
        _releaseAtMs = 0.0;
        _fireAtMs = -1.0;
        _rightReleaseAtMs = 0.0;
        _burstCooldownUntil = 0.0;
        _burstNextAtMs = 0.0;
        _burstShots = 0;
        _residualX = 0.0f;
        _residualY = 0.0f;
        _reactionAddress = 0;
        _reactionUntilMs = 0.0;
        _lockedTarget = 0;
        _firstShotTarget = 0;
        _firstShotAtMs = 0.0;
        _pistolCooldownUntil = 0.0;
        _history.Clear();
        _lastPunch = default;
        _status.HasTarget = false;
        _status.Firing = false;
        _status.Scoped = false;
        _status.TargetIndex = 0;
        _status.TargetScreen = default;
        _status.FovPixels = 0.0f;
    }

    private void ReleaseMovement()
    {
        if (_stopKey != 0)
        {
            Input.SendKey(_stopKey, false);
            _stopKey = 0;
        }

        if (_crouchHeld)
        {
            Input.SendKey(Vk.Control, false);
            _crouchHeld = false;
        }
    }

    private void UpdateHistory(World world, double now)
    {
        foreach (var player in world.Players)
        {
            if (!_history.TryGetValue(player.Address, out var samples))
            {
                samples = new List<AimSample>();
                _history[player.Address] = samples;
            }

            samples.Add(new AimSample
            {
                Time = now,
                Head = player.Head,
                Neck = player.Neck,
                Chest = player.Chest,
                Stomach = player.Stomach,
            });

            while (samples.Count > 0 && now - samples[0].Time > 600.0) samples.RemoveAt(0);
        }

        var stale = new List<ulong>();
        foreach (var (address, samples) in _history)
        {
            while (samples.Count > 0 && now - samples[0].Time > 600.0) samples.RemoveAt(0);
            if (samples.Count == 0) stale.Add(address);
        }

        foreach (var address in stale) _history.Remove(address);
    }

    private double NowMs() => _clock.Ms();

    private int RandomBetween(int lo, int hi) => hi <= lo ? lo : _rng.Next(lo, hi + 1);

    private float RandomFloat(float lo, float hi) => hi <= lo ? lo : lo + (float)_rng.NextDouble() * (hi - lo);

    private void MoveMouse(float dx, float dy)
    {
        _residualX += dx;
        _residualY += dy;

        var mx = (int)_residualX;
        var my = (int)_residualY;
        if (mx == 0 && my == 0) return;

        _residualX -= mx;
        _residualY -= my;
        Input.SendMouseMove(mx, my);
    }

    private static float EffectiveFov(Config cfg, float distance)
    {
        if (!cfg.Aim.DynamicFov) return cfg.Aim.Fov;

        var fov = cfg.Aim.Fov * (1.0f + cfg.Aim.FovScale * (distance / 1000.0f));
        if (cfg.Aim.FovMin > 0.0f) fov = MathF.Max(fov, cfg.Aim.FovMin);
        if (cfg.Aim.FovMax > 0.0f) fov = MathF.Min(fov, cfg.Aim.FovMax);
        return fov;
    }

    // Closest screen point among the hitboxes the config enables. When `sample`
    // is supplied the hit ladder is read from the backtrack history instead of
    // the current snapshot.
    private static bool BestHitboxPoint(World world, Player player, Config cfg, int width, int height,
        out Vec2 point, out float distSq, AimSample? sample = null)
    {
        var w = width;
        var h = height;
        var cx = w * 0.5f;
        var cy = h * 0.5f;

        var predict = cfg.Aim.Prediction && cfg.Aim.PredictMs > 0.0f;
        var lead = cfg.Aim.PredictMs / 1000.0f;

        var best = float.MaxValue;
        var bestPoint = default(Vec2);
        var found = false;

        void Test(int hitbox)
        {
            var p = sample != null ? sample.Point(hitbox) : player.HitPoint(hitbox);
            if (predict) p += player.Velocity * lead;

            if (!MathUtil.WorldToScreen(world.View, p, w, h, out var screen)) return;

            var dx = screen.X - cx;
            var dy = screen.Y - cy;
            var dist = dx * dx + dy * dy;
            if (dist < best)
            {
                best = dist;
                bestPoint = screen;
                found = true;
            }
        }

        if (cfg.Aim.MultiHitbox)
        {
            if (cfg.Aim.HitHead) Test(0);
            if (cfg.Aim.HitNeck) Test(1);
            if (cfg.Aim.HitChest) Test(2);
            if (cfg.Aim.HitStomach) Test(3);
        }
        else
        {
            Test(Math.Clamp(cfg.Aim.Hitbox, 0, 3));
        }

        point = bestPoint;
        distSq = best;
        return found;
    }

    // Counteracts view punch by moving the mouse the opposite way. Deltas are
    // accumulated because the game reports the total punch, not per tick.
    private void ApplyRecoilControl(World world, Config cfg, int clientWidth)
    {
        var punch = world.AimPunch;
        if (punch.X == _lastPunch.X && punch.Y == _lastPunch.Y) return;

        var degreesPerPixel = MathF.Max(1.0f, cfg.Visuals.Fov) / Math.Max(1, clientWidth);
        var pixelsPerDegree = 1.0f / MathF.Max(0.001f, degreesPerPixel);

        // +yaw is counter clockwise (left), so a positive punch.y is cancelled by
        // moving the mouse right; +pitch looks down, cancelled by moving up.
        var dx = (punch.Y - _lastPunch.Y) * cfg.Aim.RcsX * pixelsPerDegree;
        var dy = -(punch.X - _lastPunch.X) * cfg.Aim.RcsY * pixelsPerDegree;

        _lastPunch = punch;

        MoveMouse(dx / MathF.Max(1.0f, cfg.Aim.RcsSmooth), dy / MathF.Max(1.0f, cfg.Aim.RcsSmooth));
    }

    public void Tick(World world, Config cfg, int clientWidth, int clientHeight, uint targetPid,
        bool aimAllowed, bool triggerAllowed)
    {
        _status.HasTarget = false;
        _status.Firing = _leftHeld;
        _status.Scoped = _rightHeld;

        var now = NowMs();
        var dt = Math.Max(0.0001, (now - _lastTickMs) / 1000.0);
        _lastTickMs = now;

        if (_leftHeld && now >= _releaseAtMs)
        {
            Input.SendMouseButton(false, false);
            _leftHeld = false;
        }

        if (_rightHeld && now >= _rightReleaseAtMs)
        {
            Input.SendMouseButton(false, true);
            _rightHeld = false;
        }

        var focused = Input.Foreground(targetPid);
        if (!focused || !world.HaveLocal || clientWidth <= 0 || clientHeight <= 0)
        {
            ReleaseAll();
            return;
        }

        UpdateHistory(world, now);

        // Key handling: hold mode reads the key directly, toggle mode flips a
        // latched state on each press.
        bool ResolveKey(bool toggleMode, int vk, ref bool wasDown, ref bool state)
        {
            if (vk <= 0) return true;

            var down = Input.IsKeyDown(vk);
            if (toggleMode)
            {
                if (down && !wasDown) state = !state;
                wasDown = down;
                return state;
            }

            wasDown = down;
            return down;
        }

        var aimActive = aimAllowed && cfg.Aim.Enabled &&
                        ResolveKey(cfg.Aim.Toggle, cfg.Aim.Key, ref _aimKeyWasDown, ref _aimToggleState);
        var triggerActive = triggerAllowed && cfg.Trigger.Enabled &&
                            ResolveKey(cfg.Trigger.Toggle, cfg.Trigger.Key, ref _triggerKeyWasDown,
                                ref _triggerToggleState);

        if (aimActive && cfg.Aim.OnlyScoped && cfg.Scoped >= 0 && !world.LocalScoped) aimActive = false;
        if (triggerActive && cfg.Trigger.OnlyScoped && cfg.Scoped >= 0 && !world.LocalScoped)
            triggerActive = false;

        var cx = clientWidth * 0.5f;
        var cy = clientHeight * 0.5f;

        var candidates = new List<Candidate>();

        var triggerIndex = -1;
        var triggerDist = float.MaxValue;
        var triggerPoint = default(Vec2);

        for (var i = 0; i < world.Players.Count; ++i)
        {
            var p = world.Players[i];
            var sameTeam = world.LocalTeam != 0 && p.Team == world.LocalTeam;

            if (aimActive && !(cfg.Aim.TeamCheck && sameTeam))
            {
                var distanceOk = cfg.Aim.MaxDistance <= 0.0f || p.Distance <= cfg.Aim.MaxDistance;
                var visibleOk = !cfg.Aim.VisibleOnly || p.Spotted;
                var flashOk = !cfg.Aim.IgnoreFlashed || !p.Flashed;

                if (distanceOk && visibleOk && flashOk)
                {
                    AimSample? sample = null;
                    if (cfg.Aim.Backtrack && cfg.Aim.BacktrackMs > 0.0f)
                    {
                        if (_history.TryGetValue(p.Address, out var history) && history.Count > 0)
                        {
                            var wanted = now - cfg.Aim.BacktrackMs;
                            var bestDiff = 1.0e9;
                            foreach (var entry in history)
                            {
                                var diff = Math.Abs(entry.Time - wanted);
                                if (diff < bestDiff)
                                {
                                    bestDiff = diff;
                                    sample = entry;
                                }
                            }
                        }
                    }

                    if (BestHitboxPoint(world, p, cfg, clientWidth, clientHeight, out var point,
                            out var distSq, sample))
                    {
                        var fov = EffectiveFov(cfg, p.Distance);
                        if (distSq <= fov * fov)
                        {
                            var score = MathF.Sqrt(distSq);
                            if (cfg.Aim.Priority == 1) score = p.Distance;
                            if (cfg.Aim.Priority == 3) score = p.Health;

                            candidates.Add(new Candidate
                            {
                                Address = p.Address,
                                Index = i,
                                Point = point,
                                Fov = fov,
                                Score = score,
                            });
                        }
                    }
                }
            }

            if (triggerActive && !(cfg.Trigger.TeamCheck && sameTeam))
            {
                var distanceOk = cfg.Trigger.MaxDistance <= 0.0f || p.Distance <= cfg.Trigger.MaxDistance;
                var visibleOk = (!cfg.Trigger.VisibleOnly && !cfg.Trigger.WallCheck) || p.Spotted;

                if (distanceOk && visibleOk)
                {
                    if (BestHitboxPoint(world, p, cfg, clientWidth, clientHeight, out var point,
                            out var distSq))
                    {
                        if (distSq <= cfg.Trigger.Radius * cfg.Trigger.Radius && distSq < triggerDist)
                        {
                            triggerDist = distSq;
                            triggerIndex = i;
                            triggerPoint = point;
                        }
                    }
                }
            }
        }

        var chosen = -1;
        if (aimActive && !cfg.Aim.TargetSwitch && _lockedTarget != 0)
        {
            for (var i = 0; i < candidates.Count; ++i)
            {
                if (candidates[i].Address == _lockedTarget)
                {
                    chosen = i;
                    break;
                }
            }
        }

        if (chosen < 0 && candidates.Count > 0)
        {
            var best = float.MaxValue;
            for (var i = 0; i < candidates.Count; ++i)
            {
                if (candidates[i].Score < best)
                {
                    best = candidates[i].Score;
                    chosen = i;
                }
            }
        }

        var aimIndex = -1;
        var aimPoint = default(Vec2);
        var aimFov = cfg.Aim.Fov;
        ulong aimAddress = 0;

        if (aimActive && chosen >= 0)
        {
            var candidate = candidates[chosen];
            aimIndex = candidate.Index;
            aimPoint = candidate.Point;
            aimFov = candidate.Fov;
            aimAddress = candidate.Address;

            // External resolver approximation: a fixed or jittered lateral offset.
            if (cfg.Aim.ResolverMode == 1)
                aimPoint.X += (aimAddress & 1UL) != 0 ? 4.0f : -4.0f;
            else if (cfg.Aim.ResolverMode == 2)
            {
                aimPoint.X += RandomFloat(-5.0f, 5.0f);
                aimPoint.Y += RandomFloat(-3.0f, 3.0f);
            }
        }

        if (aimActive && aimIndex >= 0)
        {
            _status.HasTarget = true;
            _status.TargetIndex = aimIndex;
            _status.TargetScreen = aimPoint;
            _status.FovPixels = aimFov;

            if (_reactionAddress != aimAddress)
            {
                var hadTarget = _lockedTarget != 0;
                _reactionAddress = aimAddress;
                _lockedTarget = aimAddress;

                var delay = (double)cfg.Aim.ReactionTime + RandomFloat(0.0f, cfg.Aim.ReactionJitter);
                if (hadTarget && cfg.Aim.TargetSwitch) delay += cfg.Aim.SwitchDelay;
                _reactionUntilMs = now + delay * 1000.0;

                _firstShotTarget = aimAddress;
                _firstShotAtMs = now + cfg.Aim.FirstShotDelay * 1000.0;
            }

            if (now >= _reactionUntilMs)
            {
                var dx = aimPoint.X - cx;
                var dy = aimPoint.Y - cy;

                var smooth = MathF.Max(1.0f, cfg.Aim.Smooth);
                if (cfg.Aim.SmoothMode == 1)
                {
                    var tau = 0.02f * smooth;
                    var alpha = 1.0f - MathF.Exp(-(float)dt / tau);
                    dx *= alpha;
                    dy *= alpha;
                }
                else
                {
                    dx /= smooth;
                    dy /= smooth;
                }

                if (cfg.Aim.Humanize > 0.0f)
                {
                    var amount = cfg.Aim.Humanize * MathF.Max(0.0f, cfg.Aim.HumanizeJitter);
                    dx += RandomFloat(-amount, amount) * (MathF.Abs(dx) + 1.0f);
                    dy += RandomFloat(-amount, amount) * (MathF.Abs(dy) + 1.0f);
                }

                var length = MathF.Sqrt(dx * dx + dy * dy);
                if (cfg.Aim.AimStep > 0.0f && length > cfg.Aim.AimStep)
                {
                    var k = cfg.Aim.AimStep / length;
                    dx *= k;
                    dy *= k;
                    length = cfg.Aim.AimStep;
                }

                if (length > 0.3f) MoveMouse(dx * cfg.Aim.AimScale, dy * cfg.Aim.AimScale);
            }
        }
        else
        {
            _reactionAddress = 0;
            if (cfg.Aim.TargetSwitch) _lockedTarget = 0;
        }

        // ------------------------------------------------------------------
        // Movement assists.
        // ------------------------------------------------------------------
        var wantStop = cfg.Aim.AutoStop && aimActive && _status.HasTarget && now >= _reactionUntilMs;

        if (wantStop)
        {
            var yaw = world.ViewAngles.Y * MathUtil.Pi / 180.0f;
            var rightX = MathF.Sin(yaw);
            var rightY = -MathF.Cos(yaw);
            var dot = world.LocalVelocity.X * rightX + world.LocalVelocity.Y * rightY;

            var key = 0;
            if (dot > 15.0f) key = 'A';
            else if (dot < -15.0f) key = 'D';

            if (key != _stopKey)
            {
                if (_stopKey != 0) Input.SendKey(_stopKey, false);
                if (key != 0) Input.SendKey(key, true);
                _stopKey = key;
            }
        }
        else if (_stopKey != 0)
        {
            Input.SendKey(_stopKey, false);
            _stopKey = 0;
        }

        var wantCrouch = cfg.Aim.AutoCrouch && aimActive && _status.HasTarget;
        if (wantCrouch != _crouchHeld)
        {
            Input.SendKey(Vk.Control, wantCrouch);
            _crouchHeld = wantCrouch;
        }

        var autoPistol = (cfg.Aim.AutoPistol || cfg.Misc.AutoPistol) && Weapons.IsPistol(world.LocalWeapon);

        var rcsActive = cfg.Aim.Rcs && (_leftHeld || cfg.Aim.RcsStandalone) &&
                        (!cfg.Aim.RcsScopeOnly || world.LocalScoped) && world.AimPunch.X != 0.0f;
        if (rcsActive) ApplyRecoilControl(world, cfg, clientWidth);
        else _lastPunch = world.AimPunch;

        if (cfg.Aim.AutoScope && cfg.Scoped >= 0 && world.HaveLocal && !world.LocalScoped &&
            Weapons.IsSniper(world.LocalWeapon) && _status.HasTarget && !_rightHeld &&
            now >= _scopeCooldownUntil)
        {
            Input.SendMouseButton(true, true);
            _rightHeld = true;
            _rightReleaseAtMs = now + 40.0;
            _scopeCooldownUntil = now + 350.0;
        }

        var wantFire = false;
        if (triggerActive && triggerIndex >= 0 && now >= _burstCooldownUntil && now >= _burstNextAtMs)
        {
            wantFire = true;
            _status.TargetIndex = triggerIndex;
            _status.TargetScreen = triggerPoint;
        }

        if (cfg.Aim.AutoFire && aimActive && _status.HasTarget && now >= _reactionUntilMs &&
            now >= _firstShotAtMs)
        {
            var dx = aimPoint.X - cx;
            var dy = aimPoint.Y - cy;
            if (dx * dx + dy * dy <= 9.0f) wantFire = true;
        }

        if (autoPistol && now < _pistolCooldownUntil) wantFire = false;

        if (wantFire)
        {
            if (!_leftHeld)
            {
                if (_fireAtMs < 0.0)
                {
                    _fireAtMs = cfg.Trigger.Randomize
                        ? now + RandomBetween(cfg.Trigger.DelayMin, cfg.Trigger.DelayMax)
                        : now;
                }

                if (now >= _fireAtMs)
                {
                    var hold = cfg.Trigger.Randomize
                        ? RandomBetween(cfg.Trigger.HoldMin, cfg.Trigger.HoldMax)
                        : cfg.Trigger.HoldMin;

                    Input.SendMouseButton(true, false);
                    _leftHeld = true;
                    _releaseAtMs = now + Math.Max(1, hold);
                    _fireAtMs = -1.0;
                    ++_burstShots;

                    if (autoPistol) _pistolCooldownUntil = now + Math.Max(80, hold) + 50.0;

                    if (cfg.Trigger.Burst && _burstShots >= Math.Max(1, cfg.Trigger.BurstShots))
                    {
                        _burstShots = 0;
                        _burstCooldownUntil = now + cfg.Trigger.BurstCooldown * 1000.0;
                        _burstNextAtMs = 0.0;
                    }
                    else if (cfg.Trigger.Burst)
                    {
                        _burstNextAtMs = now + cfg.Trigger.BurstDelay * 1000.0;
                    }
                }
            }
        }
        else if (!_leftHeld)
        {
            _fireAtMs = -1.0;
            _burstShots = 0;
            _burstNextAtMs = 0.0;
        }

        _status.Firing = _leftHeld;
        _status.Scoped = _rightHeld;
    }

    private struct Candidate
    {
        public ulong Address;
        public int Index;
        public Vec2 Point;
        public float Fov;
        public float Score;
    }
}
