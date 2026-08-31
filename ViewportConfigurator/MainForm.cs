using System.Diagnostics;
using System.Globalization;

namespace CosmonarchyWidescreen;

internal sealed class MainForm : Form
{
    private readonly ConfigurationService service;
    private readonly ComboBox aspectBox = NewComboBox();
    private readonly ComboBox profileBox = NewComboBox();
    private readonly ComboBox presentationBox = NewComboBox();
    private readonly ComboBox scaleBox = NewComboBox();
    private readonly ComboBox displayBox = NewComboBox();
    private readonly ComboBox windowModeBox = NewComboBox();
    private readonly ComboBox filterBox = NewComboBox();
    private readonly NumericUpDown outputWidthBox = NewNumberBox(320, 16384);
    private readonly NumericUpDown outputHeightBox = NewNumberBox(240, 16384);
    private readonly NumericUpDown customWidthBox = NewNumberBox(640, 3840);
    private readonly NumericUpDown customHeightBox = NewNumberBox(480, 2160);
    private readonly CheckBox preserveAspectBox = new()
    {
        Text = "Preserve aspect ratio",
        AutoSize = true,
        Checked = true,
        Anchor = AnchorStyles.Left,
    };
    private readonly Label installationLabel = new()
    {
        AutoSize = true,
        Dock = DockStyle.Top,
    };
    private readonly Label summaryLabel = new()
    {
        AutoSize = false,
        Height = 52,
        Dock = DockStyle.Fill,
        TextAlign = ContentAlignment.MiddleLeft,
        Font = new Font(SystemFonts.MessageBoxFont!.FontFamily, 10,
            FontStyle.Bold),
    };
    private readonly Label warningLabel = new()
    {
        AutoSize = false,
        Height = 38,
        Dock = DockStyle.Fill,
        ForeColor = Color.DarkGoldenrod,
    };
    private readonly Label statusLabel = new()
    {
        AutoSize = false,
        Height = 26,
        Dock = DockStyle.Fill,
        TextAlign = ContentAlignment.MiddleLeft,
    };
    private bool loading;

    public MainForm(ConfigurationService service)
    {
        this.service = service;
        Text = "Cosmonarchy Widescreen & Resolution Settings";
        StartPosition = FormStartPosition.CenterScreen;
        AutoScaleDimensions = new SizeF(96F, 96F);
        AutoScaleMode = AutoScaleMode.Dpi;
        var dpiScale = DeviceDpi / 96f;
        MinimumSize = new Size((int)Math.Ceiling(690 * dpiScale),
            (int)Math.Ceiling(680 * dpiScale));
        ClientSize = new Size((int)Math.Ceiling(690 * dpiScale),
            (int)Math.Ceiling(680 * dpiScale));
        Font = SystemFonts.MessageBoxFont;

        BuildInterface();
        PopulateOptions();
        LoadSavedSettings();
        RefreshInstallationStatus();
        UpdateCalculatedOutput();
    }

    private void BuildInterface()
    {
        var outer = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(18),
            ColumnCount = 1,
            RowCount = 8,
            AutoScroll = true,
        };
        outer.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        outer.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        outer.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        outer.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        outer.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        outer.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        outer.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        outer.RowStyles.Add(new RowStyle(SizeType.AutoSize));

        var title = new Label
        {
            Text = "Cosmonarchy Widescreen Settings",
            AutoSize = true,
            Font = new Font(Font.FontFamily, 16, FontStyle.Bold),
            Margin = new Padding(0, 0, 0, 4),
        };
        var subtitle = new Label
        {
            Text = "Internal viewport and external presentation are independent.",
            AutoSize = true,
            ForeColor = SystemColors.GrayText,
            Margin = new Padding(0, 0, 0, 14),
        };
        outer.Controls.Add(title);
        outer.Controls.Add(subtitle);
        outer.Controls.Add(BuildInstallationGroup());
        outer.Controls.Add(BuildInternalGroup());
        outer.Controls.Add(BuildPresentationGroup());

