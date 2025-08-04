#!/bin/bash

# MongoDB Core Library Build Script

set -e  # Stop on error

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default configuration
BUILD_TYPE="Release"
BUILD_DIR="build"
CLEAN=false
RUN_TESTS=false
RUN_BENCHMARKS=false
SETUP_UNITY=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -b|--benchmark)
            RUN_BENCHMARKS=true
            shift
            ;;
        -u|--setup-unity)
            SETUP_UNITY=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  -d, --debug         Debug mode build"
            echo "  -c, --clean         Clean build directory"
            echo "  -t, --test          Run tests"
            echo "  -b, --benchmark     Run benchmarks"
            echo "  -u, --setup-unity   Setup Unity test framework"
            echo "  -h, --help          Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use $0 --help for help"
            exit 1
            ;;
    esac
done

echo -e "${BLUE}=== MongoDB Core Library Build Script ===${NC}"

# Setup Unity (if needed)
if [ "$SETUP_UNITY" = true ]; then
    echo -e "${BLUE}Setting up Unity test framework...${NC}"
    chmod +x scripts/setup_unity.sh
    ./scripts/setup_unity.sh
fi

# Clean build directory (if needed)
if [ "$CLEAN" = true ]; then
    echo -e "${BLUE}Cleaning build directory...${NC}"
    rm -rf $BUILD_DIR
fi

# Create build directory
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Run CMake configuration
echo -e "${BLUE}Configuring project (build type: $BUILD_TYPE)...${NC}"
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Build project
echo -e "${BLUE}Building project...${NC}"
cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo -e "${GREEN}✅ Build completed!${NC}"

# Run tests (if needed)
if [ "$RUN_TESTS" = true ]; then
    echo -e "${BLUE}Running tests...${NC}"
    if ctest --output-on-failure; then
        echo -e "${GREEN}✅ All tests passed!${NC}"
    else
        echo -e "${RED}❌ Some tests failed!${NC}"
        exit 1
    fi
fi

# Run benchmarks (if needed)
if [ "$RUN_BENCHMARKS" = true ]; then
    echo -e "${BLUE}Running benchmarks...${NC}"
    for benchmark in bin/benchmark_*; do
        if [ -x "$benchmark" ]; then
            echo -e "${BLUE}Running $benchmark${NC}"
            $benchmark
        fi
    done
fi

echo -e "${GREEN}🎉 Done!${NC}"
echo -e "Build artifacts located at: ${BLUE}$BUILD_DIR/${NC}"
echo -e "Static library: ${BLUE}$BUILD_DIR/lib/libmongory-core.a${NC}"