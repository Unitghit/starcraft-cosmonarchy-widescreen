using CosmonarchyWidescreen;

var tests = new (string Name, Action Run)[]
{
    ("apply normalizes owned duplicates", ApplyNormalizesOwnedDuplicates),
    ("snapshot uses first active value", SnapshotUsesFirstActiveValue),
    ("restore returns untouched applied value", RestoreReturnsUntouchedAppliedValue),
    ("restore preserves a later user change", RestorePreservesLaterUserChange),
    ("restore removes a newly introduced key", RestoreRemovesNewlyIntroducedKey),
    ("other sections remain untouched", OtherSectionsRemainUntouched),
};

foreach (var test in tests)
{
    test.Run();
    Console.WriteLine($"PASS {test.Name}");
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
