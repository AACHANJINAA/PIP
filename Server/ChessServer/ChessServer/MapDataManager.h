#pragma once
namespace chess
{
	struct MapObject
	{
		Vec3 _min;
		Vec3 _max;
	};
	class MapDataManager : public Singleton<MapDataManager>
	{
		friend Singleton<MapDataManager>;
	public:
		void LoadMapData(std::string_view mapDataPath);
		bool CheckForCollision(Vec3 target_pos, Vec3 player_extents);
	private:
		std::vector<MapObject> _map_objects;
	};
}

