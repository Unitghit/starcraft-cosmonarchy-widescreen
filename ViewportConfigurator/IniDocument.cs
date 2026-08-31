namespace CosmonarchyWidescreen;

internal static class IniDocument
{
    public static string UpdateSection(
        string source,
        string section,
        IReadOnlyDictionary<string, string> values)
    {
        var newline = source.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
        var lines = source.Replace("\r\n", "\n", StringComparison.Ordinal)
            .Split('\n').ToList();
        var header = $"[{section}]";
        var sectionStart = lines.FindIndex(line =>
            line.Trim().Equals(header, StringComparison.OrdinalIgnoreCase));
        if (sectionStart < 0)
            throw new InvalidDataException($"Missing [{section}] section in ddraw.ini.");

        var sectionEnd = lines.Count;
        for (var index = sectionStart + 1; index < lines.Count; ++index)
        {
            var trimmed = lines[index].Trim();
            if (trimmed.StartsWith("[", StringComparison.Ordinal) &&
                trimmed.EndsWith("]", StringComparison.Ordinal))
            {
                sectionEnd = index;
                break;
            }
        }

        var pending = new Dictionary<string, string>(values,
            StringComparer.OrdinalIgnoreCase);
        for (var index = sectionStart + 1; index < sectionEnd; ++index)
        {
            var line = lines[index];
            var trimmed = line.TrimStart();
            if (trimmed.StartsWith(';') || trimmed.StartsWith('#'))
                continue;
            var equals = line.IndexOf('=');
            if (equals <= 0)
                continue;
            var key = line[..equals].Trim();
            if (!pending.Remove(key, out var replacement))
                continue;
            lines[index] = $"{key}={replacement}";
        }

        foreach (var pair in pending.Reverse())
            lines.Insert(sectionEnd, $"{pair.Key}={pair.Value}");

        return string.Join(newline, lines);
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
}
