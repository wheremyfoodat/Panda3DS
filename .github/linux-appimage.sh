# Prepare Tools for building the AppImage
export ARCH=$(uname -m)
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage
chmod a+x linuxdeploy-${ARCH}.AppImage

# Build AppImage
./linuxdeploy-${ARCH}.AppImage --appdir AppDir -d ./.github/Alber.desktop  -e ./build/Alber -i ./docs/img/Alber.png --output appimage
