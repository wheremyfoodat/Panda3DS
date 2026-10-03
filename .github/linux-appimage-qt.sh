# Prepare Tools for building the AppImage
export ARCH=$(uname -m)
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage

chmod a+x linuxdeploy-${ARCH}.AppImage
chmod a+x linuxdeploy-plugin-qt-${ARCH}.AppImage

# Build AppImage
QMAKE=/usr/lib/qt6/bin/qmake ./linuxdeploy-${ARCH}.AppImage --appdir AppDir -d ./.github/Alber.desktop  -e ./build/Alber -i ./docs/img/Alber.png --plugin qt --output appimage
