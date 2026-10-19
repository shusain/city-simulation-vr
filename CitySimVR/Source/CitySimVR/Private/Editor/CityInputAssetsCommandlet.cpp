#include "Editor/CityInputAssetsCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

UCityInputAssetsCommandlet::UCityInputAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCityInputAssetsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	auto SaveAsset = [](UObject* Asset) -> bool
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename =
		    FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
	};

	auto CreateAction = [&SaveAsset](const TCHAR* PackagePath, const TCHAR* AssetName) -> UInputAction*
	{
		UPackage* Package = CreatePackage(PackagePath);
		UInputAction* Action = FindObject<UInputAction>(Package, AssetName);
		if (!Action)
		{
			Action = NewObject<UInputAction>(Package, AssetName, RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Action);
		}
		Action->ValueType = EInputActionValueType::Boolean;
		Action->bConsumeInput = true;
		return SaveAsset(Action) ? Action : nullptr;
	};

	UInputAction* LeftPrimary =
	    CreateAction(TEXT("/Game/CitySim/Input/IA_City_LeftPrimary"), TEXT("IA_City_LeftPrimary"));
	UInputAction* LeftSecondary =
	    CreateAction(TEXT("/Game/CitySim/Input/IA_City_LeftSecondary"), TEXT("IA_City_LeftSecondary"));
	UInputAction* RightPrimary =
	    CreateAction(TEXT("/Game/CitySim/Input/IA_City_RightPrimary"), TEXT("IA_City_RightPrimary"));
	UInputAction* RightSecondary =
	    CreateAction(TEXT("/Game/CitySim/Input/IA_City_RightSecondary"), TEXT("IA_City_RightSecondary"));
	if (!LeftPrimary || !LeftSecondary || !RightPrimary || !RightSecondary)
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_INPUT_ASSET_ERROR ActionSaveFailed"));
		return 1;
	}

	static const TCHAR* ContextPackagePath = TEXT("/Game/CitySim/Input/IMC_CityFaceButtons");
	static const TCHAR* ContextAssetName = TEXT("IMC_CityFaceButtons");
	UPackage* ContextPackage = CreatePackage(ContextPackagePath);
	UInputMappingContext* Context = FindObject<UInputMappingContext>(ContextPackage, ContextAssetName);
	if (!Context)
	{
		Context = NewObject<UInputMappingContext>(ContextPackage, ContextAssetName, RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Context);
	}
	Context->UnmapAll();
	Context->MapKey(LeftPrimary, EKeys::OculusTouch_Left_X_Click);
	Context->MapKey(LeftSecondary, EKeys::OculusTouch_Left_Y_Click);
	Context->MapKey(RightPrimary, EKeys::OculusTouch_Right_A_Click);
	Context->MapKey(RightSecondary, EKeys::OculusTouch_Right_B_Click);
	if (!SaveAsset(Context))
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_INPUT_ASSET_ERROR ContextSaveFailed"));
		return 2;
	}

	UE_LOG(LogTemp, Display, TEXT("CITYSIM_INPUT_ASSETS_CREATED Context=%s Mappings=%d"), ContextPackagePath,
	       Context->GetMappings().Num());
	return 0;
#else
	UE_LOG(LogTemp, Error, TEXT("CITYSIM_INPUT_ASSET_ERROR EditorOnly"));
	return 1;
#endif
}
