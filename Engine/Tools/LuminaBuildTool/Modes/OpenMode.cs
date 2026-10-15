using LuminaBuildTool.Configuration;
using LuminaBuildTool.Core;
using LuminaBuildTool.Platform;
using LuminaBuildTool.Rules;

namespace LuminaBuildTool.Modes;

// Opens a .lproject in the editor, building first so a stale checkout never opens against old binaries.
public static class OpenMode
{
    private const string EditorTargetName = "Lumina";

    public static async Task<int> RunAsync(CommandLine Arguments, BuildDirectories EngineDirectories, CancellationToken Cancellation)
    {
        string? ProjectFile = Arguments.GetPositional(1);
        if (string.IsNullOrEmpty(ProjectFile) || !ProjectFile.EndsWith(".lproject", StringComparison.OrdinalIgnoreCase))
        {
            throw new BuildException("Open requires a project file. Usage: LuminaBuildTool Open <Path/To/Game.lproject>");
        }

        ProjectFile = Path.GetFullPath(ProjectFile);
        if (!File.Exists(ProjectFile))
        {
            throw new BuildException($"'{ProjectFile}' does not exist.");
        }

        BuildDirectories ProjectDirectories = BuildDirectories.Discover(EngineDirectories.EngineRoot, Path.GetDirectoryName(ProjectFile));
        BuildPlatform PlatformValue = BuildPlatformRegistry.HostPlatform;
        BuildConfiguration ConfigurationValue = Arguments.GetEnum("Configuration", BuildConfiguration.Development);

        RulesAssembly Assembly = RulesAssembly.Create(ProjectDirectories, Arguments.HasFlag("RecompileRules"));

        // A script-only project has no target of its own and needs only the editor, built as the engine's so it lands there.
        string ProjectTargetName = Path.GetFileNameWithoutExtension(ProjectFile);
        bool bHasProjectTarget = Assembly.HasTarget(ProjectTargetName);
        string BuildTargetName = bHasProjectTarget ? ProjectTargetName : EditorTargetName;

        Log.Info("Building {0} before opening {1}...", BuildTargetName, Path.GetFileName(ProjectFile));

        int Result = await BuildMode.BuildTargetsAsync(
            new[] { BuildTargetName },
            Arguments,
            bHasProjectTarget ? ProjectDirectories : EngineDirectories,
            Assembly,
            TargetType.Editor,
            PlatformValue,
            ConfigurationValue,
            Cancellation).ConfigureAwait(false);

        if (Result != 0)
        {
            Log.Error("{0} did not build, so the editor was not opened.", BuildTargetName);
            return Result;
        }

        RunMode.LaunchCommand Command = RunMode.ResolveLaunch(
            Assembly, EngineDirectories, Arguments, EditorTargetName, TargetType.Editor, PlatformValue, ConfigurationValue);
        Command.Arguments.Add($"--Project={ProjectFile}");

        return RunMode.Launch(Command, bWaitForExit: false);
    }
}
