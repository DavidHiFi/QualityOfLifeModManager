using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

namespace QolSeriesInstaller;

/// <summary>
/// One app window per Plutonium folder. The first instance owns a named mutex and waits on a named
/// event; a second one finds the mutex taken, asks what to do, and - if the answer is to use the
/// window that already exists - sets that event and exits. Minimising to the tray hides the window
/// but leaves the process running, which is exactly the case that used to end up with two copies.
///
/// The names carry a hash of the folder, so a <c>--root</c> test run and the real install are
/// separate instances and never fight each other.
/// </summary>
internal static class SingleInstance
{
    private static Mutex? held;
    private static EventWaitHandle? wake;
    private static RegisteredWaitHandle? registration;

    /// <summary>True in the process that owns the instance for this folder.</summary>
    internal static bool Owns { get; private set; }

    internal static bool Acquire(string root)
    {
        var mutex = new Mutex(true, $"Local\\QolModManager.Instance.{Key(root)}", out var first);
        if (first) held = mutex; else mutex.Dispose();
        Owns = first;
        return first;
    }

    /// <summary>
    /// Wake the instance that holds the mutex. False if it went away between the mutex check and
    /// here, in which case the caller should just carry on and become the instance itself.
    /// </summary>
    internal static bool SignalExisting(string root)
    {
        if (!EventWaitHandle.TryOpenExisting($"Local\\QolModManager.Activate.{Key(root)}", out var handle)) return false;
        using (handle)
        {
            // Windows only lets the foreground process hand the foreground over. Without this the
            // other window comes back behind whatever is on top of it.
            AllowSetForegroundWindow(ASFW_ANY);
            handle.Set();
        }
        return true;
    }

    /// <summary>
    /// Owner only: run <paramref name="activate"/> on the UI thread whenever another copy is
    /// started and the user asks for this window back. A second copy must not listen, or the two
    /// would race for the same auto-reset event.
    /// </summary>
    internal static void ListenForActivation(string root, Control owner, Action activate)
    {
        if (!Owns) return;
        wake = new EventWaitHandle(false, EventResetMode.AutoReset, $"Local\\QolModManager.Activate.{Key(root)}");
        registration = ThreadPool.RegisterWaitForSingleObject(wake, (_, _) =>
        {
            try { if (owner.IsHandleCreated && !owner.IsDisposed) owner.BeginInvoke(activate); }
            catch (ObjectDisposedException) { }
            catch (InvalidOperationException) { }
        }, null, Timeout.Infinite, false);
    }

    private static string Key(string root) =>
        Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(root.TrimEnd('\\', '/').ToLowerInvariant())))[..16];

    private const int ASFW_ANY = -1;
    [DllImport("user32.dll")] private static extern bool AllowSetForegroundWindow(int processId);
}

/// <summary>
/// What a second copy shows: the app is already running, here is the window, or start another one
/// anyway. Built from the app's own palette and controls so it does not look like a stray
/// Windows message box.
/// </summary>
internal sealed class AlreadyRunningDialog : Form
{
    internal enum Choice { UseExisting, StartAnother }

    private readonly Palette p = Palettes.ByKey(AppSettings.Load().Theme);

    internal Choice Result { get; private set; } = Choice.UseExisting;

    internal AlreadyRunningDialog()
    {
        // A title of its own: the running window is called Setup.AppName, and two windows with one
        // name is confusing on the taskbar and impossible to tell apart from a test script.
        Text = $"{Setup.AppName} - Already open";
        FormBorderStyle = FormBorderStyle.FixedDialog; MaximizeBox = false; MinimizeBox = false; ShowInTaskbar = true;
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = p.Base; ForeColor = p.Ink; Font = Ui.F(10); DoubleBuffered = true;

        var iconPath = Path.Combine(Payload.Dir, "qol_installer.ico");
        Image? artwork = null;
        if (File.Exists(iconPath)) { Icon = new Icon(iconPath); artwork = new Icon(iconPath, new Size(64, 64)).ToBitmap(); }

        int pad = D(26), width = D(520);

        var mark = new LogoMark { Bounds = new Rectangle(pad, D(26), D(44), D(44)), Radius = 12, BackColor = p.Card, BorderColor = p.Card, Glyph = "", GlyphColor = p.Accent, Artwork = artwork };
        var title = new Label { Bounds = new Rectangle(pad + D(58), D(28), width - pad * 2 - D(58), D(24)), AutoSize = false, TextAlign = ContentAlignment.MiddleLeft, Text = "Already open", Font = Ui.F(14, true), ForeColor = p.Ink, BackColor = p.Base, UseMnemonic = false };
        var subtitle = new Label { Bounds = new Rectangle(pad + D(58), D(52), width - pad * 2 - D(58), D(20)), AutoSize = false, TextAlign = ContentAlignment.MiddleLeft, Text = Setup.AppName, Font = Ui.F(9), ForeColor = p.Muted, BackColor = p.Base, UseMnemonic = false };
        Controls.Add(mark); Controls.Add(title); Controls.Add(subtitle);

        var body = new Label
        {
            Bounds = new Rectangle(pad, D(92), width - pad * 2, D(60)),
            AutoSize = false,
            Text = "This app is already open. If you cannot see its window it is minimised to the notification area, next to the clock.",
            Font = Ui.F(10), ForeColor = p.Sub, BackColor = p.Base, UseMnemonic = false
        };
        var caution = new Label
        {
            Bounds = new Rectangle(pad, D(150), width - pad * 2, D(36)),
            AutoSize = false,
            Text = "A second copy works, but both write the same settings and mod folders, so they can undo each other's changes.",
            Font = Ui.F(8.5f), ForeColor = p.Muted, BackColor = p.Base, UseMnemonic = false
        };
        Controls.Add(body); Controls.Add(caution);

        int btnY = D(196), btnH = D(40), gap = D(12);
        var primaryWidth = (width - pad * 2 - gap) * 55 / 100;
        var primary = new RoundButton
        {
            Bounds = new Rectangle(pad, btnY, primaryWidth, btnH), Radius = btnH / 2, Text = "Show the open window",
            BackColor = p.Accent, HoverColor = p.AccentHover, ForeColor = p.AccentText, Font = Ui.F(9.5f, true), Cursor = Cursors.Hand, TextAlign = ContentAlignment.MiddleCenter
        };
        var secondary = new RoundButton
        {
            Bounds = new Rectangle(pad + primaryWidth + gap, btnY, width - pad * 2 - primaryWidth - gap, btnH), Radius = btnH / 2, Text = "Open another anyway",
            BackColor = p.Secondary, HoverColor = p.SecondaryHover, ForeColor = p.Ink, Font = Ui.F(9.5f, true), Cursor = Cursors.Hand, TextAlign = ContentAlignment.MiddleCenter
        };
        primary.Click += (_, _) => { Result = Choice.UseExisting; Close(); };
        secondary.Click += (_, _) => { Result = Choice.StartAnother; Close(); };
        Controls.Add(primary); Controls.Add(secondary);

        // Enter and Escape both mean "the one I already have" - that is what someone is after when
        // they click the shortcut a second time.
        AcceptButton = primary; CancelButton = primary;
        ActiveControl = primary;

        ClientSize = new Size(width, btnY + btnH + D(22));
        var dark = p.Dark ? 1 : 0;
        HandleCreated += (_, _) => { if (DwmSetWindowAttribute(Handle, 20, ref dark, 4) != 0) DwmSetWindowAttribute(Handle, 19, ref dark, 4); };
    }

    private int D(int v) => Ui.Dp(this, v);

    [DllImport("dwmapi.dll")] private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);
}
