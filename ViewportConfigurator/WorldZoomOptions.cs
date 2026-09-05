using System.Globalization;

namespace CosmonarchyWidescreen;

internal sealed record StartingZoomChoice(int Units, string DisplayName)
{
    public override string ToString() => DisplayName;
}

internal static class WorldZoomOptions
{
    public const int BaseUnits = 10000;
    public static int NormalizeExtraZoomPercent(int value) => value is 50 or 100 ? value : 0;
    public static int ReadExtraZoomPercent(IReadOnlyDictionary<string, string> values) =>
        !values.TryGetValue("extra_zoom_percent", out var text) ? 100 :
        int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var value)
            ? NormalizeExtraZoomPercent(value) : 0;
    public static bool ReadHighRefreshPointer(IReadOnlyDictionary<string,string> values) =>
        values.TryGetValue("high_refresh_pointer",out var value) && value == "1";
    public static bool ReadSmoothWorldEdges(IReadOnlyDictionary<string, string> values) =>
        !values.TryGetValue("world_filter", out var filter) ||
        filter.Equals("sharp_edges", StringComparison.OrdinalIgnoreCase);
    public static bool ReadPixelPerfectRendering(IReadOnlyDictionary<string, string> values) =>
        !values.TryGetValue("backend", out var backend) ||
        backend.Equals("single_stage", StringComparison.OrdinalIgnoreCase);
    public static int MaximumStartUnits(int width) =>
        Math.Max(20000, width * BaseUnits / 640);

    public static int ReadStartUnits(IReadOnlyDictionary<string, string> values, int width)
    {
        return values.TryGetValue("start_zoom_units", out var text) &&
               int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var units)
            ? Math.Clamp(units, BaseUnits, MaximumStartUnits(width)) : BaseUnits;
    }

    public static IReadOnlyList<StartingZoomChoice> Choices(int width, int height, int selected)
    {
        var units = new List<int>();
        foreach (var targetWidth in new[] { 1280, 640 })
            if (targetWidth <= width)
                units.Add(width * BaseUnits / targetWidth);
        units.Add(BaseUnits);
        for (var value = 11250; value <= 20000; value += 1250)
            units.Add(value);
        units.Add(Math.Clamp(selected, BaseUnits, MaximumStartUnits(width)));
        return units.Distinct().Select(value =>
        {
            var viewWidth = width * BaseUnits / value;
            var viewHeight = height * BaseUnits / value;
            var recommended = viewWidth == 1280 || viewWidth == 640;
            var percent = (value / 100m).ToString("0.##", CultureInfo.CurrentCulture);
            return new StartingZoomChoice(value,
                $"{viewWidth} x {viewHeight} view ({percent}%)" +
                (recommended ? " (Recommended)" : value == BaseUnits ? " (Full viewport)" : ""));
        }).ToArray();
    }

    public static string ConfigLines(bool smooth, int units, int width, int extraZoomPercent = 100) =>
        $"transition={(smooth ? "smooth" : "instant")}" + Environment.NewLine +
        $"start_zoom_units={Math.Clamp(units, BaseUnits, MaximumStartUnits(width))}" + Environment.NewLine +
        $"extra_zoom_percent={NormalizeExtraZoomPercent(extraZoomPercent)}";

}
