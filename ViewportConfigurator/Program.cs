namespace CosmonarchyWidescreen;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        ApplicationConfiguration.Initialize();

        var paths = new InstallationPaths(AppContext.BaseDirectory);
        var service = new ConfigurationService(paths);
        if (args.Length == 1 && args[0].StartsWith("--",
                StringComparison.Ordinal))
        {
            Environment.ExitCode = RunCommand(args[0], service);
            return;
        }

        Application.Run(new MainForm(service));
    }

    private static int RunCommand(string command, ConfigurationService service)
    {
        try
        {
            string message;
            if (command.Equals("--validate", StringComparison.OrdinalIgnoreCase))
            {
                var result = service.ValidateInstallation();
                message = result.Message;
                if (!result.Success)
                    throw new InvalidOperationException(result.Message);
            }
            else if (command.Equals("--apply-defaults",
                         StringComparison.OrdinalIgnoreCase))
            {
                var screen = Screen.PrimaryScreen ?? Screen.AllScreens[0];
                var display = new DisplayChoice(
                    Array.IndexOf(Screen.AllScreens, screen), screen);
                service.Apply(new ViewportSettings(
                    service.Profiles[0], PresentationMode.Scale, 2.5m,
                    5, 2, 3200, 1800, WindowMode.Windowed,
                    ScalingFilter.NearestNeighbor, true, display));
                message = "Default 1280x720 internal / 3200x1800 external settings applied.";
            }
            else if (command.Equals("--apply-borderless",
                         StringComparison.OrdinalIgnoreCase))
            {
                var screen = Screen.PrimaryScreen ?? Screen.AllScreens[0];
                var display = new DisplayChoice(
                    Array.IndexOf(Screen.AllScreens, screen), screen);
                var divisor = GreatestCommonDivisor(screen.Bounds.Width, 1280);
                service.Apply(new ViewportSettings(
                    service.Profiles[0], PresentationMode.FitDisplay,
                    screen.Bounds.Width / 1280m,
                    screen.Bounds.Width / divisor, 1280 / divisor,
                    screen.Bounds.Width, screen.Bounds.Height,
                    WindowMode.BorderlessFullscreen,
                    ScalingFilter.NearestNeighbor, true, display));
                message = "Borderless fullscreen settings applied.";
            }
            else if (command.StartsWith("--apply-profile=",
                         StringComparison.OrdinalIgnoreCase))
            {
                var requested = command["--apply-profile=".Length..];
                var profile = service.Profiles.FirstOrDefault(candidate =>
                    candidate.Id.StartsWith(requested + "-",
                        StringComparison.OrdinalIgnoreCase) ||
                    $"{candidate.Width}x{candidate.Height}".Equals(requested,
                        StringComparison.OrdinalIgnoreCase)) ??
                    throw new ArgumentException(
                        $"Unknown renderer profile: {requested}");
                var screen = Screen.PrimaryScreen ?? Screen.AllScreens[0];
                var display = new DisplayChoice(
                    Array.IndexOf(Screen.AllScreens, screen), screen);
                service.Apply(new ViewportSettings(
                    profile, PresentationMode.Scale, 1m, 1, 1,
                    profile.Width, profile.Height, WindowMode.Windowed,
                    ScalingFilter.NearestNeighbor, true, display));
                message = $"{profile.Width}x{profile.Height} internal profile applied.";
            }
            else if (command.StartsWith("--apply-custom=",
                         StringComparison.OrdinalIgnoreCase))
            {
                var requested = command["--apply-custom=".Length..];
                var parts = requested.Split('x', 'X');
                if (parts.Length != 2 || !int.TryParse(parts[0], out var width) ||
                    !int.TryParse(parts[1], out var height) ||
                    width is < 640 or > 3840 || height is < 480 or > 2160)
                {
                    throw new ArgumentException(
                        "Custom internal resolution must be WIDTHxHEIGHT from 640x480 through 3840x2160.");
                }
                var customTemplate = service.Profiles.First(profile =>
                    profile.AspectRatio.Equals("Custom",
                        StringComparison.OrdinalIgnoreCase));
                var profile = customTemplate with { Width = width, Height = height };
                var screen = Screen.PrimaryScreen ?? Screen.AllScreens[0];
                var display = new DisplayChoice(
                    Array.IndexOf(Screen.AllScreens, screen), screen);
                service.Apply(new ViewportSettings(
                    profile, PresentationMode.Scale, 1m, 1, 1,
                    width, height, WindowMode.Windowed,
                    ScalingFilter.NearestNeighbor, true, display));
                message = $"Custom {width}x{height} internal resolution applied.";
            }
            else if (command.Equals("--restore", StringComparison.OrdinalIgnoreCase))
            {
                service.Restore();
                message = "Original files restored.";
            }
            else
            {
                throw new ArgumentException($"Unknown command: {command}");
            }

            // A successful restore deliberately leaves no configurator-owned
            // files behind. Do not recreate the log after cleanup.
            if (!command.Equals("--restore", StringComparison.OrdinalIgnoreCase))
            {
                File.WriteAllText(service.Paths.ConfiguratorLogPath,
                    $"{DateTimeOffset.Now:O} {command}: PASS - {message}{Environment.NewLine}");
            }
            return 0;
        }
        catch (Exception exception)
        {
            File.WriteAllText(service.Paths.ConfiguratorLogPath,
                $"{DateTimeOffset.Now:O} {command}: FAIL - {exception}{Environment.NewLine}");
            return 1;
        }
    }

    private static int GreatestCommonDivisor(int left, int right)
    {
        while (right != 0)
            (left, right) = (right, left % right);
        return Math.Max(1, left);
    }
}
