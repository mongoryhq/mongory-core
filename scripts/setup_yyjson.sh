#!/usr/bin/env bash
set -euo pipefail

YYJSON_VERSION="${YYJSON_VERSION:-0.13.0}"
BASE_URL="https://raw.githubusercontent.com/ibireme/yyjson/${YYJSON_VERSION}"

mkdir -p third_party/yyjson
curl -fsSL "${BASE_URL}/src/yyjson.h" -o third_party/yyjson/yyjson.h
curl -fsSL "${BASE_URL}/src/yyjson.c" -o third_party/yyjson/yyjson.c
curl -fsSL "${BASE_URL}/LICENSE" -o third_party/yyjson/LICENSE

echo "yyjson ${YYJSON_VERSION} has been set up successfully!"
