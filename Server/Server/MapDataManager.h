#pragma once
namespace PIP
{
	struct MapObject
	{
		common::Vec3 _min;
		common::Vec3 _max;
	};
	struct HeightMapData {
		float _min_x, _max_x;
		float _min_z, _max_z;
		int _width, _height;
		std::vector<float> _heights;
		bool isLoaded = false;
	};
	class MapDataManager : public Singleton<MapDataManager>
	{
		friend Singleton<MapDataManager>;
	public:
		void LoadMapData(std::string_view mapDataPath);
		void LoadHeightMapData(std::string_view heightMapDataPath);
		void LoadHeightMapData(std::string_view r16FilePath, float minX, float maxX, float minZ, float maxZ, size_t height, size_t width);
		void LoadHeightMapDataPNG(std::string_view pngFilePath, float minX, float maxX, float minZ, float maxZ, size_t height, size_t width);
		void LoadHeightMapFromRawFile(std::string_view rawFilePath, float minX, float maxX, float minZ, float maxZ,
		                              size_t width, size_t height, bool isFloat32);
		bool CheckForCollision(common::Vec3 target_pos, common::Vec3 player_extents);
		common::Vec3 AdjustPositionToGround(common::Vec3 position);
		float GetGroundHeight(float x, float z);
	private:
		std::vector<MapObject> _map_objects;
		HeightMapData _height_map_data;

		float InterpolateHeight(float x, float z);
	};
}

