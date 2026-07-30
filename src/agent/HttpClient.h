#pragma once

#include "agent/Config.h"

#include <string>
#include <string_view>

namespace meshgate_agent {

bool postHandoff(
    AgentConfig const& config,
    std::string_view   xuid,
    std::string_view   playerName,
    std::string_view   target,
    std::string&       response
);

} // namespace meshgate_agent
