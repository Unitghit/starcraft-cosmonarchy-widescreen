using System.Diagnostics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace CosmonarchyWidescreen;

internal sealed class ConfigurationService
{
    private const string ManifestResource =
        "CosmonarchyWidescreen.Payload.compatibility-manifest.json";

    private readonly Dictionary<string, byte[]> rendererPayloads =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, string> rendererPayloadHashes =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly CompatibilityManifest manifest;

    public ConfigurationService(InstallationPaths paths)
    {
        Paths = paths;
        manifest = JsonSerializer.Deserialize<CompatibilityManifest>(
            ReadResourceText(ManifestResource),
            new JsonSerializerOptions { PropertyNameCaseInsensitive = true }) ??
            throw new InvalidDataException("The embedded compatibility manifest is invalid.");
        if (manifest.SchemaVersion != 1 || manifest.RendererProfiles.Count == 0)
            throw new InvalidDataException("Unsupported compatibility manifest schema.");
        Profiles = manifest.RendererProfiles;
        foreach (var profile in Profiles)
        {
            if (!rendererPayloads.TryAdd(profile.Id,
                    ReadResource(profile.PayloadResource)))
                throw new InvalidDataException(
                    $"Duplicate renderer profile id: {profile.Id}");
            rendererPayloadHashes.Add(profile.Id,
                Hash(rendererPayloads[profile.Id]));
        }
    }

    public InstallationPaths Paths { get; }
    public IReadOnlyList<RendererProfile> Profiles { get; }

    public ValidationResult ValidateInstallation()
    {
        var required = new[]
        {
            Paths.LauncherPath,
            Paths.StarCraftPath,
            Paths.DdrawPath,
            Paths.HotloaderPath,
            Paths.GptpPath,
            Paths.RendererPath,
        };
        var missing = required.Where(path => !File.Exists(path)).ToArray();
        if (missing.Length != 0)
        {
            return new(false,
                "Place this program beside Cosmonarchy BW.exe. Missing: " +
                string.Join(", ", missing.Select(Path.GetFileName)));
        }

        if (!HashFile(Paths.StarCraftPath).Equals(manifest.StarCraftSha256,
                StringComparison.OrdinalIgnoreCase))
            return new(false, "Unsupported StarCraft.exe build. No files were changed.");
        if (!HashFile(Paths.GptpPath).Equals(manifest.GptpSha256,
                StringComparison.OrdinalIgnoreCase))
            return new(false, "Unsupported GPTP build. No files were changed.");
        if (!HashFile(Paths.HotloaderPath).Equals(manifest.HotloaderSha256,
                StringComparison.OrdinalIgnoreCase))
            return new(false, "Unsupported QDP hotloader build. No files were changed.");

        var installedRendererHash = HashFile(Paths.RendererPath);
        var backupIsValid = File.Exists(Paths.RendererBackupPath) &&
            HashFile(Paths.RendererBackupPath).Equals(manifest.OriginalRendererSha256,
                StringComparison.OrdinalIgnoreCase);
        if (!installedRendererHash.Equals(manifest.OriginalRendererSha256,
                StringComparison.OrdinalIgnoreCase) &&
            !rendererPayloadHashes.Values.Contains(installedRendererHash,
                StringComparer.OrdinalIgnoreCase) && !backupIsValid)
        {
            return new(false,
                "The installed aidebug plugin is not a supported original or viewport build, " +
                "and no valid restoration backup exists.");
        }

        var rendererInstalled = rendererPayloadHashes.Values.Contains(
            installedRendererHash, StringComparer.OrdinalIgnoreCase);
        var viewport = ReadSavedViewport();
        var width = 0;
        var height = 0;
        var hasConfiguredDimensions =
            viewport.TryGetValue("internal_width", out var widthText) &&
            viewport.TryGetValue("internal_height", out var heightText) &&
            int.TryParse(widthText, out width) &&
            int.TryParse(heightText, out height) &&
            width is >= 640 and <= 3840 && height is >= 480 and <= 2160;
        var state = rendererInstalled && hasConfiguredDimensions ?
            $"Viewport renderer installed: {width} x {height}." :
            rendererInstalled ? "Universal viewport renderer installed." :
            "Compatible Cosmonarchy installation detected.";
        return new(true, state);
    }

    public bool IsGameRunning() => GetGameProcesses().Count != 0;

