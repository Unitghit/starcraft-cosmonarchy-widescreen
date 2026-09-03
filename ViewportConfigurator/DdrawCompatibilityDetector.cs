namespace CosmonarchyWidescreen;

internal static class DdrawCompatibilityDetector
{
    public static DdrawCompatibilityResult Classify(
        string? sha256,
        IReadOnlyList<DdrawCompatibilityProfile> testedProfiles)
    {
        if (string.IsNullOrWhiteSpace(sha256))
        {
            return new(false, false,
                "No local ddraw.dll detected.");
        }

        var match = testedProfiles.FirstOrDefault(profile =>
            profile.Sha256.Equals(sha256,
                StringComparison.OrdinalIgnoreCase));
        if (match is not null)
        {
            return new(true, true,
                $"cnc-ddraw {match.Version} detected and supported.");
        }

        return new(true, false,
            "Unrecognized ddraw.dll detected. Compatibility is unverified.");
    }
}
