#include "Engine/Readers/ArchiveParser.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size > 1024 * 1024)
		return 0;

	FILE *file = std::tmpfile();
	if (!file)
		return 0;
	if (size != 0 && std::fwrite(data, 1, size, file) != size) {
		std::fclose(file);
		return 0;
	}

	for (const auto type : {ArchiveParser::Type::Sar, ArchiveParser::Type::Nsa, ArchiveParser::Type::Ns2}) {
		ArchiveParser::Result result;
		std::string error;
		ArchiveParser::parse(file, size, type, 0, 4095, result, error);
	}
	std::fclose(file);
	return 0;
}
