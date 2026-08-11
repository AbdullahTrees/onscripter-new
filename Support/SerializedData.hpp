/**
 * Bounds-checked little-endian reader for save, environment, and log data.
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>

namespace SerializedData {

class Reader {
public:
	Reader(std::span<const uint8_t> bytes, size_t &position)
	    : bytes_(bytes), position_(position) {}

	void require(size_t count) const {
		if (position_ > bytes_.size() || count > bytes_.size() - position_)
			throw std::runtime_error("Truncated serialized data");
	}

	int8_t readI8() {
		require(1);
		return static_cast<int8_t>(bytes_[position_++]);
	}

	int16_t readI16LE() {
		require(2);
		const uint16_t value = static_cast<uint16_t>(bytes_[position_]) |
		                       static_cast<uint16_t>(bytes_[position_ + 1]) << 8;
		position_ += 2;
		return static_cast<int16_t>(value);
	}

	int32_t readI32LE() {
		require(4);
		const uint32_t value = static_cast<uint32_t>(bytes_[position_]) |
		                       static_cast<uint32_t>(bytes_[position_ + 1]) << 8 |
		                       static_cast<uint32_t>(bytes_[position_ + 2]) << 16 |
		                       static_cast<uint32_t>(bytes_[position_ + 3]) << 24;
		position_ += 4;
		return static_cast<int32_t>(value);
	}

	uint32_t readU32LE() {
		return static_cast<uint32_t>(readI32LE());
	}

	std::string readCString(size_t maximumLength = 1024 * 1024) {
		require(1);
		const size_t remaining = bytes_.size() - position_;
		const size_t limit = maximumLength == std::numeric_limits<size_t>::max() ?
		                         maximumLength : maximumLength + 1;
		const size_t searchLength = std::min(remaining, limit);
		const void *terminator = std::memchr(bytes_.data() + position_, 0, searchLength);
		if (!terminator)
			throw std::runtime_error("Unterminated or oversized serialized string");
		const size_t length = static_cast<const uint8_t *>(terminator) - (bytes_.data() + position_);
		std::string value(reinterpret_cast<const char *>(bytes_.data() + position_), length);
		position_ += length + 1;
		return value;
	}

private:
	std::span<const uint8_t> bytes_;
	size_t &position_;
};

} // namespace SerializedData
