cmake_minimum_required(VERSION 3.15)

if (NOT VERSION_MAJOR)
	set(VERSION_MAJOR 0)
endif()
if (NOT VERSION_MINOR)
	set(VERSION_MINOR 0)
endif()
if (NOT VERSION_PATCH)
	set(VERSION_PATCH 1)
endif()

# update this accordingly if there's version change
# use only stable release
set(UWEBSOCKETS_VERSION_MAJOR 20)
set(UWEBSOCKETS_VERSION_MINOR 70)
set(UWEBSOCKETS_VERSION_PATCH 0)

project("SSPlus Cache Me"
	VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
	LANGUAGES CXX)

option(DEBUG "Build ${CMAKE_PROJECT_NAME} with debug symbol" ON)
option(EXTERNAL_JSON "Use external nlohmann/json instead of the one included in the repo" OFF)

# ================================================================================

message("-- Configuring: ${CMAKE_PROJECT_NAME} - version ${CMAKE_PROJECT_VERSION}")

set(MAIN_EXE ssplus-cache-me)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

include(CheckIncludeFiles)

if (NOT EXTERNAL_JSON)
	message("-- Using BUNDLED nlohmann/json")
	set(NLOHMANN_JSON_H include/nlohmann/json.hpp)
	set(EXTERNAL_JSON_ENABLED 0)
else()
	message("-- Using EXTERNAL nlohmann/json")

	check_include_files(nlohmann/json.hpp HAVE_EXTERNAL_JSON)

	if (NOT HAVE_EXTERNAL_JSON)
		message(FATAL_ERROR "External nlohmann/json not found!")
	endif()

	set(EXTERNAL_JSON_ENABLED 1)
endif()

configure_file(
  "cmake/version.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/${MAIN_EXE}/version.h"
)

configure_file(
  "cmake/info.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/${MAIN_EXE}/info.h"
)

file(GLOB_RECURSE HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/include/${MAIN_EXE}/*.h)

file(GLOB_RECURSE SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/${MAIN_EXE}/*.cpp)

add_executable(${MAIN_EXE}
	${HEADERS}
	${NLOHMANN_JSON_H}
	libs/uWebSockets/uSockets/src/libusockets.h
	libs/uWebSockets/uSockets/src/quic.h

	${SOURCES}
	src/main.cpp)

target_compile_definitions(${MAIN_EXE} PRIVATE EXTERNAL_JSON=${EXTERNAL_JSON_ENABLED})

file(GLOB_RECURSE USOCKETS_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/src/*.c)

file(GLOB_RECURSE UWEBSOCKETS_HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/src/*.h)

set(USOCKETS_A "${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets/uSockets.a")

add_custom_target(uWebSockets
	DEPENDS ${USOCKETS_A})

add_custom_command(OUTPUT ${USOCKETS_A}
	COMMAND WITH_OPENSSL=1 WITH_ZLIB=1 make
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/libs/uWebSockets/uSockets
	DEPENDS ${USOCKETS_SOURCES} ${UWEBSOCKETS_HEADER_FILES})

add_dependencies(${MAIN_EXE} uWebSockets)

if (DEBUG)
	message("-- INFO: Will build ${CMAKE_PROJECT_NAME} with debug symbol")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

target_link_libraries(${MAIN_EXE}
	${USOCKETS_A}
	sqlite3
	ssl
	crypto
	z
	pthread)

target_include_directories(${MAIN_EXE} PRIVATE
	include
	libs
	libs/uWebSockets/uSockets/src)

set_target_properties(${MAIN_EXE} PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	EXPORT_COMPILE_COMMANDS ON)

# vim: sw=8 noet
