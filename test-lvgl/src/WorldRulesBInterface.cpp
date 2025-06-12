#include "WorldRulesBInterface.h"
#include "WorldB.h"

bool WorldRulesBInterface::isWithinBounds(int x, int y, const WorldB& world) const {
    return x >= 0 && y >= 0 && 
           static_cast<uint32_t>(x) < world.getWidth() && 
           static_cast<uint32_t>(y) < world.getHeight();
}