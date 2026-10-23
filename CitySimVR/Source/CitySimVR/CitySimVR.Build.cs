using UnrealBuildTool;
using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

public class CitySimVR : ModuleRules
{
	public CitySimVR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		RuntimeDependencies.Add("$(ProjectDir)/AudioCredits.txt", StagedFileType.NonUFS);

		// Fingerprint native source and runtime assets, including uncommitted work.
		// The same inputs identify Editor, Windows and Quest builds consistently.
		string projectDirectory = Path.GetFullPath(Path.Combine(ModuleDirectory, "../.."));
		var revisionFiles = new System.Collections.Generic.List<string>();
		revisionFiles.AddRange(Directory.GetFiles(Path.Combine(projectDirectory, "Source"), "*", SearchOption.AllDirectories));
		revisionFiles.AddRange(Directory.GetFiles(Path.Combine(projectDirectory, "Content/CitySim"), "*", SearchOption.AllDirectories));
		revisionFiles.Sort(StringComparer.Ordinal);
		using (var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256))
		{
			byte[] buffer = new byte[65536];
			foreach (string file in revisionFiles)
			{
				string extension = Path.GetExtension(file);
				if (extension != ".h" && extension != ".cpp" && extension != ".cs" && extension != ".uasset" && extension != ".umap") continue;
				ExternalDependencies.Add(file);
				using (var input = File.OpenRead(file))
				{
					byte[] name = Encoding.UTF8.GetBytes(Path.GetRelativePath(projectDirectory, file).Replace('\\', '/') + "\0" + input.Length.ToString(System.Globalization.CultureInfo.InvariantCulture) + "\0");
					hash.AppendData(name);
					int count;
					while ((count = input.Read(buffer, 0, buffer.Length)) > 0) hash.AppendData(buffer, 0, count);
				}
			}
			string revision = "dev-" + BitConverter.ToString(hash.GetHashAndReset()).Replace("-", "").Substring(0, 20).ToLowerInvariant();
			// Only the revision translation unit depends on this generated header.
			string revisionDirectory = Path.Combine(projectDirectory, "Intermediate/CityGameRevision");
			Directory.CreateDirectory(revisionDirectory);
			string revisionHeader = Path.Combine(revisionDirectory, "CityGameRevisionBuild.h");
			string revisionContent = "#pragma once\n#define CITYSIM_GAME_REVISION \"" + revision + "\"\n";
			if (!File.Exists(revisionHeader) || File.ReadAllText(revisionHeader) != revisionContent)
				File.WriteAllText(revisionHeader, revisionContent, new UTF8Encoding(false));
			PrivateIncludePaths.Add(revisionDirectory);
		}

		// Experimental executables are isolated benchmark tools, never release or
		// Android/Editor targets. This is not a player-facing save-schema change.
		int capacityGridSize = 64;
		string requestedGrid = System.Environment.GetEnvironmentVariable("CITYSIM_CAPACITY_GRID_SIZE");
		if (Target.Platform == UnrealTargetPlatform.Win64 && !Target.bBuildEditor &&
			Target.Configuration == UnrealTargetConfiguration.Development && !string.IsNullOrEmpty(requestedGrid))
		{
			if (!int.TryParse(requestedGrid, out capacityGridSize) ||
				(capacityGridSize != 64 && capacityGridSize != 80 && capacityGridSize != 96 &&
				 capacityGridSize != 112 && capacityGridSize != 128 && capacityGridSize != 160 && capacityGridSize != 192))
				throw new BuildException("Unsupported isolated capacity grid size.");
		}
		PublicDefinitions.Add("CITYSIM_CAPACITY_GRID_SIZE=" + capacityGridSize);

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"EnhancedInput",
			"Engine",
			"HeadMountedDisplay",
			"InputCore",
			"UMG",
			"Slate",
			"SlateCore",
			"Json",
			"NavigationSystem",
			"MetasoundEngine",
			"MetasoundFrontend",
			"OculusXRHMD",
			"ProceduralMeshComponent",
			"RenderCore",
			"RHI"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"AssetRegistry",
				"AssetTools",
				"BlueprintGraph",
				"MetasoundEditor",
				"UnrealEd",
				"UMGEditor"
			});
		}
	}
}
