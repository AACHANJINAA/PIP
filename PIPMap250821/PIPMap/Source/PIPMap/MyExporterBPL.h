// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MyExporterBPL.generated.h"

/**
 *
 */
UCLASS()
class PIPMAP_API UMyExporterBPL : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // 이 함수를 블루프린트에서 호출할 수 있게 만듭니다.
    UFUNCTION(BlueprintCallable, Category = "My Export Tools")
    static void ExportServerData(UObject* WorldContextObject);

    UFUNCTION(BlueprintCallable, Category = "My Export Tools")
    static void ExportClientData(UObject* WorldContextObject);
};
