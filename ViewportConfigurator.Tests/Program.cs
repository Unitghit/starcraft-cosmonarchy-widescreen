using CosmonarchyWidescreen;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;

Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);

var tests = new (string Name, Action Run)[]
{
    ("apply normalizes owned duplicates", ApplyNormalizesOwnedDuplicates),
    ("snapshot uses first active value", SnapshotUsesFirstActiveValue),
    ("restore returns untouched applied value", RestoreReturnsUntouchedAppliedValue),
    ("restore preserves a later user change", RestorePreservesLaterUserChange),
    ("restore removes a newly introduced key", RestoreRemovesNewlyIntroducedKey),
    ("other sections remain untouched", OtherSectionsRemainUntouched),
    ("known cnc-ddraw is supported", KnownCncDdrawIsSupported),
    ("unknown ddraw remains nonblocking", UnknownDdrawRemainsNonblocking),
    ("missing ddraw is reported", MissingDdrawIsReported),
    ("starting zoom choices follow aspect and internal size", StartingZoomChoices),
    ("starting zoom saves, parses, and clamps safely", StartingZoomSettings),
    ("pixel-perfect default preserves explicit Standard settings", PixelPerfectDefault),
    ("HUD sizes default, link, separate, and round trip", HudSizing),
    ("world edge filter defaults smooth and preserves explicit Sharp", WorldEdgeFilter),
    ("high-refresh pointer is optional and round trips", HighRefreshPointer),
    ("extra zoom range is optional, validated and round trips", ExtraZoomRange),
    ("settings scroll independently of the fixed action footer", SettingsLayout),
};

foreach (var test in tests)
{
    test.Run();
    Console.WriteLine($"PASS {test.Name}");
}

