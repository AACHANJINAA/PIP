#include "pch.h"
#include "MapDataManager.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
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

		for (const auto& item : data)
		{
			if (!item.contains("AABB")) continue;

			common::Vec3 min_v;
			min_v.x = item["AABB"]["Min"]["X"];
			min_v.y = item["AABB"]["Min"]["Y"];
			min_v.z = item["AABB"]["Min"]["Z"];

			common::Vec3 max_v;
			max_v.x = item["AABB"]["Max"]["X"];
			max_v.y = item["AABB"]["Max"]["Y"];
			max_v.z = item["AABB"]["Max"]["Z"];

			_map_objects.push_back({ min_v, max_v });
		}
	}

	void MapDataManager::LoadHeightMapData(std::string_view heightMapDataPath)
	{
		std::ifstream file(heightMapDataPath.data());
		if (not file)
		{
			MYERROR("Failed to open height map data file: " << heightMapDataPath);
			return;
		}
		using namespace nlohmann;
		json data = json::parse(file);

		_height_map_data._min_x = data["bounds"]["min_x"];
		_height_map_data._max_x = data["bounds"]["max_x"];
		_height_map_data._min_z = data["bounds"]["min_z"];
		_height_map_data._max_z = data["bounds"]["max_z"];
		_height_map_data._width = data["width"];
		_height_map_data._height = data["height"];

		_height_map_data._heights.clear();
		_height_map_data._heights.reserve(_height_map_data._width * _height_map_data._height);

		for (const auto& height : data["heights"])
		{
			_height_map_data._heights.emplace_back(height);
		}
		_height_map_data.isLoaded = true;
		MYLOG("Height map Loaded: " << _height_map_data._width << " * " << _height_map_data._height);
	}

	void MapDataManager::LoadHeightMapData(std::string_view r16FilePath, float minX, float maxX, float minZ, float maxZ, size_t height, size_t width)
	{
		std::ifstream file(r16FilePath.data(), std::ios::binary);
		if (!file)
		{
			MYERROR("Failed to open R16 height map file: " << r16FilePath);
			return;
		}

		// 파일 크기 확인
		file.seekg(0, std::ios::end);
		size_t file_size = file.tellg();
		file.seekg(0, std::ios::beg);

		size_t expected_size = width * height * sizeof(uint16_t);
		if (file_size < expected_size)
		{
			MYERROR("File too small. Expected: " << expected_size << " bytes, Got: " << file_size << " bytes");
			return;
		}

		_height_map_data._min_x = minX;
		_height_map_data._max_x = maxX;
		_height_map_data._min_z = minZ;
		_height_map_data._max_z = maxZ;
		_height_map_data._width = width;
		_height_map_data._height = height;

		_height_map_data._heights.clear();
		size_t map_size = width * height;
		_height_map_data._heights.reserve(map_size);

		for (size_t i = 0; i < map_size; ++i)
		{
			uint16_t height_value;
			if (!file.read((char*)(&height_value), sizeof(uint16_t)))
			{
				MYERROR("Failed to read height data at index: " << i);
				return;
			}

			float normalized_height = static_cast<float>(height_value) / 65535.0f;
			float actual_height = normalized_height * 100.0f;
			_height_map_data._heights.emplace_back(actual_height);
		}

		_height_map_data.isLoaded = true;
		MYLOG("R16 Height map loaded: " << width << " x " << height);
	}

	void MapDataManager::LoadHeightMapDataPNG(std::string_view pngFilePath, float minX, float maxX, float minZ,
		float maxZ, size_t height, size_t width)
	{
		int img_width, img_height, channels;
		unsigned char* img_data = stbi_load(pngFilePath.data(), &img_width, &img_height, &channels, 1); // 1채널(그레이스케일)로 로드

		if (!img_data)
		{
			MYERROR("Failed to load PNG height map: " << pngFilePath);
			return;
		}

		// 이미지 크기와 요청된 크기가 다르면 경고
		if (img_width != width || img_height != height)
		{
			MYERROR("PNG size mismatch. Expected: " << width << "x" << height << ", Got: " << img_width << "x" << img_height);
			stbi_image_free(img_data);
			return;
		}

		_height_map_data._min_x = minX;
		_height_map_data._max_x = maxX;
		_height_map_data._min_z = minZ;
		_height_map_data._max_z = maxZ;
		_height_map_data._width = width;
		_height_map_data._height = height;

		_height_map_data._heights.clear();
		size_t map_size = width * height;
		_height_map_data._heights.reserve(map_size);

		for (size_t i = 0; i < map_size; ++i)
		{
			// PNG 픽셀값 (0-255)을 높이값으로 변환
			float normalized_height = static_cast<float>(img_data[i]) / 255.0f;
			float actual_height = normalized_height * 100.0f; // 최대 높이 100 가정
			_height_map_data._heights.emplace_back(actual_height);
		}

		stbi_image_free(img_data);
		_height_map_data.isLoaded = true;
		MYLOG("PNG Height map loaded: " << _height_map_data._width << " * " << _height_map_data._height);
	}
	void MapDataManager::LoadHeightMapFromRawFile(std::string_view rawFilePath, float minX, float maxX, float minZ, float maxZ,
		size_t width, size_t height, bool isFloat32)
	{
		std::ifstream file(rawFilePath.data(), std::ios::binary);
		if (not file)
		{
			MYERROR("Failed to open Raw height map file: " << rawFilePath);
			return;
		}

		// 파일 크기 검증
		size_t file_size = std::filesystem::file_size(rawFilePath.data());

		size_t element_size = isFloat32 ? sizeof(float) : sizeof(uint16_t);
		size_t expected_size = width * height * element_size;

		if (file_size < expected_size)
		{
			MYERROR("Raw file size mismatch. Expected: " << expected_size << " bytes, Got: " << file_size);
			return;
		}

		_height_map_data._min_x = minX;
		_height_map_data._max_x = maxX;
		_height_map_data._min_z = minZ;
		_height_map_data._max_z = maxZ;
		_height_map_data._width = width;
		_height_map_data._height = height;

		_height_map_data._heights.clear();
		size_t map_size = width * height;
		_height_map_data._heights.reserve(map_size);

		if (isFloat32)
		{
			// 32비트 float Raw 파일
			for (size_t i = 0; i < map_size; ++i)
			{
				float height_value;
				if (!file.read((char*)(&height_value), sizeof(float)))
				{
					MYERROR("Failed to read float height at index: " << i);
					return;
				}
				_height_map_data._heights.emplace_back(height_value);
			}
		}
		else
		{
			// 16비트 uint Raw 파일 (R16과 동일)
			for (size_t i = 0; i < map_size; ++i)
			{
				uint16_t height_value;
				if (!file.read((char*)(&height_value), sizeof(uint16_t)))
				{
					MYERROR("Failed to read uint16 height at index: " << i);
					return;
				}
				float normalized = static_cast<float>(height_value) / 65535.0f;
				_height_map_data._heights.emplace_back(normalized * 100.0f);
			}
		}

		_height_map_data.isLoaded = true;
		MYLOG("Raw Height map loaded: " << width << " x " << height << " (" << (isFloat32 ? "float32" : "uint16") << ")");
	}


	common::Vec3 MapDataManager::AdjustPositionToGround(common::Vec3 position)
	{
		position.y = GetGroundHeight(position.x, position.z);
		return position;
	}
	float MapDataManager::GetGroundHeight(float x, float z)
	{
		if (not _height_map_data.isLoaded)
		{
			return 0.0f; // 높이 맵이 로드되지 않은 경우 기본값 반환
		}
		if (x > _height_map_data._max_x || x < _height_map_data._min_x ||
			z > _height_map_data._max_z || z < _height_map_data._max_z)
		{
			return 0.0f; // 범위 밖
		}

		return InterpolateHeight(x, z);
	}
	float MapDataManager::InterpolateHeight(float x, float z)
	{
		float norm_x = (x - _height_map_data._min_x) / (_height_map_data._max_x - _height_map_data._min_x);
		float norm_z = (z - _height_map_data._min_z) / (_height_map_data._max_z - _height_map_data._min_z);

		float grid_x = norm_x * (_height_map_data._width - 1);
		float grid_z = norm_z * (_height_map_data._height - 1);

		//정수 소수 부분 불리
		int x0 = static_cast<int>(std::floor(grid_x));
		int z0 = static_cast<int>(std::floor(grid_z));
		int x1 = std::min(x0 + 1, _height_map_data._width - 1);
		int z1 = std::min(z0 + 1, _height_map_data._height - 1);

		float fx = grid_x - x0;
		float fz = grid_z - z0;

		float h00 = _height_map_data._heights[z0 * _height_map_data._width + x0];
		float h10 = _height_map_data._heights[z0 * _height_map_data._width + x1];
		float h01 = _height_map_data._heights[z1 * _height_map_data._width + x0];
		float h11 = _height_map_data._heights[z1 * _height_map_data._width + x1];

		//이중 선형 보간
		float h0 = h00 * (1.f - fx) + h10 * fx;
		float h1 = h01 * (1.f - fx) + h11 * fx;

		return h0 * (1.f - fz) + h1 * fz;
	}

	bool MapDataManager::CheckForCollision(common::Vec3 target_pos, common::Vec3 player_extents)
	{
		// 1. 플레이어의 AABB(경계 상자) 계산
		common::Vec3 player_min = { target_pos.x - player_extents.x, target_pos.y - player_extents.y, target_pos.z - player_extents.z };
		common::Vec3 player_max = { target_pos.x + player_extents.x, target_pos.y + player_extents.y, target_pos.z + player_extents.z };

		// 2. 모든 맵 오브젝트와 충돌 검사
		for (const auto& map_object : _map_objects)
		{
			// 3. AABB 충돌 검사 로직
			if (player_max.x > map_object._min.x &&
				player_min.x < map_object._max.x &&
				player_max.y > map_object._min.y &&
				player_min.y < map_object._max.y &&
				player_max.z > map_object._min.z &&
				player_min.z < map_object._max.z)
			{
				return true; // 충돌 발생
			}
		}

		return false; // 충돌 없음
	}
}