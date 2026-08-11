#include "External/slre.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (!data || size < 2)
		return 0;

	const size_t expressionLength = std::min<size_t>(data[0], size - 1);
	const char *expression = reinterpret_cast<const char *>(data + 1);
	const char *input      = expression + expressionLength;
	const size_t inputLength = size - 1 - expressionLength;
	if (expressionLength > static_cast<size_t>(std::numeric_limits<int>::max()) ||
	    inputLength > static_cast<size_t>(std::numeric_limits<int>::max()))
		return 0;

	slre_regex_info info{};
	if (slre_compile(expression, static_cast<int>(expressionLength), 0, &info) > 0) {
		slre_cap captures[8]{};
		(void)slre_match_reuse(&info, input, static_cast<int>(inputLength), captures, 8);
	}
	return 0;
}
