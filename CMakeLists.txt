cmake_minimum_required(VERSION 3.12)
project(mongory-core VERSION 0.2.1 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compilation options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Sanitizer options (e.g. -DMONGORY_SANITIZERS=address,undefined or -DMONGORY_SANITIZERS=thread)
set(MONGORY_SANITIZERS "" CACHE STRING "Comma-separated sanitizer list (e.g. address,undefined or thread)")
if(MONGORY_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "MONGORY_SANITIZERS is only supported with GCC/Clang toolchains")
    endif()
    set(MONGORY_SAN_C_FLAGS "-fsanitize=${MONGORY_SANITIZERS} -fno-omit-frame-pointer")
    if(MONGORY_SANITIZERS MATCHES "undefined")
        set(MONGORY_SAN_C_FLAGS "${MONGORY_SAN_C_FLAGS} -fno-sanitize-recover=all")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MONGORY_SAN_C_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${MONGORY_SANITIZERS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=${MONGORY_SANITIZERS}")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=${MONGORY_SANITIZERS}")
endif()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(include)

## Note: vendored yyjson (third_party/yyjson) is only built for tests/benchmarks;
## mongory-core itself remains zero-dependency.

# Collect source files
file(GLOB_RECURSE CORE_SOURCES 
    "src/foundations/*.c"
    "src/matchers/*.c"
)

# Exclude test_helper
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/test_helper/.*")

# Create core library (static by default; -DBUILD_SHARED_LIBS=ON builds shared with SOVERSION)
option(BUILD_SHARED_LIBS "Build mongory-core as a shared library" OFF)
if(WIN32 AND BUILD_SHARED_LIBS)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(mongory-core ${CORE_SOURCES})
add_library(mongory-core::mongory-core ALIAS mongory-core)

set_target_properties(mongory-core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(mongory-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Link math library for platforms that require -lm (e.g., Linux)
set(MONGORY_PC_LIBS_PRIVATE "")
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    target_link_libraries(mongory-core PRIVATE ${MATH_LIBRARY})
    set(MONGORY_PC_LIBS_PRIVATE "${MONGORY_PC_LIBS_PRIVATE} -lm")
endif()
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(mongory-core PRIVATE Threads::Threads)
    set(MONGORY_PC_LIBS_PRIVATE "${MONGORY_PC_LIBS_PRIVATE} -lpthread")
endif()
string(STRIP "${MONGORY_PC_LIBS_PRIVATE}" MONGORY_PC_LIBS_PRIVATE)

# Optional raw JSON adapter (opt-in; mongory-core itself stays zero-dependency)
option(MONGORY_WITH_JSON "Build optional yyjson raw JSON adapter (mongory-core-json)" OFF)
if(MONGORY_WITH_JSON)
    find_package(yyjson QUIET)
    if(TARGET yyjson::yyjson)
        set(MONGORY_YYJSON_TARGET yyjson::yyjson)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/yyjson/yyjson.c")
        add_library(yyjson STATIC third_party/yyjson/yyjson.c)
        target_include_directories(yyjson PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third_party/yyjson>
        )
        set_target_properties(yyjson PROPERTIES POSITION_INDEPENDENT_CODE ON)
        if(NOT MSVC)
            target_compile_options(yyjson PRIVATE -Wno-unused-parameter)
        endif()
        set(MONGORY_YYJSON_TARGET yyjson)
    else()
        include(FetchContent)
        FetchContent_Declare(
            yyjson
            GIT_REPOSITORY https://github.com/ibireme/yyjson.git
            GIT_TAG 0.13.0
        )
        set(YYJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(yyjson)
        set_target_properties(yyjson PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set(MONGORY_YYJSON_TARGET yyjson)
    endif()

    add_library(mongory-core-json STATIC src/adapters/json_adapter.c)
    add_library(mongory-core::json ALIAS mongory-core-json)
    set_target_properties(mongory-core-json PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(mongory-core-json
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(mongory-core-json PUBLIC mongory-core PRIVATE ${MONGORY_YYJSON_TARGET})
endif()

# Test options
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    # Unity test framework setup (available to tests and benchmarks)
    set(UNITY_DIR ${CMAKE_SOURCE_DIR}/tests/unity)
    if(EXISTS ${UNITY_DIR}/unity.c)
        add_library(unity STATIC ${UNITY_DIR}/unity.c)
        target_include_directories(unity PUBLIC ${UNITY_DIR})
        target_compile_definitions(unity PUBLIC UNITY_USE_COLOR UNITY_OUTPUT_COLOR)
    else()
        message(FATAL_ERROR "Unity framework not found. Run setup_unity.sh first, or disable tests with -DBUILD_TESTS=OFF.")
    endif()

    enable_testing()

    # yyjson (downloaded via scripts/setup_yyjson.sh; used only by tests/benchmarks)
    if(NOT TARGET yyjson)
        set(YYJSON_DIR ${CMAKE_SOURCE_DIR}/third_party/yyjson)
        if(EXISTS ${YYJSON_DIR}/yyjson.c)
            add_library(yyjson STATIC ${YYJSON_DIR}/yyjson.c)
            target_include_directories(yyjson PUBLIC ${YYJSON_DIR})
            if(NOT MSVC)
                target_compile_options(yyjson PRIVATE -Wno-unused-parameter)
            endif()
        else()
            message(FATAL_ERROR "yyjson not found. Run scripts/setup_yyjson.sh first, or disable tests with -DBUILD_TESTS=OFF.")
        endif()
    endif()

    # Copy test JSON files to the correct location in build directory
    file(COPY tests/jsons DESTINATION ${CMAKE_BINARY_DIR}/tests)

    # Collect test files
    file(GLOB TEST_SOURCES "tests/mongory_*_test.c")
    if(NOT MONGORY_WITH_JSON)
        list(FILTER TEST_SOURCES EXCLUDE REGEX "mongory_json_adapter_test\\.c$")
    endif()

    # Test helper library (for testing)
    add_library(test_helper STATIC src/test_helper/test_helper.c)
    target_include_directories(test_helper PUBLIC src/test_helper)
    target_link_libraries(test_helper mongory-core yyjson)
    
    foreach(test_source ${TEST_SOURCES})
        get_filename_component(test_name ${test_source} NAME_WE)
        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} 
            mongory-core 
            test_helper 
            unity 
            yyjson
        )
        if(MONGORY_WITH_JSON AND test_name STREQUAL "mongory_json_adapter_test")
            target_link_libraries(${test_name} mongory-core-json)
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

    # mongory-spec conformance tests (opt-in).
    # Usage: cmake -DMONGORY_SPEC_DIR=/path/to/mongory-spec ..
    set(MONGORY_SPEC_DIR "" CACHE PATH "Path to a mongory-spec checkout for conformance tests")
    if(MONGORY_SPEC_DIR)
        if(NOT EXISTS ${MONGORY_SPEC_DIR}/fixtures)
            message(FATAL_ERROR "MONGORY_SPEC_DIR does not contain fixtures/: ${MONGORY_SPEC_DIR}")
        endif()

        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(PCRE2 libpcre2-8)
        endif()
        if(NOT PCRE2_FOUND)
            message(FATAL_ERROR "libpcre2-8 is required for mongory-spec conformance tests")
        endif()

        add_executable(mongory_spec_conformance tests/conformance/mongory_spec_conformance.c)
        target_include_directories(mongory_spec_conformance PRIVATE ${PCRE2_INCLUDE_DIRS})
        target_link_directories(mongory_spec_conformance PRIVATE ${PCRE2_LIBRARY_DIRS})
        target_link_libraries(mongory_spec_conformance
            mongory-core
            test_helper
            unity
            yyjson
            ${PCRE2_LIBRARIES}
        )
        if(MATH_LIBRARY)
            target_link_libraries(mongory_spec_conformance ${MATH_LIBRARY})
        endif()

        file(GLOB_RECURSE MONGORY_SPEC_FIXTURES ${MONGORY_SPEC_DIR}/fixtures/*.json)
        list(SORT MONGORY_SPEC_FIXTURES)
        foreach(fixture ${MONGORY_SPEC_FIXTURES})
            file(RELATIVE_PATH fixture_name ${MONGORY_SPEC_DIR}/fixtures ${fixture})
            add_test(NAME spec/${fixture_name} COMMAND mongory_spec_conformance ${fixture})
            set_tests_properties(spec/${fixture_name} PROPERTIES LABELS conformance)
        endforeach()
        list(LENGTH MONGORY_SPEC_FIXTURES MONGORY_SPEC_FIXTURE_COUNT)
        message(STATUS "mongory-spec conformance fixtures: ${MONGORY_SPEC_FIXTURE_COUNT}")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/fuzz/fuzz_matcher.c")
        add_executable(mongory_fuzz_smoke tests/fuzz/fuzz_matcher.c)
        target_compile_definitions(mongory_fuzz_smoke PRIVATE MONGORY_FUZZ_STANDALONE)
        target_link_libraries(mongory_fuzz_smoke mongory-core)
        if(MATH_LIBRARY)
            target_link_libraries(mongory_fuzz_smoke ${MATH_LIBRARY})
        endif()
        add_test(NAME mongory_fuzz_smoke COMMAND mongory_fuzz_smoke)
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/fuzz/fuzz_json_condition.c")
        add_executable(mongory_fuzz_json_smoke tests/fuzz/fuzz_json_condition.c)
        target_compile_definitions(mongory_fuzz_json_smoke PRIVATE MONGORY_FUZZ_STANDALONE)
        target_link_libraries(mongory_fuzz_json_smoke mongory-core yyjson)
        if(MATH_LIBRARY)
            target_link_libraries(mongory_fuzz_json_smoke ${MATH_LIBRARY})
        endif()
        add_test(NAME mongory_fuzz_json_smoke COMMAND mongory_fuzz_json_smoke)
    endif()
endif()

# Benchmark options
option(BUILD_BENCHMARKS "Build the benchmarks" ON)
if(BUILD_BENCHMARKS)
    if(NOT TARGET unity)
        message(STATUS "Unity framework not found; skipping benchmarks.")
    else()
        if(NOT TARGET yyjson)
            set(YYJSON_DIR ${CMAKE_SOURCE_DIR}/third_party/yyjson)
            if(EXISTS ${YYJSON_DIR}/yyjson.c)
                add_library(yyjson STATIC ${YYJSON_DIR}/yyjson.c)
                target_include_directories(yyjson PUBLIC ${YYJSON_DIR})
                if(NOT MSVC)
                    target_compile_options(yyjson PRIVATE -Wno-unused-parameter)
                endif()
            else()
                message(FATAL_ERROR "yyjson not found for benchmarks. Run scripts/setup_yyjson.sh first, or disable benchmarks with -DBUILD_BENCHMARKS=OFF.")
            endif()
        endif()

        # Collect benchmark files
        file(GLOB BENCHMARK_SOURCES "benchmarks/*.c")
        if(NOT MONGORY_WITH_JSON)
            list(FILTER BENCHMARK_SOURCES EXCLUDE REGEX "json_filter\\.c$")
        endif()
        
        foreach(benchmark_source ${BENCHMARK_SOURCES})
            get_filename_component(benchmark_name ${benchmark_source} NAME_WE)
            set(benchmark_target "benchmark_${benchmark_name}")
            add_executable(${benchmark_target} ${benchmark_source})
            target_link_libraries(${benchmark_target} 
                mongory-core 
                test_helper 
                unity 
                yyjson
            )
            if(MONGORY_WITH_JSON AND benchmark_name STREQUAL "json_filter")
                target_link_libraries(${benchmark_target} mongory-core-json)
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

# Install rules, CMake package config, and pkg-config
install(TARGETS mongory-core
    EXPORT mongory-coreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN "adapters" EXCLUDE
)

if(MONGORY_WITH_JSON)
    install(FILES include/mongory-core/adapters/json.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mongory-core/adapters
    )
    install(TARGETS mongory-core-json
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

install(EXPORT mongory-coreTargets
    FILE mongory-coreTargets.cmake
    NAMESPACE mongory-core::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mongory-core
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mongory-coreConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/mongory-coreConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mongory-core
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/mongory-coreConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mongory-coreConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/mongory-coreConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mongory-core
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/mongory-core.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/mongory-core.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mongory-core.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Display configuration information
message(STATUS "=== MongoDB Core Library Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION} (SOVERSION ${PROJECT_VERSION_MAJOR})")
message(STATUS "Shared library (BUILD_SHARED_LIBS): ${BUILD_SHARED_LIBS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "==========================================")