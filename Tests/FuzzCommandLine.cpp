#include "Engine/Core/CommandLine.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size > 1024 * 1024)
		return 0;

	std::vector<std::string_view> arguments;
	size_t start = 0;
	for (size_t i = 0; i <= size && arguments.size() < 128; ++i) {
		if (i == size || data[i] == 0) {
			arguments.emplace_back(reinterpret_cast<const char *>(data + start), i - start);
			start = i + 1;
		}
	}

	std::string error;
	CommandLine::validate(arguments, error);
	for (const std::string_view argument : arguments) {
		CommandLine::optionRequiresValue(argument);
		CommandLine::environmentOptionName(argument);
	}
	return 0;
}
