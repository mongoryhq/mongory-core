cmake_minimum_required(VERSION 3.12)
project(mongory-core VERSION 1.0.0 LANGUAGES C)

# 設置 C 標準
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 編譯選項
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# 輸出目錄
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 包含目錄
include_directories(include)

# 查找依賴庫
find_package(PkgConfig QUIET)

# 嘗試使用 pkg-config 查找 cjson
if(PkgConfig_FOUND)
    pkg_check_modules(CJSON libcjson)
endif()

# 如果 pkg-config 找不到或不可用，手動查找
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

# 包含 cJSON 頭文件目錄
include_directories(${CJSON_INCLUDE_DIRS})

# 蒐集源文件
file(GLOB_RECURSE CORE_SOURCES 
    "src/foundations/*.c"
    "src/matchers/*.c"
)

# 排除 test_helper
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/test_helper/.*")

# 創建核心靜態庫
add_library(mongory-core STATIC ${CORE_SOURCES})
if(CJSON_LDFLAGS)
    target_link_libraries(mongory-core ${CJSON_LDFLAGS})
else()
    target_link_libraries(mongory-core ${CJSON_LIBRARIES})
endif()

# Test helper 庫（用於測試）
add_library(test_helper STATIC src/test_helper/test_helper.c)
target_include_directories(test_helper PUBLIC src/test_helper)

# Unity 測試框架設置
set(UNITY_DIR ${CMAKE_SOURCE_DIR}/tests/unity)
if(EXISTS ${UNITY_DIR}/unity.c)
    add_library(unity STATIC ${UNITY_DIR}/unity.c)
    target_include_directories(unity PUBLIC ${UNITY_DIR})
    target_compile_definitions(unity PUBLIC UNITY_USE_COLOR UNITY_OUTPUT_COLOR)
else()
    message(STATUS "Unity framework not found. Run setup_unity.sh first.")
endif()

# 測試選項
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS AND TARGET unity)
    enable_testing()
    
    # 複製測試 JSON 文件到構建目錄的正確位置
    file(COPY tests/jsons DESTINATION ${CMAKE_BINARY_DIR}/tests)
    
    # 蒐集測試文件
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
        target_include_directories(${test_name} PRIVATE 
            tests
            ${UNITY_DIR}
        )
        target_compile_definitions(${test_name} PRIVATE 
            UNITY_USE_COLOR 
            UNITY_OUTPUT_COLOR
        )
        
        # 添加到 CTest
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()

# 基準測試選項
option(BUILD_BENCHMARKS "Build the benchmarks" ON)
if(BUILD_BENCHMARKS AND TARGET unity)
    # 蒐集基準測試文件
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

# 自定義目標：設置 Unity
add_custom_target(setup-unity
    COMMAND chmod +x ${CMAKE_SOURCE_DIR}/scripts/setup_unity.sh
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/setup_unity.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Setting up Unity test framework"
)

# 自定義目標：格式化代碼
add_custom_target(format
    COMMAND find ${CMAKE_SOURCE_DIR} -name "*.c" -o -name "*.h" | xargs clang-format -i
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting code with clang-format"
)

# 安裝規則
install(TARGETS mongory-core
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# 顯示配置資訊
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