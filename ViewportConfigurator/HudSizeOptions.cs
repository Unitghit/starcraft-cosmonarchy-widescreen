namespace CosmonarchyWidescreen;

internal sealed record HudSizeChoice(int ReferenceHeight, string DisplayName)
{
    public override string ToString() => DisplayName;
}

internal static class HudSizeOptions
{
    public static HudSizeChoice[] Choices() =>
    [
        new(0, "Match internal resolution (current size)"),
        new(480, "640 x 480-sized"),
        new(720, "1280 x 720-sized"),
        new(900, "1600 x 900-sized"),
        new(1080, "1920 x 1080-sized"),
        new(1440, "2560 x 1440-sized"),
        new(2160, "3840 x 2160-sized"),
    ];

    public static int Normalize(int reference) => reference is >= 480 and <= 2160 ? reference : 0;
    public static int ReadReference(IReadOnlyDictionary<string, string> section, string key) =>
        section.TryGetValue(key, out var value) && int.TryParse(value, out var reference)
            ? Normalize(reference) : 0;
    public static bool ReadSeparate(IReadOnlyDictionary<string, string> section) =>
        section.TryGetValue("separate_top_text", out var value) && value == "1";
}
