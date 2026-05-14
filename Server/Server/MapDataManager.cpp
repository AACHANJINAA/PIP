#include "pch.h"
#include "MapDataManager.h"
#include "glTFMeshLoader.h"
#include "JoltHelper.h"


namespace PIP
{
	void MapDataManager::LoadMapData(std::string_view mapDataPath)
	{
		std::ifstream file(mapDataPath.data());
		if (not file)
		{
			MYERROR("Failed to open map data file: " << mapDataPath);
			return;
		}

		using namespace nlohmann;
		json data = json::parse(file);

		_map_objects.clear(); // 기존 데이터 초기화

		for (const auto& item : data)
		{
			// 핵심 필드인 Center가 있는지 확인
			if (!item.contains("Center")) continue;

			MapObject obj;

			// 1. Center 파싱 (x, y, z 소문자)
			obj._center.x = item["Center"]["x"];
			obj._center.y = item["Center"]["y"];
			obj._center.z = item["Center"]["z"];

			// 2. Rotation 파싱 (x, y, z, w 소문자)
			obj._rotation.x = item["Rotation"]["x"];
			obj._rotation.y = item["Rotation"]["y"];
			obj._rotation.z = item["Rotation"]["z"];
			obj._rotation.w = item["Rotation"]["w"];

			// 3. Extent 파싱 (x, y, z 소문자)
			obj._extent.x = item["Extent"]["x"];
			obj._extent.y = item["Extent"]["y"];
			obj._extent.z = item["Extent"]["z"];

			_map_objects.push_back(obj);
		}
		MYLOG("Map Data Loaded: " << _map_objects.size() << " objects (OBB)");
	}

	void MapDataManager::LoadHeightMapData(std::string_view heightMapDataJSONPath)
	{
		// Common::TerrainData 로드
		common::TerrainData new_terrain_data;
		if (!new_terrain_data.LoadFromJSON(heightMapDataJSONPath.data(),false))
		{
			MYERROR("Failed to load height map via Common::TerrainData: " << heightMapDataJSONPath);
		}
		else
		{
			const auto& info = new_terrain_data.GetInfo();
			const auto& heightMap = new_terrain_data.GetHeightData();

			MYLOG("[TerrainData] Info: X[" << info.min_x << " ~ " << info.max_x
				<< "], Z[" << info.min_z << " ~ " << info.max_z << "]" << std::endl);

			MYLOG("Height map Loaded via Common: " << info.width << " * " << info.height
				<< ", Scale Y: " << info.height_scale);


			// --- [핵심] 여기서 딱 한 번 Shape을 생성함 ---
			JPH::HeightFieldShapeSettings settings;
			settings.mOffset = JPH::Vec3(info.min_x, 0.0f, info.min_z);

			float dx = (info.max_x - info.min_x) / (info.width - 1);
			float dz = (info.max_z - info.min_z) / (info.height - 1);
			settings.mScale = JPH::Vec3(dx, 1.0f, dz);
			settings.mSampleCount = static_cast<JPH::uint32>(info.width);

			settings.mHeightSamples.resize(heightMap.size());
			memcpy(settings.mHeightSamples.data(), heightMap.data(), heightMap.size() * sizeof(float));

			auto result = settings.Create();

			TerrainTile tile;
			tile.data = std::move(new_terrain_data);
			if (result.IsValid()) {
				tile.shape = result.Get(); // 생성된 Shape 저장 (Ref Count 증가)
			}

			// 전역 경계 갱신
			_worldMinX = (std::min)(_worldMinX, info.min_x);
			_worldMaxX = (std::max)(_worldMaxX, info.max_x);
			_worldMinZ = (std::min)(_worldMinZ, info.min_z);
			_worldMaxZ = (std::max)(_worldMaxZ, info.max_z);

			_terrainTiles.push_back(std::move(tile));
		}
	}

	void MapDataManager::LoadMainLandscapeData(std::string_view landscapeDirPath)
	{
		std::filesystem::path landscapeBaseDir(landscapeDirPath);
		if (!std::filesystem::exists(landscapeBaseDir)) {
			MYERROR("MainLandscape directory not found: " << landscapeDirPath);
			return;
		}

		_terrainTiles.clear();

		for (const auto& entry : std::filesystem::directory_iterator(landscapeBaseDir))
		{
			if (!entry.is_directory()) continue;
			if (entry.path().filename().string().find("Landscape") != 0) continue;

			std::string folderName = entry.path().filename().string();
			std::filesystem::path metadataPath = entry.path() / "metadata.json";
			if (!std::filesystem::exists(metadataPath)) continue;

			common::TerrainData data;
			// 다중 지형(MainLandscape)은 절대 높이를 유지해야 하므로 false 전달
			if (data.LoadFromJSON(metadataPath.string(), false)) {
				const auto& info = data.GetInfo();
				const auto& heightMap = data.GetHeightData();

				// --- [핵심] 여기서 딱 한 번 Shape을 생성함 ---
				JPH::HeightFieldShapeSettings settings;
				settings.mOffset = JPH::Vec3(info.min_x, 0.0f, info.min_z);

				float dx = (info.max_x - info.min_x) / (info.width - 1);
				float dz = (info.max_z - info.min_z) / (info.height - 1);
				settings.mScale = JPH::Vec3(dx, 1.0f, dz);
				settings.mSampleCount = static_cast<JPH::uint32>(info.width);

				settings.mHeightSamples.resize(heightMap.size());
				memcpy(settings.mHeightSamples.data(), heightMap.data(), heightMap.size() * sizeof(float));

				auto result = settings.Create();

				TerrainTile tile;
				tile.name = folderName;
				tile.data = std::move(data);
				if (result.IsValid()) {
					tile.shape = result.Get(); // 생성된 Shape 저장 (Ref Count 증가)
				}

				// 전역 경계 갱신
				_worldMinX = (std::min)(_worldMinX, info.min_x);
				_worldMaxX = (std::max)(_worldMaxX, info.max_x);
				_worldMinZ = (std::min)(_worldMinZ, info.min_z);
				_worldMaxZ = (std::max)(_worldMaxZ, info.max_z);

				_terrainTiles.push_back(std::move(tile));
			}
		}
		MYLOG("Total Landscapes & Shapes Loaded: " << _terrainTiles.size());
	}

