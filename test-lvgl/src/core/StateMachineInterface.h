#pragma once

namespace DirtSim {

template <typename EventType>
class StateMachineInterface {
public:
    virtual ~StateMachineInterface() = default;
    virtual void queueEvent(const EventType& event) = 0;
};

} // namespace DirtSim
