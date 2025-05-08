#!/bin/bash

# 創建 Unity 目錄
mkdir -p tests/unity

# 下載 Unity 檔案
curl -o tests/unity/unity.h https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.h
curl -o tests/unity/unity.c https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.c
curl -o tests/unity/unity_internals.h https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity_internals.h

echo "Unity test framework has been set up successfully!" 