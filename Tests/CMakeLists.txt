cmake_minimum_required(VERSION 3.20)

project(onscripter_new_tests LANGUAGES C CXX)

include(CTest)

option(ONS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ONS_BUILD_FUZZERS "Build the libFuzzer targets" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(ONS_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_library(ons_test_options INTERFACE)
target_include_directories(ons_test_options INTERFACE "${ONS_SOURCE_ROOT}")

# The tests intentionally use assert() for both checks and expressions with
# side effects. Keep those checks active when validating a Release build.
if(MSVC)
	target_compile_options(ons_test_options INTERFACE /UNDEBUG)
else()
	target_compile_options(ons_test_options INTERFACE -UNDEBUG)
endif()

if(ONS_ENABLE_SANITIZERS)
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
		message(FATAL_ERROR "Sanitizers require Clang or GCC")
	endif()
	target_compile_options(ons_test_options INTERFACE
		-fsanitize=address,undefined
		-fno-omit-frame-pointer
	)
	target_link_options(ons_test_options INTERFACE -fsanitize=address,undefined)
endif()

function(add_ons_test target)
	add_executable(${target} ${ARGN})
	target_link_libraries(${target} PRIVATE ons_test_options)
	add_test(NAME ${target} COMMAND ${target})
endfunction()

add_ons_test(archive_parser_tests
	ArchiveParserTests.cpp
	"${ONS_SOURCE_ROOT}/Engine/Readers/ArchiveParser.cpp"
)
add_ons_test(command_line_tests
	CommandLineTests.cpp
	"${ONS_SOURCE_ROOT}/Engine/Core/CommandLine.cpp"
)
add_ons_test(serialized_data_tests SerializedDataTests.cpp)
add_ons_test(slre_tests
	SlreTests.cpp
	"${ONS_SOURCE_ROOT}/External/slre.c"
)
add_ons_test(variable_tests VariableTests.cpp)

if(ONS_BUILD_FUZZERS)
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		message(FATAL_ERROR "The fuzz targets require Clang and libFuzzer")
	endif()

	function(add_ons_fuzzer target)
		add_executable(${target} ${ARGN})
		target_link_libraries(${target} PRIVATE ons_test_options)
		target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
		target_link_options(${target} PRIVATE -fsanitize=fuzzer)
	endfunction()

	add_ons_fuzzer(fuzz_archive_parser
		FuzzArchiveParser.cpp
		"${ONS_SOURCE_ROOT}/Engine/Readers/ArchiveParser.cpp"
	)
	add_ons_fuzzer(fuzz_command_line
		FuzzCommandLine.cpp
		"${ONS_SOURCE_ROOT}/Engine/Core/CommandLine.cpp"
	)
	add_ons_fuzzer(fuzz_serialized_data FuzzSerializedData.cpp)
	add_ons_fuzzer(fuzz_slre
		FuzzSlre.cpp
		"${ONS_SOURCE_ROOT}/External/slre.c"
	)
endif()
