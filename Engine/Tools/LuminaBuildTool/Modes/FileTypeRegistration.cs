using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Security;
using LuminaBuildTool.Core;
using Microsoft.Win32;

namespace LuminaBuildTool.Modes;

// Gives the engine's descriptor files the Lumina icon and an Open Project verb, per user so no elevation is needed.
public static class FileTypeRegistration
{
    private static readonly (string Extension, string ProgId, string Description)[] FileTypes =
    {
        (".lproject", "Lumina.Project", "Lumina Project"),
        (".lplugin", "Lumina.Plugin", "Lumina Plugin"),
    };

    private const int ShellAssociationChanged = 0x08000000;

    public static void Register(string EngineRoot)
    {
        if (!OperatingSystem.IsWindows())
        {
            Log.Info("File type icons are an Explorer feature, so there is nothing to register on this platform.");
            return;
        }

        string IconPath = Path.Combine(EngineRoot, "Engine", "Resources", "Icons", "Lumina.ico");
        if (!File.Exists(IconPath))
        {
            Log.Warning("{0} is missing, so .lproject and .lplugin keep their current icon.", IconPath);
            return;
        }

        try
        {
            WriteClasses(IconPath);
            WriteOpenProjectVerb(IconPath, Path.GetFullPath(Path.Combine(EngineRoot, "LuminaBuild.bat")));
        }
        catch (Exception Ex) when (Ex is SecurityException or UnauthorizedAccessException or IOException)
        {
            Log.Warning("Could not register the Lumina file icons: {0}", Ex.Message);
        }
    }

    [SupportedOSPlatform("windows")]
    private static void WriteClasses(string IconPath)
    {
        using RegistryKey Classes = Registry.CurrentUser.CreateSubKey(@"Software\Classes");
        foreach ((string Extension, string ProgId, string Description) in FileTypes)
        {
            using (RegistryKey ExtensionKey = Classes.CreateSubKey(Extension))
            {
                ExtensionKey.SetValue(string.Empty, ProgId);
            }

            using RegistryKey TypeKey = Classes.CreateSubKey(ProgId);
            TypeKey.SetValue(string.Empty, Description);
            using RegistryKey IconKey = TypeKey.CreateSubKey("DefaultIcon");
            IconKey.SetValue(string.Empty, $"\"{IconPath}\",0");

            // An app picked through Open With outranks the type, and Windows guards that choice, so it is only reported.
            using RegistryKey? Choice = Registry.CurrentUser.OpenSubKey($@"Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\{Extension}\UserChoice");
            if (Choice?.GetValue("ProgId") is string Chosen && Chosen != ProgId)
            {
                Log.Warning("{0} files open with {1}, whose icon Explorer shows instead. Clear that choice in Settings > Apps > Default apps to see the Lumina icon.", Extension, Chosen);
                continue;
            }

            Log.Info("{0} files show the Lumina icon.", Extension);
        }

        // Explorer caches icons per extension and only drops them when told an association changed.
        SHChangeNotify(ShellAssociationChanged, 0, IntPtr.Zero, IntPtr.Zero);
    }

    // Keyed by extension rather than our type, so it shows even when another app owns double-click.
    [SupportedOSPlatform("windows")]
    private static void WriteOpenProjectVerb(string IconPath, string BuildScript)
    {
        using RegistryKey Verb = Registry.CurrentUser.CreateSubKey(@"Software\Classes\SystemFileAssociations\.lproject\shell\LuminaOpen");
        Verb.SetValue(string.Empty, "Open Project");
        Verb.SetValue("Icon", $"\"{IconPath}\",0");

        // The window stays open after a failed build so its errors can be read.
        using RegistryKey Command = Verb.CreateSubKey("command");
        Command.SetValue(string.Empty, $"cmd.exe /c \"\"{BuildScript}\" Open \"%1\" || pause\"");

        Log.Info("Right-clicking a .lproject offers Open Project, which builds it when stale.");
    }

    [DllImport("shell32.dll")]
    private static extern void SHChangeNotify(int EventId, uint Flags, IntPtr Item1, IntPtr Item2);
}
