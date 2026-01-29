#include "stdafx.h"
#include "Behavior.h"

Behavior::Behavior() : Component(), _isEnabled{ true }
{
}


void Behavior::set_enabled(bool isEnabled)
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

