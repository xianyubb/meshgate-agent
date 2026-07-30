#include "agent/Config.h"

#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace meshgate_agent {
namespace {

std::optional<std::string> jsonStringField(std::string_view body, std::string_view key) {
    auto const quotedKey = std::string{"\""} + std::string{key} + '"';
    auto const keyPos    = body.find(quotedKey);
    if (keyPos == std::string_view::npos) return std::nullopt;

    auto const colon = body.find(':', keyPos + quotedKey.size());
    if (colon == std::string_view::npos) return std::nullopt;

    auto const firstQuote = body.find('"', colon + 1);
    if (firstQuote == std::string_view::npos) return std::nullopt;

    std::string value;
    bool        escaped = false;
    for (std::size_t i = firstQuote + 1; i < body.size(); ++i) {
        char const ch = body[i];
        if (escaped) {
            value.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') return value;
        value.push_back(ch);
    }

    return std::nullopt;
}

std::optional<std::uint16_t> jsonPortField(std::string_view body, std::string_view key) {
    auto const quotedKey = std::string{"\""} + std::string{key} + '"';
    auto const keyPos    = body.find(quotedKey);
    if (keyPos == std::string_view::npos) return std::nullopt;

    auto const colon = body.find(':', keyPos + quotedKey.size());
    if (colon == std::string_view::npos) return std::nullopt;

    std::size_t start = colon + 1;
    while (start < body.size() && std::isspace(static_cast<unsigned char>(body[start]))) ++start;

    std::size_t end = start;
    while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end]))) ++end;
    if (start == end) return std::nullopt;

    auto const value = static_cast<unsigned long>(std::stoul(std::string{body.substr(start, end - start)}));
    if (value == 0 || value > 65535) return std::nullopt;

    return static_cast<std::uint16_t>(value);
}

} // namespace

AgentConfig loadConfigFile(std::filesystem::path const& path) {
    AgentConfig config;

    std::ifstream input{path};
    if (!input) return config;

    std::stringstream buffer;
    buffer << input.rdbuf();

    auto const body = buffer.str();
    if (auto value = jsonStringField(body, "apiHost")) config.apiHost = *value;
    if (auto value = jsonPortField(body, "apiPort")) config.apiPort = *value;
    if (auto value = jsonStringField(body, "bearerToken")) config.bearerToken = *value;
    if (auto value = jsonStringField(body, "command")) config.command = *value;

    return config;
}

} // namespace meshgate_agent
