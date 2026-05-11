#include "pch.h"
#include "StageManager.h"

#include "CastleStage.h"
#include "MainStage.h"
#include "BossStage.h"

namespace PIP::SERVER
{
	void StageManager::initialize()
	{
		register_stage<MainStage>("MainStage");
		register_stage<CastleStage>("CastleStage");
		register_stage<BossStage>("BossStage");
	}
}

