#!/bin/bash

# Settings for the SwiftUI frontend
ARCH=arm64
CONFIGURATION=Release
SDK=iphonesimulator18.2

# Settings for the emulator core
EMULATOR_BUILD_TYPE=Release

# Simulator settings
RUN_SIMULATOR=false

# Go to the parent directory and build the emulator core
cd ../..
cmake -B build -DENABLE_VULKAN=OFF -DBUILD_HYDRA_CORE=ON -DENABLE_USER_BUILD=ON -DCMAKE_TOOLCHAIN_FILE=third_party/ios_toolchain/ios.toolchain.cmake -DPLATFORM=SIMULATORARM64 -DDEPLOYMENT_TARGET="13.0"
cmake --build build --config ${EMULATOR_BUILD_TYPE}

# Copy the bridging header from the emulator source to the Swift interop module folder
cp ./include/ios_driver.h ./src/pandios/Alber/Headers/ios_driver.h

# Come back to the iOS directory, build the SwiftUI xcode project and link them together
cd src/pandios
xcodebuild build -configuration ${CONFIGURATION} -sdk ${SDK} -arch ${ARCH}

# To run the app in the simulator:
# Boot the iPhone, install the app on the iphone, then open the sim & launch the app
if [ "$RUN_SIMULATOR" = true ] ; then
    xcrun simctl boot "iPhone 16 Pro"
    xcrun simctl install "iPhone 16 Pro" "build/Release-iphonesimulator/Pandios.app"
    open /Applications/Xcode.app/Contents/Developer/Applications/Simulator.app
    xcrun simctl launch --console booted "Alber.Pandios"
fi