	void MapDataManager::LoadStaticMeshShapes(const std::string& tileName, std::string_view jsonPath, bool enableBinSave)
	{
		namespace fs = std::filesystem;

		// 1. JSON 파일 경로 및 부모 폴더(basePath) 설정
		fs::path jsonFullPath(jsonPath);
		fs::path basePath = jsonFullPath.parent_path();

		// 2. JSON 파일 열기
		std::ifstream file(jsonFullPath);
		if (!file.is_open()) {
			MYERROR("[MapData] Scene file load error: " << jsonFullPath);
			return;
		}

		nlohmann::json sceneJson;
		try {
			file >> sceneJson;
			file.close();
		}
		catch (const nlohmann::json::exception& e) {
			MYERROR("[MapData] Scene file parse error: " << e.what());
			return;
		}

		// 메모리 캐시: "메쉬이름_ScaleX_ScaleY_ScaleZ"를 키로 사용
		std::unordered_map<std::string, JPH::ShapeRefC> meshLibrary;

		// 3. JSON 배열 순회
		for (const auto& objectJson : sceneJson) {
			std::string meshFile = objectJson.value("MeshFile", "");
			if (meshFile.empty()) continue;

			// 클라이언트와 동일한 메쉬 경로 생성: (JSON 폴더) / (MeshFile 경로)
			fs::path meshPath = basePath / meshFile;
			std::string meshName = meshPath.stem().string();

			const auto& transJson = objectJson["Transform"];
			JPH::Vec3 scale(
				transJson["Scale"].value("X", 1.0f),
				transJson["Scale"].value("Y", 1.0f),
				transJson["Scale"].value("Z", 1.0f)
			);

			char scaleStr[128];
			sprintf_s(scaleStr, "_%.3f_%.3f_%.3f", scale.GetX(), scale.GetY(), scale.GetZ());
			std::string libKey = meshName + scaleStr;
			fs::path cachePath = meshPath.parent_path() / (libKey + ".jbin");

			// 3. Shape 생성 또는 로드
			if (meshLibrary.find(libKey) == meshLibrary.end()) {
				JPH::ShapeRefC finalShape;

				// [LOAD] 바이너리 캐시 확인
				if (fs::exists(cachePath)) {
					std::ifstream ifile(cachePath, std::ios::binary);
					JPH::StreamInWrapper stream(ifile);
					auto res = JPH::Shape::sRestoreFromBinaryState(stream);
					if (res.IsValid()) {
						finalShape = res.Get();
						MYLOG("[MapData] 캐시 로드 성공: " << libKey);
					}
				}

				// [BUILD & BAKE] 캐시가 없으면 glTF 로드 후 스케일 굽기
				if (!finalShape) {
					auto meshes = glTFMeshLoader::LoadStaticMesh(meshPath.string());
					if (!meshes.empty()) {
						JPH::TriangleList triangles;
						for (const auto& m : meshes) {
							for (size_t i = 0; i < m.indices.size(); i += 3) {
								// 정점에 스케일을 직접 곱해서 굽습니다 (Bake)
								triangles.push_back(JPH::Triangle(
									PIP::Utils::ToJolt(m.vertices[m.indices[i]]) * scale,
									PIP::Utils::ToJolt(m.vertices[m.indices[i + 1]]) * scale,
									PIP::Utils::ToJolt(m.vertices[m.indices[i + 2]]) * scale
								));
							}
						}

						JPH::MeshShapeSettings settings(triangles);
						settings.Sanitize(); // 수치 안정성 확보를 위해 필수 호출

						auto result = settings.Create();
						if (result.IsValid()) {
							finalShape = result.Get();
							if (enableBinSave)
							{
								// [SAVE] 구워진 Shape를 바이너리로 저장
								std::ofstream ofile(cachePath, std::ios::binary);
								if (ofile.is_open()) {
									JPH::StreamOutWrapper stream(ofile);
									finalShape->SaveBinaryState(stream);
									MYLOG("[MapData] 캐시 생성 및 저장: " << cachePath.filename().string());
								}
							}
						}
						else 
						{
							MYERROR("[MapData] Shape 생성 실패: " << libKey);
						}
					}
				}

				if (finalShape) meshLibrary[libKey] = finalShape;
			}

			if (!meshLibrary.contains(libKey)) continue;

			// 4. 위치 및 회전만 적용하여 배치 (Scale은 이미 Shape에 구워짐)
			JPH::Vec3 pos(
				transJson["Location"].value("X", 0.0f),
				transJson["Location"].value("Y", 0.0f),
				transJson["Location"].value("Z", 0.0f)
			);
			JPH::Quat rot(
				transJson["Rotation"].value("X", 0.0f),
				transJson["Rotation"].value("Y", 0.0f),
				transJson["Rotation"].value("Z", 0.0f),
				transJson["Rotation"].value("W", 1.0f)
			);

			_staticMeshTiles.push_back({ tileName, meshName, meshLibrary[libKey], pos, rot });
		}

		MYLOG("[MapData] Scene '" << jsonFullPath.filename().string() << "' 로드 완료.");
	}
	void MapDataManager::LoadAllStaticMeshes(std::string_view baseDirPath)
	{
		namespace fs = std::filesystem;
		for (const auto& entry : fs::directory_iterator(baseDirPath))
		{
			if (entry.is_directory())
			{
				std::string tileName = entry.path().filename().string();
				// 폴더명과 동일한 이름의 .gltf 파일이 있는지 확인 (Unreal Batch Export 규칙)
				fs::path gltfPath = entry.path() / (tileName + ".gltf");

				if (fs::exists(gltfPath))
				{
					LoadStaticMeshShapes(tileName, gltfPath.string());
				}
			}
		}
		MYLOG("All Static Mesh Tiles Loaded from: " << baseDirPath);
	}

