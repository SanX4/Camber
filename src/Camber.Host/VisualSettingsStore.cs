using System.Text.Json;
using Camber.Core;

namespace Camber.Host;
internal static class VisualSettingsStore
{
#if CAMBER_ISOLATED_TEST_HOST
    private static readonly string PathName = Path.Combine(TestDataPaths.Root,"visual-settings.json");
#else
    private static readonly string PathName = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Camber", "visual-settings.json");
#endif
    public static VisualSettings Load()
    {
        try { var value=JsonSerializer.Deserialize<VisualSettings>(File.ReadAllText(PathName)) ?? new();
            return Enum.IsDefined(value.Enhancement)?value:value with {Enhancement=EnhancementMode.Original}; }
        catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException) { return new(); }
    }
    public static void Save(VisualSettings settings)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(PathName)!);
        File.WriteAllText(PathName + ".tmp", JsonSerializer.Serialize(settings));
        File.Move(PathName + ".tmp", PathName, true);
    }
}
