#include "Editor/CityEditorAssetLibrary.h"
#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IAssetTools.h"
#include "FileHelpers.h"
#include "ObjectTools.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#endif

bool UCityEditorAssetLibrary::RemoveUnreferencedRedirectors(const TArray<FString>& SourcePackages)
{
#if WITH_EDITOR
	TArray<UObject*> Redirectors;
	auto& RedirectorRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	RedirectorRegistry.ScanPathsSynchronous({TEXT("/Game/CitySim")}, true);
	// Validate every input before invoking the operation. Never force-delete an asset.
	for (const FString& Name : SourcePackages)
	{
		if (!Name.StartsWith(TEXT("/Game/CitySim/")) || !FPackageName::IsValidLongPackageName(Name)) return false;
		const FString RedirectorFilename = FPackageName::LongPackageNameToFilename(Name, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().FileExists(*RedirectorFilename)) continue;
		UPackage* Package = LoadPackage(nullptr, *Name, LOAD_None);
		UE_LOG(LogTemp, Display, TEXT("Reviewed old package lookup: requested=%s loaded=%s"), *Name, *GetPathNameSafe(Package));
		UObjectRedirector* Redirector = Package ? FindObject<UObjectRedirector>(Package, *FPackageName::GetLongPackageAssetName(Name)) : nullptr;
		if (!Redirector)
		{
			UE_LOG(LogTemp, Error, TEXT("Existing old package is not a redirector: %s"), *Name);
			return false;
		}
		TArray<FName> RedirectorReferencers;
		RedirectorRegistry.GetReferencers(FName(*Name), RedirectorReferencers);
		if (!RedirectorReferencers.IsEmpty())
		{
			for (FName Referencer : RedirectorReferencers)
				UE_LOG(LogTemp, Error, TEXT("Redirector %s still referenced by %s"), *Name, *Referencer.ToString());
			return false;
		}
		Redirectors.AddUnique(Redirector);
	}
	if (!Redirectors.IsEmpty())
	{
		// RenameReferencingSoftObjectPaths and dependent saves have already
		// repaired references. FixupReferencers opens a modal report in5.7;
		// use its normal deletion primitive only after zero-reference checks.
		UE_LOG(LogTemp, Display, TEXT("Removing %d reviewed unreferenced redirectors"), Redirectors.Num());
		for (UObject* Redirector : Redirectors) Redirector->RemoveFromRoot();
		const int32 RedirectorCount = Redirectors.Num();
		if (ObjectTools::DeleteObjects(Redirectors, false) != RedirectorCount) return false;
	}
	for (const FString& Name : SourcePackages)
	{
		const FString RedirectorFilename = FPackageName::LongPackageNameToFilename(Name, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().FileExists(*RedirectorFilename))
		{
			UE_LOG(LogTemp, Error, TEXT("Redirector remains after fixup: %s"), *Name);
			return false;
		}
	}
	return true;
#else
	return false;
#endif
}

bool UCityEditorAssetLibrary::RepointImportedSource(UObject* Asset, const FString& Filename, int32 Index)
{
#if WITH_EDITOR
	if (!Asset || Index < 0 || !IFileManager::Get().FileExists(*Filename)) return false;
	const FObjectProperty* Property = FindFProperty<FObjectProperty>(Asset->GetClass(), TEXT("AssetImportData"));
	UAssetImportData* Data = Property ? Cast<UAssetImportData>(Property->GetObjectPropertyValue_InContainer(Asset)) : nullptr;
	if (!Data || Index >= Data->GetSourceFileCount()) return false;
	FString ImportRelativeFilename = FPaths::ConvertRelativePathToFull(Filename);
	const FString ImportPackageFilename = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(
		Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()));
	if (!FPaths::MakePathRelativeTo(ImportRelativeFilename, *ImportPackageFilename) || !FPaths::IsRelative(ImportRelativeFilename)) return false;
	Asset->Modify();
	Data->Modify();
	// UpdateFilenameOnly sanitizes sources outside ProjectDir relative to the
	// Engine directory, embedding this CI workspace. ResolveImportFilename also
	// supports package-relative paths. Change only that field, preserving even
	// unknown original MD5/timestamps rather than recalculating source history.
	Data->SourceData.SourceFiles[Index].RelativeFilename = ImportRelativeFilename;
	Asset->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

TArray<FString> UCityEditorAssetLibrary::GetImportedSourceFiles(UObject* Asset)
{
#if WITH_EDITOR
	if (!Asset) return {};
	const FObjectProperty* Property = FindFProperty<FObjectProperty>(Asset->GetClass(), TEXT("AssetImportData"));
	const UAssetImportData* Data = Property ? Cast<UAssetImportData>(Property->GetObjectPropertyValue_InContainer(Asset)) : nullptr;
	return Data ? Data->ExtractFilenames() : TArray<FString>();
#else
	return {};
#endif
}

bool UCityEditorAssetLibrary::RenameAssetPreservingRedirector(UObject* Asset, const FString& DestinationPackage,
    const TArray<UObject*>& ReferencingAssets)
{
#if WITH_EDITOR
	if (!Asset || !Asset->IsAsset() ||
	    !Asset->GetOutermost()->GetName().StartsWith(TEXT("/Game/CitySim/")) ||
	    !DestinationPackage.StartsWith(TEXT("/Game/CitySim/")) || !FPackageName::IsValidLongPackageName(DestinationPackage) ||
	    FPackageName::DoesPackageExist(DestinationPackage)) return false;
	const FString AssetOldPath = Asset->GetPathName();
	UPackage* AssetOldPackage = Asset->GetOutermost();
	ObjectTools::FPackageGroupName AssetTarget;
	AssetTarget.PackageName = DestinationPackage;
	AssetTarget.ObjectName = FPackageName::GetLongPackageAssetName(DestinationPackage);
	TSet<UPackage*> AssetRefusedPackages;
	FText AssetRenameError;
	// This is the same engine operation used by AssetTools. Retain the old
	// redirector explicitly for assets held by native defaults/dependencies.
	// Keep redirectors, registry notifications, metadata/thumbnail transfer and
	// reference fixup. Never change unattended flags or dismiss arbitrary dialogs.
	if (!ObjectTools::RenameSingleObject(Asset, AssetTarget, AssetRefusedPackages, AssetRenameError, nullptr, true))
	{
		UE_LOG(LogTemp, Error, TEXT("Reviewed asset rename failed: %s"), *AssetRenameError.ToString());
		return false;
	}
	TArray<UPackage*> AssetPackages{AssetOldPackage, Asset->GetOutermost()};
	for (UObject* Referencer : ReferencingAssets)
	{
		if (Referencer && Referencer->GetOutermost()->GetName().StartsWith(TEXT("/Game/CitySim/")))
			AssetPackages.AddUnique(Referencer->GetOutermost());
	}
	TMap<FSoftObjectPath, FSoftObjectPath> AssetPathMap;
	AssetPathMap.Add(FSoftObjectPath(AssetOldPath), FSoftObjectPath(Asset));
	FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameReferencingSoftObjectPaths(AssetPackages, AssetPathMap);
	return UEditorLoadingAndSavingUtils::SavePackages(AssetPackages, true);
#else
	return false;
#endif
}
