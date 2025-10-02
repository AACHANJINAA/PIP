#include "MyExporterBPL.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Exporters/Exporter.h"         // CJ설명 : UExporter 사용을 위해 추가
#include "HAL/PlatformFileManager.h"    // CJ설명 : 파일/디렉토리 관리를 위해 추가
#endif

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h" // UTexture2D로 캐스팅하기 위해 추가
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"


// FVector를 { "X": 값, "Y": 값, "Z": 값 } 형태의 FJsonObject로 변환하는 헬퍼 함수
TSharedPtr<FJsonObject> VectorToJsonObject(const FVector& InVector)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetNumberField(TEXT("X"), InVector.X);
    JsonObject->SetNumberField(TEXT("Y"), InVector.Y);
    JsonObject->SetNumberField(TEXT("Z"), InVector.Z);
    return JsonObject;
}

// FRotator를 { "Pitch": 값, "Yaw": 값, "Roll": 값 } 형태의 FJsonObject로 변환하는 헬퍼 함수
TSharedPtr<FJsonObject> RotatorToJsonObject(const FRotator& InRotator)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
    JsonObject->SetNumberField(TEXT("Pitch"), InRotator.Pitch);
    JsonObject->SetNumberField(TEXT("Yaw"), InRotator.Yaw);
    JsonObject->SetNumberField(TEXT("Roll"), InRotator.Roll);
    return JsonObject;
}

void UMyExporterBPL::ExportServerData(UObject* WorldContextObject)
{
#if WITH_EDITOR
    TArray<TSharedPtr<FJsonValue>> ActorJsonArray;

    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    TArray<AActor*> FoundActors;
    if (EditorActorSubsystem)
    {
        FoundActors = EditorActorSubsystem->GetAllLevelActors();
    }

    for (AActor* Actor : FoundActors)
    {
        UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
        if (!MeshComponent || !MeshComponent->GetStaticMesh())
        {
            continue;
        }

        TSharedPtr<FJsonObject> ActorJsonObject = MakeShareable(new FJsonObject());
        ActorJsonObject->SetStringField(TEXT("Name"), Actor->GetActorLabel());
        ActorJsonObject->SetStringField(TEXT("Mesh"), MeshComponent->GetStaticMesh()->GetName());

        // 위치 정보 변환 및 저장
        FVector UnrealLocation = MeshComponent->GetComponentLocation() / 100.0f;
        FVector ExportedLocation(UnrealLocation.X, UnrealLocation.Y, UnrealLocation.Z);
        ActorJsonObject->SetObjectField(TEXT("Location"), VectorToJsonObject(ExportedLocation));

        // AABB 정보 변환 및 저장
        FBox WorldBounds = MeshComponent->Bounds.GetBox();
        FVector UnrealMin = WorldBounds.Min / 100.0f;
        FVector UnrealMax = WorldBounds.Max / 100.0f;
        FVector ExportedMin(UnrealMin.X, UnrealMin.Y, UnrealMin.Z);
        FVector ExportedMax(UnrealMax.X, UnrealMax.Y, UnrealMax.Z);

        TSharedPtr<FJsonObject> AABBObject = MakeShareable(new FJsonObject());
        AABBObject->SetObjectField(TEXT("Min"), VectorToJsonObject(ExportedMin));
        AABBObject->SetObjectField(TEXT("Max"), VectorToJsonObject(ExportedMax));
        ActorJsonObject->SetObjectField(TEXT("AABB"), AABBObject);

        ActorJsonArray.Add(MakeShareable(new FJsonValueObject(ActorJsonObject)));
    }

    // 파일 저장
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(ActorJsonArray, Writer);
    FString FilePath = FPaths::ProjectSavedDir() + TEXT("MapData/ExportedServerData.json");
    FFileHelper::SaveStringToFile(OutputString, *FilePath);
    UE_LOG(LogTemp, Warning, TEXT("Server data export complete! File saved to: %s"), *FilePath);
#else
    UE_LOG(LogTemp, Error, TEXT("ExportServerData can only be called from the editor."));
#endif
}

