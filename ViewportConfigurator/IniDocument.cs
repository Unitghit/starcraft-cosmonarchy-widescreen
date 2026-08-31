namespace CosmonarchyWidescreen;

internal sealed record IniValueSnapshot(bool Present, string? Value)
{
    public static IniValueSnapshot Missing { get; } = new(false, null);
    public static IniValueSnapshot FromValue(string value) => new(true, value);
}

internal static class IniDocument
{
    public static string UpdateSection(
        string source,
        string section,
        IReadOnlyDictionary<string, string> values)
    {
        var snapshots = values.ToDictionary(
            pair => pair.Key,
            pair => IniValueSnapshot.FromValue(pair.Value),
            StringComparer.OrdinalIgnoreCase);
        return RewriteSection(source, section, snapshots);
    }

    public static string RewriteSection(
        string source,
        string section,
        IReadOnlyDictionary<string, IniValueSnapshot> values)
    {
        var newline = source.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
        var lines = source.Replace("\r\n", "\n", StringComparison.Ordinal)
            .Split('\n').ToList();
        var (sectionStart, sectionEnd) = FindSection(lines, section);
        var canonical = new Dictionary<string, KeyValuePair<string, IniValueSnapshot>>(
            StringComparer.OrdinalIgnoreCase);
        foreach (var pair in values)
            canonical[pair.Key] = pair;

        var handled = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var rewritten = new List<string>(lines.Count);
        rewritten.AddRange(lines.Take(sectionStart + 1));

        for (var index = sectionStart + 1; index < sectionEnd; ++index)
        {
            var line = lines[index];
            if (!TryReadActiveKey(line, out var key) ||
                !canonical.TryGetValue(key, out var target))
            {
                rewritten.Add(line);
                continue;
            }

            // Keep a single canonical occurrence for an owned key. Later active
            // duplicates are removed so cnc-ddraw cannot read a stale copy.
            if (!handled.Add(target.Key))
                continue;
            if (target.Value.Present)
                rewritten.Add($"{target.Key}={target.Value.Value ?? string.Empty}");
        }

        foreach (var pair in values)
        {
            if (handled.Contains(pair.Key) || !pair.Value.Present)
                continue;
            rewritten.Add($"{pair.Key}={pair.Value.Value ?? string.Empty}");
        }

        rewritten.AddRange(lines.Skip(sectionEnd));
        return string.Join(newline, rewritten);
    }

    public static Dictionary<string, IniValueSnapshot> SnapshotSection(
        string source,
        string section,
        IEnumerable<string> keys)
    {
        var requested = new HashSet<string>(keys, StringComparer.OrdinalIgnoreCase);
        var result = requested.ToDictionary(
            key => key,
            _ => IniValueSnapshot.Missing,
            StringComparer.OrdinalIgnoreCase);
        var found = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var lines = source.Replace("\r\n", "\n", StringComparison.Ordinal)
            .Split('\n').ToList();
        var (sectionStart, sectionEnd) = FindSection(lines, section);

        for (var index = sectionStart + 1; index < sectionEnd; ++index)
        {
            if (!TryReadActiveEntry(lines[index], out var key, out var value) ||
                !requested.Contains(key) || !found.Add(key))
                continue;
            result[key] = IniValueSnapshot.FromValue(value);
        }
        return result;
    }

    public static string RestoreOwnedSection(
        string source,
        string section,
        IReadOnlyDictionary<string, DdrawOwnedSettingState> ownedSettings)
    {
        if (ownedSettings.Count == 0)
            return source;

        var current = SnapshotSection(source, section, ownedSettings.Keys);
        var targets = new Dictionary<string, IniValueSnapshot>(
            StringComparer.OrdinalIgnoreCase);
        foreach (var pair in ownedSettings)
        {
            var currentValue = current[pair.Key];
            var stillApplied = currentValue.Present && string.Equals(
                currentValue.Value?.Trim(),
                pair.Value.AppliedValue.Trim(),
                StringComparison.OrdinalIgnoreCase);
            targets[pair.Key] = stillApplied ?
                new IniValueSnapshot(
                    pair.Value.OriginalPresent,
                    pair.Value.OriginalValue) :
                currentValue;
        }
        return RewriteSection(source, section, targets);
    }

    public static Dictionary<string, string> ReadSection(string path, string section)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (!File.Exists(path))
            return result;

        var active = false;
        foreach (var rawLine in File.ReadLines(path))
        {
            var line = rawLine.Trim();
            if (line.StartsWith('[') && line.EndsWith(']'))
            {
                active = line[1..^1].Equals(section,
                    StringComparison.OrdinalIgnoreCase);
                continue;
            }
            if (!active || line.Length == 0 || line.StartsWith(';') ||
                line.StartsWith('#'))
                continue;
            var equals = line.IndexOf('=');
            if (equals <= 0)
                continue;
            result[line[..equals].Trim()] = line[(equals + 1)..].Trim();
        }
        return result;
    }

    private static (int Start, int End) FindSection(
        IReadOnlyList<string> lines,
        string section)
    {
        var header = $"[{section}]";
        var sectionStart = -1;
        for (var index = 0; index < lines.Count; ++index)
        {
            if (!lines[index].Trim().Equals(header, StringComparison.OrdinalIgnoreCase))
                continue;
            sectionStart = index;
            break;
        }
        if (sectionStart < 0)
            throw new InvalidDataException($"Missing [{section}] section in ddraw.ini.");

        var sectionEnd = lines.Count;
        for (var index = sectionStart + 1; index < lines.Count; ++index)
        {
            var trimmed = lines[index].Trim();
            if (!trimmed.StartsWith("[", StringComparison.Ordinal) ||
                !trimmed.EndsWith("]", StringComparison.Ordinal))
                continue;
            sectionEnd = index;
            break;
        }
        return (sectionStart, sectionEnd);
    }

    private static bool TryReadActiveKey(string line, out string key)
    {
        key = string.Empty;
        var trimmed = line.TrimStart();
        if (trimmed.Length == 0 || trimmed.StartsWith(';') || trimmed.StartsWith('#'))
            return false;
        var equals = line.IndexOf('=');
        if (equals <= 0)
            return false;
        key = line[..equals].Trim();
        return key.Length != 0;
    }

    private static bool TryReadActiveEntry(
        string line,
        out string key,
        out string value)
    {
        value = string.Empty;
        if (!TryReadActiveKey(line, out key))
            return false;
        var equals = line.IndexOf('=');
        value = line[(equals + 1)..].Trim();
        return true;
    }
}
