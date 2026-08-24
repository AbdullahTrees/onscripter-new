#include "External/slre.h"

#include <cassert>
#include <cstring>
#include <string>

int main() {
	{
		slre_regex_info info{};
		assert(slre_compile("[", 1, 0, &info) == SLRE_INVALID_CHARACTER_SET);
		assert(slre_compile("[abc", 4, 0, &info) == SLRE_INVALID_CHARACTER_SET);

		// A trailing escape has no byte after it. The compiler must reject the
		// expression without letting its operator-length probe read past the end.
		constexpr char TrailingEscape[] = {'\\'};
		assert(slre_compile(TrailingEscape, 1, 0, &info) == SLRE_INVALID_METACHARACTER);
	}

	{
		constexpr char Expression[] = R"((\[.+?\]))";
		slre_regex_info info{};
		assert(slre_compile(Expression, static_cast<int>(std::strlen(Expression)), 0, &info) > 0);
		slre_cap capture{};
		const std::string input = "dialogue [@] tail";
		const int consumed = slre_match_reuse(&info, input.data(), static_cast<int>(input.size()), &capture, 1);
		assert(consumed == 12);
		assert(capture.len == 3);
		assert(std::string(capture.ptr, static_cast<size_t>(capture.len)) == "[@]");
	}

	{
		// Nested ambiguous quantifiers used to permit unbounded recursive
		// backtracking. The matcher must stop at its deterministic work budget.
		constexpr char Expression[] = "(a+)+b";
		slre_regex_info info{};
		assert(slre_compile(Expression, static_cast<int>(std::strlen(Expression)), 0, &info) > 0);
		const std::string input(20000, 'a');
		assert(slre_match_reuse(&info, input.data(), static_cast<int>(input.size()), nullptr, 0) ==
		       SLRE_MATCH_LIMIT);
	}
}