	void MapDataManager::LoadExportedScene(const std::string& groupName, std::string_view jsonPath)
	{
		std::ifstream file(jsonPath.data());
		if (!file.is_open()) return;

		nlohmann::json sceneJson;
		file >> sceneJson;

		std::filesystem::path basePath = std::filesystem::path(jsonPath).parent_path();

		for (const auto& objJson : sceneJson) {
			std::string meshFile = objJson.value("MeshFile", "");
			if (meshFile.empty()) continue;

			// 1. Transform 행렬 계산 (Location, Rotation, Scale)
			const auto& transJson = objJson["Transform"];

			// 클라이언트와 동일하게 Z축 반전 적용하여 행렬 생성
			XMMATRIX scaleMat = XMMatrixScaling(transJson["Scale"]["X"], transJson["Scale"]["Y"],
				transJson["Scale"]["Z"]);
			XMMATRIX rotMat = XMMatrixRotationQuaternion(XMVectorSet(
				transJson["Rotation"]["X"], transJson["Rotation"]["Y"],
				transJson["Rotation"]["Z"], transJson["Rotation"]["W"])); // Z 반전
			XMMATRIX locMat = XMMatrixTranslation(
				transJson["Location"]["X"], transJson["Location"]["Y"],
				transJson["Location"]["Z"]); // Z 반전

			XMMATRIX worldMat = scaleMat * rotMat * locMat;
			XMFLOAT4X4 externalTransform;
			XMStoreFloat4x4(&externalTransform, worldMat);

			// 2. 개별 glTF 로드 (위에서 만든 행렬 적용)
			std::string fullPath = (basePath / meshFile).string();
			auto meshes = glTFMeshLoader::LoadStaticMeshWithTransform(fullPath, externalTransform);

			// 3. Jolt Shape 생성 및 저장
			for (auto& meshData : meshes) {
				JPH::TriangleList triangles;
				for (size_t i = 0; i < meshData.indices.size(); i += 3) {
					triangles.push_back(JPH::Triangle(
						PIP::Utils::ToJolt(meshData.vertices[meshData.indices[i]]),
						PIP::Utils::ToJolt(meshData.vertices[meshData.indices[i + 1]]),
						PIP::Utils::ToJolt(meshData.vertices[meshData.indices[i + 2]])
					));
				}

				auto result = JPH::MeshShapeSettings(triangles).Create();
				if (result.IsValid()) {
					// 그룹 이름(groupName)을 tileName으로 사용하여 나중에 한 번에 찾을 수 있게 함
					_staticMeshTiles.push_back({ groupName, meshData.name, result.Get() });
				}
			}
		}
		MYLOG("Loaded Exported Scene for [" << groupName << "] from " << jsonPath);
	}

