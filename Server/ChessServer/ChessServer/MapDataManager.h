#pragma once
namespace PIP
{
	struct MapObject
	{
		common::Vec3 _min;
		common::Vec3 _max;
	};
	class MapDataManager : public Singleton<MapDataManager>
	{
		friend Singleton<MapDataManager>;
	public:
		void LoadMapData(std::string_view mapDataPath);
		bool CheckForCollision(common::Vec3 target_pos, common::Vec3 player_extents);
	private:
		std::vector<MapObject> _map_objects;
	};
}

