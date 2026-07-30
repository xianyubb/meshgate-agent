#pragma once

#include "agent/Config.h"

#include "ll/api/mod/NativeMod.h"

namespace meshgate_agent {

class MeshGateAgent {
public:
    static MeshGateAgent& getInstance();

    MeshGateAgent() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

private:
    ll::mod::NativeMod& mSelf;
    AgentConfig       mConfig;
};

} // namespace meshgate_agent
