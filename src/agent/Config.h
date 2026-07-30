#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace meshgate_agent {

struct AgentConfig {
    std::string   apiHost{"127.0.0.1"};
    std::uint16_t apiPort{31920};
    std::string   bearerToken{"replace-with-the-same-token-as-meshgate"};
    std::string   command{"meshgate"};
};

AgentConfig loadConfigFile(std::filesystem::path const& path);

} // namespace meshgate_agent