    public void RequestGameShutdown(TimeSpan timeout)
    {
        var processes = GetGameProcesses();
        // Close the launcher/host before StarCraft so it cannot interpret the
        // game exit as a request to relaunch the child while settings files are
        // about to be replaced.
        foreach (var process in processes.OrderBy(process =>
                     process.ProcessName.Equals("StarCraft",
                         StringComparison.OrdinalIgnoreCase) ? 1 : 0))
        {
            try
            {
                if (process.MainWindowHandle != IntPtr.Zero)
                    process.CloseMainWindow();
            }
            catch
            {
                // The final timeout check reports any process that remains.
            }
        }

        var deadline = DateTime.UtcNow + timeout;
        while (DateTime.UtcNow < deadline && GetGameProcesses().Count != 0)
        {
            Application.DoEvents();
            Thread.Sleep(100);
        }
        if (GetGameProcesses().Count != 0)
            throw new InvalidOperationException(
                "Cosmonarchy is still running. Close it and save again.");
    }

    public void Apply(ViewportSettings settings)
    {
        var validation = ValidateInstallation();
        if (!validation.Success)
            throw new InvalidOperationException(validation.Message);
        if (IsGameRunning())
            throw new InvalidOperationException("Close Cosmonarchy before saving settings.");
        if (settings.Profile.Width is < 640 or > 3840 ||
            settings.Profile.Height is < 480 or > 2160)
        {
            throw new ArgumentOutOfRangeException(nameof(settings),
                "Internal resolution must be between 640 x 480 and 3840 x 2160.");
        }

        Directory.CreateDirectory(Paths.BackupDirectory);
        EnsureBackups();
        if (!rendererPayloads.TryGetValue(settings.Profile.Id, out var rendererPayload) ||
            !rendererPayloadHashes.TryGetValue(settings.Profile.Id,
                out var rendererPayloadHash))
            throw new InvalidOperationException(
                $"Renderer profile {settings.Profile.Id} is not embedded.");

        var originalRenderer = File.ReadAllBytes(Paths.RendererPath);
        var originalDdraw = File.ReadAllBytes(Paths.DdrawPath);
        var originalConfig = File.Exists(Paths.ViewportConfigPath) ?
            File.ReadAllBytes(Paths.ViewportConfigPath) : null;

        try
        {
            AtomicWrite(Paths.RendererPath, rendererPayload);
            AtomicWrite(Paths.DdrawPath,
                Encoding.UTF8.GetBytes(BuildDdraw(settings)));
            AtomicWrite(Paths.ViewportConfigPath,
                new UTF8Encoding(false).GetBytes(BuildViewportConfig(settings)));
            WriteState(settings);

            if (!HashFile(Paths.RendererPath).Equals(rendererPayloadHash,
                    StringComparison.OrdinalIgnoreCase))
                throw new IOException("Installed renderer hash verification failed.");
            if (!HashFile(Paths.GptpPath).Equals(manifest.GptpSha256,
                    StringComparison.OrdinalIgnoreCase))
                throw new IOException("GPTP changed unexpectedly during installation.");
        }
        catch
        {
            AtomicWrite(Paths.RendererPath, originalRenderer);
            AtomicWrite(Paths.DdrawPath, originalDdraw);
            if (originalConfig is null)
                File.Delete(Paths.ViewportConfigPath);
            else
                AtomicWrite(Paths.ViewportConfigPath, originalConfig);
            throw;
        }
    }

