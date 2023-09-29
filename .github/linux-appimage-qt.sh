# Prepare Tools for building the AppImage
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

chmod a+x linuxdeploy-x86_64.AppImage
chmod a+x linuxdeploy-plugin-qt-x86_64.AppImage

# Build AppImage
./linuxdeploy-x86_64.AppImage --appdir AppDir -d ./.github/Alber.desktop  -e ./build/Alber -i ./docs/img/Alber.png --output appimage --plugin qt
