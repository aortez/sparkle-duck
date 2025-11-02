#pragma once

#include "Event.h"

namespace DirtSim {

class StateMachineInterface {
public:
    virtual ~StateMachineInterface() = default;

    virtual void queueEvent(const Event& event) = 0;
};

} // namespace DirtSim
