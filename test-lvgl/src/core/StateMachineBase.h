#pragma once

#include <memory>
#include <string>

namespace DirtSim {

class EventProcessor;

class StateMachineBase {
public:
    StateMachineBase();
    virtual ~StateMachineBase();

    bool shouldExit() const { return shouldExit_; }
    void setShouldExit(bool value) { shouldExit_ = value; }

    virtual std::string getCurrentStateName() const = 0;
    virtual void processEvents() = 0;

protected:
    bool shouldExit_ = false;
};

} // namespace DirtSim
