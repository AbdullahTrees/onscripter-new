/**
 * Pure command-line validation helpers, separated from engine side effects so
 * malformed argument vectors can be unit-tested and fuzzed.
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CommandLine {

bool optionRequiresValue(std::string_view option);
std::optional<std::string> environmentOptionName(std::string_view option);
bool validate(const std::vector<std::string_view> &arguments, std::string &error);

} // namespace CommandLine
