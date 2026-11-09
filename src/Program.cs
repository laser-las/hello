using System.Text;

namespace Halcyon;

internal static class Program
{
    private sealed class Options
    {
        public string? ConfigPath;
        public string? ImportPath;
        public string? SavePath;
        public string? PresetName;
        public bool Dump;
        public bool NoOverlay;
        public bool Help;
    }

    // Everything the scanner thread publishes for the render loop. World and
    // AimStatus are replaced as whole objects so the reader never walks a list
    // that is being mutated underneath it.
    private sealed class Shared
    {
        public volatile World World = new();
        public volatile AimStatus AimStatus = new();
        public volatile bool Esp = true;
        public volatile bool Aim = true;
        public volatile bool Trigger = true;
        public volatile bool Stop;
        public volatile bool MenuOpen;
        public volatile bool ResolveRequested;
        public volatile int ClientWidth;
        public volatile int ClientHeight;
    }

    private sealed class KeyEdge
    {
        private bool _down;

        public bool Pressed(int vk)
        {
            if (vk <= 0)
            {
                _down = false;
                return false;
            }

            var now = Input.IsKeyDown(vk);
            var edge = now && !_down;
            _down = now;
            return edge;
        }
    }

    [STAThread]
    private static int Main(string[] args)
    {
        AttachConsole();

        try
        {
            Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        }
        catch
        {
            // Older Windows builds fall back to the manifest declaration.
        }

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        Native.timeBeginPeriod(1);

        try
        {
            var code = Run(args);
            Log.CloseFile();
            return code;
        }
        catch (Exception ex)
        {
            Log.Info("fatal: {0}", ex);
            Log.CloseFile();
            return 1;
        }
        finally
        {
            Native.timeEndPeriod(1);
        }
    }

    private static int Run(string[] args)
    {
        var options = ParseOptions(args);
        if (options.Help)
        {
            PrintUsage();
            return 0;
        }

        var baseDir = AppContext.BaseDirectory;
        var configPath = options.ConfigPath ?? Path.Combine(baseDir, "config.ini");
        var logPath = Path.Combine(baseDir, "halcyon.log");

        Log.OpenFile(logPath);

        var cfg = new Config();

        if (!File.Exists(configPath))
        {
            cfg.ApplyPreset(Preset.Legit);
            if (!cfg.Save(configPath))
            {
                Log.Info("config: cannot create {0}", configPath);
                return 1;
            }

            Log.Info("config: wrote defaults to {0}", configPath);
        }
        else if (!cfg.Load(configPath))
        {
            Log.Info("config: cannot read {0}", configPath);
            return 1;
        }

        if (options.ImportPath != null)
        {
            var applied = cfg.ImportOffsets(options.ImportPath);
            Log.Info("import: {0} entries applied", applied);
        }

        if (options.PresetName != null)
        {
            var preset = Config.PresetFromString(options.PresetName);
            if (preset == Preset.Custom)
            {
                Log.Info("unknown preset: {0}", options.PresetName);
                return 1;
            }

            cfg.ApplyPreset(preset);
            Log.Info("preset applied: {0}", Config.PresetName(preset));
        }

        if (options.SavePath != null)
        {
            if (cfg.Save(options.SavePath)) Log.Info("config written to {0}", options.SavePath);
            else Log.Info("could not write {0}", options.SavePath);
        }

        Log.Info("config loaded: {0}", configPath);

        var proc = new Halcyon.Process();

        try
        {
            while (!proc.Attach(cfg.Process))
            {
                if (options.Dump)
                {
                    Log.Info("process {0} is not running", cfg.Process);
                    return 1;
                }

                if (Input.IsKeyDown(cfg.QuitKey))
                {
                    Log.Info("stopped while waiting for {0}", cfg.Process);
                    return 0;
                }

                Log.Info("waiting for {0} ...", cfg.Process);
                Thread.Sleep(1000);
            }

            Log.Info("attached to pid {0} ({1}), layout={2}", proc.Pid, cfg.Process, cfg.Layout);

            var client = proc.Module(cfg.ClientModule);
            if (client != null)
                Log.Info("module {0} base=0x{1:X} size=0x{2:X}", client.Name, client.Base, client.Size);
            else
                Log.Info("module {0} not found", cfg.ClientModule);

            var game = new Game(proc, cfg);
            var offsets = game.Resolve();

            if (options.Dump)
            {
                Log.Info(
                    "fields: health=0x{0:X} team=0x{1:X} life=0x{2:X} origin=0x{3:X} bone=0x{4:X} head_bone={5} stride=0x{6:X}",
                    cfg.Health, cfg.Team, cfg.LifeState, cfg.Origin, cfg.BoneMatrix, cfg.HeadBone, cfg.EntityStride);
                Log.Info("optional: armor=0x{0:X} weapon=0x{1:X} ammo=0x{2:X} velocity=0x{3:X} punch=0x{4:X}",
                    cfg.Armor, cfg.Weapon, cfg.Ammo, cfg.Velocity, cfg.AimPunch);
                Log.Info("resolved: entity_list=0x{0:X} local_player=0x{1:X} view_matrix=0x{2:X}",
                    offsets.EntityList, offsets.LocalPlayer, offsets.ViewMatrix);
                Log.Info(offsets.Valid ? "all required offsets resolved" : "incomplete offsets, check config.ini");
                return offsets.Valid ? 0 : 2;
            }

            if (!offsets.Valid)
                Log.Info("warning: offsets incomplete, nothing will be drawn until config.ini is filled in");

            var shared = new Shared
            {
                Esp = cfg.Esp.Enabled,
                Aim = cfg.Aim.Enabled,
                Trigger = cfg.Trigger.Enabled,
            };

            var aim = new Aim();
            var misc = new Misc();

            var scanner = new Thread(() => ScannerLoop(proc, game, cfg, shared, aim, misc))
            {
                IsBackground = true,
                Name = "halcyon-scanner",
                Priority = ThreadPriority.AboveNormal,
            };
            scanner.Start();

            try
            {
                return options.NoOverlay
                    ? RunHeadless(cfg, shared, scanner)
                    : RunOverlay(cfg, shared, scanner, configPath);
            }
            finally
            {
                shared.Stop = true;
                if (!scanner.Join(3000)) Log.Info("scanner: did not stop in time");
            }
        }
        finally
        {
            proc.Dispose();
        }
    }

