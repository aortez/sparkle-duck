#pragma once

#include "Timers.h"
#include <string>

class ScopeTimer {
public:
    explicit ScopeTimer(Timers& timers, const std::string& name) : m_timers(timers), m_name(name)
    {
        m_timers.startTimer(m_name);
    }

    ~ScopeTimer() { m_timers.stopTimer(m_name); }

private:
    Timers& m_timers;
    std::string m_name;
};