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
        FVector ExportedLocation(UnrealLocation.Y, UnrealLocation.Z, UnrealLocation.X);
        ActorJsonObject->SetObjectField(TEXT("Location"), VectorToJsonObject(ExportedLocation));

        // AABB 정보 변환 및 저장
        FBox WorldBounds = MeshComponent->Bounds.GetBox();
        FVector UnrealMin = WorldBounds.Min / 100.0f;
        FVector UnrealMax = WorldBounds.Max / 100.0f;
        FVector ExportedMin(UnrealMin.Y, UnrealMin.Z, UnrealMin.X);
        FVector ExportedMax(UnrealMax.Y, UnrealMax.Z, UnrealMax.X);

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
}

void UMyExporterBPL::ExportClientData(UObject* WorldContextObject)
{
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

        // 트랜스폼 데이터 변환
        FVector UnrealLocation = MeshComponent->GetComponentLocation() / 100.0f;
        FQuat UnrealQuat = MeshComponent->GetComponentQuat();
        FVector UnrealScale = MeshComponent->GetComponentScale();

        FVector ExportedLocation(UnrealLocation.Y, UnrealLocation.Z, UnrealLocation.X);
        FQuat ExportedQuat(UnrealQuat.Y, UnrealQuat.Z, UnrealQuat.X, UnrealQuat.W);
        FVector ExportedScale(UnrealScale.Y, UnrealScale.Z, UnrealScale.X);

        // --- 수정된 부분: Transform 객체 생성 ---
        TSharedPtr<FJsonObject> TransformObject = MakeShareable(new FJsonObject());
        TransformObject->SetObjectField(TEXT("Location"), VectorToJsonObject(ExportedLocation));
        TransformObject->SetObjectField(TEXT("Rotation"), RotatorToJsonObject(ExportedQuat.Rotator()));
        TransformObject->SetObjectField(TEXT("Scale"), VectorToJsonObject(ExportedScale));
        ActorJsonObject->SetObjectField(TEXT("Transform"), TransformObject);
        // --- 수정 끝 ---

        // 텍스처 정보 추출
        TArray<TSharedPtr<FJsonValue>> TexturesJsonArray;
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
                    if (Texture)
                    {
                        TexturesJsonArray.Add(MakeShareable(new FJsonValueString(Texture->GetPathName())));
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
}