	void MapDataManager::LoadServerExportData(const std::string& groupName, std::string_view jsonPath, bool enableBinSave)
	{
		std::ifstream file(jsonPath.data());
		if (!file.is_open()) {
			MYERROR("[MapData] Failed to open Server Export Data: " << jsonPath);
			return;
		}

		using namespace nlohmann;
		json root;
		try {
			file >> root;
		} catch (const json::parse_error& e) {
			MYERROR("[MapData] JSON Parse Error in " << jsonPath << ": " << e.what());
			return;
		}

		// JSON 좌표 파싱 헬퍼
		auto ToJoltVec = [](const json& j) {
			if (j.is_null()) return JPH::Vec3::sZero();
			float x = j.value("X", 0.0f);
			float y = j.value("Y", 0.0f);
			float z = j.value("Z", 0.0f);

			// Y-up 좌수 -> Y-up 우수 변환: Z축 부호만 반전
			return JPH::Vec3(x, y, z);
		};
		auto ToJoltScale = [](const json& j) {
			if (j.is_null()) return JPH::Vec3::sReplicate(1.0f);
			// 스케일은 축만 맞추고 부호 반전(-)은 절대 하면 안 됩니다.
			return JPH::Vec3(j.value("X", 1.0f), j.value("Y", 1.0f), j.value("Z", 1.0f));
		};
		auto ToJoltQuat = [](const json& j) {
			float x = j.value("X", 0.0f);
			float y = j.value("Y", 0.0f);
			float z = j.value("Z", 0.0f);
			float w = j.value("W", 1.0f);

			JPH::Quat q(x, y, z, w); 

			return q.LengthSq() > 1.0e-8f ? q.Normalized() : JPH::Quat::sIdentity();
		};

		// 1. Mesh Library 파싱
		std::unordered_map<std::string, JPH::Ref<JPH::ShapeSettings>> meshLibrary;
		if (root.contains("MeshLibrary")) {
			const auto& libJson = root["MeshLibrary"];
			for (auto it = libJson.begin(); it != libJson.end(); ++it) {
				std::string meshName = it.key();
				const auto& meshInfo = it.value();
				std::string colType = meshInfo.value("CollisionType", "Simple");
				const auto& colData = meshInfo["CollisionData"];

				JPH::Ref<JPH::ShapeSettings> finalSettings;
				int convex_count = 0;
				if (colType == "Complex") {
					JPH::TriangleList triangles;
					const auto& verts = colData["Vertices"];
					const auto& indices = colData["Indices"];
					for (size_t i = 0; i < indices.size(); i += 3) {
						int i0 = indices[i].get<int>() * 3;
						int i1 = indices[i + 2].get<int>() * 3; // i+1 대신 i+2
						int i2 = indices[i + 1].get<int>() * 3; // i+2 대신 i+1 (순서 교체)
						triangles.push_back(JPH::Triangle(
							JPH::Float3(verts[i0], verts[i0 + 1], verts[i0 + 2]),
							JPH::Float3(verts[i1], verts[i1 + 1], verts[i1 + 2]),
							JPH::Float3(verts[i2], verts[i2 + 1], verts[i2 + 2])
						));
					}
					JPH::MeshShapeSettings* meshSettings = new JPH::MeshShapeSettings(triangles);
					meshSettings->Sanitize();

					auto result = meshSettings->Create();
					if (result.IsValid()) {
						finalSettings = meshSettings; // Ref로 저장되므로 주소 전달 가능

						// [디버그] 생성된 메쉬의 속성 확인
						JPH::ShapeRefC tempShape = result.Get();
						MYLOG("Mesh: " << meshName << " | COM: " << tempShape->GetCenterOfMass().ToInt() << " | Triangles: "<< triangles.size());
					}
					else {
						MYERROR("[MapData] MeshShape Create Failed: " << meshName << " Error: " << result.GetError().c_str());
					}
				}
				else if (colType == "Simple") {
					struct SubPart {
						JPH::Vec3 p; 
						JPH::Quat r;
						JPH::Ref<JPH::ShapeSettings> s;
					};
					std::vector<SubPart> parts;
					// 1. ConvexHulls 처리
					if (colData.contains("ConvexHulls") && colData["ConvexHulls"].is_array()) {
						for (const auto& hull : colData["ConvexHulls"]) {
							auto convex = new JPH::ConvexHullShapeSettings();
							const auto& vertices = hull["Vertices"];

							if (vertices.size() < 3) {
								MYERROR("[MapData] ConvexHull has too few vertices: " << meshName);
								continue;
							}
							convex_count++;
							for (const auto& v : vertices) {
								convex->mPoints.push_back(ToJoltVec(v));
							}

							convex->mMaxConvexRadius = 0.05f;

							auto testRes = convex->Create();
							if (testRes.IsValid()) {
								//JPH::ShapeRefC tempShape = testRes.Get();
								//MYLOG("Convex meshName: " << meshName << " | COM: " << tempShape->GetCenterOfMass().ToInt() << " | points: " << convex->mPoints.size());
								//MYLOG("Convex Count: " << convex_count);
								parts.push_back({ JPH::Vec3::sZero(), JPH::Quat::sIdentity(), convex });
							}
							else {
								MYERROR("[MapData] Convex sub-part invalid in " << meshName << " Error: " <<
									testRes.GetError().c_str());
							}
						}
					}

					// 2. Boxes 처리 (모델러의 ExtentX, ExtentY, ExtentZ 대응)
					if (colData.contains("Boxes") && colData["Boxes"].is_array()) {
						for (const auto& box : colData["Boxes"]) {
							// Extent는 이미 반폭이므로 그대로 사용합니다.
							float hx = box.value("ExtentX", 0.0f);
							float hy = box.value("ExtentY", 0.0f);
							float hz = box.value("ExtentZ", 0.0f);

							if (hx <= 0.01f || hy <= 0.01f || hz <= 0.01f)
							{
								MYERROR("[MapData] Box too small in " << meshName << " [" << hx << "," << hy << "," << hz << "]");
								continue;
							}
							auto boxSettings = new JPH::BoxShapeSettings(JPH::Vec3(hx, hy, hz)); // new 사용
							
							boxSettings->mConvexRadius = 0.05f;

							auto testRes = boxSettings->Create();
							if (testRes.IsValid()) {
								//MYLOG("Box meshName: " << meshName << " | COM: " << testRes.Get()->GetCenterOfMass().ToInt() << " | Extent: [" << hx << "," << hy << "," << hz << "]");
								parts.push_back({ ToJoltVec(box["Center"]), ToJoltQuat(box["Rotation"]), boxSettings });
							}
							else {
								MYERROR("[MapData] Box sub-part invalid in " << meshName << " Error: " <<
									testRes.GetError().c_str());
							}
						}
					}

					// 3. Spheres 처리 (추가됨)
					if (colData.contains("Spheres") && colData["Spheres"].is_array()) {
						for (const auto& sphere : colData["Spheres"]) {
							float radius = sphere.value("Radius", 0.0f);
							if (radius <= 0.01f) continue;

							auto sphereSettings = new JPH::SphereShapeSettings(radius); // new 사용
							JPH::Vec3 center = sphere.contains("Center") ? ToJoltVec(sphere["Center"]) : JPH::Vec3::sZero();

							auto testRes = sphereSettings->Create();
							if (testRes.IsValid()) {
								//MYLOG("Sphere meshName: " << meshName << " | COM: " << testRes.Get()->GetCenterOfMass().ToInt() << " | Radius: " << radius);
								parts.push_back({ center, JPH::Quat::sIdentity(), sphereSettings });
							}
							else {
								MYERROR("[MapData] Sphere sub-part invalid in " << meshName << " Error: " <<
									testRes.GetError().c_str());
							}

						}
					}

					// 4. Capsules 처리 (추가됨)
					if (colData.contains("Capsules") && colData["Capsules"].is_array()) {
						for (const auto& cap : colData["Capsules"]) {
							float radius = cap.value("Radius", 0.0f);
							float halfHeight = cap.value("HalfHeight", 0.0f); // 언리얼 캡슐은 보통 HalfHeight 사용
							if (radius <= 0.01f || halfHeight <= 0.01f) continue;

							auto capSettings = new JPH::CapsuleShapeSettings(halfHeight, radius); // new 사용
							JPH::Vec3 center = cap.contains("Center") ? ToJoltVec(cap["Center"]) : JPH::Vec3::sZero();
							JPH::Quat rotation = cap.contains("Rotation") ? ToJoltQuat(cap["Rotation"]) : JPH::Quat::sIdentity();


							auto testRes = capSettings->Create();
							if (testRes.IsValid()) {
								//MYLOG("Capsule meshName: " << meshName << " | COM: " << testRes.Get()->GetCenterOfMass().ToInt() << " | Radius: " << radius << ", HalfHeight: " << halfHeight);
								parts.push_back({ center, rotation, capSettings });
							}
							else {
								MYERROR("[MapData] Capsule sub-part invalid in " << meshName << " Error: " <<
									testRes.GetError().c_str());
							}
							
						}
					}
					// [핵심 수정] 자식이 하나라도 있을 때만 Create() 호출
					if (parts.size() == 1 && parts[0].p == JPH::Vec3::sZero() && parts[0].r == JPH::Quat::sIdentity()) {
						finalSettings = parts[0].s;
					}
					else if (!parts.empty()) 
					{
						auto compound = new JPH::StaticCompoundShapeSettings();
						for (auto& p : parts)
						{
							if (p.s == nullptr) continue;

							JPH::Shape::ShapeResult res = p.s->Create();
							if (res.IsValid()) {
								// [중요] Jolt는 Pivot 기준이 아닌 COM 기준으로 AddShape를 해야 합니다.
								// 모델러가 준 p.p 위치에 자식의 COM 오프셋을 더해줘야 정확합니다.
								compound->AddShape(p.p, p.r, p.s);
							} 
							else
							{
								MYERROR("[MapData] Failed to create sub-shape for compound in " << meshName << " Error: " << res.GetError().c_str());
							}
						}

						auto testRes = compound->Create();
						if (testRes.IsValid()) {
							finalSettings = compound;
						}
						else {
							MYERROR("[MapData] StaticCompoundShape setup failed for " << meshName << " Error: " <<
								testRes.GetError().c_str());
						}
					}
				}
				if (convex_count > 0)
				{
					MYLOG("Convex Count for " << meshName << ": " << convex_count);
					
				}
				if (finalSettings) 
					meshLibrary[meshName] = finalSettings;
			}
		}

		// 2. Actors 파싱
		JPH::Ref<JPH::StaticCompoundShapeSettings> worldCompound = new JPH::StaticCompoundShapeSettings();
		int instanceCount = 0;
		int find_mesh_count = 0;
		int com_err_count = 0;
		if (root.contains("Instances")) {
			for (const auto& actor : root["Instances"]) {
				// 정규화 헬퍼(ToJoltQuat) 사용 필수
				JPH::Vec3 actorPos = ToJoltVec(actor["WorldPos"]);
				JPH::Quat actorRot = ToJoltQuat(actor["WorldRot"]);
				std::string actorName = actor.value("ActorName", "UnnamedActor");
				for (const auto& part : actor["Parts"]) {
					std::string meshName = part.value("MeshName", "");
					auto it = meshLibrary.find(meshName);
					if (it == meshLibrary.end()) continue;

					JPH::Vec3 relPos = ToJoltVec(part["RelPos"]);
					JPH::Quat relRot = ToJoltQuat(part["RelRot"]);
					JPH::Vec3 scale = ToJoltScale(part["Scale"]);

					// 위치 및 회전 계산 (정규화된 actorRot 사용)
					JPH::Vec3 finalPos = actorPos + actorRot * relPos;
					JPH::Quat finalRot = (actorRot * relRot).Normalized();
					if (finalRot.LengthSq() > 0.0f)
					{
						finalRot = finalRot.Normalized();
					}
					JPH::Result<JPH::Ref<JPH::Shape>> result;
					if ((scale - JPH::Vec3::sReplicate(1.0f)).LengthSq() < 1.0e-6f) {
						result = it->second->Create();
					}
					else {
						auto scaled = new JPH::ScaledShapeSettings(it->second, scale);
						result = scaled->Create();
					}

					if (result.IsValid()) {
						JPH::ShapeRefC finalShape = result.Get();
						if (meshName == "SM_House_village_02_Merged" && actorName == "BP_house_02_Optimized17")
						{
							_findMeshShape.push_back({ groupName, meshName, finalShape, finalPos, finalRot }); // 디버그용으로 특정 메쉬 저장
						}
						//JPH::Vec3 com = finalShape->GetCenterOfMass();
						//if (com.LengthSq() > 0.001f) {
						//	// Pivot과 COM이 일치하지 않는 경우 로그 출력
						//	// 이 경우 Body 생성 시 position + rotation * com 처리를 해줘야 정확합니다.
						//	MYLOG("[MapData] " << actorName << "'s Shape with COM offset: " << meshName << " COM: " << com.GetX() << ", " << com.GetY()
						//	<< ", " << com.GetZ());
						//	com_err_count++;
						//}
						_staticMeshTiles.push_back({ groupName, meshName, finalShape, finalPos, finalRot });
						instanceCount++;
					}
					else {
						MYERROR("[MapData] Shape Create Failed for Mesh: " << meshName << " Error: " << result.GetError().c_str());
					}
				}
			}
		}

		// 3. 결과 저장
		MYLOG("[MapData] Loaded " << instanceCount << " individual instances for [" << groupName << "]");
		//MYLOG("[MapData] Found " << find_mesh_count << " instances of SM_House_village_02_Merged");
		//MYLOG("[MapData] Instances with COM offset error: " << com_err_count);
	}

