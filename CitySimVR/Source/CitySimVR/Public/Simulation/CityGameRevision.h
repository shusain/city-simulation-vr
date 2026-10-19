#pragma once
#include "CoreMinimal.h"

// Build.cs fingerprints native source, including uncommitted changes.
struct CITYSIMVR_API FCityGameRevision
{
	static FString Current();
	static bool IsValid(const FString& Value)
	{
		if (Value.IsEmpty() || Value.Len() > 64)
			return false;
		for (const TCHAR C : Value)
			if (!(C >= 'a' && C <= 'z') && !(C >= 'A' && C <= 'Z') && !(C >= '0' && C <= '9') && C != '-' &&
			    C != '_' && C != '.')
				return false;
		return true;
	}
};