static void SettingsLayout()
{
    Exception? failure = null;
    var thread = new Thread(() =>
    {
        try
        {
            // An absent installation exercises the real form without touching game files.
            var path = Path.Combine(Path.GetTempPath(), "viewport-layout-" + Guid.NewGuid(), "Release");
            using var form = new MainForm(new ConfigurationService(new InstallationPaths(path)));
            form.ShowInTaskbar = false;
            form.Opacity = 0;
            form.Show();
            Application.DoEvents();
            var scroll = (Panel)form.Controls.Find("SettingsScroll", true).Single();
            var content = (TableLayoutPanel)form.Controls.Find("SettingsContent", true).Single();
            var footer = form.Controls.Find("ActionFooter", true).Single();
            var summary = form.Controls.Find("ResolutionSummary", true).Single();
            var warning = form.Controls.Find("ResolutionWarning", true).Single();
            True(summary.Parent == footer && warning.Parent == footer,
                "Resolution summary and warning must be outside scrolling settings");
            var before = form.BuildSettings();
            Equal(100, before.ExtraZoomPercent);
            var scale = form.DeviceDpi / 96f;
            foreach (var size in new[] { new Size(690, 420), new Size(690, 680), new Size(900, 760) })
            {
                form.ClientSize = new Size((int)(size.Width * scale), (int)(size.Height * scale));
                Application.DoEvents();
                var captureDirectory = Environment.GetEnvironmentVariable("VIEWPORT_LAYOUT_CAPTURE");
                if (!string.IsNullOrEmpty(captureDirectory))
                {
                    Directory.CreateDirectory(captureDirectory);
                    using var bitmap = new Bitmap(form.Width, form.Height);
                    form.DrawToBitmap(bitmap, new Rectangle(Point.Empty, form.Size));
                    bitmap.Save(Path.Combine(captureDirectory, $"layout-{size.Width}x{size.Height}.png"));
                }
                True(scroll.VerticalScroll.Visible, "Overflow must show a vertical scrollbar");
                True(!scroll.HorizontalScroll.Visible, "Settings must fit the available width");
                True(scroll.Bottom <= footer.Top, "Settings cannot overlap the footer");
                var footerBounds = footer.RectangleToScreen(footer.ClientRectangle);
                var summaryBounds = summary.RectangleToScreen(summary.ClientRectangle);
                True(footerBounds.Contains(summaryBounds), "Resolution summary must fit in footer");
                True(form.RectangleToScreen(form.ClientRectangle).Contains(footerBounds), "Footer must stay inside client");
                var buttons = Descendants(footer).OfType<Button>().ToArray();
                Equal(3, buttons.Length);
                foreach (var button in buttons)
                {
                    True(footerBounds.Contains(button.RectangleToScreen(button.ClientRectangle)), "Action button clipped");
                    True(summaryBounds.Bottom <= button.RectangleToScreen(button.ClientRectangle).Top,
                        "Resolution summary must remain above action buttons");
                }

                scroll.ScrollControlIntoView(content.Controls[content.Controls.Count - 1]);
                Application.DoEvents();
                Equal(footerBounds, footer.RectangleToScreen(footer.ClientRectangle));
                Equal(summaryBounds, summary.RectangleToScreen(summary.ClientRectangle));
                var last = content.Controls[content.Controls.Count - 1];
                True(scroll.RectangleToScreen(scroll.ClientRectangle).Contains(last.RectangleToScreen(last.ClientRectangle)),
                    "Last settings row must be reachable above footer");
                scroll.AutoScrollPosition = Point.Empty;
                foreach (Control child in content.Controls) child.Visible = false;
                Application.DoEvents();
                True(!scroll.VerticalScroll.Visible, "Scrollbar must disappear when content fits");
                Equal(footerBounds, footer.RectangleToScreen(footer.ClientRectangle));
                foreach (Control child in content.Controls) child.Visible = true;
                Application.DoEvents();
                warning.Text = "True 4K internal rendering can take several minutes to load. For faster 4K output, use 1920 x 1080 internal at 2x scale.";
                Application.DoEvents();
                True(warning.Visible, "Resolution warnings must remain visible in footer");
                True(scroll.Bottom <= footer.Top, "Visible warning must not overlap settings");
                var expandedFooter = footer.RectangleToScreen(footer.ClientRectangle);
                True(form.RectangleToScreen(form.ClientRectangle).Contains(expandedFooter), "Warning must not push footer offscreen");
                True(expandedFooter.Contains(warning.RectangleToScreen(warning.ClientRectangle)), "Warning must not be clipped");
                foreach (var button in buttons)
                    True(warning.RectangleToScreen(warning.ClientRectangle).Bottom <=
                        button.RectangleToScreen(button.ClientRectangle).Top, "Warning overlaps action buttons");
                warning.Text = "";
                Application.DoEvents();
                True(!warning.Visible, "Empty warning must not reserve footer space");
            }
            Equal(before, form.BuildSettings());
            VerifyDropdownWheel(form, scroll, footer);
            True(!Directory.Exists(path), "Layout test must not write an installation");
            form.Close();
        }
        catch (Exception error) { failure = error; }
    });
    thread.SetApartmentState(ApartmentState.STA);
    thread.Start();
    thread.Join();
    if (failure is not null) throw new InvalidOperationException("GUI layout regression", failure);
}

static void VerifyDropdownWheel(MainForm form, Panel scroll, Control footer)
{
    const int mouseWheel = 0x020A;
    var before = form.BuildSettings();
    var footerBounds = footer.Bounds;
    foreach (var combo in Descendants(scroll).OfType<ComboBox>())
    {
        True(combo is PageScrollComboBox, "Every dropdown must use page-only wheel behavior");
        var enabled = combo.Enabled;
        combo.Enabled = true;
        combo.Select();
        var selected = combo.SelectedIndex;
        var changed = 0;
        EventHandler onChange = (_, _) => ++changed;
        combo.SelectedIndexChanged += onChange;
        foreach (var open in new[] { false, true })
        {
            // Route real WM_MOUSEWHEEL through the production ComboBox WndProc.
            scroll.AutoScrollPosition = Point.Empty;
            combo.DroppedDown = open;
            var point = combo.PointToScreen(Point.Empty);
            var coordinates = (IntPtr)((point.X & 0xffff) | (point.Y << 16));
            SendWheel(combo.Handle, mouseWheel, (IntPtr)(-120 << 16), coordinates);
            Application.DoEvents();
            Equal(selected, combo.SelectedIndex);
            Equal(0, changed);
            True(scroll.AutoScrollPosition.Y < 0, "Wheel over dropdown must scroll the page down");
            True(!combo.DroppedDown, "Wheel closes an open list before scrolling page");
            SendWheel(combo.Handle, mouseWheel, (IntPtr)(120 << 16), coordinates);
            Application.DoEvents();
            Equal(selected, combo.SelectedIndex);
            Equal(0, changed);
            Equal(Point.Empty, scroll.AutoScrollPosition);
            Equal(footerBounds, footer.Bounds);
        }
        combo.SelectedIndexChanged -= onChange;
        combo.Enabled = enabled;
    }
    Equal(before, form.BuildSettings());
    Console.WriteLine("PASS all dropdowns reject wheel selection and scroll page, closed and open");
}

