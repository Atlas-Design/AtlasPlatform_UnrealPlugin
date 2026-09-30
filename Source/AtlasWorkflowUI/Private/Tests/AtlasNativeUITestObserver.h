#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/AtlasHistoryTypes.h"
#include "AtlasNativeUITestObserver.generated.h"
UCLASS()
class UAtlasNativeUITestObserver : public UObject
{
    GENERATED_BODY()
public:
    int32 Changes = 0;
    UFUNCTION() void QueryChanged(FAtlasHistoryQuery Query) { ++Changes; }
    UFUNCTION() void Changed() { ++Changes; }
};
