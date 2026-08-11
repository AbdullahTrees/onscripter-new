/**
 *  nscmake.cpp
 *  ONScripter-RU
 *
 *  Script compression tool.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include <zlib.h>

#include <iostream>
#include <fstream>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IN 0x10000000
#define DEF_OUT "script.file"

int main(int argc, char **argv) {
	char *out_filename = NULL, *in_filename = NULL;
	FILE *out_fp;
	bool use_stdout = false, use_stdin = false;
	unsigned char key_table[256] = {
		0xc0, 0xbc, 0x86, 0x66, 0x84, 0xf3, 0xbe, 0x90, 0xb0, 0x02, 0x98, 0x5e, 0x0f, 0x9c, 0x7b, 0xf4,
		0xd9, 0x91, 0xdb, 0xeb, 0x81, 0x74, 0x3a, 0xe3, 0x76, 0x94, 0x21, 0x93, 0x63, 0x68, 0x0d, 0xa1,
		0xba, 0xaa, 0x1b, 0xa0, 0x49, 0x2b, 0xe1, 0xe7, 0x38, 0xa6, 0x25, 0x53, 0x40, 0x4a, 0xec, 0x29,
		0x36, 0xbf, 0xf2, 0x9f, 0xac, 0x0c, 0xcb, 0x00, 0x1f, 0xf1, 0x7c, 0x80, 0x4f, 0x60, 0x82, 0x62,
		0x14, 0x6d, 0xd8, 0x32, 0x13, 0x2f, 0xe0, 0x99, 0xf7, 0x10, 0xd1, 0x30, 0x64, 0x4e, 0x8c, 0xde,
		0xc1, 0x6a, 0xad, 0xa7, 0xb5, 0x95, 0xcf, 0xc6, 0x0b, 0x2d, 0x69, 0x24, 0x5c, 0xc5, 0x03, 0xda,
		0xd6, 0x8e, 0xa3, 0x88, 0x31, 0x17, 0x3c, 0xb3, 0xa8, 0xb4, 0x01, 0x0e, 0xfc, 0x37, 0x65, 0x16,
		0x6c, 0xbb, 0x50, 0x55, 0x2a, 0xe5, 0x77, 0x97, 0x09, 0xb1, 0x04, 0x67, 0xc7, 0x79, 0x71, 0x7a,
		0x43, 0xd0, 0x22, 0x58, 0x0a, 0x57, 0xb7, 0xae, 0x4d, 0xc8, 0xe9, 0x46, 0xd3, 0x5b, 0x96, 0xcc,
		0x3f, 0xe6, 0x3e, 0x54, 0x5f, 0x1d, 0xfa, 0xf0, 0x3d, 0x7d, 0x83, 0xa5, 0xfd, 0xef, 0x15, 0x8b,
		0x70, 0x6b, 0xe2, 0xff, 0x07, 0xd7, 0x92, 0x41, 0x61, 0x75, 0x6f, 0x7f, 0xc4, 0xd5, 0xf9, 0x05,
		0x34, 0xfe, 0x5d, 0xdc, 0xb9, 0xe8, 0xab, 0xca, 0xc3, 0x35, 0x08, 0x3b, 0xa2, 0xbd, 0x8f, 0x7e,
		0x2e, 0x44, 0x5a, 0x12, 0xed, 0xe4, 0x11, 0x1e, 0xc2, 0x78, 0xf5, 0xaf, 0xf6, 0x72, 0x28, 0x9d,
		0x6e, 0x39, 0xd2, 0xea, 0x45, 0x73, 0x47, 0x9e, 0x26, 0x89, 0x85, 0x52, 0x33, 0xdf, 0xa4, 0x48,
		0x23, 0xce, 0x1c, 0x8d, 0x18, 0x27, 0x9a, 0xb6, 0xa9, 0xee, 0xb8, 0xc9, 0x2c, 0xfb, 0x59, 0x56,
		0x20, 0x42, 0xcd, 0x51, 0xb2, 0x06, 0x19, 0x4b, 0x9b, 0xd4, 0x8a, 0x4c, 0xf8, 0x87, 0x1a, 0xdd
	};
	std::vector<unsigned char> out_buffer;
	constexpr std::uint32_t version = 110;
	constexpr std::size_t maximumInputSize = MAX_IN;

	if (argc > 1) {
		if (!strcmp(argv[1], "-o")) {
			argc--;
			argv++;
			if (argc < 3)
				argc = -1;
			else {
#ifndef WIN32
				if (strcmp(argv[1], "-")) {
					// "-" arg means "use stdout"
#endif
					out_filename = argv[1];
#ifndef WIN32
				} else {
					use_stdout = true;
				}
#endif
				argc--;
				argv++;
			}
		}
#ifndef WIN32
		if ((argc > 1) && !strcmp(argv[1], "-")) {
			// "-" arg means "use stdin"
			use_stdin = true;
		}
#endif
	} else
		argc = -1;
	if (argc < 0) {
		fprintf(stderr, "Usage: nscmake [-o ons_file] in_file(s)\n");
		fprintf(stderr, "	(ons_file defaults to \"script.file\")\n");
		return 1;
	}

	auto appendInput = [&](std::istream &input, const char *name) {
		std::array<char, 64 * 1024> chunk{};
		while (input) {
			input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
			const std::streamsize count = input.gcount();
			if (count > 0) {
				const std::size_t amount = static_cast<std::size_t>(count);
				if (amount >= maximumInputSize - out_buffer.size()) {
					fprintf(stderr, "Input is too big, maximum size is %d bytes\n", MAX_IN);
					return false;
				}
				out_buffer.insert(out_buffer.end(), chunk.data(), chunk.data() + amount);
			}
		}
		if (!input.eof()) {
			fprintf(stderr, "Couldn't read '%s'\n", name);
			return false;
		}
		return true;
	};

	if (use_stdin) {
		if (!appendInput(std::cin, "standard input"))
			return 1;
	} else {
		do {
			in_filename = argv[1];
			std::ifstream in_fp(in_filename, std::ios::binary);

			if (!in_fp.is_open()) {
				fprintf(stderr, "Couldn't open '%s' for reading\n", in_filename);
				return 1;
			}

			if (!appendInput(in_fp, in_filename))
				return 1;
			argc--;
			argv++;
		} while (argc > 1);
	}

	const std::size_t out_size = out_buffer.size();
	for (std::size_t i = 0; i < out_size; i++) {
		out_buffer[i] = key_table[out_buffer[i] ^ 0x71] ^ 0x45;
	}

	const unsigned char empty_input = 0;
	uLongf compressed_size = compressBound(static_cast<uLong>(out_size));
	std::vector<unsigned char> compressed_data(compressed_size);
	int gz_result = compress(compressed_data.data(),
	                         &compressed_size,
	                         out_buffer.empty() ? &empty_input : out_buffer.data(),
	                         static_cast<uLong>(out_size));

	if (gz_result != Z_OK) {
		fprintf(stderr, "Compression error\n");
		return 1;
	}

	if (use_stdout)
		out_fp = stdout;
	else if (out_filename)
		out_fp = fopen(out_filename, "wb");
	else
		out_fp = fopen(DEF_OUT, "wb");

	if (!out_fp && !use_stdout) {
		if (out_filename)
			fprintf(stderr, "Couldn't open '%s' for writing\n", out_filename);
		else
			fprintf(stderr, "Couldn't open '%s' for writing\n", DEF_OUT);
		return 1;
	}

	auto write_u32_le = [out_fp](std::uint32_t value) {
		unsigned char bytes[4] = {
			static_cast<unsigned char>(value),
			static_cast<unsigned char>(value >> 8),
			static_cast<unsigned char>(value >> 16),
			static_cast<unsigned char>(value >> 24)};
		return fwrite(bytes, sizeof(bytes), 1, out_fp) == 1;
	};

	if (compressed_size > std::numeric_limits<std::uint32_t>::max() ||
	    out_size > std::numeric_limits<std::uint32_t>::max() ||
	    fwrite("ONS2", 4, 1, out_fp) != 1 ||
	    !write_u32_le(static_cast<std::uint32_t>(compressed_size)) ||
	    !write_u32_le(static_cast<std::uint32_t>(out_size)) ||
	    !write_u32_le(version)) {
		fprintf(stderr, "Couldn't write script header\n");
		if (!use_stdout)
			fclose(out_fp);
		return 1;
	}

	for (uLongf i = 0; i < compressed_size; i++) {
		if (fputc(key_table[compressed_data[i] ^ 0x23] ^ 0x86, out_fp) == EOF) {
			fprintf(stderr, "Couldn't write compressed script\n");
			if (!use_stdout)
				fclose(out_fp);
			return 1;
		}
	}

	const bool flushFailed = fflush(out_fp) != 0;
	const bool closeFailed = !use_stdout && fclose(out_fp) != 0;
	if (flushFailed || closeFailed) {
		fprintf(stderr, "Couldn't finalize script output\n");
		return 1;
	}

	return 0;
}
