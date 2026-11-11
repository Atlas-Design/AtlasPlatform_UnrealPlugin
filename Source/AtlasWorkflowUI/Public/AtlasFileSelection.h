#pragma once
#include "CoreMinimal.h"
#include "Types/AtlasSchemaTypes.h"
#include "AtlasFileSelection.generated.h"

UENUM(BlueprintType)
enum class EAtlasFileSource : uint8 { File, UnrealAsset, AtlasId };
USTRUCT(BlueprintType)
struct ATLASWORKFLOWUI_API FAtlasFileChoice
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EAtlasFileSource Source=EAtlasFileSource::File;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FAtlasFileReference File;
    UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite) TObjectPtr<UObject> Asset;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAtlasSelectionChanged);

/** Selection only: rendering, validation and mutation never export/import/upload. */
UCLASS(BlueprintType)
class ATLASWORKFLOWUI_API UAtlasFileSelection : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly) FAtlasParameterDef Definition;
    UPROPERTY(BlueprintReadOnly) TArray<FAtlasFileChoice> Items;
    UPROPERTY(BlueprintReadOnly) FString Error;
    UPROPERTY(BlueprintReadOnly) FString ServiceOrigin;
    UPROPERTY(BlueprintReadOnly) bool bMutationEnabled=true;
    UPROPERTY(BlueprintAssignable) FAtlasSelectionChanged OnChanged;
    int64 Revision=0;
    void Configure(const FAtlasParameterDef& Def,const FAtlasValue& Value,const FString& Origin);
    void SetMutationEnabled(bool Enabled);
    bool ValidateChoice(const FAtlasFileChoice& Choice,FString& OutError) const;
    bool Validate(FString& OutError) const;
    bool ResolveDrop(const TArray<FString>& Paths,const TArray<UObject*>& Assets,bool bSingle,TArray<FAtlasFileChoice>& Out,FString& OutError) const;
    bool Apply(const TArray<FAtlasFileChoice>& Choices,const FGuid& ReplaceId,int64 ExpectedRevision);
    bool Remove(const FGuid& Id);
    bool Move(const FGuid& Id,int32 To);
    int32 Find(const FGuid& Id) const;
    bool Prepare(FAtlasValue& Out,FString& OutError) const;
    FAtlasValue GetReferenceValue() const;
    FString SourceDescription(const FAtlasFileChoice& Choice) const;
    static bool UsesOriginalSource(const FAtlasParameterDef& Def);
    static bool CanSelectEngineAsset(const FAtlasParameterDef& Def);
private:
    void Changed();
};