    private static void ScannerLoop(Halcyon.Process proc, Game game, Config cfg, Shared shared, Aim aim, Misc misc)
    {
        var period = 1000.0 / Math.Clamp(cfg.AimHz, 30, 2000);
        var tickClock = new Clock();
        var master = new Clock();
        var nextResolveAt = 0.0;
        var readFailures = 0;

        while (!shared.Stop)
        {
            tickClock.Reset();

            if (!proc.Running)
            {
                Log.Info("target process exited");
                shared.Stop = true;
                break;
            }

            if (shared.ResolveRequested)
            {
                shared.ResolveRequested = false;
                game.Resolve();
            }

            if (!game.Resolved.Valid && master.Ms() >= nextResolveAt)
            {
                nextResolveAt = master.Ms() + 2000.0;
                game.Resolve();
            }

            var world = new World();
            if (!game.Update(world))
            {
                if (++readFailures >= 25)
                {
                    Log.Info("memory read failed, stopping");
                    shared.Stop = true;
                    break;
                }

                Thread.Sleep(40);
                continue;
            }

            readFailures = 0;

            var width = shared.ClientWidth;
            var height = shared.ClientHeight;

            if (shared.MenuOpen)
            {
                aim.Reset();
                misc.Reset();
                shared.AimStatus = new AimStatus();
            }
            else if (width > 0 && height > 0)
            {
                aim.Tick(world, cfg, width, height, proc.Pid, shared.Aim, shared.Trigger);
                shared.AimStatus = Snapshot(aim.Status);
                misc.Tick(world, cfg, proc.Pid);
            }

            shared.World = world;

            var remaining = period - tickClock.Ms();
            if (remaining >= 1.0) Thread.Sleep((int)Math.Round(remaining));
            else Thread.Yield();
        }

        aim.Reset();
        misc.Reset();
    }

    private static AimStatus Snapshot(AimStatus status)
    {
        return new AimStatus
        {
            HasTarget = status.HasTarget,
            Firing = status.Firing,
            Scoped = status.Scoped,
            TargetIndex = status.TargetIndex,
            TargetScreen = status.TargetScreen,
            FovPixels = status.FovPixels,
        };
    }

