#include "Support/SerializedData.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char *message) {
	if (!condition) {
		std::cerr << "FAILED: " << message << '\n';
		std::exit(1);
	}
}

template<typename Callback>
void requireThrows(Callback callback, const char *message) {
	try {
		callback();
	} catch (const std::runtime_error &) {
		return;
	}
	require(false, message);
}

} // namespace

int main() {
	const std::vector<uint8_t> bytes{0xfe, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 'o', 'k', 0};
	size_t position = 0;
	SerializedData::Reader reader(bytes, position);
	require(reader.readI8() == -2, "signed byte");
	require(reader.readI16LE() == 0x1234, "little-endian 16-bit value");
	require(reader.readI32LE() == 0x12345678, "little-endian 32-bit value");
	require(reader.readCString() == "ok", "bounded string");
	require(position == bytes.size(), "reader position");
	requireThrows([&] { reader.readI8(); }, "read past end");

	const std::vector<uint8_t> truncated{1, 2, 3};
	position = 0;
	SerializedData::Reader shortReader(truncated, position);
	requireThrows([&] { shortReader.readI32LE(); }, "truncated 32-bit value");
	require(position == 0, "failed integer read does not advance");

	const std::vector<uint8_t> unterminated{'a', 'b', 'c'};
	position = 0;
	SerializedData::Reader stringReader(unterminated, position);
	requireThrows([&] { stringReader.readCString(); }, "unterminated string");
	requireThrows([&] { stringReader.readCString(2); }, "oversized string");

	position = bytes.size() + 1;
	SerializedData::Reader invalidPosition(bytes, position);
	requireThrows([&] { invalidPosition.require(0); }, "invalid initial position");

	std::cout << "serialized-data tests passed\n";
	return 0;
}
