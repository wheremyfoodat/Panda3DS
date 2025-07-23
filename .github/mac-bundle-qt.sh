# Taken from pcsx-redux create-app-bundle.sh
# For Plist buddy
PATH="$PATH:/usr/libexec"

# Construct the app iconset.
mkdir alber.iconset

# Create a mask for rounding our icon. We don't want it to be square, as most MacOS icons are rounded
convert -size 1024x1024 xc:none -draw "roundrectangle 0,0,1024,1024,220,220" rounded_mask.png
convert docs/img/mac_icon.ico -alpha on -background none -resize 1024x1024 PNG32:temp.png
# Apply the mask to our icon
convert temp.png rounded_mask.png -compose DstIn -composite temp.png

convert temp.png -resize 16x16    alber.iconset/icon_16x16.png
convert temp.png -resize 32x32    alber.iconset/icon_16x16@2x.png
convert temp.png -resize 32x32    alber.iconset/icon_32x32.png
convert temp.png -resize 64x64    alber.iconset/icon_32x32@2x.png
convert temp.png -resize 128x128  alber.iconset/icon_128x128.png
convert temp.png -resize 256x256  alber.iconset/icon_128x128@2x.png
convert temp.png -resize 256x256  alber.iconset/icon_256x256.png
convert temp.png -resize 512x512  alber.iconset/icon_256x256@2x.png
convert temp.png -resize 512x512  alber.iconset/icon_512x512.png
convert temp.png -resize 1024x1024 alber.iconset/icon_512x512@2x.png
iconutil --convert icns alber.iconset
rm rounded_mask.png temp.png

# Set up the .app directory
mkdir -p Alber.app/Contents/MacOS/Libraries
mkdir Alber.app/Contents/Resources

# Copy binary into App
cp ./build/Alber Alber.app/Contents/MacOS/Alber
chmod a+x Alber.app/Contents/Macos/Alber

# Copy icons into App
cp alber.icns Alber.app/Contents/Resources/AppIcon.icns

# Fix up Plist stuff
PlistBuddy Alber.app/Contents/Info.plist -c "add CFBundleDisplayName string Alber"
PlistBuddy Alber.app/Contents/Info.plist -c "add CFBundleIconName string AppIcon"
PlistBuddy Alber.app/Contents/Info.plist -c "add CFBundleIconFile string AppIcon"
PlistBuddy Alber.app/Contents/Info.plist -c "add NSHighResolutionCapable bool true"
PlistBuddy Alber.app/Contents/version.plist -c "add ProjectName string Alber"

PlistBuddy Alber.app/Contents/Info.plist -c "add CFBundleExecutable string Alber"
PlistBuddy Alber.app/Contents/Info.plist -c "add CFBundleDevelopmentRegion string en"
PlistBuddy Alber.app/Contents/Info.plist -c "add CFBundleInfoDictionaryVersion string 6.0"
PlistBuddy Alber.app/Contents/Info.plist -c "add CFBundleName string Panda3DS"
PlistBuddy Alber.app/Contents/Info.plist -c "add CFBundlePackageType string APPL"
PlistBuddy Alber.app/Contents/Info.plist -c "add NSHumanReadableCopyright string Copyright 2023 Panda3DS Team"

PlistBuddy Alber.app/Contents/Info.plist -c "add LSMinimumSystemVersion string 10.15"

# Bundle dylibs
ruby .github/mac-libs.rb ./build/

# relative rpath
install_name_tool -add_rpath @loader_path/../Frameworks Alber.app/Contents/MacOS/Alber
