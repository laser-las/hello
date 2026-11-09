namespace Halcyon;

// Movement and chat automation that does not depend on the aim snapshot path:
// bunny hop, auto strafe, auto reload, chat spam / kill say and a small knife
// bot. All input goes through SendInput and is only sent while the game owns
// the foreground window.
public sealed class Misc
{
    private readonly Clock _clock = new();

    private double _bhopDownAt;
    private double _bhopNextAt;
    private int _strafeKey;
    private double _reloadNextAt;

    private bool _knifeHeld;
    private double _knifeReleaseAt;
    private double _knifeNextAt;

    private readonly Queue<string> _chatQueue = new();
    private int _chatStage;
    private double _chatStageAt;
    private string _chatCurrent = "";
    private double _chatNextAt;

    private readonly Dictionary<ulong, double> _lastSeen = new();

    public void Reset()
    {
        ReleaseMovement();
        _chatQueue.Clear();
        _chatCurrent = "";
        _chatStage = 0;
        _lastSeen.Clear();
    }

    private void ReleaseMovement()
    {
        if (_bhopDownAt > 0.0)
        {
            Input.SendKey(Vk.Space, false);
            _bhopDownAt = 0.0;
        }

        if (_strafeKey != 0)
        {
            Input.SendKey(_strafeKey, false);
            _strafeKey = 0;
        }

        if (_knifeHeld)
        {
            Input.SendMouseButton(false, false);
            _knifeHeld = false;
        }
    }

    private void QueueChat(string text)
    {
        if (string.IsNullOrEmpty(text)) return;
        if (_chatQueue.Count >= 8) return;
        _chatQueue.Enqueue(text);
    }

    private void UpdateChat(uint targetPid, double now)
    {
        if (_chatStage > 0 && !Input.Foreground(targetPid))
        {
            _chatStage = 0;
            _chatCurrent = "";
        }

        if (_chatStage == 0 && _chatQueue.Count > 0 && Input.Foreground(targetPid))
        {
            _chatCurrent = _chatQueue.Dequeue();
            _chatStage = 1;
            _chatStageAt = now;
        }

        if (_chatStage == 0) return;
        if (now < _chatStageAt) return;

        switch (_chatStage)
        {
            case 1:
                Input.Tap('Y');
                _chatStage = 2;
                _chatStageAt = now + 70.0;
                break;
            case 2:
                Input.SendText(_chatCurrent);
                _chatStage = 3;
                _chatStageAt = now + 60.0;
                break;
            case 3:
                Input.Tap(Vk.Return);
                _chatStage = 0;
                _chatCurrent = "";
                _chatStageAt = now + 250.0;
                break;
            default:
                _chatStage = 0;
                break;
        }
    }