	bool MapDataManager::LoadNavMesh(const std::string& name, std::string_view obj_path)
	{
		MYLOG("Loading NavMesh: " << name << " from OBJ: " << obj_path);
		RawMeshData rawData;
		if (!ParseOBJ(obj_path, rawData)) {
			MYERROR("Failed to parse NavMesh OBJ: " << obj_path);
			return false;
		}

		if (rawData.vertices.empty()) return false;

		// 1. 바운딩 박스 계산
		float bmin[3], bmax[3];
		bmin[0] = bmax[0] = rawData.vertices[0];
		bmin[1] = bmax[1] = rawData.vertices[1];
		bmin[2] = bmax[2] = rawData.vertices[2];

		for (size_t i = 3; i < rawData.vertices.size(); i += 3) {
			bmin[0] = (std::min)(bmin[0], rawData.vertices[i]);
			bmax[0] = (std::max)(bmax[0], rawData.vertices[i]);
			bmin[1] = (std::min)(bmin[1], rawData.vertices[i + 1]);
			bmax[1] = (std::max)(bmax[1], rawData.vertices[i + 1]);
			bmin[2] = (std::min)(bmin[2], rawData.vertices[i + 2]);
			bmax[2] = (std::max)(bmax[2], rawData.vertices[i + 2]);
		}

		// 2. Detour 생성 파라미터 설정
		dtNavMeshCreateParams params;
		memset(&params, 0, sizeof(params));

		// 해상도 설정 (정밀도와 맵 크기 사이의 트레이드오프)
		// 0.1f (10cm) 정도면 6.5km 맵까지 unsigned short로 커버 가능
		params.cs = 0.1f; 
		params.ch = 0.1f;

		// 정점을 Voxel 단위(unsigned short)로 변환
		std::vector<unsigned short> voxelVerts;
		voxelVerts.reserve(rawData.vertices.size());
		for (size_t i = 0; i < rawData.vertices.size(); i += 3) {
			voxelVerts.push_back(static_cast<unsigned short>((rawData.vertices[i] - bmin[0]) / params.cs));
			voxelVerts.push_back(static_cast<unsigned short>((rawData.vertices[i + 1] - bmin[1]) / params.ch));
			voxelVerts.push_back(static_cast<unsigned short>((rawData.vertices[i + 2] - bmin[2]) / params.cs));
		}

		params.verts = voxelVerts.data();
		params.vertCount = static_cast<int>(voxelVerts.size() / 3);
		
		// 폴리곤 데이터 재구성 (nvp * 2 구조)
		params.nvp = 6; // 최대 정점 수
		std::vector<unsigned short> detourPolys(rawData.polyCounts.size() * 2 * params.nvp, 0xffff);
		int vertOffset = 0;
		for (int i = 0; i < rawData.polyCounts.size(); ++i) {
			int pCount = rawData.polyCounts[i];
			for (int j = 0; j < pCount && j < params.nvp; ++j) {
				detourPolys[i * 2 * params.nvp + j] = static_cast<unsigned short>(rawData.indices[vertOffset + j]);
			}
			vertOffset += pCount;
		}

		params.polys = detourPolys.data();
		params.polyCount = static_cast<int>(rawData.polyCounts.size());
		
		// 기본 플래그 및 구역 설정
		std::vector<unsigned short> polyFlags(params.polyCount, 1); // 1: Walkable
		params.polyFlags = polyFlags.data();
		std::vector<unsigned char> polyAreas(params.polyCount, 0);
		params.polyAreas = polyAreas.data();

		params.bmin[0] = bmin[0]; params.bmin[1] = bmin[1]; params.bmin[2] = bmin[2];
		params.bmax[0] = bmax[0]; params.bmax[1] = bmax[1]; params.bmax[2] = bmax[2];
		
		params.walkableHeight = 2.0f;
		params.walkableRadius = 0.5f;
		params.walkableClimb = 0.5f;

		// 3. NavMesh 데이터 생성 (바이너리 블롭)
		unsigned char* navData = nullptr;
		int navDataSize = 0;
		if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
			MYERROR("dtCreateNavMeshData failed for NavMesh: " << name);
			return false;
		}