[DllImport("user32.dll", EntryPoint = "SendMessageW")]
static extern IntPtr SendWheel(IntPtr window, int message, IntPtr wParam, IntPtr lParam);

static IEnumerable<Control> Descendants(Control parent)
{
    foreach (Control child in parent.Controls)
    {
        yield return child;
        foreach (var descendant in Descendants(child)) yield return descendant;
    }
}

static void HudSizing()
{
    var empty = new Dictionary<string, string>();
    Equal(0, HudSizeOptions.ReadReference(empty, "hud_reference_height"));
    True(!HudSizeOptions.ReadSeparate(empty), "Top size should be linked by default");
    foreach (var invalid in new[] { "bad", "-1", "479", "2161", "99999999999" })
        Equal(0, HudSizeOptions.ReadReference(new Dictionary<string,string> { ["hud_reference_height"] = invalid }, "hud_reference_height"));
    var profile = new RendererProfile("test", "test", 1920, 1080, "16:9", "test");
    foreach (var separate in new[] { false, true })
    foreach (var reference in new[] { 0, 480, 720, 769, 1080, 2160 })
    {
        var settings = new ViewportSettings(profile, PresentationMode.Scale, 1m, 1, 1,
            1920, 1080, WindowMode.Windowed, ScalingFilter.NearestNeighbor, true,
            new DisplayChoice(0, System.Windows.Forms.Screen.PrimaryScreen!),
            HudReferenceHeight: reference, SeparateTopTextSize: separate, TopTextReferenceHeight: 1440);
        var config = ConfigurationService.BuildViewportConfig(settings);
        Contains(config, $"hud_reference_height={reference}");
        Contains(config, $"separate_top_text={(separate ? 1 : 0)}");
        Contains(config, "top_reference_height=1440");
        Contains(config, "internal_height=1080");
        Equal(reference, HudSizeOptions.ReadReference(new Dictionary<string,string>
            { ["hud_reference_height"] = reference.ToString() }, "hud_reference_height"));
    }
}

static void ExtraZoomRange()
{
    Equal(100, WorldZoomOptions.ReadExtraZoomPercent(new Dictionary<string, string>()));
    var profile = new RendererProfile("test", "test", 1920, 1080, "16:9", "test");
    var settings = new ViewportSettings(profile, PresentationMode.Scale, 1m, 1, 1,
        1920, 1080, WindowMode.Windowed, ScalingFilter.NearestNeighbor, true,
        new DisplayChoice(0, Screen.PrimaryScreen!), StartingZoomUnits: 15000);
    Equal(100, settings.ExtraZoomPercent);
    Contains(ConfigurationService.BuildViewportConfig(settings), "extra_zoom_percent=100");
    Contains(WorldZoomOptions.ConfigLines(true, 15000, 1920), "extra_zoom_percent=100");
    foreach (var value in new[] { 0, 50, 100, -1, 25, 200, int.MaxValue })
    {
        var expected = value is 50 or 100 ? value : 0;
        Equal(expected, WorldZoomOptions.ReadExtraZoomPercent(new Dictionary<string, string>
            { ["extra_zoom_percent"] = value.ToString() }));
        var config = ConfigurationService.BuildViewportConfig(settings with { ExtraZoomPercent = value });
        Contains(config, $"extra_zoom_percent={expected}");
        Contains(config, "start_zoom_units=15000");
        Contains(config, "internal_width=1920");
    }
    Equal(0, WorldZoomOptions.ReadExtraZoomPercent(new Dictionary<string, string>
        { ["extra_zoom_percent"] = "9999999999999999999" }));
}

