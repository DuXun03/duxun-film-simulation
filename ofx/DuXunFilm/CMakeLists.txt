cmake_minimum_required(VERSION 3.20)
project(DuXunFilmSim VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenFX SDK path (user must set or clone)
set(OFX_SDK "${CMAKE_SOURCE_DIR}/../OpenFX" CACHE PATH "Path to OpenFX SDK")

# Bundle output name
set(PLUGIN_NAME "独寻胶片模拟.ofx.bundle")
set(PLUGIN_CONTENTS "${CMAKE_BINARY_DIR}/${PLUGIN_NAME}/Contents")

# Plugin source files
set(SOURCES
    DuXunPlugin.cpp
    DuXunPluginFactory.cpp
    DuXunProcess.cpp
)

set(HEADERS
    DuXunPlugin.h
    DuXunProcess.h
)

# Create shared library
add_library(DuXunFilmSim SHARED ${SOURCES} ${HEADERS})

target_include_directories(DuXunFilmSim PRIVATE
    ${OFX_SDK}/include
    ${OFX_SDK}/Support/include
)

# Windows-specific settings
if(WIN32)
    set_target_properties(DuXunFilmSim PROPERTIES
        PREFIX ""
        SUFFIX ".ofx"
    )
    target_compile_definitions(DuXunFilmSim PRIVATE
        OFX_EXTENSIONS_VEGAS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# macOS settings
if(APPLE)
    set_target_properties(DuXunFilmSim PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "ofx.bundle"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
    )
endif()

# Install to build output
install(TARGETS DuXunFilmSim
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "."
)

# Copy film stock LUTs into bundle
install(DIRECTORY "${CMAKE_SOURCE_DIR}/../../presets/film_stocks/"
    DESTINATION "${PLUGIN_CONTENTS}/Resources/LUTs"
    FILES_MATCHING PATTERN "*.cube"
)

# Add preset data header
install(FILES "${CMAKE_SOURCE_DIR}/../../presets/film_stocks/film_presets.h"
    DESTINATION "${PLUGIN_CONTENTS}/Resources"
)

message(STATUS "DuXun FilmSim Plugin configured")
message(STATUS "  OFX SDK: ${OFX_SDK}")
message(STATUS "  Build: cmake --build . --config Release")
