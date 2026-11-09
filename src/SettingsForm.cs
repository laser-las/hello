using System.Drawing;
using System.Globalization;

namespace Halcyon;

// Tabbed WinForms configuration surface. The in-game menu from the C++ build is
// replaced with a normal window so every option is editable with standard
// controls while the overlay stays click-through.
public sealed class SettingsForm : Form
{
    private readonly Config _cfg;
    private readonly string _configPath;
    private readonly List<Action> _loaders = new();
    private readonly List<Action> _appliers = new();
    private readonly ToolTip _tip = new();
    private readonly Label _status = new();

    private readonly System.Windows.Forms.Timer _captureTimer = new() { Interval = 25 };
    private KeyBox? _captureBox;
    private long _captureArmAt;

    private bool _allowClose;

    public event Action? ExitRequested;
    public event Action? Applied;

    public bool Capturing => _captureBox != null;

    private static readonly Color Back = Color.FromArgb(24, 24, 28);
    private static readonly Color PanelBack = Color.FromArgb(32, 32, 38);
    private static readonly Color TextColor = Color.FromArgb(232, 232, 236);
    private static readonly Color Accent = Color.FromArgb(124, 77, 255);

    public SettingsForm(Config cfg, string configPath)
    {
        _cfg = cfg;
        _configPath = configPath;

        Text = "halcyon settings";
        Width = 900;
        Height = 840;
        MinimumSize = new Size(720, 560);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Back;
        ForeColor = TextColor;
        KeyPreview = true;
        MinimizeBox = false;

        var workingArea = Screen.PrimaryScreen?.WorkingArea ?? new Rectangle(0, 0, 1280, 800);
        Width = Math.Min(Width, Math.Max(MinimumSize.Width, workingArea.Width - 80));
        Height = Math.Min(Height, Math.Max(MinimumSize.Height, workingArea.Height - 80));

        BuildUi();
        LoadAll();

        _captureTimer.Tick += CaptureTick;
        FormClosing += (_, e) =>
        {
            if (_allowClose) return;
            e.Cancel = true;
            StopCapture();
            Hide();
        };
    }

    protected override void OnVisibleChanged(EventArgs e)
    {
        if (!Visible) StopCapture();
        base.OnVisibleChanged(e);
    }

    public void CloseForExit()
    {
        _allowClose = true;
        Close();
    }

    public void LoadAll()
    {
        foreach (var load in _loaders) load();
    }

    public void ApplyAll()
    {
        foreach (var apply in _appliers) apply();
        Applied?.Invoke();
    }

