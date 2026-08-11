/**
 * Checked SAR/NSA/NS2 archive index parser.
 */

#include "Engine/Readers/ArchiveParser.hpp"

#include <cstdint>
#include <limits>

namespace ArchiveParser {
namespace {

bool readExact(FILE *file, void *destination, size_t size) {
	return size == 0 || std::fread(destination, 1, size, file) == size;
}

bool readU8(FILE *file, uint8_t &value) {
	return readExact(file, &value, sizeof(value));
}

bool readU16BE(FILE *file, uint16_t &value) {
	uint8_t bytes[2];
	if (!readExact(file, bytes, sizeof(bytes)))
		return false;
	value = static_cast<uint16_t>(bytes[0] << 8 | bytes[1]);
	return true;
}

bool readU32(FILE *file, uint32_t &value, bool littleEndian = false) {
	uint8_t bytes[4];
	if (!readExact(file, bytes, sizeof(bytes)))
		return false;
	if (littleEndian) {
		value = static_cast<uint32_t>(bytes[0]) |
		        static_cast<uint32_t>(bytes[1]) << 8 |
		        static_cast<uint32_t>(bytes[2]) << 16 |
		        static_cast<uint32_t>(bytes[3]) << 24;
	} else {
		value = static_cast<uint32_t>(bytes[0]) << 24 |
		        static_cast<uint32_t>(bytes[1]) << 16 |
		        static_cast<uint32_t>(bytes[2]) << 8 |
		        static_cast<uint32_t>(bytes[3]);
	}
	return true;
}

bool checkedAdd(size_t left, size_t right, size_t &result) {
	if (right > std::numeric_limits<size_t>::max() - left)
		return false;
	result = left + right;
	return true;
}

bool seekAbsolute(FILE *file, size_t offset) {
#ifdef _WIN32
	if (offset > static_cast<size_t>(std::numeric_limits<__int64>::max()))
		return false;
	return _fseeki64(file, static_cast<__int64>(offset), SEEK_SET) == 0;
#else
	if (offset > static_cast<size_t>(std::numeric_limits<off_t>::max()))
		return false;
	return fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
#endif
}

bool tell(FILE *file, size_t &position) {
#ifdef _WIN32
	const __int64 raw = _ftelli64(file);
#else
	const off_t raw = ftello(file);
#endif
	if (raw < 0 || static_cast<uintmax_t>(raw) > std::numeric_limits<size_t>::max())
		return false;
	position = static_cast<size_t>(raw);
	return true;
}

bool fail(std::string &error, const char *message) {
	error = message;
	return false;
}

} // namespace

bool parse(FILE *file, size_t archiveSize, Type type, size_t archiveOffset,
           size_t maximumNameLength, Result &result, std::string &error) {
	result = {};
	error.clear();
	if (!file)
		return fail(error, "null archive handle");
	if (maximumNameLength == 0)
		return fail(error, "zero filename limit");
	if (archiveOffset > archiveSize || !seekAbsolute(file, archiveOffset))
		return fail(error, "archive offset is outside the file");

	Result parsed;
	if (type == Type::Ns2) {
		uint32_t relativeBase = 0;
		if (!readU32(file, relativeBase, true) ||
		    !checkedAdd(archiveOffset, relativeBase, parsed.baseOffset) ||
		    parsed.baseOffset > archiveSize)
			return fail(error, "invalid NS2 base offset");

		size_t dataOffset = parsed.baseOffset;
		while (true) {
			size_t headerPosition = 0;
			if (!tell(file, headerPosition) || headerPosition > parsed.baseOffset)
				return fail(error, "NS2 header exceeds its data offset");
			if (headerPosition == parsed.baseOffset)
				break;

			uint8_t ch = 0;
			if (!readU8(file, ch))
				return fail(error, "truncated NS2 header");
			if (ch != '"') {
				if (headerPosition + 1 == parsed.baseOffset)
					break;
				return fail(error, "invalid NS2 filename delimiter");
			}

			Entry entry;
			while (true) {
				size_t namePosition = 0;
				if (!tell(file, namePosition) || namePosition >= parsed.baseOffset || !readU8(file, ch))
					return fail(error, "unterminated NS2 filename");
				if (ch == '"')
					break;
				if (ch == 0 || entry.name.size() >= maximumNameLength)
					return fail(error, "invalid or oversized NS2 filename");
				if ('a' <= ch && ch <= 'z')
					ch += 'A' - 'a';
				entry.name.push_back(static_cast<char>(ch));
			}
			if (entry.name.empty())
				return fail(error, "empty NS2 filename");

			size_t lengthPosition = 0;
			uint32_t length = 0;
			if (!tell(file, lengthPosition) || lengthPosition > parsed.baseOffset ||
			    parsed.baseOffset - lengthPosition < 4 || !readU32(file, length, true))
				return fail(error, "truncated NS2 length");
			entry.offset = dataOffset;
			entry.length = entry.originalLength = length;
			if (!checkedAdd(dataOffset, entry.length, dataOffset) || dataOffset > archiveSize)
				return fail(error, "NS2 entry exceeds the archive");
			parsed.entries.push_back(std::move(entry));
		}
	} else {
		uint16_t fileCount = 0;
		uint32_t relativeBase = 0;
		if (!readU16BE(file, fileCount) || !readU32(file, relativeBase) ||
		    !checkedAdd(archiveOffset, relativeBase, parsed.baseOffset) ||
		    parsed.baseOffset > archiveSize)
			return fail(error, "invalid SAR/NSA base offset");

		const bool nsa = type == Type::Nsa;
		const size_t minimumEntrySize = nsa ? 14 : 9;
		if (fileCount > archiveSize / minimumEntrySize)
			return fail(error, "implausible SAR/NSA file count");
		parsed.entries.reserve(fileCount);

		for (size_t i = 0; i < fileCount; ++i) {
			Entry entry;
			uint8_t ch = 0;
			while (true) {
				size_t namePosition = 0;
				if (!tell(file, namePosition) || namePosition >= parsed.baseOffset || !readU8(file, ch))
					return fail(error, "unterminated SAR/NSA filename");
				if (ch == 0)
					break;
				if (entry.name.size() >= maximumNameLength)
					return fail(error, "oversized SAR/NSA filename");
				if ('a' <= ch && ch <= 'z')
					ch += 'A' - 'a';
				entry.name.push_back(static_cast<char>(ch));
			}
			if (entry.name.empty())
				return fail(error, "empty SAR/NSA filename");

			uint8_t compression = 0;
			if (nsa && !readU8(file, compression))
				return fail(error, "truncated NSA compression field");
			entry.compressed = compression != 0;

			uint32_t relativeOffset = 0, length = 0, originalLength = 0;
			if (!readU32(file, relativeOffset) || !readU32(file, length) ||
			    (nsa && !readU32(file, originalLength)) ||
			    !checkedAdd(parsed.baseOffset, relativeOffset, entry.offset))
				return fail(error, "truncated or overflowing SAR/NSA entry");
			entry.length = length;
			entry.originalLength = nsa ? originalLength : length;
			size_t entryEnd = 0;
			if (!checkedAdd(entry.offset, entry.length, entryEnd) || entryEnd > archiveSize)
				return fail(error, "SAR/NSA entry exceeds the archive");
			parsed.entries.push_back(std::move(entry));
		}

		size_t headerEnd = 0;
		if (!tell(file, headerEnd) || headerEnd > parsed.baseOffset)
			return fail(error, "SAR/NSA header overlaps file data");
	}

	result = std::move(parsed);
	return true;
}

} // namespace ArchiveParser