    public void Restore()
    {
        if (IsGameRunning())
            throw new InvalidOperationException("Close Cosmonarchy before restoring files.");
        if (!File.Exists(Paths.RendererBackupPath) ||
            !File.Exists(Paths.DdrawBackupPath))
            throw new InvalidOperationException("No complete original backup is available.");
        if (!HashFile(Paths.RendererBackupPath).Equals(
                manifest.OriginalRendererSha256, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("The original renderer backup failed validation.");

        var originalRenderer = File.ReadAllBytes(Paths.RendererPath);
        var originalDdraw = File.ReadAllBytes(Paths.DdrawPath);
        var originalConfig = File.Exists(Paths.ViewportConfigPath) ?
            File.ReadAllBytes(Paths.ViewportConfigPath) : null;
        var originalState = File.Exists(Paths.StatePath) ?
            File.ReadAllBytes(Paths.StatePath) : null;
        var rendererBackup = File.ReadAllBytes(Paths.RendererBackupPath);
        var ddrawBackup = File.ReadAllBytes(Paths.DdrawBackupPath);

        try
        {
            AtomicWrite(Paths.RendererPath, rendererBackup);
            AtomicWrite(Paths.DdrawPath, ddrawBackup);
            DeleteIfPresent(Paths.ViewportConfigPath);
            DeleteIfPresent(Paths.StatePath);

            if (!HashFile(Paths.RendererPath).Equals(
                    manifest.OriginalRendererSha256,
                    StringComparison.OrdinalIgnoreCase))
                throw new IOException("Restored renderer verification failed.");
            if (!HashFile(Paths.DdrawPath).Equals(Hash(ddrawBackup),
                    StringComparison.OrdinalIgnoreCase))
                throw new IOException("Restored ddraw settings verification failed.");
            if (File.Exists(Paths.ViewportConfigPath) || File.Exists(Paths.StatePath))
                throw new IOException("Widescreen configuration cleanup failed.");
        }
        catch
        {
            AtomicWrite(Paths.RendererPath, originalRenderer);
            AtomicWrite(Paths.DdrawPath, originalDdraw);
            RestoreOptionalFile(Paths.ViewportConfigPath, originalConfig);
            RestoreOptionalFile(Paths.StatePath, originalState);
            throw;
        }

        DeleteOwnedResiduals();
    }

    public void LaunchGame()
    {
        Process.Start(new ProcessStartInfo
        {
            FileName = Paths.LauncherPath,
            WorkingDirectory = Paths.ReleaseDirectory,
            UseShellExecute = true,
        });
    }

    public Dictionary<string, string> ReadSavedViewport() =>
        IniDocument.ReadSection(Paths.ViewportConfigPath, "viewport");

    public Dictionary<string, string> ReadSavedPresentation() =>
        IniDocument.ReadSection(Paths.ViewportConfigPath, "presentation");

    private void EnsureBackups()
    {
        if (!File.Exists(Paths.RendererBackupPath))
        {
            var installedHash = HashFile(Paths.RendererPath);
            if (!installedHash.Equals(manifest.OriginalRendererSha256,
                    StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidOperationException(
                    "The viewport renderer is already installed, but no verified original " +
                    "aidebug backup exists. Restore the official file before continuing.");
            }
            File.Copy(Paths.RendererPath, Paths.RendererBackupPath, false);
        }
        if (!File.Exists(Paths.DdrawBackupPath))
            File.Copy(Paths.DdrawPath, Paths.DdrawBackupPath, false);
    }

    private string BuildDdraw(ViewportSettings settings)
    {
        var source = File.ReadAllText(Paths.DdrawPath);
        var fullscreen = settings.WindowMode != WindowMode.Windowed;
        var windowed = settings.WindowMode != WindowMode.ExclusiveFullscreen;
        var border = settings.WindowMode == WindowMode.Windowed;
        var position = GetWindowPosition(settings);
        var nearest = settings.Filter == ScalingFilter.NearestNeighbor;
        var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["width"] = settings.OutputWidth.ToString(),
            ["height"] = settings.OutputHeight.ToString(),
            ["fullscreen"] = fullscreen.ToString().ToLowerInvariant(),
            ["windowed"] = windowed.ToString().ToLowerInvariant(),
            ["maintas"] = settings.PreserveAspectRatio.ToString().ToLowerInvariant(),
            ["boxing"] = "false",
            ["maxfps"] = "333",
            ["adjmouse"] = "true",
            ["shader"] = nearest ? "Shaders\\nearest-neighbor.glsl" : "Bilinear",
            ["d3d9_filter"] = nearest ? "0" : "1",
            ["posX"] = position.X.ToString(),
            ["posY"] = position.Y.ToString(),
            ["border"] = border.ToString().ToLowerInvariant(),
            ["resizable"] = border.ToString().ToLowerInvariant(),
            ["savesettings"] = "0",
            ["maxgameticks"] = "-1",
            ["nonexclusive"] = "false",
            ["singlecpu"] = "false",
        };
        return IniDocument.UpdateSection(source, "ddraw", values);
    }

    private static Point GetWindowPosition(ViewportSettings settings)
    {
        if (settings.WindowMode != WindowMode.Windowed)
            return settings.Display.Bounds.Location;
        var area = settings.Display.WorkingArea;
        return new Point(
            area.Left + (area.Width - settings.OutputWidth) / 2,
            area.Top + (area.Height - settings.OutputHeight) / 2);
    }

    private static string BuildViewportConfig(ViewportSettings settings)
    {
        var mode = settings.WindowMode == WindowMode.Windowed ?
            settings.PresentationMode switch
            {
                PresentationMode.Scale => "scale",
                PresentationMode.FitDisplay => "fit_display",
                _ => "exact_output",
            } : "fullscreen";
        var windowMode = settings.WindowMode switch
        {
            WindowMode.Windowed => "windowed",
            WindowMode.BorderlessFullscreen => "borderless_fullscreen",
            _ => "exclusive_fullscreen",
        };
        var filter = settings.Filter == ScalingFilter.NearestNeighbor ?
            "nearest" : "smooth";

        return $"""
            ; Generated by Cosmonarchy Widescreen Settings. Edit through the GUI.
            [viewport]
            config_version=1
            renderer_profile={settings.Profile.Id}
            internal_width={settings.Profile.Width}
            internal_height={settings.Profile.Height}
            top_ui_layout={(settings.TopTextLayout == TopTextLayout.ScreenEdges ? "screen_edges" : "centered_4_3")}

            [presentation]
            mode={mode}
            scale_numerator={settings.ScaleNumerator}
            scale_denominator={settings.ScaleDenominator}
            output_width={settings.OutputWidth}
            output_height={settings.OutputHeight}
            window_mode={windowMode}
            display_device={settings.Display.DeviceName}
            filter={filter}
            preserve_aspect_ratio={settings.PreserveAspectRatio.ToString().ToLowerInvariant()}
            """ + Environment.NewLine;
    }

    private void WriteState(ViewportSettings settings)
    {
        Directory.CreateDirectory(Paths.StateDirectory);
        var rendererSha256 = rendererPayloadHashes[settings.Profile.Id];
        var state = new
        {
            schemaVersion = 1,
            installedAt = DateTimeOffset.UtcNow,
            rendererProfile = settings.Profile.Id,
            rendererSha256,
            originalRendererSha256 = HashFile(Paths.RendererBackupPath),
            stableGptpSha256 = HashFile(Paths.GptpPath),
        };
        AtomicWrite(Paths.StatePath,
            new UTF8Encoding(false).GetBytes(JsonSerializer.Serialize(state,
                new JsonSerializerOptions { WriteIndented = true })));
    }

    private static List<Process> GetGameProcesses() =>
        Process.GetProcesses().Where(process =>
            process.ProcessName.Equals("StarCraft", StringComparison.OrdinalIgnoreCase) ||
            process.ProcessName.Equals("Cosmonarchy BW", StringComparison.OrdinalIgnoreCase) ||
            process.ProcessName.Equals("bwl_host", StringComparison.OrdinalIgnoreCase))
            .ToList();

    private static void AtomicWrite(string path, byte[] bytes)
    {
        var directory = Path.GetDirectoryName(path) ??
            throw new InvalidOperationException($"No parent directory for {path}.");
        Directory.CreateDirectory(directory);
        var temporary = Path.Combine(directory,
            $".{Path.GetFileName(path)}.{Guid.NewGuid():N}.tmp");
        try
        {
            File.WriteAllBytes(temporary, bytes);
            File.Move(temporary, path, true);
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }
    }

    private static void DeleteIfPresent(string path)
    {
        if (File.Exists(path))
            File.Delete(path);
    }

    private void DeleteOwnedResiduals()
    {
        DeleteIfPresent(Paths.ViewportConfigPath);
        DeleteIfPresent(Paths.StatePath);
        DeleteIfPresent(Paths.ConfiguratorLogPath);

        foreach (var path in Directory.EnumerateFiles(
                     Paths.ReleaseDirectory, "fixed_zoom*",
                     SearchOption.TopDirectoryOnly))
            File.Delete(path);

        DeleteIfPresent(Paths.RendererBackupPath);
        DeleteIfPresent(Paths.DdrawBackupPath);
        DeleteDirectoryIfEmpty(Paths.BackupDirectory);
        DeleteDirectoryIfEmpty(Paths.StateDirectory);
    }

    private static void DeleteDirectoryIfEmpty(string path)
    {
        if (Directory.Exists(path) &&
            !Directory.EnumerateFileSystemEntries(path).Any())
            Directory.Delete(path, false);
    }

    private static void RestoreOptionalFile(string path, byte[]? bytes)
    {
        if (bytes is null)
            DeleteIfPresent(path);
        else
            AtomicWrite(path, bytes);
    }

    private static byte[] ReadResource(string name)
    {
        using var stream = Assembly.GetExecutingAssembly()
            .GetManifestResourceStream(name) ??
            throw new InvalidOperationException($"Missing embedded resource {name}.");
        using var memory = new MemoryStream();
        stream.CopyTo(memory);
        return memory.ToArray();
    }

    private static string ReadResourceText(string name) =>
        Encoding.UTF8.GetString(ReadResource(name));

    private static string HashFile(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream));
    }

    private static string Hash(byte[] bytes) =>
        Convert.ToHexString(SHA256.HashData(bytes));
}
