using System.Drawing;

namespace CosmonarchyWidescreen;

internal sealed record RendererProfile(
    string Id,
    string DisplayName,
    int Width,
    int Height,
    string AspectRatio,
    string PayloadResource)
{
    public override string ToString() => DisplayName;
}

internal sealed record ScaleChoice(decimal Value, string DisplayName)
{
    public override string ToString() => DisplayName;
}

internal sealed record DisplayChoice(int Index, Screen Screen)
{
    public string DeviceName => Screen.DeviceName;
    public Rectangle Bounds => Screen.Bounds;
    public Rectangle WorkingArea => Screen.WorkingArea;
    public override string ToString() =>
        $"Display {Index + 1}: {Bounds.Width} x {Bounds.Height}" +
        (Screen.Primary ? " (Primary)" : string.Empty);
}

internal enum PresentationMode
{
    Scale,
    FitDisplay,
    ExactOutput,
}

internal enum WindowMode
{
    Windowed,
    BorderlessFullscreen,
    ExclusiveFullscreen,
}

internal enum ScalingFilter
{
    NearestNeighbor,
    Smooth,
}

internal enum TopTextLayout
{
    Centered4x3,
    ScreenEdges,
}

internal sealed record ViewportSettings(
    RendererProfile Profile,
    PresentationMode PresentationMode,
    decimal RequestedScale,
    int ScaleNumerator,
    int ScaleDenominator,
    int OutputWidth,
    int OutputHeight,
    WindowMode WindowMode,
    ScalingFilter Filter,
    bool PreserveAspectRatio,
    DisplayChoice Display,
    TopTextLayout TopTextLayout = TopTextLayout.Centered4x3);

internal sealed record ValidationResult(bool Success, string Message);

internal sealed class DdrawOwnedSettingState
{
    public bool OriginalPresent { get; init; }
    public string? OriginalValue { get; init; }
    public string AppliedValue { get; init; } = string.Empty;
}

internal sealed class InstallationState
{
    public int SchemaVersion { get; init; }
    public DateTimeOffset InstalledAt { get; init; }
    public string RendererProfile { get; init; } = string.Empty;
    public string RendererSha256 { get; init; } = string.Empty;
    public string OriginalRendererSha256 { get; init; } = string.Empty;
    public string StableGptpSha256 { get; init; } = string.Empty;
    public Dictionary<string, DdrawOwnedSettingState> DdrawOwnedSettings { get; init; } =
        new(StringComparer.OrdinalIgnoreCase);
}

internal sealed record CompatibilityManifest(
    int SchemaVersion,
    string StarCraftSha256,
    string GptpSha256,
    string HotloaderSha256,
    string OriginalRendererSha256,
    IReadOnlyList<RendererProfile> RendererProfiles);

internal sealed class InstallationPaths
{
    public InstallationPaths(string executableDirectory)
    {
        ReleaseDirectory = Path.GetFullPath(executableDirectory)
            .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        WorkspaceDirectory = Directory.GetParent(ReleaseDirectory)?.FullName ??
            ReleaseDirectory;
    }

    public string ReleaseDirectory { get; }
    public string WorkspaceDirectory { get; }
    public string LauncherPath => Path.Combine(ReleaseDirectory, "Cosmonarchy BW.exe");
    public string PluginsDirectory => Path.Combine(ReleaseDirectory, "plugins");
    public string RendererPath => Path.Combine(PluginsDirectory, "aize_debug.qdp");
    public string GptpPath => Path.Combine(PluginsDirectory, "gptp.qdp");
    public string HotloaderPath => Path.Combine(PluginsDirectory, "_qdp-hotloader.qdp");
    public string StarCraftDirectory => Path.Combine(WorkspaceDirectory, "Starcraft");
    public string StarCraftPath => Path.Combine(StarCraftDirectory, "StarCraft.exe");
    public string DdrawPath => Path.Combine(StarCraftDirectory, "ddraw.ini");
    public string ViewportConfigPath =>
        Path.Combine(ReleaseDirectory, "cosmonarchy_viewport.ini");
    public string StateDirectory =>
        Path.Combine(ReleaseDirectory, ".cosmonarchy-widescreen");
    public string BackupDirectory => Path.Combine(StateDirectory, "backup");
    public string RendererBackupPath => Path.Combine(BackupDirectory, "aize_debug.qdp");
    public string DdrawBackupPath => Path.Combine(BackupDirectory, "ddraw.ini");
    public string StatePath => Path.Combine(StateDirectory, "installation.json");
    public string ConfiguratorLogPath =>
        Path.Combine(ReleaseDirectory, "cosmonarchy_widescreen_configurator.log");
}
