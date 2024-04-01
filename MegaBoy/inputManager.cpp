#include "inputManager.h"
#include <map>

std::map<int, uint8_t> keyConfig =
{
	{4, 4}
};

bool inputManager::update(int scancode, int action)
{
	auto key = keyConfig.find(scancode);

	if (key != keyConfig.end())
	{

		return true;
	}

	return false;
}