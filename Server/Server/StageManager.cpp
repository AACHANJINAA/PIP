#include "pch.h"
#include "StageManager.h"

#include "CastleStage.h"
#include "MainStage.h"

namespace PIP::SERVER
{
	void StageManager::initialize()
	{
		register_stage<MainStage>("MainStage");
		register_stage<CastleStage>("CastleStage");
	}
}

