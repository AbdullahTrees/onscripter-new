#include "Engine/Core/CommandLine.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char *message) {
	if (!condition) {
		std::cerr << "FAILED: " << message << '\n';
		std::exit(1);
	}
}

} // namespace

int main() {
	std::string error;
	require(CommandLine::validate({}, error), "empty argument vector");
	require(CommandLine::validate({"--root", "game", "--window"}, error), "valued option");
	require(!CommandLine::validate({"--root"}, error) && error == "option requires a value: --root",
	        "missing option value");
	require(!CommandLine::validate({"--env[SAFE_NAME]"}, error), "environment option needs value");
	require(CommandLine::validate({"--env[SAFE_NAME]", "value"}, error), "environment option value");

	const auto environment = CommandLine::environmentOptionName("--env[SAFE_NAME]");
	require(environment && *environment == "SAFE_NAME", "environment name extraction");
	require(!CommandLine::environmentOptionName("--env["), "truncated environment option");
	require(!CommandLine::environmentOptionName("--env[]"), "empty environment name");
	require(!CommandLine::environmentOptionName("--env[A-B]"), "unsafe environment name");
	require(!CommandLine::environmentOptionName("--env[A]suffix"), "environment suffix");
	require(!CommandLine::environmentOptionName("--env[\xC3\x89]"), "non-ASCII environment name");
	require(!CommandLine::validate({"--env[broken", "value"}, error), "malformed environment option");

	const std::string oversized(32769, 'x');
	require(!CommandLine::validate({oversized}, error), "oversized argument");
	require(!CommandLine::validate({"--root", oversized}, error), "oversized option value");
	std::cout << "command-line tests passed\n";
	return 0;
}
