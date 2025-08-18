cmake_minimum_required(VERSION 3.12)
project(mongory-core VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compilation options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(include)

# Find dependency libraries
find_package(PkgConfig QUIET)

# Try to find cjson using pkg-config
if(PkgConfig_FOUND)
    pkg_check_modules(CJSON libcjson)
endif()

# If pkg-config can't find it or is unavailable, search manually
if(NOT CJSON_FOUND)
    find_path(CJSON_INCLUDE_DIR 
        NAMES cjson/cJSON.h cJSON.h
        PATHS
            /usr/include
            /usr/local/include
            /opt/homebrew/include
        PATH_SUFFIXES cjson
    )
    
    find_library(CJSON_LIBRARY
        NAMES cjson
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/homebrew/lib
    )
    
    if(CJSON_INCLUDE_DIR AND CJSON_LIBRARY)
        set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIR})
        set(CJSON_LIBRARIES ${CJSON_LIBRARY})
        set(CJSON_LDFLAGS ${CJSON_LIBRARY})
        set(CJSON_FOUND TRUE)
        message(STATUS "Found cJSON manually: ${CJSON_LIBRARY}")
    endif()
endif()

if(NOT CJSON_FOUND)
    message(FATAL_ERROR "cJSON library not found. Please install libcjson-dev (Ubuntu/Debian) or cjson (Homebrew)")
endif()

# Include cJSON header directories
include_directories(${CJSON_INCLUDE_DIRS})

# Collect source files
file(GLOB_RECURSE CORE_SOURCES 
    "src/foundations/*.c"
    "src/matchers/*.c"
)

# Exclude test_helper
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/test_helper/.*")

# Create core static library
add_library(mongory-core STATIC ${CORE_SOURCES})
if(CJSON_LDFLAGS)
    target_link_libraries(mongory-core ${CJSON_LDFLAGS})
else()
    target_link_libraries(mongory-core ${CJSON_LIBRARIES})
endif()

# Link math library for platforms that require -lm (e.g., Linux)
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(mongory-core ${MATH_LIBRARY})
endif()

# Test helper library (for testing)
add_library(test_helper STATIC src/test_helper/test_helper.c)
target_include_directories(test_helper PUBLIC src/test_helper)
if(CJSON_LDFLAGS)
    target_link_libraries(test_helper mongory-core ${CJSON_LDFLAGS})
else()
    target_link_libraries(test_helper mongory-core ${CJSON_LIBRARIES})
endif()

# Unity test framework setup
set(UNITY_DIR ${CMAKE_SOURCE_DIR}/tests/unity)
if(EXISTS ${UNITY_DIR}/unity.c)
    add_library(unity STATIC ${UNITY_DIR}/unity.c)
    target_include_directories(unity PUBLIC ${UNITY_DIR})
    target_compile_definitions(unity PUBLIC UNITY_USE_COLOR UNITY_OUTPUT_COLOR)
else()
    message(STATUS "Unity framework not found. Run setup_unity.sh first.")
endif()

# Test options
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS AND TARGET unity)
    enable_testing()
    
    # Copy test JSON files to the correct location in build directory
    file(COPY tests/jsons DESTINATION ${CMAKE_BINARY_DIR}/tests)
    
    # Collect test files
    file(GLOB TEST_SOURCES "tests/mongory_*_test.c")
    
    foreach(test_source ${TEST_SOURCES})
        get_filename_component(test_name ${test_source} NAME_WE)
        add_executable(${test_name} ${test_source})
        if(CJSON_LDFLAGS)
            target_link_libraries(${test_name} 
                mongory-core 
                test_helper 
                unity 
                ${CJSON_LDFLAGS}
            )
        else()
            target_link_libraries(${test_name} 
                mongory-core 
                test_helper 
                unity 
                ${CJSON_LIBRARIES}
            )
        endif()
        if(MATH_LIBRARY)
            target_link_libraries(${test_name} ${MATH_LIBRARY})
        endif()
        target_include_directories(${test_name} PRIVATE 
            tests
            ${UNITY_DIR}
        )
        target_compile_definitions(${test_name} PRIVATE 
            UNITY_USE_COLOR 
            UNITY_OUTPUT_COLOR
        )
        
        # Add to CTest
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()

# Benchmark options
option(BUILD_BENCHMARKS "Build the benchmarks" ON)
if(BUILD_BENCHMARKS AND TARGET unity)
    # Collect benchmark files
    file(GLOB BENCHMARK_SOURCES "benchmarks/*.c")
    
    foreach(benchmark_source ${BENCHMARK_SOURCES})
        get_filename_component(benchmark_name ${benchmark_source} NAME_WE)
        set(benchmark_target "benchmark_${benchmark_name}")
        add_executable(${benchmark_target} ${benchmark_source})
        if(CJSON_LDFLAGS)
            target_link_libraries(${benchmark_target} 
                mongory-core 
                test_helper 
                unity 
                ${CJSON_LDFLAGS}
            )
        else()
            target_link_libraries(${benchmark_target} 
                mongory-core 
                test_helper 
                unity 
                ${CJSON_LIBRARIES}
            )
        endif()
        if(MATH_LIBRARY)
            target_link_libraries(${benchmark_target} ${MATH_LIBRARY})
        endif()
        target_include_directories(${benchmark_target} PRIVATE 
            tests
            ${UNITY_DIR}
        )
        target_compile_definitions(${benchmark_target} PRIVATE 
            UNITY_USE_COLOR 
            UNITY_OUTPUT_COLOR
        )
    endforeach()
endif()

# Custom target: Setup Unity
add_custom_target(setup-unity
    COMMAND chmod +x ${CMAKE_SOURCE_DIR}/scripts/setup_unity.sh
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/setup_unity.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Setting up Unity test framework"
)

# Custom target: Format code
add_custom_target(format
    COMMAND find ${CMAKE_SOURCE_DIR} -name "*.c" -o -name "*.h" | xargs clang-format -i
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting code with clang-format"
)

# Install rules
install(TARGETS mongory-core
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Display configuration information
message(STATUS "=== MongoDB Core Library Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "cJSON found: ${CJSON_FOUND}")
if(CJSON_FOUND)
    message(STATUS "cJSON include dirs: ${CJSON_INCLUDE_DIRS}")
    message(STATUS "cJSON libraries: ${CJSON_LIBRARIES}")
endif()
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "==========================================")