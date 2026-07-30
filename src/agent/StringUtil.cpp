#include "agent/StringUtil.h"

#include <algorithm>
#include <cctype>

namespace meshgate_agent {

std::string trim(std::string value) {
    auto const notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

} // namespace meshgate_agent
