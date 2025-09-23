#include "stdafx.h"
#include "Behaviour.h"

Behaviour::Behaviour() : Component(), _isEnabled{ true }
{
}


void Behaviour::set_enabled(bool isEnabled)
{
	if (_isEnabled != isEnabled)
	{
		_isEnabled = isEnabled;
		if (_isEnabled)
		{
			on_enable();
		}
		else
		{
			on_disable();
		}
	}
}

