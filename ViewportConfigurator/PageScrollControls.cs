namespace CosmonarchyWidescreen;

// Dropdown wheel input belongs to the settings page, never to a setting's value.
// Keep mouse clicks, typing, and keyboard navigation on the standard controls.
internal sealed class SettingsScrollPanel : Panel
{
    internal static void ForwardWheel(Control source, MouseEventArgs e)
    {
        for (Control? parent = source.Parent; parent is not null; parent = parent.Parent)
        {
            if (parent is not SettingsScrollPanel page) continue;
            var point = page.PointToClient(source.PointToScreen(e.Location));
            page.OnMouseWheel(new MouseEventArgs(e.Button, e.Clicks, point.X, point.Y, e.Delta));
            break;
        }
        if (e is HandledMouseEventArgs handled) handled.Handled = true;
    }
}

internal sealed class PageScrollComboBox : ComboBox
{
    protected override void OnMouseWheel(MouseEventArgs e)
    {
        // A popup must not remain floating above a page that has scrolled away.
        if (DroppedDown) DroppedDown = false;
        SettingsScrollPanel.ForwardWheel(this, e);
    }
}