void UMyExporterBPL::ExportClientData(UObject* WorldContextObject)
{
#if WITH_EDITOR
    TArray<TSharedPtr<FJsonValue>> ActorJsonArray;

    // CJ설명 : DDS 파일을 저장할 기본 디렉토리 경로 설정
    const FString BaseExportDir = FPaths::ProjectSavedDir() + TEXT("MapData/");
    const FString TextureExportDir = BaseExportDir + TEXT("Textures/");

    // CJ설명 : TextureExportDir가 존재하는지 확인하고, 없으면 생성
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*TextureExportDir))
    {
        PlatformFile.CreateDirectoryTree(*TextureExportDir);
    }

    UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    TArray<AActor*> FoundActors;
    if (EditorActorSubsystem)
    {
        FoundActors = EditorActorSubsystem->GetAllLevelActors();
    }

    for (AActor* Actor : FoundActors)
    {
        UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
        if (!MeshComponent || !MeshComponent->GetStaticMesh())
        {
            continue;
        }

        TSharedPtr<FJsonObject> ActorJsonObject = MakeShareable(new FJsonObject());
        ActorJsonObject->SetStringField(TEXT("Name"), Actor->GetActorLabel());
        ActorJsonObject->SetStringField(TEXT("Mesh"), MeshComponent->GetStaticMesh()->GetName());

        // 트랜스폼 정보 변환
        FVector UnrealLocation = MeshComponent->GetComponentLocation() / 100.0f;
        FQuat UnrealQuat = MeshComponent->GetComponentQuat();
        FVector UnrealScale = MeshComponent->GetComponentScale();

        FVector ExportedLocation(UnrealLocation.X, UnrealLocation.Z, UnrealLocation.Y);
        FQuat ExportedQuat(UnrealQuat.X, UnrealQuat.Y, UnrealQuat.Z, UnrealQuat.W);
        FVector ExportedScale(UnrealScale.X, UnrealScale.Z, UnrealScale.Y);

        // --- 수정된 부분: Transform 객체 생성 ---
        TSharedPtr<FJsonObject> TransformObject = MakeShareable(new FJsonObject());
        TransformObject->SetObjectField(TEXT("Location"), VectorToJsonObject(ExportedLocation));
        TransformObject->SetObjectField(TEXT("Rotation"), RotatorToJsonObject(ExportedQuat.Rotator()));
        TransformObject->SetObjectField(TEXT("Scale"), VectorToJsonObject(ExportedScale));
        ActorJsonObject->SetObjectField(TEXT("Transform"), TransformObject);
        // --- 수정 끝 ---

        // 텍스처 정보 저장 -> CJ 수정
        TArray<TSharedPtr<FJsonValue>> TexturesJsonArray;
        TSet<UTexture*> ExportedTextures; // 중복된 텍스처 익스포트를 방지하기 위한 Set

        int32 NumMaterials = MeshComponent->GetNumMaterials();
        for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
        {
            UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);
            if (Material)
            {
                TArray<UTexture*> UsedTextures;
                Material->GetUsedTextures(UsedTextures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);

                for (UTexture* Texture : UsedTextures)
                {
                    // UTexture2D 타입이고, 아직 익스포트되지 않은 텍스처만 처리
                    UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
                    if (Texture2D && !ExportedTextures.Contains(Texture2D))
                    {
                        // PNG 파일 경로 생성 (임시 원본으로 사용할 파일)
                        FString TextureFileName = Texture2D->GetName() + TEXT(".png");
                        FString TextureFullFilePath = TextureExportDir + TextureFileName;

                        // Texture2D를 PNG로 내보낼 수 있는 Exporter 찾기
                        UExporter* Exporter = UExporter::FindExporter(Texture2D, TEXT("PNG"));

                        if (Exporter)
                        {
                            // 찾아낸 Exporter를 사용하여 파일을 내보내기
                            int32 ExportResult = UExporter::ExportToFile(Texture2D, Exporter, *TextureFullFilePath, false, false);

                            if (ExportResult == 1)
                            {
                                // JSON에는 최종 결과물인 dds 경로를 기록
                                FString RelativeDdsPath = TEXT("Textures/") + Texture2D->GetName() + TEXT(".dds");
                                TexturesJsonArray.Add(MakeShareable(new FJsonValueString(RelativeDdsPath)));
                                UE_LOG(LogTemp, Log, TEXT("Exported Source Texture: %s"), *TextureFullFilePath);
                            }
                            else
                            {
                                UE_LOG(LogTemp, Warning, TEXT("Failed to export texture: %s"), *Texture2D->GetPathName());
                            }
                        }
                        else
                        {
                            UE_LOG(LogTemp, Error, TEXT("PNG Exporter not found for texture: %s"), *Texture2D->GetPathName());
                        }

                        // 처리된 텍스처를 Set에 추가하여 중복 방지
                        ExportedTextures.Add(Texture2D);
                    }
                }
            }
        }
        ActorJsonObject->SetArrayField(TEXT("Textures"), TexturesJsonArray);

        ActorJsonArray.Add(MakeShareable(new FJsonValueObject(ActorJsonObject)));
    }

    // 파일 저장
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(ActorJsonArray, Writer);

    FString FilePath = FPaths::ProjectSavedDir() + TEXT("MapData/ExportedClientData.json");
    FFileHelper::SaveStringToFile(OutputString, *FilePath);
    UE_LOG(LogTemp, Warning, TEXT("Export Complete! File saved to: %s"), *FilePath);
#else
    UE_LOG(LogTemp, Error, TEXT("ExportClientData can only be called from the editor."));
#endif
}