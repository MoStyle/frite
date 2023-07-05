#!/bin/bash

echo "Deploying AppImages for " $1

# Icons
mkdir -p frite.AppDir/usr/share/icons/hicolor/512x512/apps
mkdir -p frite.AppDir/usr/share/icons/hicolor/256x256/apps
mkdir -p frite.AppDir/usr/share/icons/hicolor/128x128/apps
mkdir -p frite.AppDir/usr/share/icons/hicolor/64x64/apps
mkdir -p frite.AppDir/usr/share/icons/hicolor/48x48/apps
mkdir -p frite.AppDir/usr/share/icons/hicolor/32x32/apps
cp ../src/images/fries.png frite.AppDir/usr/share/icons/hicolor/512x512/apps/frite.png
convert -resize 256 frite.AppDir/usr/share/icons/hicolor/512x512/apps/frite.png frite.AppDir/usr/share/icons/hicolor/256x256/apps/frite.png
convert -resize 128 frite.AppDir/usr/share/icons/hicolor/512x512/apps/frite.png frite.AppDir/usr/share/icons/hicolor/128x128/apps/frite.png
convert -resize 64 frite.AppDir/usr/share/icons/hicolor/512x512/apps/frite.png frite.AppDir/usr/share/icons/hicolor/64x64/apps/frite.png
convert -resize 48 frite.AppDir/usr/share/icons/hicolor/512x512/apps/frite.png frite.AppDir/usr/share/icons/hicolor/48x48/apps/frite.png
convert -resize 32 frite.AppDir/usr/share/icons/hicolor/512x512/apps/frite.png frite.AppDir/usr/share/icons/hicolor/32x32/apps/frite.png

# Desktop file
mkdir -p frite.AppDir/usr/share/applications
cp frite.desktop frite.AppDir/usr/share/applications/

# AppImage
wget -c -nv "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt-continuous-x86_64.AppImage
./linuxdeployqt-continuous-x86_64.AppImage frite.AppDir/usr/share/applications/frite.desktop -verbose=1 -no-translations -always-overwrite -appimage -extra-plugins=iconengines,platforms,platforminputcontexts,xcbglintegrations

mv frite*.AppImage frite-x86_64.AppImage
