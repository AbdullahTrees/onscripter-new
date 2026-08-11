#include "Engine/Readers/ArchiveParser.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char *message) {
	if (!condition) {
		std::cerr << "FAILED: " << message << '\n';
		std::exit(1);
	}
}

void appendU16BE(std::vector<uint8_t> &bytes, uint16_t value) {
	bytes.push_back(static_cast<uint8_t>(value >> 8));
	bytes.push_back(static_cast<uint8_t>(value));
}

void appendU32BE(std::vector<uint8_t> &bytes, uint32_t value) {
	bytes.push_back(static_cast<uint8_t>(value >> 24));
	bytes.push_back(static_cast<uint8_t>(value >> 16));
	bytes.push_back(static_cast<uint8_t>(value >> 8));
	bytes.push_back(static_cast<uint8_t>(value));
}

void appendU32LE(std::vector<uint8_t> &bytes, uint32_t value) {
	bytes.push_back(static_cast<uint8_t>(value));
	bytes.push_back(static_cast<uint8_t>(value >> 8));
	bytes.push_back(static_cast<uint8_t>(value >> 16));
	bytes.push_back(static_cast<uint8_t>(value >> 24));
}

void appendName(std::vector<uint8_t> &bytes, const std::string &name) {
	bytes.insert(bytes.end(), name.begin(), name.end());
	bytes.push_back(0);
}

bool parseBytes(const std::vector<uint8_t> &bytes, ArchiveParser::Type type,
	            ArchiveParser::Result &result, std::string &error,
	            size_t offset = 0, size_t maximumNameLength = 4095) {
	FILE *file = std::tmpfile();
	require(file != nullptr, "tmpfile creation");
	if (!bytes.empty())
		require(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size(), "tmpfile write");
	const bool parsed = ArchiveParser::parse(file, bytes.size(), type, offset,
	                                         maximumNameLength, result, error);
	std::fclose(file);
	return parsed;
}

void testSar() {
	std::vector<uint8_t> bytes;
	appendU16BE(bytes, 1);
	appendU32BE(bytes, 22);
	appendName(bytes, "foo.txt");
	appendU32BE(bytes, 0);
	appendU32BE(bytes, 3);
	bytes.insert(bytes.end(), {'a', 'b', 'c'});

	ArchiveParser::Result result;
	std::string error;
	require(parseBytes(bytes, ArchiveParser::Type::Sar, result, error), "valid SAR parses");
	require(result.baseOffset == 22, "SAR base offset");
	require(result.entries.size() == 1, "SAR entry count");
	require(result.entries[0].name == "FOO.TXT", "SAR name normalization");
	require(result.entries[0].offset == 22 && result.entries[0].length == 3, "SAR range");
	require(result.entries[0].originalLength == 3 && !result.entries[0].compressed, "SAR metadata");
}

void testNsa() {
	std::vector<uint8_t> bytes;
	appendU16BE(bytes, 1);
	appendU32BE(bytes, 29);
	appendName(bytes, "voice.ogg");
	bytes.push_back(2);
	appendU32BE(bytes, 0);
	appendU32BE(bytes, 2);
	appendU32BE(bytes, 7);
	bytes.insert(bytes.end(), {'O', 'g'});

	ArchiveParser::Result result;
	std::string error;
	require(parseBytes(bytes, ArchiveParser::Type::Nsa, result, error), "valid NSA parses");
	require(result.entries.size() == 1 && result.entries[0].name == "VOICE.OGG", "NSA name");
	require(result.entries[0].length == 2 && result.entries[0].originalLength == 7, "NSA lengths");
	require(result.entries[0].compressed, "NSA compression metadata");
}

void testNs2() {
	std::vector<uint8_t> bytes;
	appendU32LE(bytes, 17);
	bytes.push_back('"');
	bytes.insert(bytes.end(), {'f', 'o', 'o', '.', 't', 'x', 't'});
	bytes.push_back('"');
	appendU32LE(bytes, 3);
	bytes.insert(bytes.end(), {'x', 'y', 'z'});

	ArchiveParser::Result result;
	std::string error;
	require(parseBytes(bytes, ArchiveParser::Type::Ns2, result, error), "valid NS2 parses");
	require(result.baseOffset == 17 && result.entries.size() == 1, "NS2 index");
	require(result.entries[0].name == "FOO.TXT", "NS2 name normalization");
	require(result.entries[0].offset == 17 && result.entries[0].length == 3, "NS2 range");
}

void testEmbeddedSarOffset() {
	std::vector<uint8_t> bytes = {0xaa, 0xbb, 0xcc};
	appendU16BE(bytes, 1);
	appendU32BE(bytes, 16);
	appendName(bytes, "a");
	appendU32BE(bytes, 0);
	appendU32BE(bytes, 1);
	bytes.push_back(0x42);

	ArchiveParser::Result result;
	std::string error;
	require(parseBytes(bytes, ArchiveParser::Type::Sar, result, error, 3), "embedded SAR parses");
	require(result.baseOffset == 19 && result.entries[0].offset == 19, "embedded SAR offsets");
}

void testMalformedInputs() {
	ArchiveParser::Result result;
	std::string error;
	for (const auto type : {ArchiveParser::Type::Sar, ArchiveParser::Type::Nsa, ArchiveParser::Type::Ns2})
		require(!parseBytes({}, type, result, error), "empty archive rejected");

	std::vector<uint8_t> outside;
	appendU16BE(outside, 1);
	appendU32BE(outside, 16);
	appendName(outside, "x");
	appendU32BE(outside, 0);
	appendU32BE(outside, 8);
	require(!parseBytes(outside, ArchiveParser::Type::Sar, result, error), "out-of-file SAR entry rejected");

	std::vector<uint8_t> unterminated;
	appendU32LE(unterminated, 8);
	unterminated.insert(unterminated.end(), {'"', 'a', 'b', 'c'});
	require(!parseBytes(unterminated, ArchiveParser::Type::Ns2, result, error), "unterminated NS2 name rejected");

	std::vector<uint8_t> oversized;
	appendU16BE(oversized, 1);
	appendU32BE(oversized, 18);
	appendName(oversized, "abc");
	appendU32BE(oversized, 0);
	appendU32BE(oversized, 0);
	require(!parseBytes(oversized, ArchiveParser::Type::Sar, result, error, 0, 2), "oversized name rejected");

	require(!ArchiveParser::parse(nullptr, 0, ArchiveParser::Type::Sar, 0, 16, result, error),
	        "null handle rejected");
}

} // namespace

int main() {
	testSar();
	testNsa();
	testNs2();
	testEmbeddedSarOffset();
	testMalformedInputs();
	std::cout << "archive parser tests passed\n";
	return 0;
}