		// 4. NavMesh 및 Query 인스턴스 초기화
		auto info = std::make_shared<NavMeshInfo>();
		info->navMesh = dtAllocNavMesh();
		if (!info->navMesh || dtStatusFailed(info->navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA))) {
			MYERROR("dtNavMesh init failed for: " << name);
			return false;
		}

		info->navQuery = dtAllocNavMeshQuery();
		if (!info->navQuery || dtStatusFailed(info->navQuery->init(info->navMesh, 2048))) {
			MYERROR("dtNavMeshQuery init failed for: " << name);
			return false;
		}

		_navMeshes[name] = info;
		MYLOG("NavMesh '" << name << "' Loaded Successfully. Polys: " << params.polyCount);
		return true;
	}
	bool MapDataManager::FindPath(const std::string& name, const common::Vec3& start, const common::Vec3& end, std::vector<common::Vec3>& outPath) {
		auto it = _navMeshes.find(name);
		if (it == _navMeshes.end()) return false;
		auto query = it->second->navQuery;

		float startPos[3] = { start.x, start.y, start.z };
		float endPos[3] = { end.x, end.y, end.z };

		// 1. 시작점과 도착점에서 가장 가까운 폴리곤 찾기
		dtQueryFilter filter;
		filter.setIncludeFlags(1); // 걷기 가능 플래그(1)만 탐색
		float extents[3] = { 2.0f, 4.0f, 2.0f }; // 탐색 범위 (오차 허용 범위)

		dtPolyRef startPoly, endPoly;
		float nearestStart[3], nearestEnd[3];

		query->findNearestPoly(startPos, extents, &filter, &startPoly, nearestStart);
		query->findNearestPoly(endPos, extents, &filter, &endPoly, nearestEnd);

		if (!startPoly || !endPoly) return false;

		// 2. 폴리곤 경로 찾기 (A*)
		static const int MAX_POLYS = 256;
		dtPolyRef polyPath[MAX_POLYS];
		int pathCount = 0;

		dtStatus status = query->findPath(startPoly, endPoly, nearestStart, nearestEnd, &filter, polyPath, &pathCount, MAX_POLYS);

		if (dtStatusFailed(status) || pathCount <= 0) return false;

		// 3. 직선 경로 추출 (String Pulling / Funnel Algorithm)
		static const int MAX_STEER_POINTS = 64;
		float steerPath[MAX_STEER_POINTS * 3];
		unsigned char steerFlags[MAX_STEER_POINTS];
		dtPolyRef steerPolys[MAX_STEER_POINTS];
		int steerCount = 0;

		status = query->findStraightPath(nearestStart, nearestEnd, polyPath, pathCount, steerPath, steerFlags, steerPolys, &steerCount, MAX_STEER_POINTS);

		if (dtStatusFailed(status)) return false;

		// 4. 결과 좌표 벡터에 담기
		for (int i = 0; i < steerCount; ++i) {
			outPath.push_back({ steerPath[i * 3 + 0], steerPath[i * 3 + 1], steerPath[i * 3 + 2] });
		}

		return true;
	}
	bool MapDataManager::ParseOBJ(std::string_view path, RawMeshData& outData)
	{
		std::ifstream file(path.data());
		if (!file.is_open()) return false;

		std::string line;
		while (std::getline(file, line)) {
			if (line.substr(0, 2) == "v ") { // 정점 데이터
				std::istringstream s(line.substr(2));
				float x, y, z;
				s >> x >> y >> z;
				outData.vertices.push_back(x);
				outData.vertices.push_back(y);
				outData.vertices.push_back(z);
			}
			else if (line.substr(0, 2) == "f ") { // 면(폴리곤) 데이터
				std::istringstream s(line.substr(2));
				std::string part;
				int count = 0;
				while (s >> part) {
					// v/vt/vn 형식 대응
					size_t slashPos = part.find('/');
					int idx = std::stoi(part.substr(0, slashPos)) - 1;
					outData.indices.push_back(idx);
					count++;
				}
				outData.polyCounts.push_back(count);
			}
		}
		return true;
	}

	// 해당 위치에서 가장 가까운 네비메쉬 위 좌표 반환 (스냅 기능)
	bool MapDataManager::GetClosestPoint(const std::string& name, const common::Vec3& pos, common::Vec3& outPos) {
		auto it = _navMeshes.find(name);
		if (it == _navMeshes.end()) return false;
		auto query = it->second->navQuery;

		dtQueryFilter filter;
		filter.setIncludeFlags(1);
		float extents[3] = { 1.0f, 5.0f, 1.0f };
		dtPolyRef polyRef;
		float p[3] = { pos.x, pos.y, pos.z };
		float nearest[3];

		bool res = dtStatusSucceed(query->findNearestPoly(p, extents, &filter, &polyRef, nearest));
		if (res) outPos = { nearest[0], nearest[1], nearest[2] };
		return res;
	}

	// 두 지점 사이에 장애물(네비메쉬 단절)이 있는지 체크 (Raycast)
	bool MapDataManager::IsWalkable(const std::string& name, const common::Vec3& start, const common::Vec3& end) {
		auto it = _navMeshes.find(name);
		if (it == _navMeshes.end()) return false;
		auto query = it->second->navQuery;

		dtQueryFilter filter;
		filter.setIncludeFlags(1);
		float t = 0;
		float hitNormal[3];
		dtPolyRef polyPath[32];
		int pathCount = 0;
		dtPolyRef startPoly;
		float nearest[3];

		float s[3] = { start.x, start.y, start.z };
		float e[3] = { end.x, end.y, end.z };

		float halfExtents[3] = { 2.0f, 4.0f, 2.0f };
		query->findNearestPoly(s, halfExtents, &filter, &startPoly, nearest);

		dtStatus status = query->raycast(startPoly, s, e, &filter, &t, hitNormal, polyPath, &pathCount, 32);

		// t가 1.0 이상이면 가로막는 것 없이 도달 가능하다는 뜻
		return dtStatusSucceed(status) && t >= 1.0f;
	}

	std::vector<const StaticMeshTile*> MapDataManager::GetStaticMeshGroup(const std::string& groupName) const
	{
		std::vector<const StaticMeshTile*> result;

		auto it = _manualGroups.find(groupName);
		if (it == _manualGroups.end()) return result;

		// 그룹에 등록된 타일 이름들 ("Tile_X-1_Y-1" 등)을 순회하며
		// 해당 타일에 속한 모든 StaticMeshTile의 포인터를 담음
		for (const auto& targetTileName : it->second) {
			for (const auto& smTile : _staticMeshTiles) {
				if (smTile.tileName == targetTileName) {
					result.push_back(&smTile);
				}
			}
		}
		return result;
	}


	void MapDataManager::AddTerrainGroup(const std::string& groupName, const std::vector<std::string>& tileNames)
	{
		_manualGroups[groupName] = tileNames;
		MYLOG("[MapData] Group '" << groupName << "' defined with " << tileNames.size() << " tiles.");
	}

	std::vector<const TerrainTile*> MapDataManager::GetTerrainGroup(const std::string& groupName) const
	{
		std::vector<const TerrainTile*> result;

		auto it = _manualGroups.find(groupName);
		if (it == _manualGroups.end()) {
			MYERROR("[MapData] Group '" << groupName << "' not found!");
			return result;
		}

		// 그룹 내 정의된 타일 이름을 순회하며 실제 타일 포인터를 찾아 담음
		for (const auto& tileName : it->second) {
			for (const auto& tile : _terrainTiles) {
				if (tile.name == tileName) {
					result.push_back(&tile);
					break;
				}
			}
		}
		return result;
	}


	common::Vec3 MapDataManager::AdjustPositionToGround(common::Vec3 position)
	{
		position.y = GetGroundHeight(position.x, position.z);
		return position;
	}

	bool MapDataManager::IsInsideMap(float x, float z) const
	{
		return x >= _worldMinX && x <= _worldMaxX && z >= _worldMinZ && z <= _worldMaxZ;
	}

	float MapDataManager::GetGroundHeight(float x, float z) const
	{
		// 모든 지형 타일을 순회하며 해당 좌표가 포함된 타일의 높이를 반환
		for (const auto& tile : _terrainTiles) {
			if (tile.data.IsInsideMap(x, z)) {
				return tile.data.GetHeightAt(x, z);
			}
		}
		return 0.0f;
	}

}