        var summaryPanel = new Panel { Dock = DockStyle.Fill, Height = 96 };
        summaryPanel.Controls.Add(warningLabel);
        summaryPanel.Controls.Add(summaryLabel);
        warningLabel.Dock = DockStyle.Bottom;
        summaryLabel.Dock = DockStyle.Top;
        outer.Controls.Add(summaryPanel);
        outer.Controls.Add(new Panel { Dock = DockStyle.Fill });
        outer.Controls.Add(BuildButtons());
        Controls.Add(outer);
    }

    private GroupBox BuildInstallationGroup()
    {
        var group = new GroupBox
        {
            Text = "Installation",
            Dock = DockStyle.Top,
            AutoSize = true,
            Padding = new Padding(12),
            Margin = new Padding(0, 0, 0, 10),
        };
        installationLabel.Text = service.Paths.ReleaseDirectory;
        group.Controls.Add(installationLabel);
        return group;
    }

    private GroupBox BuildInternalGroup()
    {
        var group = NewGroup("Internal viewport resolution");
        var grid = NewGrid();
        grid.Controls.Add(NewLabel("Aspect ratio:"), 0, 0);
        grid.Controls.Add(aspectBox, 1, 0);
        grid.Controls.Add(NewLabel("Renderer profile:"), 0, 1);
        grid.Controls.Add(profileBox, 1, 1);
        var customDimensions = new FlowLayoutPanel
        {
            AutoSize = true,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false,
            Dock = DockStyle.Fill,
        };
        customWidthBox.Value = 1280;
        customHeightBox.Value = 720;
        customDimensions.Controls.Add(customWidthBox);
        customDimensions.Controls.Add(new Label
        {
            Text = " x ", AutoSize = true, Padding = new Padding(0, 7, 0, 0),
        });
        customDimensions.Controls.Add(customHeightBox);
        grid.Controls.Add(NewLabel("Custom internal:"), 0, 2);
        grid.Controls.Add(customDimensions, 1, 2);
        var note = new Label
        {
            Text = "Changes world coverage and HUD geometry. 16:9 presets are " +
                "supported; 4:3 presets remain experimental. Custom dimensions " +
                "may use any value from 640 x 480 through 3840 x 2160.",
            AutoSize = true,
            ForeColor = SystemColors.GrayText,
            MaximumSize = new Size(430, 0),
        };
        grid.Controls.Add(note, 1, 3);
        group.Controls.Add(grid);
        return group;
    }

    private GroupBox BuildPresentationGroup()
    {
        var group = NewGroup("External presentation and display");
        var grid = NewGrid();
        grid.Controls.Add(NewLabel("Display:"), 0, 0);
        grid.Controls.Add(displayBox, 1, 0);
        grid.Controls.Add(NewLabel("Display mode:"), 0, 1);
        grid.Controls.Add(windowModeBox, 1, 1);
        grid.Controls.Add(NewLabel("Output mode:"), 0, 2);
        grid.Controls.Add(presentationBox, 1, 2);
        grid.Controls.Add(NewLabel("Presentation scale:"), 0, 3);
        grid.Controls.Add(scaleBox, 1, 3);

        var dimensions = new FlowLayoutPanel
        {
            AutoSize = true,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false,
            Dock = DockStyle.Fill,
        };
        dimensions.Controls.Add(outputWidthBox);
        dimensions.Controls.Add(new Label
        {
            Text = " x ", AutoSize = true, Padding = new Padding(0, 7, 0, 0),
        });
        dimensions.Controls.Add(outputHeightBox);
        grid.Controls.Add(NewLabel("External output:"), 0, 4);
        grid.Controls.Add(dimensions, 1, 4);
        grid.Controls.Add(NewLabel("Scaling filter:"), 0, 5);
        grid.Controls.Add(filterBox, 1, 5);
        grid.Controls.Add(preserveAspectBox, 1, 6);
        group.Controls.Add(grid);
        return group;
    }

    private Control BuildButtons()
    {
        var panel = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            AutoSize = true,
            ColumnCount = 2,
            RowCount = 2,
        };
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        panel.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        panel.Controls.Add(statusLabel, 0, 0);
        panel.SetColumnSpan(statusLabel, 2);

        var restore = new Button
        {
            Text = "Restore Original",
            AutoSize = true,
            MinimumSize = new Size(125, 34),
        };
        restore.Click += (_, _) => RestoreOriginal();
        panel.Controls.Add(restore, 0, 1);

        var actions = new FlowLayoutPanel
        {
            AutoSize = true,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false,
        };
        var save = new Button
        {
            Text = "Save",
            AutoSize = true,
            MinimumSize = new Size(90, 34),
        };
        save.Click += (_, _) => Save(false);
        var saveAndPlay = new Button
        {
            Text = "Save && Play",
            AutoSize = true,
            MinimumSize = new Size(125, 34),
        };
        saveAndPlay.Click += (_, _) => Save(true);
        actions.Controls.Add(save);
        actions.Controls.Add(saveAndPlay);
        panel.Controls.Add(actions, 1, 1);
        return panel;
    }

    private void PopulateOptions()
    {
        loading = true;
        aspectBox.Items.AddRange(new object[]
        {
            "All aspect ratios",
            "16:9 widescreen",
            "4:3 standard",
            "Custom resolution",
        });
        aspectBox.SelectedIndex = 1;
        PopulateProfileOptions(null);

        presentationBox.Items.AddRange(new object[]
        {
            "Scale multiplier",
            "Fit selected display",
            "Exact output size",
        });
        presentationBox.SelectedIndex = 0;

        scaleBox.Items.AddRange(new object[]
        {
            new ScaleChoice(1m, "1x"),
            new ScaleChoice(1.25m, "1.25x"),
            new ScaleChoice(1.5m, "1.5x"),
            new ScaleChoice(1.75m, "1.75x"),
            new ScaleChoice(2m, "2x"),
            new ScaleChoice(2.5m, "2.5x"),
            new ScaleChoice(3m, "3x"),
            new ScaleChoice(4m, "4x"),
        });
        scaleBox.SelectedIndex = 5;

        var screens = Screen.AllScreens;
        for (var index = 0; index < screens.Length; ++index)
            displayBox.Items.Add(new DisplayChoice(index, screens[index]));
        displayBox.SelectedIndex = Math.Max(0,
            Array.FindIndex(screens, screen => screen.Primary));

        windowModeBox.Items.AddRange(new object[]
        {
            "Windowed",
            "Borderless fullscreen (Recommended)",
            "Exclusive fullscreen",
        });
        windowModeBox.SelectedIndex = 0;

        filterBox.Items.AddRange(new object[]
        {
            "Nearest-neighbor",
            "Smooth",
        });
        filterBox.SelectedIndex = 0;

        aspectBox.SelectedIndexChanged += (_, _) =>
        {
            if (loading)
                return;
            var preferred = (profileBox.SelectedItem as RendererProfile)?.Id;
            PopulateProfileOptions(preferred);
            UpdateCalculatedOutput();
        };
        foreach (var box in new[]
                 { profileBox, presentationBox, scaleBox, displayBox,
                   windowModeBox, filterBox })
            box.SelectedIndexChanged += (_, _) => UpdateCalculatedOutput();
        customWidthBox.ValueChanged += (_, _) => UpdateCalculatedOutput();
        customHeightBox.ValueChanged += (_, _) => UpdateCalculatedOutput();
        outputWidthBox.ValueChanged += (_, _) => UpdateCalculatedOutput();
        outputHeightBox.ValueChanged += (_, _) => UpdateCalculatedOutput();
        preserveAspectBox.CheckedChanged += (_, _) => UpdateCalculatedOutput();
        loading = false;
    }

    private void LoadSavedSettings()
    {
        var viewport = service.ReadSavedViewport();
        var presentation = service.ReadSavedPresentation();
        if (viewport.Count == 0 || presentation.Count == 0)
            return;

        loading = true;
        var savedWidth = 1280;
        var savedHeight = 720;
        if (viewport.TryGetValue("internal_width", out var savedWidthText))
            int.TryParse(savedWidthText, out savedWidth);
        if (viewport.TryGetValue("internal_height", out var savedHeightText))
            int.TryParse(savedHeightText, out savedHeight);
        customWidthBox.Value = Math.Clamp(savedWidth,
            (int)customWidthBox.Minimum, (int)customWidthBox.Maximum);
        customHeightBox.Value = Math.Clamp(savedHeight,
            (int)customHeightBox.Minimum, (int)customHeightBox.Maximum);
        if (viewport.TryGetValue("renderer_profile", out var profileId))
        {
            var savedProfile = service.Profiles.FirstOrDefault(profile =>
                profile.Id.Equals(profileId, StringComparison.OrdinalIgnoreCase));
            if (savedProfile is null &&
                savedWidth >= 640 && savedHeight >= 480)
                savedProfile = service.Profiles.FirstOrDefault(profile =>
                    profile.Width == savedWidth && profile.Height == savedHeight) ??
                    service.Profiles.FirstOrDefault(profile =>
                        profile.AspectRatio == "Custom");
            if (savedProfile is not null)
            {
                aspectBox.SelectedIndex = savedProfile.AspectRatio switch
                {
                    "4:3" => 2,
                    "Custom" => 3,
                    _ => 1,
                };
                PopulateProfileOptions(savedProfile.Id);
            }
        }
        if (presentation.TryGetValue("mode", out var mode))
        {
            presentationBox.SelectedIndex = mode switch
            {
                "fit_display" or "fullscreen" => 1,
                "exact_output" => 2,
                _ => 0,
            };
        }
        if (presentation.TryGetValue("window_mode", out var windowMode))
        {
            windowModeBox.SelectedIndex = windowMode switch
            {
                "borderless_fullscreen" => 1,
                "exclusive_fullscreen" => 2,
                _ => 0,
            };
        }
        if (presentation.TryGetValue("display_device", out var displayDevice))
        {
            var index = displayBox.Items.Cast<DisplayChoice>().ToList()
                .FindIndex(display => display.DeviceName.Equals(displayDevice,
                    StringComparison.OrdinalIgnoreCase));
            if (index >= 0)
                displayBox.SelectedIndex = index;
        }
        if (presentation.TryGetValue("scale_numerator", out var numeratorText) &&
            presentation.TryGetValue("scale_denominator", out var denominatorText) &&
            decimal.TryParse(numeratorText, out var numerator) &&
            decimal.TryParse(denominatorText, out var denominator) && denominator != 0)
        {
            var scale = numerator / denominator;
            var index = scaleBox.Items.Cast<ScaleChoice>().ToList()
                .FindIndex(choice => choice.Value == scale);
            if (index >= 0)
                scaleBox.SelectedIndex = index;
        }
        SetNumericFromSection(presentation, "output_width", outputWidthBox);
        SetNumericFromSection(presentation, "output_height", outputHeightBox);
        if (presentation.TryGetValue("filter", out var filter))
            filterBox.SelectedIndex = filter == "smooth" ? 1 : 0;
        if (presentation.TryGetValue("preserve_aspect_ratio", out var preserve) &&
            bool.TryParse(preserve, out var parsedPreserve))
            preserveAspectBox.Checked = parsedPreserve;
        loading = false;
    }

    private void PopulateProfileOptions(string? preferredProfileId)
    {
        var previousLoading = loading;
        loading = true;
        var aspect = aspectBox.SelectedIndex switch
        {
            1 => "16:9",
            2 => "4:3",
            3 => "Custom",
            _ => null,
        };
        var profiles = service.Profiles.Where(profile => aspect is null ||
            profile.AspectRatio.Equals(aspect,
                StringComparison.OrdinalIgnoreCase)).ToArray();
        profileBox.BeginUpdate();
        try
        {
            profileBox.Items.Clear();
            profileBox.Items.AddRange(profiles.Cast<object>().ToArray());
            var selectedIndex = Array.FindIndex(profiles, profile =>
                profile.Id.Equals(preferredProfileId,
                    StringComparison.OrdinalIgnoreCase));
            profileBox.SelectedIndex = selectedIndex >= 0 ? selectedIndex : 0;
        }
        finally
        {
            profileBox.EndUpdate();
            loading = previousLoading;
        }
    }

    private void RefreshInstallationStatus()
    {
        var validation = service.ValidateInstallation();
        installationLabel.Text = $"{service.Paths.ReleaseDirectory}{Environment.NewLine}" +
            validation.Message;
        installationLabel.ForeColor = validation.Success ? Color.DarkGreen : Color.DarkRed;
    }

    private void UpdateCalculatedOutput()
    {
        if (loading || profileBox.SelectedItem is not RendererProfile ||
            displayBox.SelectedItem is not DisplayChoice display)
            return;

        var profile = GetSelectedProfile();
        var custom = profile.AspectRatio == "Custom";
        customWidthBox.Enabled = custom;
        customHeightBox.Enabled = custom;

        var windowMode = GetWindowMode();
        var presentationMode = GetPresentationMode();
        var fullscreen = windowMode != WindowMode.Windowed;
        presentationBox.Enabled = !fullscreen;
        scaleBox.Enabled = !fullscreen && presentationMode == PresentationMode.Scale;
        outputWidthBox.Enabled = !fullscreen &&
            presentationMode == PresentationMode.ExactOutput;
        outputHeightBox.Enabled = outputWidthBox.Enabled;

        var output = CalculateOutput(profile, display, windowMode, presentationMode);
        if (!outputWidthBox.Enabled)
        {
            loading = true;
            outputWidthBox.Value = Math.Clamp(output.Width,
                (int)outputWidthBox.Minimum, (int)outputWidthBox.Maximum);
            outputHeightBox.Value = Math.Clamp(output.Height,
                (int)outputHeightBox.Minimum, (int)outputHeightBox.Maximum);
            loading = false;
        }

        var effectiveScaleX = output.Width / (decimal)profile.Width;
        var effectiveScaleY = output.Height / (decimal)profile.Height;
        summaryLabel.Text =
            $"{profile.Width} x {profile.Height} internal  →  " +
            $"{output.Width} x {output.Height} external  " +
            $"({effectiveScaleX:0.##}x × {effectiveScaleY:0.##}x)";

        warningLabel.Text = string.Empty;
        if (windowMode == WindowMode.Windowed &&
            (output.Width > display.WorkingArea.Width ||
             output.Height > display.WorkingArea.Height))
        {
            warningLabel.Text =
                "The selected window is larger than this display's usable area.";
        }
        else if (preserveAspectBox.Checked && effectiveScaleX != effectiveScaleY)
        {
            warningLabel.Text =
                "This display has a different aspect ratio; fullscreen may use borders.";
        }
        else if (profile.Width >= 3840 || profile.Height >= 2160)
        {
            warningLabel.Text =
                "True 4K internal rendering can take several minutes to load. For faster 4K output, use 1920 x 1080 internal at 2x scale.";
        }
        else if (profile.Width > 1920 || profile.Height > 1080)
        {
            warningLabel.Text =
                "High internal resolutions render substantially more world pixels and may load or run more slowly.";
        }
    }

    private ViewportSettings BuildSettings()
    {
        var profile = GetSelectedProfile();
        var display = (DisplayChoice)displayBox.SelectedItem!;
        var presentationMode = GetPresentationMode();
        var windowMode = GetWindowMode();
        var output = CalculateOutput(profile, display, windowMode, presentationMode);
        var scale = scaleBox.SelectedItem is ScaleChoice choice ? choice.Value : 1m;
        var (numerator, denominator) = ToFraction(
            output.Width / (decimal)profile.Width);
        return new(profile, presentationMode, scale, numerator, denominator,
            output.Width, output.Height, windowMode,
            filterBox.SelectedIndex == 1 ? ScalingFilter.Smooth :
                ScalingFilter.NearestNeighbor,
            preserveAspectBox.Checked, display);
    }

    private RendererProfile GetSelectedProfile()
    {
        var profile = (RendererProfile)profileBox.SelectedItem!;
        return profile.AspectRatio == "Custom" ? profile with
        {
            Width = (int)customWidthBox.Value,
            Height = (int)customHeightBox.Value,
        } : profile;
    }

    private Size CalculateOutput(RendererProfile profile, DisplayChoice display,
        WindowMode windowMode, PresentationMode presentationMode)
    {
        if (windowMode != WindowMode.Windowed)
            return display.Bounds.Size;
        if (presentationMode == PresentationMode.FitDisplay)
        {
            var area = display.WorkingArea;
            if (!preserveAspectBox.Checked)
                return area.Size;
            var scale = Math.Min(area.Width / (decimal)profile.Width,
                area.Height / (decimal)profile.Height);
            return new Size(Math.Max(320, (int)Math.Floor(profile.Width * scale)),
                Math.Max(240, (int)Math.Floor(profile.Height * scale)));
        }
        if (presentationMode == PresentationMode.ExactOutput)
            return new Size((int)outputWidthBox.Value, (int)outputHeightBox.Value);
        var selectedScale = scaleBox.SelectedItem is ScaleChoice choice ?
            choice.Value : 1m;
        return new Size((int)(profile.Width * selectedScale),
            (int)(profile.Height * selectedScale));
    }

    private void Save(bool launch)
    {
        try
        {
            if (!EnsureGameClosed())
                return;
            var settings = BuildSettings();
            service.Apply(settings);
            statusLabel.ForeColor = Color.DarkGreen;
            statusLabel.Text = "Settings saved and verified.";
            RefreshInstallationStatus();
            if (launch)
                service.LaunchGame();
        }
        catch (Exception exception)
        {
            ShowFailure("Could not save settings", exception);
        }
    }

    private void RestoreOriginal()
    {
        try
        {
            if (!EnsureGameClosed())
                return;
            if (MessageBox.Show(this,
                    "Restore the original aidebug plugin and ddraw settings?",
                    "Restore original files", MessageBoxButtons.YesNo,
                    MessageBoxIcon.Question) != DialogResult.Yes)
                return;
            service.Restore();
            statusLabel.ForeColor = Color.DarkGreen;
            statusLabel.Text = "Original files restored.";
            RefreshInstallationStatus();
        }
        catch (Exception exception)
        {
            ShowFailure("Could not restore original files", exception);
        }
    }

    private bool EnsureGameClosed()
    {
        if (!service.IsGameRunning())
            return true;
        if (MessageBox.Show(this,
                "Cosmonarchy must close before settings can be changed. Close it now?",
                "Cosmonarchy is running", MessageBoxButtons.YesNo,
                MessageBoxIcon.Question) != DialogResult.Yes)
            return false;
        service.RequestGameShutdown(TimeSpan.FromSeconds(15));
        return true;
    }

    private void ShowFailure(string title, Exception exception)
    {
        statusLabel.ForeColor = Color.DarkRed;
        statusLabel.Text = exception.Message;
        File.AppendAllText(service.Paths.ConfiguratorLogPath,
            $"{DateTimeOffset.Now:O} {title}: {exception}{Environment.NewLine}");
        MessageBox.Show(this, exception.Message, title, MessageBoxButtons.OK,
            MessageBoxIcon.Error);
    }

    private PresentationMode GetPresentationMode() => presentationBox.SelectedIndex switch
    {
        1 => PresentationMode.FitDisplay,
        2 => PresentationMode.ExactOutput,
        _ => PresentationMode.Scale,
    };

    private WindowMode GetWindowMode() => windowModeBox.SelectedIndex switch
    {
        1 => WindowMode.BorderlessFullscreen,
        2 => WindowMode.ExclusiveFullscreen,
        _ => WindowMode.Windowed,
    };

    private static (int Numerator, int Denominator) ToFraction(decimal value)
    {
        const int precision = 10000;
        var numerator = (int)Math.Round(value * precision,
            MidpointRounding.AwayFromZero);
        var denominator = precision;
        var divisor = GreatestCommonDivisor(Math.Abs(numerator), denominator);
        return (numerator / divisor, denominator / divisor);
    }

    private static int GreatestCommonDivisor(int left, int right)
    {
        while (right != 0)
            (left, right) = (right, left % right);
        return Math.Max(1, left);
    }

    private static void SetNumericFromSection(
        IReadOnlyDictionary<string, string> values, string key,
        NumericUpDown control)
    {
        if (values.TryGetValue(key, out var text) && decimal.TryParse(text,
                NumberStyles.Integer, CultureInfo.InvariantCulture, out var value))
            control.Value = Math.Clamp(value, control.Minimum, control.Maximum);
    }

    private static ComboBox NewComboBox() => new()
    {
        DropDownStyle = ComboBoxStyle.DropDownList,
        Dock = DockStyle.Fill,
        MinimumSize = new Size(260, 0),
    };

    private static NumericUpDown NewNumberBox(int minimum, int maximum) => new()
    {
        Minimum = minimum,
        Maximum = maximum,
        Width = 110,
        ThousandsSeparator = true,
    };

    private static GroupBox NewGroup(string text) => new()
    {
        Text = text,
        Dock = DockStyle.Top,
        AutoSize = true,
        Padding = new Padding(12),
        Margin = new Padding(0, 0, 0, 10),
    };

    private static TableLayoutPanel NewGrid()
    {
        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            ColumnCount = 2,
        };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        return grid;
    }

    private static Label NewLabel(string text) => new()
    {
        Text = text,
        AutoSize = true,
        Anchor = AnchorStyles.Left,
        Padding = new Padding(0, 6, 0, 0),
    };
}
