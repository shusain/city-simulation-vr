#include "Editor/CityMetalReflectionAssetsCommandlet.h"
#if WITH_EDITOR
#include "Components/SphereReflectionCaptureComponent.h"
#include "Editor.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "EngineUtils.h"
#include "UObject/SavePackage.h"
#endif

UCityMetalReflectionAssetsCommandlet::UCityMetalReflectionAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCityMetalReflectionAssetsCommandlet::Main(const FString& Params)
{
	return BakeRoomReflection();
}

int32 UCityMetalReflectionAssetsCommandlet::BakeRoomReflection()
{
#if WITH_EDITOR
	const FString Map = TEXT("/Game/CitySim/Maps/CitySandbox");
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(Map);
	if (!World || !GEditor)
		return 1;
	USphereReflectionCaptureComponent* Capture = nullptr;
	for (TActorIterator<ACitySandboxEnvironment> It(World); It; ++It)
	{
		if (Capture)
			return 1;
		Capture = It->FindComponentByClass<USphereReflectionCaptureComponent>();
	}
	if (!Capture || !Capture->Cubemap || Capture->ReflectionSourceType != EReflectionSourceType::SpecifiedCubemap)
		return 1;
	GEditor->BuildReflectionCaptures(World);
	const auto* Data = Capture->GetMapBuildData();
	if (!Data || Data->CubemapSize <= 0 || Data->FullHDRCapturedData.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_ROOM_REFLECTION_BAKE_FAILED"));
		return 1;
	}
	UPackage* BuildPackage = World->PersistentLevel->MapBuildData->GetOutermost();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	const FString Filename =
	    FPackageName::LongPackageNameToFilename(BuildPackage->GetName(), FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(BuildPackage, World->PersistentLevel->MapBuildData, *Filename, Args) ||
	    !UEditorLoadingAndSavingUtils::SaveMap(World, Map))
		return 1;
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_ROOM_REFLECTION_BAKED Size=%d HDRBytes=%d Map=%s"), Data->CubemapSize,
	       Data->FullHDRCapturedData.Num(), *Map);
	return 0;
#else
	return 1;
#endif
}