static void StartingZoomChoices()
{
    var wide = WorldZoomOptions.Choices(1600, 900, 10000);
    Contains(wide[0].DisplayName, "1280 x 720");
    Equal(12500, wide[0].Units);
    Contains(wide[1].DisplayName, "640 x 360");
    Equal(25000, wide[1].Units);
    Contains(WorldZoomOptions.Choices(1280, 960, 10000)[1].DisplayName, "640 x 480");
    Equal(60000, WorldZoomOptions.Choices(3840, 2160, 10000)[1].Units);
    foreach (var (width, height) in new[] { (640,480), (800,600), (1365,769), (3840,2160) })
    {
        var choices = WorldZoomOptions.Choices(width, height, 999999);
        Equal(choices.Count, choices.Select(x => x.Units).Distinct().Count());
        True(choices.All(x => x.Units >= 10000 && x.Units <= WorldZoomOptions.MaximumStartUnits(width)), "Zoom out of range");
    }
}

static void StartingZoomSettings()
{
    Equal(10000, WorldZoomOptions.ReadStartUnits(new Dictionary<string,string>(), 1600));
    Equal(25000, WorldZoomOptions.ReadStartUnits(new Dictionary<string,string> { ["start_zoom_units"] = "999999" }, 1600));
    Equal(10000, WorldZoomOptions.ReadStartUnits(new Dictionary<string,string> { ["start_zoom_units"] = "bad" }, 1600));
    var profile = new RendererProfile("test", "test", 1600, 900, "16:9", "test");
    var settings = new ViewportSettings(profile, PresentationMode.Scale, 1m, 1, 1,
        1600, 900, WindowMode.Windowed, ScalingFilter.NearestNeighbor, true,
        new DisplayChoice(0, System.Windows.Forms.Screen.PrimaryScreen!),
        WorldZoomEnabled: true, SmoothWorldZoom: false, StartingZoomUnits: 25000);
    var config = ConfigurationService.BuildViewportConfig(settings);
    Contains(config, "transition=instant");
    Contains(config, "start_zoom_units=25000");
    Contains(config, "internal_width=1600");
    Contains(config, "output_width=1600");
    Contains(config, "backend=single_stage");
    Contains(ConfigurationService.BuildViewportConfig(settings with { SingleStagePresentation = false }), "backend=cnc_ddraw");
    True(!config.Contains("steps="), "Removed zoom-step option must not be serialized");
    True(!ConfigurationService.BuildViewportConfig(settings with { SingleStagePresentation = true }).Contains("steps="),
        "Single-stage zoom steps are automatic, not an additional setting");
    Contains(ConfigurationService.BuildViewportConfig(settings with { SingleStagePresentation = true }), "backend=single_stage");
    Contains(ConfigurationService.BuildViewportConfig(settings with { SmoothWorldZoom = true }), "transition=smooth");
}

static void PixelPerfectDefault()
{
    True(WorldZoomOptions.ReadPixelPerfectRendering(new Dictionary<string,string>()), "Missing backend defaults to Pixel-perfect");
    True(WorldZoomOptions.ReadPixelPerfectRendering(new Dictionary<string,string> { ["backend"]="single_stage" }), "Pixel-perfect retained");
    True(WorldZoomOptions.ReadPixelPerfectRendering(new Dictionary<string,string> { ["backend"]="SINGLE_STAGE" }), "Case-insensitive renderer loading");
    True(!WorldZoomOptions.ReadPixelPerfectRendering(new Dictionary<string,string> { ["backend"]="cnc_ddraw" }), "Explicit Standard retained");
    True(!WorldZoomOptions.ReadPixelPerfectRendering(new Dictionary<string,string> { ["backend"]="unknown" }), "Unknown explicit renderer falls back to Standard");
}

