#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AtlasWidgetMigrationCommandlet.generated.h"

/** Explicit inspection/migration tool. Never runs automatically on editor startup. */
UCLASS()
class UAtlasWidgetMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
