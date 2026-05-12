cmake_minimum_required(VERSION 3.10)
project(ACUFullTest VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find threading library
find_package(Threads REQUIRED)
include(GNUInstallDirs)

# Enable testing
enable_testing()

# Compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")

# Enable position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(MAINFOLDER ${PROJECT_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH "${MAINFOLDER}/bin")
set(LIBRARY_OUTPUT_PATH "${MAINFOLDER}/lib")

# Collect all source files for formatting and analysis
file(
    GLOB_RECURSE
    ALL_SOURCES
    "src/*.cpp"
    "src/*.h"
    "tests/*.cpp"
    "tests/*.h"
)

# Clang-format
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    # cmake-format: off
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCES}
        COMMENT "Formatting code with clang-format"
    )
    # cmake-format: on
endif()

# Cppcheck
find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    # cmake-format: off
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK}
            --enable=all
            --std=c++17
            --language=c++
            --platform=unix64
            --suppress=missingIncludeSystem
            --suppress=missingInclude
            --inline-suppr
            --quiet
            --error-exitcode=1
            ${ALL_SOURCES}
        COMMENT "Running cppcheck static analysis"
    )
    # cmake-format: on
endif()

# Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    # cmake-format: off
    add_custom_target(doc_doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
    # cmake-format: on
else()
    message("Doxygen need to be installed to generate the doxygen documentation")
endif()

# Add src directory
add_subdirectory(src)

# Collect all source files from src for combined libraries
file(GLOB_RECURSE SRC_SOURCES "src/*.cpp")

# Create static library
add_library(acu_full_test STATIC ${SRC_SOURCES})
target_include_directories(acu_full_test PUBLIC src)
target_link_libraries(acu_full_test Threads::Threads)

# Create shared library
add_library(acu_full_test_shared SHARED ${SRC_SOURCES})
target_include_directories(acu_full_test_shared PUBLIC src)
target_link_libraries(acu_full_test_shared Threads::Threads)

# Install rules for aggregate libraries and headers
install(TARGETS acu_full_test
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(TARGETS acu_full_test_shared
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/acaida_hal
    FILES_MATCHING PATTERN "*.h"
)

# Add tests directory
add_subdirectory(tests)