    private static int RunHeadless(Config cfg, Shared shared, Thread scanner)
    {
        Log.Info("running without overlay, press the quit key to stop");

        var quitKey = new KeyEdge();
        var panicKey = new KeyEdge();
        var espKey = new KeyEdge();
        var aimKey = new KeyEdge();
        var triggerKey = new KeyEdge();

        while (!shared.Stop)
        {
            var quit = quitKey.Pressed(cfg.QuitKey);
            var panic = panicKey.Pressed(cfg.PanicKey);
            var esp = espKey.Pressed(cfg.EspToggleKey);
            var aim = aimKey.Pressed(cfg.AimToggleKey);
            var trigger = triggerKey.Pressed(cfg.TriggerToggleKey);

            if (quit) break;

            if (panic)
            {
                shared.Esp = false;
                shared.Aim = false;
                shared.Trigger = false;
                Log.Info("panic: everything disabled");
            }
            else
            {
                if (esp) shared.Esp = !shared.Esp;
                if (aim) shared.Aim = !shared.Aim;
                if (trigger) shared.Trigger = !shared.Trigger;
            }

            Thread.Sleep(20);
        }

        shared.Stop = true;
        scanner.Join(3000);
        Log.Info("stopped");
        return 0;
    }

    private static int RunOverlay(Config cfg, Shared shared, Thread scanner, string configPath)
    {
        using var overlay = new Overlay(cfg.WindowTitle, cfg.WindowClass);
        using var settings = new SettingsForm(cfg, configPath);

        settings.ExitRequested += () => shared.Stop = true;

        // Only re-resolve when one of the address specs actually changed, so
        // closing the window does not rescan client.dll on every edit.
        var offsetFingerprint = OffsetFingerprint(cfg);
        settings.Applied += () =>
        {
            var next = OffsetFingerprint(cfg);
            if (next == offsetFingerprint) return;
            offsetFingerprint = next;
            shared.ResolveRequested = true;
        };

        if (overlay.Handle == IntPtr.Zero)
        {
            Log.Info("overlay creation failed");
            shared.Stop = true;
            scanner.Join(3000);
            return 1;
        }

        cfg.LogSummary();

        var menuKey = new KeyEdge();
        var quitKey = new KeyEdge();
        var espKey = new KeyEdge();
        var aimKey = new KeyEdge();
        var triggerKey = new KeyEdge();
        var panicKey = new KeyEdge();

        var esp = new EspRenderer();
        var frameClock = new Clock();
        var fpsClock = new Clock();
        var framePeriod = cfg.RenderFps > 0 ? 1000.0 / Math.Clamp(cfg.RenderFps, 1, 1000) : 0.0;

        var frames = 0;
        var fps = 0.0f;
        var settingsWasVisible = false;
        var state = new EspState();

        while (!shared.Stop)
        {
            Application.DoEvents();

            // Read every edge first so the latch state stays correct while a
            // capture box or a settings control is focused.
            var menuPressed = menuKey.Pressed(cfg.MenuKey);
            var quitPressed = quitKey.Pressed(cfg.QuitKey);
            var espPressed = espKey.Pressed(cfg.EspToggleKey);
            var aimPressed = aimKey.Pressed(cfg.AimToggleKey);
            var triggerPressed = triggerKey.Pressed(cfg.TriggerToggleKey);
            var panicPressed = panicKey.Pressed(cfg.PanicKey);

            var capturing = settings.Capturing;

            if (!capturing)
            {
                if (menuPressed)
                {
                    if (settings.Visible)
                    {
                        settings.Hide();
                    }
                    else
                    {
                        settings.Show();
                        settings.Activate();
                        settings.BringToFront();
                    }
                }

                if (quitPressed) break;

                if (!settings.Visible)
                {
                    if (espPressed) shared.Esp = !shared.Esp;
                    if (aimPressed) shared.Aim = !shared.Aim;
                    if (triggerPressed) shared.Trigger = !shared.Trigger;
                }

                if (panicPressed)
                {
                    shared.Esp = false;
                    shared.Aim = false;
                    shared.Trigger = false;
                    Log.Info("panic: everything disabled");
                }
            }

            if (settings.Visible != settingsWasVisible)
            {
                shared.MenuOpen = settings.Visible;

                if (!settings.Visible)
                {
                    // The settings window edits a working copy, so push it into
                    // the live config when it closes, like the old drawn menu.
                    settings.ApplyAll();
                    overlay.RefocusGame();
                }

                settingsWasVisible = settings.Visible;
            }

            frameClock.Reset();

            var world = shared.World;
            var status = shared.AimStatus;

            if (overlay.BeginFrame())
            {
                shared.ClientWidth = overlay.WidthPx;
                shared.ClientHeight = overlay.HeightPx;

                state.EspOn = shared.Esp;
                state.AimOn = shared.Aim;
                state.TriggerOn = shared.Trigger;
                state.HasAimTarget = status.HasTarget;
                state.AimTarget = status.TargetScreen;
                state.Fps = fps;
                state.ScanMs = world.ScanMs;
                state.Players = world.Players.Count;
                state.Preset = Config.PresetName(cfg.Preset);

                esp.Draw(overlay, world, cfg, state);
                overlay.EndFrame();
            }

            ++frames;
            if (fpsClock.Ms() >= 500.0)
            {
                fps = (float)(frames * 1000.0 / fpsClock.Ms());
                frames = 0;
                fpsClock.Reset();
            }

            var remaining = framePeriod - frameClock.Ms();
            if (remaining >= 1.0) Thread.Sleep((int)Math.Round(remaining));
            else Thread.Yield();
        }

        shared.Stop = true;
        if (!scanner.Join(3000)) Log.Info("scanner: did not stop in time");

        settings.CloseForExit();
        Log.Info("stopped");
        return 0;
    }

