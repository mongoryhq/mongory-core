#!/bin/bash

# create Unity directory
mkdir -p tests/unity

# download Unity files
curl -o tests/unity/unity.h https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.h
curl -o tests/unity/unity.c https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.c
curl -o tests/unity/unity_internals.h https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity_internals.h

echo "Unity test framework has been set up successfully!"
