#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyFileHelper.generated.h"

UCLASS()
class UMyFileHelper : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    public:
    // 블루프린트에서 부를 수 있게 노출!
    UFUNCTION(BlueprintCallable, Category = "MyTools")
    static bool SaveStringToFile(FString SaveDirectory, FString FileName, FString SaveText, bool AllowOverwriting = true);
};