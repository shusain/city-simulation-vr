#include "Editor/CityAlarmAssetsCommandlet.h"
#if WITH_EDITOR
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundSource.h"
#include "Editor.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#endif
UCityAlarmAssetsCommandlet::UCityAlarmAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}
int32 UCityAlarmAssetsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	EMetaSoundBuilderResult Result;
	FMetaSoundBuilderNodeOutputHandle Play;
	FMetaSoundBuilderNodeInputHandle Finished;
	TArray<FMetaSoundBuilderNodeInputHandle> Audio;
	auto* Builder = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>()->CreateSourceBuilder(
	    TEXT("CityAlarmAuthor"), Play, Finished, Audio, Result);
	if (!Builder || Result != EMetaSoundBuilderResult::Succeeded || Audio.Num() != 1)
		return 1;
	bool Valid = true;
	auto Add = [&](const TCHAR* Namespace, const TCHAR* Name, const TCHAR* Variant)
	{
		auto Node = Builder->AddNodeByClassName({Namespace, Name, Variant}, Result);
		Valid &= Result == EMetaSoundBuilderResult::Succeeded;
		return Node;
	};
	auto Set = [&](const FMetaSoundNodeHandle& Node, const TCHAR* Pin, float Value)
	{
		Builder->SetNodeInputDefault(Node, Pin, Value, Result);
		Valid &= Result == EMetaSoundBuilderResult::Succeeded;
	};
	auto Connect = [&](const FMetaSoundNodeHandle& A, const TCHAR* Out, const FMetaSoundNodeHandle& B, const TCHAR* In)
	{
		Builder->ConnectNodes(A, Out, B, In, Result);
		Valid &= Result == EMetaSoundBuilderResult::Succeeded;
	};
	const auto LFO = Add(TEXT("UE"), TEXT("Sine"), TEXT("Audio"));
	Set(LFO, TEXT("Frequency"), .4f);
	const auto Sweep = Add(TEXT("UE"), TEXT("Multiply"), TEXT("Audio by Float"));
	Set(Sweep, TEXT("AdditionalOperands"), 180.f);
	Connect(LFO, TEXT("Audio"), Sweep, TEXT("PrimaryOperand"));
	const auto Tone = Add(TEXT("UE"), TEXT("Sine"), TEXT("Audio"));
	Set(Tone, TEXT("Frequency"), 550.f);
	Connect(Sweep, TEXT("Out"), Tone, TEXT("Modulation"));
	const auto Envelope = Add(TEXT("AD Envelope"), TEXT("AD Envelope"), TEXT("Audio"));
	Set(Envelope, TEXT("Attack Time"), .3f);
	Set(Envelope, TEXT("Decay Time"), 4.7f);
	Set(Envelope, TEXT("Decay Curve"), .35f);
	auto In = Builder->FindNodeInputByName(Envelope, TEXT("Trigger"), Result);
	Valid &= Result == EMetaSoundBuilderResult::Succeeded;
	Builder->ConnectNodes(Play, In, Result);
	Valid &= Result == EMetaSoundBuilderResult::Succeeded;
	const auto Gate = Add(TEXT("UE"), TEXT("Multiply"), TEXT("Audio"));
	Connect(Tone, TEXT("Audio"), Gate, TEXT("PrimaryOperand"));
	Connect(Envelope, TEXT("Out Envelope"), Gate, TEXT("AdditionalOperands"));
	const auto Gain = Add(TEXT("UE"), TEXT("Multiply"), TEXT("Audio by Float"));
	Set(Gain, TEXT("AdditionalOperands"), .4f);
	Connect(Gate, TEXT("Out"), Gain, TEXT("PrimaryOperand"));
	Builder->ConnectNodeToGraphOutput(Gain, TEXT("Out"), Audio[0], Result);
	Valid &= Result == EMetaSoundBuilderResult::Succeeded;
	Builder->ConnectNodeToGraphOutput(Envelope, TEXT("On Done"), Finished, Result);
	Valid &= Result == EMetaSoundBuilderResult::Succeeded;
	if (!Valid)
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_ALARM_GRAPH_FAILED"));
		return 1;
	}
	auto Asset = GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>()->BuildToAsset(
	    Builder, TEXT("City Sim VR"), TEXT("MS_CityDisasterAlarm"), TEXT("/Game/CitySim/Audio"), Result);
	auto* Source = Cast<UMetaSoundSource>(Asset.GetObject());
	if (!Source || Result != EMetaSoundBuilderResult::Succeeded)
		return 1;
	Source->Volume = 1.f;
	Source->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	const FString Filename = FPackageName::LongPackageNameToFilename(Source->GetOutermost()->GetName(),
	                                                                 FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(Source->GetOutermost(), Source, *Filename, Args))
		return 1;
	UE_LOG(LogTemp, Display,
	       TEXT("CITYSIM_ALARM_ASSET_SAVED Built-in MetaSound oscillators/envelope, 5 seconds, peak gain 0.4"));
	return 0;
#else
	return 1;
#endif
}