    private void BuildUi()
    {
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 2,
            BackColor = Back,
        };
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 48));

        var tabs = new TabControl
        {
            Dock = DockStyle.Fill,
            Font = new Font("Segoe UI", 9.5f),
        };
        root.Controls.Add(tabs, 0, 0);

        BuildGeneral(tabs);
        BuildAim(tabs);
        BuildTrigger(tabs);
        BuildEsp(tabs);
        BuildVisuals(tabs);
        BuildRadar(tabs);
        BuildMisc(tabs);
        BuildOffsets(tabs);
        BuildKeys(tabs);
        BuildLoop(tabs);

        var bottom = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false,
            BackColor = PanelBack,
            Padding = new Padding(8, 8, 8, 8),
        };

        bottom.Controls.Add(MakeButton("Apply", (_, _) =>
        {
            ApplyAll();
            SetStatus("applied");
        }));
        bottom.Controls.Add(MakeButton("Save", (_, _) =>
        {
            ApplyAll();
            SetStatus(_cfg.Save(_configPath) ? "saved to " + _configPath : "save failed");
        }));
        bottom.Controls.Add(MakeButton("Reload file", (_, _) =>
        {
            if (_cfg.Load(_configPath))
            {
                LoadAll();
                ApplyAll();
                SetStatus("reloaded " + _configPath);
            }
            else
            {
                SetStatus("cannot read " + _configPath);
            }
        }));
        bottom.Controls.Add(MakeButton("Exit client", (_, _) => ExitRequested?.Invoke()));

        _status.AutoSize = true;
        _status.ForeColor = Color.Gainsboro;
        _status.Margin = new Padding(16, 10, 6, 0);
        bottom.Controls.Add(_status);

        var close = MakeButton("Close", (_, _) => Hide());
        close.Margin = new Padding(6, 0, 0, 0);
        bottom.Controls.Add(close);

        root.Controls.Add(bottom, 0, 1);
        Controls.Add(root);
    }

    private static Button MakeButton(string text, EventHandler onClick)
    {
        var button = new Button
        {
            Text = text,
            AutoSize = true,
            Height = 28,
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(52, 52, 62),
            ForeColor = TextColor,
            Margin = new Padding(0, 0, 8, 0),
        };
        button.FlatAppearance.BorderColor = Accent;
        button.Click += onClick;
        return button;
    }

    private FlowLayoutPanel Tab(TabControl tabs, string title)
    {
        var page = new TabPage(title)
        {
            BackColor = Back,
            ForeColor = TextColor,
            Padding = new Padding(6),
        };

        var panel = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoScroll = true,
            BackColor = Back,
            Padding = new Padding(4),
        };

        page.Controls.Add(panel);
        tabs.TabPages.Add(page);
        return panel;
    }

    private static FlowLayoutPanel Row(FlowLayoutPanel parent, string label, string? tip = null)
    {
        var row = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false,
            AutoSize = true,
            Margin = new Padding(0, 1, 0, 1),
        };

        var text = new Label
        {
            Text = label,
            Width = 280,
            Height = 24,
            TextAlign = ContentAlignment.MiddleLeft,
            ForeColor = TextColor,
            Margin = new Padding(0, 2, 8, 0),
        };

        row.Controls.Add(text);
        parent.Controls.Add(row);
        return row;
    }

    private void AddNote(FlowLayoutPanel panel, string text)
    {
        var label = new Label
        {
            Text = text,
            AutoSize = true,
            MaximumSize = new Size(800, 0),
            ForeColor = Color.FromArgb(170, 170, 180),
            Margin = new Padding(2, 2, 2, 8),
        };
        panel.Controls.Add(label);
    }

    private void AddCheck(FlowLayoutPanel panel, string label, Func<bool> get, Action<bool> set,
        string? tip = null)
    {
        var row = Row(panel, label, tip);
        var check = new CheckBox
        {
            Text = "",
            Width = 24,
            Height = 24,
            ForeColor = TextColor,
        };
        row.Controls.Add(check);
        if (tip != null) _tip.SetToolTip(check, tip);
        _loaders.Add(() => check.Checked = get());
        _appliers.Add(() => set(check.Checked));
    }

    private void AddInt(FlowLayoutPanel panel, string label, int min, int max, Func<int> get,
        Action<int> set, string? tip = null)
    {
        var row = Row(panel, label, tip);
        var num = new NumericUpDown
        {
            Minimum = min,
            Maximum = max,
            Width = 130,
            BackColor = PanelBack,
            ForeColor = TextColor,
            BorderStyle = BorderStyle.FixedSingle,
        };
        row.Controls.Add(num);
        if (tip != null) _tip.SetToolTip(num, tip);
        _loaders.Add(() => num.Value = Math.Clamp(get(), min, max));
        _appliers.Add(() => set((int)num.Value));
    }

    private void AddFloat(FlowLayoutPanel panel, string label, decimal min, decimal max, decimal increment,
        Func<float> get, Action<float> set, string? tip = null)
    {
        var row = Row(panel, label, tip);
        var num = new NumericUpDown
        {
            Minimum = min,
            Maximum = max,
            DecimalPlaces = 3,
            Increment = increment,
            Width = 130,
            BackColor = PanelBack,
            ForeColor = TextColor,
            BorderStyle = BorderStyle.FixedSingle,
        };
        row.Controls.Add(num);
        if (tip != null) _tip.SetToolTip(num, tip);
        _loaders.Add(() =>
        {
            var value = (decimal)get();
            num.Value = Math.Clamp(value, min, max);
        });
        _appliers.Add(() => set((float)num.Value));
    }

    private void AddText(FlowLayoutPanel panel, string label, Func<string> get, Action<string> set)
    {
        var row = Row(panel, label);
        var box = new TextBox
        {
            Width = 360,
            BackColor = PanelBack,
            ForeColor = TextColor,
            BorderStyle = BorderStyle.FixedSingle,
        };
        row.Controls.Add(box);
        _loaders.Add(() => box.Text = get());
        _appliers.Add(() => set(box.Text));
    }

    private void AddOffset(FlowLayoutPanel panel, string label, Func<string> get, Action<string> set)
    {
        var row = Row(panel, label, "0x1F2A3B0 or sig:48 8B 0D ? ? ? ?");
        var box = new TextBox
        {
            Width = 360,
            BackColor = PanelBack,
            ForeColor = TextColor,
            BorderStyle = BorderStyle.FixedSingle,
        };
        row.Controls.Add(box);
        _tip.SetToolTip(box, "Module relative offset (0x...) or signature (sig:48 8B 0D ? ? ? ?)");
        _loaders.Add(() => box.Text = get());
        _appliers.Add(() => set(box.Text));
    }

    private void AddCombo(FlowLayoutPanel panel, string label, string[] items, Func<int> get, Action<int> set)
    {
        var row = Row(panel, label);
        var combo = new ComboBox
        {
            Width = 220,
            DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = PanelBack,
            ForeColor = TextColor,
            FlatStyle = FlatStyle.Flat,
        };
        combo.Items.AddRange(items);
        row.Controls.Add(combo);
        _loaders.Add(() => combo.SelectedIndex = Math.Clamp(get(), 0, items.Length - 1));
        _appliers.Add(() => set(combo.SelectedIndex));
    }

    private void AddStringCombo(FlowLayoutPanel panel, string label, string[] items, Func<string> get,
        Action<string> set)
    {
        var row = Row(panel, label);
        var combo = new ComboBox
        {
            Width = 220,
            DropDownStyle = ComboBoxStyle.DropDownList,
            BackColor = PanelBack,
            ForeColor = TextColor,
            FlatStyle = FlatStyle.Flat,
        };
        combo.Items.AddRange(items);
        row.Controls.Add(combo);
        _loaders.Add(() =>
        {
            var index = Array.FindIndex(items, item => string.Equals(item, get(), StringComparison.OrdinalIgnoreCase));
            combo.SelectedIndex = index >= 0 ? index : 0;
        });
        _appliers.Add(() => set(combo.SelectedItem?.ToString() ?? items[0]));
    }

    private void AddKey(FlowLayoutPanel panel, string label, Func<int> get, Action<int> set)
    {
        var row = Row(panel, label, "Click, then press the key or mouse button. Esc cancels.");
        var box = new KeyBox
        {
            Width = 150,
            Height = 24,
            FlatStyle = FlatStyle.Flat,
            BackColor = PanelBack,
            ForeColor = TextColor,
        };
        box.FlatAppearance.BorderColor = Accent;
        box.Click += (_, _) => BeginCapture(box);
        row.Controls.Add(box);

        _loaders.Add(() =>
        {
            box.Value = get();
            box.Text = Vk.ToName(box.Value);
        });
        _appliers.Add(() => set(box.Value));
    }

    private void AddColor(FlowLayoutPanel panel, string label, Func<uint> get, Action<uint> set)
    {
        var row = Row(panel, label);
        var swatch = new Button
        {
            Width = 72,
            Height = 24,
            FlatStyle = FlatStyle.Flat,
            Text = "",
        };
        swatch.FlatAppearance.BorderColor = Color.Gainsboro;

        var rgb = 0u;

        swatch.Click += (_, _) =>
        {
            using var dialog = new ColorDialog
            {
                FullOpen = true,
                Color = Color.FromArgb((int)(0xFF000000u | rgb)),
            };
            if (dialog.ShowDialog(this) != DialogResult.OK) return;
            rgb = (uint)(dialog.Color.ToArgb() & 0xFFFFFF);
            swatch.BackColor = Color.FromArgb((int)(0xFF000000u | rgb));
        };

        var alphaLabel = new Label
        {
            Text = "alpha",
            Width = 44,
            Height = 24,
            TextAlign = ContentAlignment.MiddleLeft,
            ForeColor = TextColor,
            Margin = new Padding(8, 2, 4, 0),
        };

        var alpha = new NumericUpDown
        {
            Minimum = 0,
            Maximum = 255,
            Width = 70,
            BackColor = PanelBack,
            ForeColor = TextColor,
            BorderStyle = BorderStyle.FixedSingle,
        };

        row.Controls.Add(swatch);
        row.Controls.Add(alphaLabel);
        row.Controls.Add(alpha);

        _loaders.Add(() =>
        {
            var value = get();
            rgb = value & 0xFFFFFF;
            swatch.BackColor = Color.FromArgb((int)(0xFF000000u | rgb));
            alpha.Value = (value >> 24) & 0xFF;
        });
        _appliers.Add(() => set((((uint)alpha.Value & 0xFF) << 24) | (rgb & 0xFFFFFF)));
    }

    private void BeginCapture(KeyBox box)
    {
        if (_captureBox != null) StopCapture();
        _captureBox = box;
        box.Text = "press a key...";
        _captureArmAt = Environment.TickCount64 + 250;
        _captureTimer.Start();
    }

    private void StopCapture()
    {
        _captureTimer.Stop();
        _captureBox = null;
    }

    private void CaptureTick(object? sender, EventArgs e)
    {
        if (_captureBox == null)
        {
            StopCapture();
            return;
        }

        if (Environment.TickCount64 < _captureArmAt) return;

        if ((Native.GetAsyncKeyState(Vk.Escape) & 0x8000) != 0)
        {
            var box = _captureBox;
            StopCapture();
            box.Text = Vk.ToName(box.Value);
            return;
        }

        for (var vk = 2; vk < 256; ++vk)
        {
            if ((Native.GetAsyncKeyState(vk) & 0x8000) == 0) continue;

            var box = _captureBox;
            StopCapture();
            box.Value = vk;
            box.Text = Vk.ToName(vk);
            return;
        }
    }

    private void SetStatus(string text)
    {
        _status.Text = text;
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (!Capturing && e.KeyCode == Keys.Escape)
        {
            Hide();
            e.Handled = true;
            return;
        }

        base.OnKeyDown(e);
    }

    private void BuildGeneral(TabControl tabs)
    {
        var panel = Tab(tabs, "General");
        AddNote(panel,
            "Presets overwrite the aim / trigger / ESP groups. The config file is the source of truth at startup; save after editing.");

        foreach (var preset in new[] { Preset.Legit, Preset.SemiLegit, Preset.SemiRage, Preset.Rage })
        {
            var row = Row(panel, "");
            var p = preset;
            row.Controls.Add(MakeButton("Apply " + Config.PresetName(p), (_, _) =>
            {
                _cfg.ApplyPreset(p);
                LoadAll();
                SetStatus("preset: " + Config.PresetName(p));
            }));
        }

        AddStringCombo(panel, "window layout", new[] { "source2", "source1" }, () => _cfg.Layout,
            value => _cfg.Layout = value);
        AddText(panel, "process", () => _cfg.Process, value => _cfg.Process = value);
        AddText(panel, "window title", () => _cfg.WindowTitle, value => _cfg.WindowTitle = value);
        AddText(panel, "window class", () => _cfg.WindowClass, value => _cfg.WindowClass = value);
        AddText(panel, "client module", () => _cfg.ClientModule, value => _cfg.ClientModule = value);
    }

    private void BuildAim(TabControl tabs)
    {
        var panel = Tab(tabs, "Aimbot");
        AddCheck(panel, "enabled", () => _cfg.Aim.Enabled, value => _cfg.Aim.Enabled = value);
        AddKey(panel, "key", () => _cfg.Aim.Key, value => _cfg.Aim.Key = value);
        AddCheck(panel, "toggle (press instead of hold)", () => _cfg.Aim.Toggle, value => _cfg.Aim.Toggle = value);

        AddCombo(panel, "hitbox", new[] { "head", "neck", "chest", "stomach" }, () => _cfg.Aim.Hitbox,
            value => _cfg.Aim.Hitbox = value);
        AddCheck(panel, "multi hitbox", () => _cfg.Aim.MultiHitbox, value => _cfg.Aim.MultiHitbox = value);
        AddCheck(panel, "hit head", () => _cfg.Aim.HitHead, value => _cfg.Aim.HitHead = value);
        AddCheck(panel, "hit neck", () => _cfg.Aim.HitNeck, value => _cfg.Aim.HitNeck = value);
        AddCheck(panel, "hit chest", () => _cfg.Aim.HitChest, value => _cfg.Aim.HitChest = value);
        AddCheck(panel, "hit stomach", () => _cfg.Aim.HitStomach, value => _cfg.Aim.HitStomach = value);
        AddCombo(panel, "priority", new[] { "crosshair", "distance", "fov", "health" }, () => _cfg.Aim.Priority,
            value => _cfg.Aim.Priority = value);
        AddCheck(panel, "target switch", () => _cfg.Aim.TargetSwitch, value => _cfg.Aim.TargetSwitch = value);
        AddFloat(panel, "switch delay (s)", 0, 2, 0.01m, () => _cfg.Aim.SwitchDelay,
            value => _cfg.Aim.SwitchDelay = value);
        AddFloat(panel, "max distance (0 = off)", 0, 10000, 50m, () => _cfg.Aim.MaxDistance,
            value => _cfg.Aim.MaxDistance = value);
        AddCheck(panel, "team check", () => _cfg.Aim.TeamCheck, value => _cfg.Aim.TeamCheck = value);
        AddCheck(panel, "visible only", () => _cfg.Aim.VisibleOnly, value => _cfg.Aim.VisibleOnly = value,
            "Needs m_bspotted in the offsets block.");
        AddCheck(panel, "ignore smoke", () => _cfg.Aim.IgnoreSmoke, value => _cfg.Aim.IgnoreSmoke = value,
            "Reserved: needs engine state the external client cannot read reliably.");
        AddCheck(panel, "ignore flashed", () => _cfg.Aim.IgnoreFlashed, value => _cfg.Aim.IgnoreFlashed = value);
        AddCheck(panel, "only scoped", () => _cfg.Aim.OnlyScoped, value => _cfg.Aim.OnlyScoped = value);

        AddFloat(panel, "fov (pixels)", 1, 2000, 5m, () => _cfg.Aim.Fov, value => _cfg.Aim.Fov = value);
        AddCheck(panel, "dynamic fov", () => _cfg.Aim.DynamicFov, value => _cfg.Aim.DynamicFov = value);
        AddFloat(panel, "fov min", 0, 2000, 5m, () => _cfg.Aim.FovMin, value => _cfg.Aim.FovMin = value);
        AddFloat(panel, "fov max", 0, 2000, 5m, () => _cfg.Aim.FovMax, value => _cfg.Aim.FovMax = value);
        AddFloat(panel, "fov scale per 1000u", 0, 10, 0.05m, () => _cfg.Aim.FovScale,
            value => _cfg.Aim.FovScale = value);
        AddFloat(panel, "smooth (1 = snap)", 1, 200, 0.5m, () => _cfg.Aim.Smooth, value => _cfg.Aim.Smooth = value);
        AddCombo(panel, "smooth mode", new[] { "linear", "exponential", "humanized" }, () => _cfg.Aim.SmoothMode,
            value => _cfg.Aim.SmoothMode = value);
        AddFloat(panel, "humanize", 0, 1, 0.01m, () => _cfg.Aim.Humanize, value => _cfg.Aim.Humanize = value);
        AddFloat(panel, "humanize jitter", 0, 2, 0.01m, () => _cfg.Aim.HumanizeJitter,
            value => _cfg.Aim.HumanizeJitter = value);
        AddFloat(panel, "aim step cap (px)", 0, 500, 1m, () => _cfg.Aim.AimStep, value => _cfg.Aim.AimStep = value);
        AddFloat(panel, "mouse scale", 0.05m, 10, 0.05m, () => _cfg.Aim.AimScale, value => _cfg.Aim.AimScale = value,
            "Mouse counts per screen pixel. Tune against your sensitivity.");
        AddFloat(panel, "reaction time (s)", 0, 2, 0.01m, () => _cfg.Aim.ReactionTime,
            value => _cfg.Aim.ReactionTime = value);
        AddFloat(panel, "reaction jitter (s)", 0, 1, 0.01m, () => _cfg.Aim.ReactionJitter,
            value => _cfg.Aim.ReactionJitter = value);
        AddFloat(panel, "first shot delay (s)", 0, 2, 0.01m, () => _cfg.Aim.FirstShotDelay,
            value => _cfg.Aim.FirstShotDelay = value);

        AddCheck(panel, "auto fire", () => _cfg.Aim.AutoFire, value => _cfg.Aim.AutoFire = value);
        AddCheck(panel, "auto scope", () => _cfg.Aim.AutoScope, value => _cfg.Aim.AutoScope = value);
        AddCheck(panel, "auto pistol", () => _cfg.Aim.AutoPistol, value => _cfg.Aim.AutoPistol = value);
        AddCheck(panel, "auto stop", () => _cfg.Aim.AutoStop, value => _cfg.Aim.AutoStop = value);
        AddCheck(panel, "auto crouch", () => _cfg.Aim.AutoCrouch, value => _cfg.Aim.AutoCrouch = value);

        AddCheck(panel, "recoil control", () => _cfg.Aim.Rcs, value => _cfg.Aim.Rcs = value);
        AddCheck(panel, "rcs standalone", () => _cfg.Aim.RcsStandalone, value => _cfg.Aim.RcsStandalone = value);
        AddFloat(panel, "rcs x", 0, 2, 0.01m, () => _cfg.Aim.RcsX, value => _cfg.Aim.RcsX = value);
        AddFloat(panel, "rcs y", 0, 2, 0.01m, () => _cfg.Aim.RcsY, value => _cfg.Aim.RcsY = value);
        AddFloat(panel, "rcs smooth", 1, 30, 0.5m, () => _cfg.Aim.RcsSmooth, value => _cfg.Aim.RcsSmooth = value);
        AddCheck(panel, "rcs scope only", () => _cfg.Aim.RcsScopeOnly, value => _cfg.Aim.RcsScopeOnly = value);

        AddCheck(panel, "prediction", () => _cfg.Aim.Prediction, value => _cfg.Aim.Prediction = value);
        AddFloat(panel, "predict ms", 0, 250, 1m, () => _cfg.Aim.PredictMs, value => _cfg.Aim.PredictMs = value);
        AddCheck(panel, "backtrack", () => _cfg.Aim.Backtrack, value => _cfg.Aim.Backtrack = value,
            "External approximation from the sampled history.");
        AddFloat(panel, "backtrack ms", 0, 600, 5m, () => _cfg.Aim.BacktrackMs, value => _cfg.Aim.BacktrackMs = value);
        AddCombo(panel, "resolver", new[] { "off", "static", "jitter" }, () => _cfg.Aim.ResolverMode,
            value => _cfg.Aim.ResolverMode = value);
        AddCheck(panel, "draw fov", () => _cfg.Aim.DrawFov, value => _cfg.Aim.DrawFov = value);
        AddColor(panel, "fov color", () => _cfg.Aim.FovColor, value => _cfg.Aim.FovColor = value);
        AddFloat(panel, "fov alpha", 0, 1, 0.05m, () => _cfg.Aim.FovAlpha, value => _cfg.Aim.FovAlpha = value);
        AddCheck(panel, "silent aim (reserved)", () => _cfg.Aim.Silent, value => _cfg.Aim.Silent = value,
            "External client cannot write usercmds; kept for config compatibility.");
    }

    private void BuildTrigger(TabControl tabs)
    {
        var panel = Tab(tabs, "Trigger");
        AddCheck(panel, "enabled", () => _cfg.Trigger.Enabled, value => _cfg.Trigger.Enabled = value);
        AddKey(panel, "key", () => _cfg.Trigger.Key, value => _cfg.Trigger.Key = value);
        AddCheck(panel, "toggle", () => _cfg.Trigger.Toggle, value => _cfg.Trigger.Toggle = value);
        AddFloat(panel, "radius (pixels)", 1, 200, 1m, () => _cfg.Trigger.Radius, value => _cfg.Trigger.Radius = value);
        AddFloat(panel, "max distance (0 = off)", 0, 10000, 50m, () => _cfg.Trigger.MaxDistance,
            value => _cfg.Trigger.MaxDistance = value);
        AddCheck(panel, "team check", () => _cfg.Trigger.TeamCheck, value => _cfg.Trigger.TeamCheck = value);
        AddCheck(panel, "visible only", () => _cfg.Trigger.VisibleOnly, value => _cfg.Trigger.VisibleOnly = value);
        AddCheck(panel, "wall check", () => _cfg.Trigger.WallCheck, value => _cfg.Trigger.WallCheck = value);
        AddCheck(panel, "only scoped", () => _cfg.Trigger.OnlyScoped, value => _cfg.Trigger.OnlyScoped = value);
        AddCheck(panel, "randomize delay", () => _cfg.Trigger.Randomize, value => _cfg.Trigger.Randomize = value);
        AddInt(panel, "delay min (ms)", 0, 1000, () => _cfg.Trigger.DelayMin, value => _cfg.Trigger.DelayMin = value);
        AddInt(panel, "delay max (ms)", 0, 1000, () => _cfg.Trigger.DelayMax, value => _cfg.Trigger.DelayMax = value);
        AddInt(panel, "hold min (ms)", 1, 1000, () => _cfg.Trigger.HoldMin, value => _cfg.Trigger.HoldMin = value);
        AddInt(panel, "hold max (ms)", 1, 1000, () => _cfg.Trigger.HoldMax, value => _cfg.Trigger.HoldMax = value);
        AddCheck(panel, "burst", () => _cfg.Trigger.Burst, value => _cfg.Trigger.Burst = value);
        AddInt(panel, "burst shots", 1, 30, () => _cfg.Trigger.BurstShots, value => _cfg.Trigger.BurstShots = value);
        AddFloat(panel, "burst delay (s)", 0, 2, 0.01m, () => _cfg.Trigger.BurstDelay,
            value => _cfg.Trigger.BurstDelay = value);
        AddFloat(panel, "burst cooldown (s)", 0, 5, 0.05m, () => _cfg.Trigger.BurstCooldown,
            value => _cfg.Trigger.BurstCooldown = value);
    }

    private void BuildEsp(TabControl tabs)
    {
        var panel = Tab(tabs, "ESP");
        AddCheck(panel, "enabled", () => _cfg.Esp.Enabled, value => _cfg.Esp.Enabled = value);
        AddCheck(panel, "team check", () => _cfg.Esp.TeamCheck, value => _cfg.Esp.TeamCheck = value);
        AddFloat(panel, "max distance", 100, 10000, 100m, () => _cfg.Esp.MaxDistance,
            value => _cfg.Esp.MaxDistance = value);
        AddCheck(panel, "visible check", () => _cfg.Esp.VisibleCheck, value => _cfg.Esp.VisibleCheck = value);
        AddCheck(panel, "on-screen only", () => _cfg.Esp.OofCheck, value => _cfg.Esp.OofCheck = value);
        AddFloat(panel, "fade start", 0, 10000, 100m, () => _cfg.Esp.FadeStart, value => _cfg.Esp.FadeStart = value);

        AddCheck(panel, "box", () => _cfg.Esp.Box, value => _cfg.Esp.Box = value);
        AddCombo(panel, "box style", new[] { "full", "corner", "3d" }, () => _cfg.Esp.BoxStyle,
            value => _cfg.Esp.BoxStyle = value);
        AddFloat(panel, "box thickness", 0.5m, 8, 0.1m, () => _cfg.Esp.BoxThickness,
            value => _cfg.Esp.BoxThickness = value);
        AddCheck(panel, "filled box", () => _cfg.Esp.FilledBox, value => _cfg.Esp.FilledBox = value);
        AddColor(panel, "fill color", () => _cfg.Esp.FillColor, value => _cfg.Esp.FillColor = value);

        AddCheck(panel, "health bar", () => _cfg.Esp.HealthBar, value => _cfg.Esp.HealthBar = value);
        AddCombo(panel, "health bar position", new[] { "left", "right", "top" }, () => _cfg.Esp.HealthBarPos,
            value => _cfg.Esp.HealthBarPos = value);
        AddCheck(panel, "health text", () => _cfg.Esp.HealthText, value => _cfg.Esp.HealthText = value);
        AddCheck(panel, "armor bar", () => _cfg.Esp.ArmorBar, value => _cfg.Esp.ArmorBar = value);
        AddCheck(panel, "name", () => _cfg.Esp.Name, value => _cfg.Esp.Name = value);
        AddCheck(panel, "weapon", () => _cfg.Esp.Weapon, value => _cfg.Esp.Weapon = value);
        AddCheck(panel, "distance", () => _cfg.Esp.Distance, value => _cfg.Esp.Distance = value);
        AddCheck(panel, "money", () => _cfg.Esp.Money, value => _cfg.Esp.Money = value);
        AddCheck(panel, "ammo", () => _cfg.Esp.Ammo, value => _cfg.Esp.Ammo = value);
        AddCheck(panel, "head dot", () => _cfg.Esp.HeadDot, value => _cfg.Esp.HeadDot = value);
        AddCheck(panel, "snapline", () => _cfg.Esp.Snapline, value => _cfg.Esp.Snapline = value);
        AddCombo(panel, "snapline origin", new[] { "bottom", "top", "center" }, () => _cfg.Esp.SnaplineOrigin,
            value => _cfg.Esp.SnaplineOrigin = value);
        AddCheck(panel, "skeleton", () => _cfg.Esp.Skeleton, value => _cfg.Esp.Skeleton = value);
        AddFloat(panel, "skeleton thickness", 0.5m, 6, 0.1m, () => _cfg.Esp.SkeletonThickness,
            value => _cfg.Esp.SkeletonThickness = value);
        AddCheck(panel, "flags", () => _cfg.Esp.Flags, value => _cfg.Esp.Flags = value);
        AddCheck(panel, "glow", () => _cfg.Esp.Glow, value => _cfg.Esp.Glow = value);
        AddCheck(panel, "offscreen arrows", () => _cfg.Esp.OffscreenArrows, value => _cfg.Esp.OffscreenArrows = value);
        AddCheck(panel, "sound esp (reserved)", () => _cfg.Esp.SoundEsp, value => _cfg.Esp.SoundEsp = value,
            "Reserved: needs engine audio state that is not exposed to an external reader.");
        AddFloat(panel, "sound radius", 0, 5000, 50m, () => _cfg.Esp.SoundRadius, value => _cfg.Esp.SoundRadius = value);
        AddCheck(panel, "bomb (reserved)", () => _cfg.Esp.Bomb, value => _cfg.Esp.Bomb = value);
        AddCheck(panel, "bomb timer (reserved)", () => _cfg.Esp.BombTimer, value => _cfg.Esp.BombTimer = value);
        AddCheck(panel, "dropped weapons (reserved)", () => _cfg.Esp.DroppedWeapons,
            value => _cfg.Esp.DroppedWeapons = value);
        AddCheck(panel, "grenades (reserved)", () => _cfg.Esp.Grenades, value => _cfg.Esp.Grenades = value);
        AddCheck(panel, "local player (reserved)", () => _cfg.Esp.LocalPlayer, value => _cfg.Esp.LocalPlayer = value);
        AddInt(panel, "text size (0 = auto)", 0, 40, () => _cfg.Esp.TextSize, value => _cfg.Esp.TextSize = value);
        AddCheck(panel, "outline text", () => _cfg.Esp.OutlineText, value => _cfg.Esp.OutlineText = value);

        AddColor(panel, "visible color", () => _cfg.Esp.ColorVisible, value => _cfg.Esp.ColorVisible = value);
        AddColor(panel, "occluded color", () => _cfg.Esp.ColorOccluded, value => _cfg.Esp.ColorOccluded = value);
        AddColor(panel, "snapline color", () => _cfg.Esp.ColorSnapline, value => _cfg.Esp.ColorSnapline = value);
        AddColor(panel, "text color", () => _cfg.Esp.ColorText, value => _cfg.Esp.ColorText = value);
        AddColor(panel, "health color", () => _cfg.Esp.ColorHealth, value => _cfg.Esp.ColorHealth = value);
        AddColor(panel, "armor color", () => _cfg.Esp.ColorArmor, value => _cfg.Esp.ColorArmor = value);
        AddColor(panel, "skeleton color", () => _cfg.Esp.ColorSkeleton, value => _cfg.Esp.ColorSkeleton = value);
        AddColor(panel, "head dot color", () => _cfg.Esp.ColorHeadDot, value => _cfg.Esp.ColorHeadDot = value);
        AddColor(panel, "glow color", () => _cfg.Esp.ColorGlow, value => _cfg.Esp.ColorGlow = value);
    }

    private void BuildVisuals(TabControl tabs)
    {
        var panel = Tab(tabs, "Visuals");
        AddCheck(panel, "fov changer (reserved)", () => _cfg.Visuals.FovChanger, value => _cfg.Visuals.FovChanger = value,
            "Reserved: requires writing the camera, which the external client cannot do.");
        AddFloat(panel, "game fov (for recoil math)", 10, 180, 1m, () => _cfg.Visuals.Fov,
            value => _cfg.Visuals.Fov = value);
        AddCheck(panel, "third person (reserved)", () => _cfg.Visuals.ThirdPerson,
            value => _cfg.Visuals.ThirdPerson = value);
        AddFloat(panel, "third person distance", 0, 500, 5m, () => _cfg.Visuals.ThirdPersonDist,
            value => _cfg.Visuals.ThirdPersonDist = value);
        AddCheck(panel, "no scope (reserved)", () => _cfg.Visuals.NoScope, value => _cfg.Visuals.NoScope = value);

        AddCheck(panel, "crosshair", () => _cfg.Visuals.Crosshair, value => _cfg.Visuals.Crosshair = value);
        AddCombo(panel, "crosshair style", new[] { "cross", "dot", "circle", "t" }, () => _cfg.Visuals.CrosshairStyle,
            value => _cfg.Visuals.CrosshairStyle = value);
        AddFloat(panel, "crosshair size", 0, 80, 1m, () => _cfg.Visuals.CrosshairSize,
            value => _cfg.Visuals.CrosshairSize = value);
        AddFloat(panel, "crosshair gap", 0, 80, 1m, () => _cfg.Visuals.CrosshairGap,
            value => _cfg.Visuals.CrosshairGap = value);
        AddFloat(panel, "crosshair thickness", 0.5m, 10, 0.1m, () => _cfg.Visuals.CrosshairThickness,
            value => _cfg.Visuals.CrosshairThickness = value);
        AddColor(panel, "crosshair color", () => _cfg.Visuals.CrosshairColor,
            value => _cfg.Visuals.CrosshairColor = value);

        AddCheck(panel, "hitmarker", () => _cfg.Visuals.Hitmarker, value => _cfg.Visuals.Hitmarker = value);
        AddFloat(panel, "hitmarker size", 1, 100, 1m, () => _cfg.Visuals.HitmarkerSize,
            value => _cfg.Visuals.HitmarkerSize = value);
        AddColor(panel, "hitmarker color", () => _cfg.Visuals.HitmarkerColor,
            value => _cfg.Visuals.HitmarkerColor = value);
        AddCheck(panel, "damage indicator", () => _cfg.Visuals.DamageIndicator,
            value => _cfg.Visuals.DamageIndicator = value);
        AddCheck(panel, "spectator list (reserved)", () => _cfg.Visuals.SpectatorList,
            value => _cfg.Visuals.SpectatorList = value);

        AddCheck(panel, "watermark", () => _cfg.Visuals.Watermark, value => _cfg.Visuals.Watermark = value);
        AddCheck(panel, "keybind list", () => _cfg.Visuals.KeybindList, value => _cfg.Visuals.KeybindList = value);
        AddCheck(panel, "fps counter", () => _cfg.Visuals.FpsCounter, value => _cfg.Visuals.FpsCounter = value);
        AddCheck(panel, "ping counter (reserved)", () => _cfg.Visuals.PingCounter,
            value => _cfg.Visuals.PingCounter = value);
        AddCheck(panel, "time counter", () => _cfg.Visuals.TimeCounter, value => _cfg.Visuals.TimeCounter = value);

        AddCheck(panel, "no flash (reserved)", () => _cfg.Visuals.NoFlash, value => _cfg.Visuals.NoFlash = value);
        AddFloat(panel, "flash alpha", 0, 1, 0.05m, () => _cfg.Visuals.FlashAlpha, value => _cfg.Visuals.FlashAlpha = value);
        AddCheck(panel, "no smoke (reserved)", () => _cfg.Visuals.NoSmoke, value => _cfg.Visuals.NoSmoke = value);
        AddCheck(panel, "no decals (reserved)", () => _cfg.Visuals.NoDecals, value => _cfg.Visuals.NoDecals = value);
        AddCheck(panel, "night mode", () => _cfg.Visuals.NightMode, value => _cfg.Visuals.NightMode = value);
        AddFloat(panel, "night alpha", 0, 1, 0.05m, () => _cfg.Visuals.NightAlpha, value => _cfg.Visuals.NightAlpha = value);
        AddCheck(panel, "remove hands (reserved)", () => _cfg.Visuals.RemoveHands,
            value => _cfg.Visuals.RemoveHands = value);

        AddColor(panel, "accent", () => _cfg.Visuals.Accent, value => _cfg.Visuals.Accent = value);
        AddColor(panel, "accent dim", () => _cfg.Visuals.AccentDim, value => _cfg.Visuals.AccentDim = value);
    }

    private void BuildRadar(TabControl tabs)
    {
        var panel = Tab(tabs, "Radar");
        AddCheck(panel, "enabled", () => _cfg.Radar.Enabled, value => _cfg.Radar.Enabled = value);
        AddFloat(panel, "size", 80, 800, 10m, () => _cfg.Radar.Size, value => _cfg.Radar.Size = value);
        AddFloat(panel, "zoom", 0.1m, 10, 0.1m, () => _cfg.Radar.Zoom, value => _cfg.Radar.Zoom = value);
        AddInt(panel, "position x", 0, 4000, () => _cfg.Radar.PosX, value => _cfg.Radar.PosX = value);
        AddInt(panel, "position y", 0, 4000, () => _cfg.Radar.PosY, value => _cfg.Radar.PosY = value);
        AddCheck(panel, "rotate with view", () => _cfg.Radar.Rotate, value => _cfg.Radar.Rotate = value);
        AddCheck(panel, "show enemies", () => _cfg.Radar.ShowEnemies, value => _cfg.Radar.ShowEnemies = value);
        AddCheck(panel, "show team", () => _cfg.Radar.ShowTeam, value => _cfg.Radar.ShowTeam = value);
        AddCheck(panel, "show bomb (reserved)", () => _cfg.Radar.ShowBomb, value => _cfg.Radar.ShowBomb = value);
        AddCheck(panel, "show dropped (reserved)", () => _cfg.Radar.ShowDropped, value => _cfg.Radar.ShowDropped = value);
        AddFloat(panel, "dot size", 1, 20, 0.5m, () => _cfg.Radar.DotSize, value => _cfg.Radar.DotSize = value);
        AddColor(panel, "enemy color", () => _cfg.Radar.ColorEnemy, value => _cfg.Radar.ColorEnemy = value);
        AddColor(panel, "team color", () => _cfg.Radar.ColorTeam, value => _cfg.Radar.ColorTeam = value);
        AddColor(panel, "bomb color", () => _cfg.Radar.ColorBomb, value => _cfg.Radar.ColorBomb = value);
    }

    private void BuildMisc(TabControl tabs)
    {
        var panel = Tab(tabs, "Misc");
        AddCheck(panel, "bunny hop", () => _cfg.Misc.Bhop, value => _cfg.Misc.Bhop = value);
        AddKey(panel, "bhop key", () => _cfg.Misc.BhopKey, value => _cfg.Misc.BhopKey = value);
        AddCheck(panel, "auto strafe", () => _cfg.Misc.AutoStrafe, value => _cfg.Misc.AutoStrafe = value);
        AddFloat(panel, "strafe strength", 0, 1, 0.05m, () => _cfg.Misc.StrafeStrength,
            value => _cfg.Misc.StrafeStrength = value);
        AddCheck(panel, "jump bug (reserved)", () => _cfg.Misc.JumpBug, value => _cfg.Misc.JumpBug = value);
        AddCheck(panel, "edge jump (reserved)", () => _cfg.Misc.EdgeJump, value => _cfg.Misc.EdgeJump = value);
        AddKey(panel, "edge jump key", () => _cfg.Misc.EdgeJumpKey, value => _cfg.Misc.EdgeJumpKey = value);

        AddCheck(panel, "auto pistol", () => _cfg.Misc.AutoPistol, value => _cfg.Misc.AutoPistol = value);
        AddCheck(panel, "auto reload", () => _cfg.Misc.AutoReload, value => _cfg.Misc.AutoReload = value);
        AddCheck(panel, "knife bot", () => _cfg.Misc.KnifeBot, value => _cfg.Misc.KnifeBot = value);
        AddCheck(panel, "auto accept (reserved)", () => _cfg.Misc.AutoAccept, value => _cfg.Misc.AutoAccept = value);
        AddCheck(panel, "auto defuse (reserved)", () => _cfg.Misc.AutoDefuse, value => _cfg.Misc.AutoDefuse = value);
        AddCheck(panel, "auto buy (reserved)", () => _cfg.Misc.AutoBuy, value => _cfg.Misc.AutoBuy = value);
        AddText(panel, "auto buy text", () => _cfg.Misc.AutoBuyText, value => _cfg.Misc.AutoBuyText = value);
        AddCheck(panel, "fake latency (reserved)", () => _cfg.Misc.FakeLatency, value => _cfg.Misc.FakeLatency = value);
        AddInt(panel, "fake latency ms", 0, 1000, () => _cfg.Misc.FakeLatencyMs, value => _cfg.Misc.FakeLatencyMs = value);

        AddCheck(panel, "chat spam", () => _cfg.Misc.ChatSpam, value => _cfg.Misc.ChatSpam = value);
        AddText(panel, "chat spam text", () => _cfg.Misc.ChatSpamText, value => _cfg.Misc.ChatSpamText = value);
        AddFloat(panel, "chat spam delay (s)", 0.5m, 60, 0.5m, () => _cfg.Misc.ChatSpamDelay,
            value => _cfg.Misc.ChatSpamDelay = value);
        AddCheck(panel, "kill say", () => _cfg.Misc.KillSay, value => _cfg.Misc.KillSay = value);
        AddText(panel, "kill say text", () => _cfg.Misc.KillSayText, value => _cfg.Misc.KillSayText = value);
        AddCheck(panel, "clan tag (reserved)", () => _cfg.Misc.ClanTag, value => _cfg.Misc.ClanTag = value);
        AddText(panel, "clan tag text", () => _cfg.Misc.ClanTagText, value => _cfg.Misc.ClanTagText = value);
        AddCheck(panel, "anti aim (reserved)", () => _cfg.Misc.AntiAim, value => _cfg.Misc.AntiAim = value);
        AddInt(panel, "anti aim mode", 0, 5, () => _cfg.Misc.AntiAimMode, value => _cfg.Misc.AntiAimMode = value);
        AddCheck(panel, "auto gg (reserved)", () => _cfg.Misc.AutoGG, value => _cfg.Misc.AutoGG = value);
        AddCheck(panel, "auto molotov (reserved)", () => _cfg.Misc.AutoMolotov, value => _cfg.Misc.AutoMolotov = value);
        AddCheck(panel, "preserve killfeed (reserved)", () => _cfg.Misc.PreserveKillfeed,
            value => _cfg.Misc.PreserveKillfeed = value);
    }

    private void BuildOffsets(TabControl tabs)
    {
        var panel = Tab(tabs, "Offsets");
        AddNote(panel,
            "Update these after every game patch. entity_list / local_player / view_matrix accept a module offset or a signature.");

        AddOffset(panel, "entity_list", () => _cfg.EntityList.Raw, value => _cfg.EntityList = Config.ParseOffsetSpec(value));
        AddOffset(panel, "local_player", () => _cfg.LocalPlayer.Raw, value => _cfg.LocalPlayer = Config.ParseOffsetSpec(value));
        AddOffset(panel, "view_matrix", () => _cfg.ViewMatrix.Raw, value => _cfg.ViewMatrix = Config.ParseOffsetSpec(value));

        AddInt(panel, "entity_stride", 0x10, 0x1000, () => _cfg.EntityStride, value => _cfg.EntityStride = value);
        AddInt(panel, "max_entities", 1, 1024, () => _cfg.MaxEntities, value => _cfg.MaxEntities = value);

        AddInt(panel, "m_ihealth", -1, int.MaxValue, () => _cfg.Health, value => _cfg.Health = value);
        AddInt(panel, "m_iteamnum", -1, int.MaxValue, () => _cfg.Team, value => _cfg.Team = value);
        AddInt(panel, "m_lifestate", -1, int.MaxValue, () => _cfg.LifeState, value => _cfg.LifeState = value);
        AddInt(panel, "m_vecorigin", -1, int.MaxValue, () => _cfg.Origin, value => _cfg.Origin = value);
        AddInt(panel, "m_vecoffset", -1, int.MaxValue, () => _cfg.EyeOffset, value => _cfg.EyeOffset = value);
        AddInt(panel, "m_bonematrix", -1, int.MaxValue, () => _cfg.BoneMatrix, value => _cfg.BoneMatrix = value);
        AddCheck(panel, "bone_indirect", () => _cfg.BoneIndirect, value => _cfg.BoneIndirect = value);
        AddInt(panel, "head_bone", 0, 255, () => _cfg.HeadBone, value => _cfg.HeadBone = value);
        AddInt(panel, "m_dormant", -1, int.MaxValue, () => _cfg.Dormant, value => _cfg.Dormant = value);
        AddInt(panel, "m_bspotted", -1, int.MaxValue, () => _cfg.Spotted, value => _cfg.Spotted = value);
        AddInt(panel, "m_szname", -1, int.MaxValue, () => _cfg.Name, value => _cfg.Name = value);
        AddInt(panel, "m_weapon", -1, int.MaxValue, () => _cfg.Weapon, value => _cfg.Weapon = value);
        AddInt(panel, "m_armor", -1, int.MaxValue, () => _cfg.Armor, value => _cfg.Armor = value);
        AddInt(panel, "m_scoped", -1, int.MaxValue, () => _cfg.Scoped, value => _cfg.Scoped = value);
        AddInt(panel, "m_flashed", -1, int.MaxValue, () => _cfg.Flashed, value => _cfg.Flashed = value);
        AddInt(panel, "m_defusing", -1, int.MaxValue, () => _cfg.Defusing, value => _cfg.Defusing = value);
        AddInt(panel, "m_money", -1, int.MaxValue, () => _cfg.Money, value => _cfg.Money = value);
        AddInt(panel, "m_ammo", -1, int.MaxValue, () => _cfg.Ammo, value => _cfg.Ammo = value);
        AddInt(panel, "m_velocity", -1, int.MaxValue, () => _cfg.Velocity, value => _cfg.Velocity = value);
        AddInt(panel, "m_aimpunch", -1, int.MaxValue, () => _cfg.AimPunch, value => _cfg.AimPunch = value);
        AddInt(panel, "m_viewangles", -1, int.MaxValue, () => _cfg.ViewAngles, value => _cfg.ViewAngles = value);
    }

    private void BuildKeys(TabControl tabs)
    {
        var panel = Tab(tabs, "Keys");
        AddNote(panel, "Global hotkeys. The aim and trigger keys live on their own tabs.");
        AddKey(panel, "menu", () => _cfg.MenuKey, value => _cfg.MenuKey = value);
        AddKey(panel, "quit", () => _cfg.QuitKey, value => _cfg.QuitKey = value);
        AddKey(panel, "esp toggle", () => _cfg.EspToggleKey, value => _cfg.EspToggleKey = value);
        AddKey(panel, "aim toggle", () => _cfg.AimToggleKey, value => _cfg.AimToggleKey = value);
        AddKey(panel, "trigger toggle", () => _cfg.TriggerToggleKey, value => _cfg.TriggerToggleKey = value);
        AddKey(panel, "panic", () => _cfg.PanicKey, value => _cfg.PanicKey = value);
    }

    private void BuildLoop(TabControl tabs)
    {
        var panel = Tab(tabs, "Loop");
        AddInt(panel, "render fps (0 = uncapped)", 0, 1000, () => _cfg.RenderFps, value => _cfg.RenderFps = value);
        AddInt(panel, "aim hz", 30, 2000, () => _cfg.AimHz, value => _cfg.AimHz = value);
    }

    private sealed class KeyBox : Button
    {
        public int Value;
    }
}
