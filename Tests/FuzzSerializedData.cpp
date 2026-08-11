#include "Support/SerializedData.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size > 1024 * 1024)
		return 0;
	const std::span<const uint8_t> bytes(data, size);
	for (size_t start : {size_t{0}, size / 2, size, size + (size != SIZE_MAX ? 1 : 0)}) {
		size_t position = start;
		try {
			SerializedData::Reader reader(bytes, position);
			reader.readI8();
			reader.readI16LE();
			reader.readI32LE();
			reader.readCString(size % 4096);
		} catch (const std::runtime_error &) {
		}
	}
	return 0;
}
