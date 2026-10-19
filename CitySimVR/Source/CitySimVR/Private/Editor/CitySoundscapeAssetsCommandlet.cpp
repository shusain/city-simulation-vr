#include "Editor/CitySoundscapeAssetsCommandlet.h"
#include "Presentation/CitySoundscapeAssets.h"
#if WITH_EDITOR
#include "Editor.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundSource.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeDoppler.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Misc/Parse.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#endif

UCitySoundscapeAssetsCommandlet::UCitySoundscapeAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}
int32 UCitySoundscapeAssetsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	TArray<TSharedPtr<FJsonValue>> Records;
	const bool TrafficOnly = FParse::Param(*Params, TEXT("TrafficOnly"));
	for (const auto& Entry : CityAudioAssets)
	{
		const bool Traffic = FString(Entry.GraphName).StartsWith(TEXT("SC_"));
		if (TrafficOnly && !Traffic)
			continue;
		auto* Wave = LoadObject<USoundWave>(nullptr, Entry.WavePackage);
		if (!Wave || Wave->NumChannels != 1 || !FMath::IsNearlyEqual(Wave->Duration, Entry.Seconds, .001f) ||
		    Wave->bLooping != Entry.bLoop || Wave->Volume != 1.f || Wave->Pitch != 1.f)
			return 1;
		if (Traffic)
		{
			const FString PackageName = FString(CityAudioGraphFolder) / Entry.GraphName;
			// Focused new packages only; never rewrite the50 accepted MetaSounds.
			if (FPackageName::DoesPackageExist(PackageName))
				return 1;
			auto* Package = CreatePackage(*PackageName);
			auto* Cue = NewObject<USoundCue>(Package, Entry.GraphName, RF_Public | RF_Standalone);
			auto* Doppler = NewObject<USoundNodeDoppler>(Cue, NAME_None, RF_Transactional);
			auto* Player = NewObject<USoundNodeWavePlayer>(Cue, NAME_None, RF_Transactional);
			Player->SetSoundWave(Wave);
			Player->bLooping = true;
			Doppler->DopplerIntensity = 1.f;
			Doppler->bUseSmoothing = true;
			Doppler->SmoothingInterpSpeed = 5.f;
			Doppler->ChildNodes = {Player};
			Cue->AllNodes = {Doppler, Player};
			Cue->SetupSoundNode(Doppler, false);
			Cue->SetupSoundNode(Player, false);
			Cue->FirstNode = Doppler;
			Cue->VolumeMultiplier = 1.f;
			Cue->PitchMultiplier = 1.f;
			Cue->LinkGraphNodesFromSoundNodes();
			Cue->CacheAggregateValues();
			Package->MarkPackageDirty();
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.SaveFlags = SAVE_NoError;
			const FString Filename =
			    FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			if (!UPackage::SavePackage(Package, Cue, *Filename, Args))
				return 1;
			auto Record = MakeShared<FJsonObject>();
			Record->SetStringField(TEXT("id"), Entry.Id);
			Record->SetStringField(TEXT("graph"), Cue->GetPathName());
			Record->SetBoolField(TEXT("loop"), true);
			Record->SetStringField(TEXT("motion"),
			                       TEXT("Native smoothed SoundNodeDoppler; actual emitter/listener velocity"));
			Records.Add(MakeShared<FJsonValueObject>(Record));
			UE_LOG(LogTemp, Display, TEXT("SOUNDSCAPE_TRAFFIC_CUE_PASS %s"), Entry.Id);
			continue;
		}
		EMetaSoundBuilderResult Result;
		FMetaSoundBuilderNodeOutputHandle Play;
		FMetaSoundBuilderNodeInputHandle Finished;
		TArray<FMetaSoundBuilderNodeInputHandle> Audio;
		auto* Builder = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>()->CreateSourceBuilder(
		    FName(Entry.GraphName), Play, Finished, Audio, Result);
		if (!Builder || Result != EMetaSoundBuilderResult::Succeeded || Audio.Num() != 1)
			return 1;
		bool Valid = true;
		auto Check = [&]() { Valid &= Result == EMetaSoundBuilderResult::Succeeded; };
		const auto Player = Builder->AddNodeByClassName({TEXT("UE"), TEXT("Wave Player"), TEXT("Mono")}, Result);
		Check();
		Builder->SetNodeInputDefault(Player, TEXT("Wave Asset"), static_cast<UObject*>(Wave), Result);
		Check();
		Builder->SetNodeInputDefault(Player, TEXT("Loop"), Entry.bLoop, Result);
		Check();
		Builder->SetNodeInputDefault(Player, TEXT("Loop Start"), 0.f, Result);
		Check();
		Builder->SetNodeInputDefault(Player, TEXT("Loop Duration"), -1.f, Result);
		Check();
		const auto Start = Builder->FindNodeInputByName(Player, TEXT("Play"), Result);
		Check();
		Builder->ConnectNodes(Play, Start, Result);
		Check();
		Builder->ConnectNodeToGraphOutput(Player, TEXT("Out Mono"), Audio[0], Result);
		Check();
		// Stop/interrupt never schedules another phase. Runtime eligibility owns transitions.
		if (!Entry.bLoop)
		{
			Builder->ConnectNodeToGraphOutput(Player, TEXT("On Finished"), Finished, Result);
			Check();
		}
		if (!Valid)
		{
			UE_LOG(LogTemp, Error, TEXT("SOUNDSCAPE_GRAPH_FAILED %s"), Entry.Id);
			return 1;
		}
		const auto Asset = GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>()->BuildToAsset(
		    Builder, TEXT("City Sim VR"), Entry.GraphName, CityAudioGraphFolder, Result);
		auto* Source = Cast<UMetaSoundSource>(Asset.GetObject());
		if (!Source || Result != EMetaSoundBuilderResult::Succeeded)
			return 1;
		Source->Volume = 1.f;
		Source->Pitch = 1.f;
		Source->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const auto Filename = FPackageName::LongPackageNameToFilename(Source->GetOutermost()->GetName(),
		                                                              FPackageName::GetAssetPackageExtension());
		if (!UPackage::SavePackage(Source->GetOutermost(), Source, *Filename, Args))
			return 1;
		auto Record = MakeShared<FJsonObject>();
		Record->SetStringField(TEXT("id"), Entry.Id);
		Record->SetStringField(TEXT("graph"), Source->GetPathName());
		Record->SetBoolField(TEXT("loop"), Entry.bLoop);
		Record->SetNumberField(TEXT("seconds"), Entry.Seconds);
		Records.Add(MakeShared<FJsonValueObject>(Record));
		UE_LOG(LogTemp, Display, TEXT("SOUNDSCAPE_METASOUND_GRAPH_PASS %s"), Entry.Id);
	}
	auto Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("status"), TEXT("Pass"));
	Report->SetArrayField(TEXT("assets"), Records);
	FString Json;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
	if (!FFileHelper::SaveStringToFile(
	        Json, *(FPaths::ProjectDir() / (TrafficOnly ? TEXT("../Artifacts/Soundscape/TrafficAlarmRefinement/Graphs.json")
	                                                    : TEXT("../Artifacts/Soundscape/Integration/Graphs.json")))))
		return 1;
	UE_LOG(LogTemp, Display, TEXT("SOUNDSCAPE_AUDIO_GRAPH_PASS %d graphs; explicit whole-wave loops; Music preserved"),
	       Records.Num());
	return 0;
#else
	return 1;
#endif
}
