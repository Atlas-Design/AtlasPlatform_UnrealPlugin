#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AtlasLibraryMigrationCommandlet.generated.h"

/** One-time, explicit migration of the shipped workflow dropdown. Never runs on editor startup. */
UCLASS()
class UAtlasLibraryMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