    public void Tick(World world, Config cfg, uint targetPid)
    {
        var now = _clock.Ms();
        var focused = Input.Foreground(targetPid);

        if (!focused)
        {
            ReleaseMovement();
            return;
        }

        // ------------------------------------------------------------------
        // Bunny hop: pulse the jump key while the bind is held so every landing
        // receives a fresh key-down edge.
        // ------------------------------------------------------------------
        var wantBhop = cfg.Misc.Bhop && cfg.Misc.BhopKey > 0 && Input.IsKeyDown(cfg.Misc.BhopKey);
        if (wantBhop)
        {
            if (_bhopDownAt == 0.0 && now >= _bhopNextAt)
            {
                Input.SendKey(Vk.Space, true);
                _bhopDownAt = now;
            }
            else if (_bhopDownAt > 0.0 && now - _bhopDownAt >= 10.0)
            {
                Input.SendKey(Vk.Space, false);
                _bhopDownAt = 0.0;
                _bhopNextAt = now + 12.0;
            }
        }
        else if (_bhopDownAt > 0.0)
        {
            Input.SendKey(Vk.Space, false);
            _bhopDownAt = 0.0;
        }

        // ------------------------------------------------------------------
        // Auto strafe: while the local player is airborne, hold the strafe key
        // that curves the velocity in the direction it is already travelling.
        // ------------------------------------------------------------------
        var speed2d = MathF.Sqrt(world.LocalVelocity.X * world.LocalVelocity.X +
                                 world.LocalVelocity.Y * world.LocalVelocity.Y);
        var airborne = MathF.Abs(world.LocalVelocity.Z) > 1.0f;
        var desiredStrafe = 0;

        if (cfg.Misc.AutoStrafe && world.HaveLocal && airborne && speed2d > 30.0f)
        {
            var yaw = world.ViewAngles.Y * MathUtil.Pi / 180.0f;
            var rightX = MathF.Sin(yaw);
            var rightY = -MathF.Cos(yaw);
            var dot = world.LocalVelocity.X * rightX + world.LocalVelocity.Y * rightY;

            desiredStrafe = dot > 0.0f ? 'A' : 'D';

            if (cfg.Misc.StrafeStrength < 0.5f)
            {
                var phase = (int)(now / 40.0);
                if ((phase & 1) != 0) desiredStrafe = 0;
            }
        }

        if (desiredStrafe != _strafeKey)
        {
            if (_strafeKey != 0) Input.SendKey(_strafeKey, false);
            if (desiredStrafe != 0) Input.SendKey(desiredStrafe, true);
            _strafeKey = desiredStrafe;
        }

        // ------------------------------------------------------------------
        // Auto reload: m_ammo has to be configured for this to be meaningful.
        // ------------------------------------------------------------------
        if (cfg.Misc.AutoReload && cfg.Ammo >= 0 && world.HaveLocal && world.LocalAmmo == 0 &&
            Weapons.IsGun(world.LocalWeapon) && now >= _reloadNextAt)
        {
            Input.Tap('R');
            _reloadNextAt = now + 450.0;
        }

        // ------------------------------------------------------------------
        // Knife bot: tap attack when an enemy is inside knife range.
        // ------------------------------------------------------------------
        if (_knifeHeld && now >= _knifeReleaseAt)
        {
            Input.SendMouseButton(false, false);
            _knifeHeld = false;
        }

        if (cfg.Misc.KnifeBot && Weapons.IsKnife(world.LocalWeapon) && now >= _knifeNextAt)
        {
            var inRange = false;
            foreach (var player in world.Players)
            {
                if (world.LocalTeam != 0 && player.Team == world.LocalTeam) continue;
                if (player.Distance <= 75.0f)
                {
                    inRange = true;
                    break;
                }
            }

            if (inRange)
            {
                Input.SendMouseButton(true, false);
                _knifeHeld = true;
                _knifeReleaseAt = now + 110.0;
                _knifeNextAt = now + 260.0;
            }
        }

        // ------------------------------------------------------------------
        // Chat spam and kill say.
        // ------------------------------------------------------------------
        if (cfg.Misc.ChatSpam && !string.IsNullOrEmpty(cfg.Misc.ChatSpamText) && now >= _chatNextAt &&
            _chatQueue.Count == 0 && _chatStage == 0)
        {
            QueueChat(cfg.Misc.ChatSpamText);
            _chatNextAt = now + MathF.Max(0.5f, cfg.Misc.ChatSpamDelay) * 1000.0;
        }

        // Approximate kill detection: an entity that was alive, in range, and has
        // now been gone for more than a second is treated as a kill. Dormant
        // players can produce a false positive, which is why the feature is off
        // by default.
        if (cfg.Misc.KillSay && !string.IsNullOrEmpty(cfg.Misc.KillSayText))
        {
            foreach (var player in world.Players) _lastSeen[player.Address] = now;

            var gone = new List<ulong>();
            foreach (var (address, seen) in _lastSeen)
            {
                if (now - seen > 1100.0)
                {
                    QueueChat(cfg.Misc.KillSayText);
                    gone.Add(address);
                }
            }

            foreach (var address in gone) _lastSeen.Remove(address);
        }
        else
        {
            _lastSeen.Clear();
        }

        UpdateChat(targetPid, now);
    }
}