static void WorldEdgeFilter()
{
    foreach (var value in new[] { "", "unknown", "nearest" })
        True(!WorldZoomOptions.ReadSmoothWorldEdges(new Dictionary<string,string> { ["world_filter"] = value }), "Unknown filter must stay sharp");
    True(WorldZoomOptions.ReadSmoothWorldEdges(new Dictionary<string,string>()), "Missing filter defaults to smooth edges");
    True(WorldZoomOptions.ReadSmoothWorldEdges(new Dictionary<string,string> { ["world_filter"] = "SHARP_EDGES" }), "Filter loading is case insensitive");
    var profile = new RendererProfile("test", "test", 1920, 1080, "16:9", "test");
    var settings = new ViewportSettings(profile, PresentationMode.Scale, 2m, 2, 1,
        3840,2160,WindowMode.Windowed,ScalingFilter.NearestNeighbor,true,
        new DisplayChoice(0,System.Windows.Forms.Screen.PrimaryScreen!));
    Contains(ConfigurationService.BuildViewportConfig(settings), "world_filter=sharp_edges");
    Contains(ConfigurationService.BuildViewportConfig(settings with { SmoothWorldEdges=false }), "world_filter=nearest");
    var config = ConfigurationService.BuildViewportConfig(settings with { SmoothWorldEdges=true });
    Contains(config, "world_filter=sharp_edges");
    Contains(config, "backend=single_stage");
    Contains(config, "filter=nearest");
}

static void HighRefreshPointer()
{
    True(!WorldZoomOptions.ReadHighRefreshPointer(new Dictionary<string,string>()),"Pointer layer defaults off");
    True(!WorldZoomOptions.ReadHighRefreshPointer(new Dictionary<string,string>{["high_refresh_pointer"]="invalid"}),"Unknown pointer option stays off");
    True(WorldZoomOptions.ReadHighRefreshPointer(new Dictionary<string,string>{["high_refresh_pointer"]="1"}),"Pointer setting loads");
    var profile=new RendererProfile("test","test",1920,1080,"16:9","test");
    var settings=new ViewportSettings(profile,PresentationMode.Scale,2m,2,1,3840,2160,WindowMode.Windowed,
        ScalingFilter.NearestNeighbor,true,new DisplayChoice(0,System.Windows.Forms.Screen.PrimaryScreen!));
    Contains(ConfigurationService.BuildViewportConfig(settings),"high_refresh_pointer=0");
    Contains(ConfigurationService.BuildViewportConfig(settings with { HighRefreshPointer=true }),"high_refresh_pointer=1");
    var original="[ddraw]\nmaxfps=120\nminfps=5\n";
    var state=State("minfps",true,"5","-1");
    var applied=IniDocument.UpdateSection(original,"ddraw",new Dictionary<string,string>{["minfps"]="-1"});
    Contains(IniDocument.RestoreOwnedSection(applied,"ddraw",state),"minfps=5");
}

static void ApplyNormalizesOwnedDuplicates()
{
    const string source = "[ddraw]\r\nwidth=640\r\n; width=comment\r\ncustom=keep\r\nWIDTH=800\r\nheight=480\r\n";
    var output = IniDocument.UpdateSection(source, "ddraw",
        new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["width"] = "1280",
            ["height"] = "720",
        });
    Equal(1, ActiveKeyCount(output, "width"));
    Contains(output, "width=1280");
    Contains(output, "; width=comment");
    Contains(output, "custom=keep");
    Contains(output, "height=720");
    True(!output.Replace("\r\n", string.Empty).Contains('\n'),
        "CRLF newline style changed.");
}

static void SnapshotUsesFirstActiveValue()
{
    const string source = "[ddraw]\nwidth=640\nwidth=800\n";
    var snapshot = IniDocument.SnapshotSection(source, "ddraw", new[] { "width" });
    Equal("640", snapshot["width"].Value);
}

