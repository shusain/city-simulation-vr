#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CityEditorAssetLibrary.generated.h"

/** Editor-only operations missing from the engine's Python asset API. */
UCLASS()
class CITYSIMVR_API UCityEditorAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Remove only verified CitySim redirectors with no remaining package referencers. */
	UFUNCTION(BlueprintCallable, Category = "CitySim|Editor", meta = (DevelopmentOnly))
	static bool RemoveUnreferencedRedirectors(const TArray<FString>& SourcePackages);
	/** Store a package-relative import filename, retaining its recorded hash/timestamp. */
	UFUNCTION(BlueprintCallable, Category = "CitySim|Editor", meta = (DevelopmentOnly))
	static bool RepointImportedSource(UObject* Asset, const FString& Filename, int32 Index);
	UFUNCTION(BlueprintCallable, Category = "CitySim|Editor", meta = (DevelopmentOnly))
	static TArray<FString> GetImportedSourceFiles(UObject* Asset);
	/** Explicit move retaining a redirector; caller must update C++ paths, fix references and validate the cook. */
	UFUNCTION(BlueprintCallable, Category = "CitySim|Editor", meta = (DevelopmentOnly))
	static bool RenameAssetPreservingRedirector(UObject* Asset, const FString& DestinationPackage, const TArray<UObject*>& ReferencingAssets);
};
