#include "Editor/CityXRPawnConfiguratorCommandlet.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#endif

UCityXRPawnConfiguratorCommandlet::UCityXRPawnConfiguratorCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCityXRPawnConfiguratorCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	static const TCHAR* PawnObjectPath = TEXT("/Game/CitySim/XR/BP_CityXRPawn.BP_CityXRPawn");
	static const FName TeleportGraphName(TEXT("TeleportTrace"));
	static const FName LaunchSpeedVariableName(TEXT("LocalTeleportLaunchSpeed"));
	static const FString LaunchSpeedValue(TEXT("1800.000000"));

	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, PawnObjectPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_XR_CONFIG_ERROR MissingAsset=%s"), PawnObjectPath);
		return 1;
	}

	UEdGraph* TeleportGraph = nullptr;
	for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
	{
		if (FunctionGraph && FunctionGraph->GetFName() == TeleportGraphName)
		{
			TeleportGraph = FunctionGraph;
			break;
		}
	}

	if (!TeleportGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_XR_CONFIG_ERROR MissingGraph=%s"), *TeleportGraphName.ToString());
		return 2;
	}

	UK2Node_FunctionEntry* FunctionEntry = nullptr;
	for (UEdGraphNode* Node : TeleportGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* Candidate = Cast<UK2Node_FunctionEntry>(Node))
		{
			FunctionEntry = Candidate;
			break;
		}
	}

	if (!FunctionEntry)
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_XR_CONFIG_ERROR MissingFunctionEntry"));
		return 3;
	}

	bool bUpdated = false;
	for (FBPVariableDescription& Variable : FunctionEntry->LocalVariables)
	{
		if (Variable.VarName == LaunchSpeedVariableName)
		{
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_XR_TELEPORT_SPEED Previous=%s Current=%s"), *Variable.DefaultValue,
			       *LaunchSpeedValue);
			Variable.DefaultValue = LaunchSpeedValue;
			bUpdated = true;
			break;
		}
	}

	if (!bUpdated)
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_XR_CONFIG_ERROR MissingVariable=%s"), *LaunchSpeedVariableName.ToString());
		return 4;
	}

	Blueprint->Modify();
	FunctionEntry->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_XR_CONFIG_ERROR CompileFailed"));
		return 5;
	}

	UPackage* Package = Blueprint->GetOutermost();
	Package->MarkPackageDirty();
	const FString Filename =
	    FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs))
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_XR_CONFIG_ERROR SaveFailed=%s"), *Filename);
		return 6;
	}

	UE_LOG(LogTemp, Display, TEXT("CITYSIM_XR_PAWN_CONFIGURED Asset=%s"), PawnObjectPath);
	return 0;
#else
	UE_LOG(LogTemp, Error, TEXT("CITYSIM_XR_CONFIG_ERROR EditorOnly"));
	return 1;
#endif
}