static void RestoreReturnsUntouchedAppliedValue()
{
    const string source = "[ddraw]\nwidth=1280\nwidth=800\ncustom=keep\n";
    var output = IniDocument.RestoreOwnedSection(source, "ddraw",
        State("width", true, "640", "1280"));
    Contains(output, "width=640");
    Equal(1, ActiveKeyCount(output, "width"));
    Contains(output, "custom=keep");
}

static void RestorePreservesLaterUserChange()
{
    const string source = "[ddraw]\nwidth=1920\nwidth=800\ncustom=keep\n";
    var output = IniDocument.RestoreOwnedSection(source, "ddraw",
        State("width", true, "640", "1280"));
    Contains(output, "width=1920");
    Equal(1, ActiveKeyCount(output, "width"));
    Contains(output, "custom=keep");
}

static void RestoreRemovesNewlyIntroducedKey()
{
    const string source = "[ddraw]\nadjmouse=true\ncustom=keep\n";
    var output = IniDocument.RestoreOwnedSection(source, "ddraw",
        State("adjmouse", false, null, "true"));
    Equal(0, ActiveKeyCount(output, "adjmouse"));
    Contains(output, "custom=keep");
}

static void OtherSectionsRemainUntouched()
{
    const string source = "[ddraw]\nwidth=640\n[game]\nwidth=999\ncustom=keep\n";
    var output = IniDocument.UpdateSection(source, "ddraw",
        new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["width"] = "1280",
        });
    Contains(output, "[ddraw]\nwidth=1280");
    Contains(output, "[game]\nwidth=999\ncustom=keep");
}

static void KnownCncDdrawIsSupported()
{
    var result = DdrawCompatibilityDetector.Classify(
        "abcdef", new[] { new DdrawCompatibilityProfile("7.1", "ABCDEF") });
    True(result.Detected, "Known cnc-ddraw was not detected.");
    True(result.Tested, "Known cnc-ddraw was not marked as tested.");
    Contains(result.Message, "7.1");
}

static void UnknownDdrawRemainsNonblocking()
{
    var result = DdrawCompatibilityDetector.Classify(
        "123456", new[] { new DdrawCompatibilityProfile("7.1", "ABCDEF") });
    True(result.Detected, "Unknown ddraw.dll was not detected.");
    True(!result.Tested, "Unknown ddraw.dll was marked as tested.");
    Contains(result.Message, "unverified");
}

static void MissingDdrawIsReported()
{
    var result = DdrawCompatibilityDetector.Classify(
        null, Array.Empty<DdrawCompatibilityProfile>());
    True(!result.Detected, "Missing ddraw.dll was reported as detected.");
    True(!result.Tested, "Missing ddraw.dll was reported as tested.");
    Contains(result.Message, "No local ddraw.dll");
}

static Dictionary<string, DdrawOwnedSettingState> State(
    string key,
    bool originalPresent,
    string? originalValue,
    string appliedValue) =>
    new(StringComparer.OrdinalIgnoreCase)
    {
        [key] = new DdrawOwnedSettingState
        {
            OriginalPresent = originalPresent,
            OriginalValue = originalValue,
            AppliedValue = appliedValue,
        },
    };

static int ActiveKeyCount(string source, string key) =>
    source.Replace("\r\n", "\n", StringComparison.Ordinal)
        .Split('\n')
        .Count(line =>
        {
            var trimmed = line.TrimStart();
            if (trimmed.StartsWith(';') || trimmed.StartsWith('#'))
                return false;
            var equals = line.IndexOf('=');
            return equals > 0 && line[..equals].Trim().Equals(
                key, StringComparison.OrdinalIgnoreCase);
        });

static void Contains(string actual, string expected)
{
    if (!actual.Contains(expected, StringComparison.Ordinal))
        throw new InvalidOperationException($"Expected output to contain: {expected}");
}

static void Equal<T>(T expected, T actual)
{
    if (!EqualityComparer<T>.Default.Equals(expected, actual))
        throw new InvalidOperationException($"Expected {expected}, got {actual}.");
}

static void True(bool condition, string message)
{
    if (!condition)
        throw new InvalidOperationException(message);
}
