/**
 * Checked SAR/NSA/NS2 archive index parser.
 *
 * This module deliberately has no engine ownership or logging dependencies so
 * it can be unit-tested and fuzzed independently of the game runtime.
 */

#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace ArchiveParser {

enum class Type {
	Sar,
	Nsa,
	Ns2,
};

struct Entry {
	std::string name;
	size_t offset{0};
	size_t length{0};
	size_t originalLength{0};
	bool compressed{false};
};

struct Result {
	size_t baseOffset{0};
	std::vector<Entry> entries;
};

bool parse(FILE *file, size_t archiveSize, Type type, size_t archiveOffset,
           size_t maximumNameLength, Result &result, std::string &error);

} // namespace ArchiveParser