    private static string OffsetFingerprint(Config cfg)
    {
        return string.Join('\n',
            cfg.EntityList.Raw, cfg.LocalPlayer.Raw, cfg.ViewMatrix.Raw, cfg.ClientModule, cfg.Layout);
    }

    private static Options ParseOptions(string[] args)
    {
        var options = new Options();

        for (var i = 0; i < args.Length; ++i)
        {
            switch (args[i])
            {
                case "--config" when i + 1 < args.Length:
                    options.ConfigPath = args[++i];
                    break;
                case "--import" when i + 1 < args.Length:
                    options.ImportPath = args[++i];
                    break;
                case "--save" when i + 1 < args.Length:
                    options.SavePath = args[++i];
                    break;
                case "--preset" when i + 1 < args.Length:
                    options.PresetName = args[++i];
                    break;
                case "--dump":
                    options.Dump = true;
                    break;
                case "--no-overlay":
                    options.NoOverlay = true;
                    break;
                case "--help":
                case "-h":
                    options.Help = true;
                    break;
                default:
                    Log.Info("argument ignored: {0}", args[i]);
                    break;
            }
        }

        return options;
    }

    private static void PrintUsage()
    {
        var exe = Path.GetFileName(Environment.ProcessPath ?? "halcyon.exe");
        var text = new StringBuilder();
        text.AppendLine("usage: " + exe + " [options]");
        text.AppendLine("  --config <path>   config file (default: config.ini next to the exe)");
        text.AppendLine("  --import <path>   apply a flat offset dump (dwEntityList = 0x..., m_iHealth: 344)");
        text.AppendLine("  --save <path>     write the merged configuration back out");
        text.AppendLine("  --preset <name>   legit | semilegit | semirage | rage");
        text.AppendLine("  --dump            resolve offsets, print them and exit");
        text.AppendLine("  --no-overlay      aim/trigger only, no ESP window");
        text.AppendLine();
        text.AppendLine("INSERT opens the settings window, the quit key stops the client.");

        Console.Write(text.ToString());
        Log.Write(text.ToString().TrimEnd());
    }

    private static void AttachConsole()
    {
        // A WinExe starts without a console. When launched from a terminal,
        // reattach so --dump and the log stream are visible there.
        if (!Native.AttachConsole(Native.ATTACH_PARENT_PROCESS)) return;

        try
        {
            var utf8 = new UTF8Encoding(false);

            var stdout = Console.OpenStandardOutput();
            Console.SetOut(new StreamWriter(stdout, utf8) { AutoFlush = true });

            var stderr = Console.OpenStandardError();
            Console.SetError(new StreamWriter(stderr, utf8) { AutoFlush = true });

            Console.OutputEncoding = utf8;
        }
        catch
        {
            // No usable console; the log file still receives everything.
        }
    }
}
