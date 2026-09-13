#!/bin/bash

set -e
unset SOURCE_DATE_EPOCH

# Manually create the AppImage (reproducibly) since linuxdeployqt is not able to create cross-compiled AppImages

APPDIR="$PWD/wowlet.AppDir"

mkdir -p "$APPDIR"
mkdir -p "$APPDIR/usr/share/applications/"
mkdir -p "$APPDIR/usr/bin"

cp "src/assets/wowlet.desktop" "$APPDIR/usr/share/applications/wowlet.desktop"
cp "src/assets/wowlet.desktop" "$APPDIR/wowlet.desktop"
cp "src/assets/images/appicons/64x64.png" "$APPDIR/wowlet.png"
cp "build/bin/wowlet" "$APPDIR/usr/bin/wowlet"
chmod +x "$APPDIR/usr/bin/wowlet"

cp "contrib/AppImage/AppRun" "$APPDIR/"
chmod +x "$APPDIR/AppRun"

find wowlet.AppDir/ -exec touch -h -a -m -t 202101010100.00 {} \;

mksquashfs wowlet.AppDir wowlet.squashfs -comp zstd -info -root-owned -no-xattrs -noappend -fstime 0
# mksquashfs writes a timestamp to the header
printf '\x00\x00\x00\x00' | dd conv=notrunc of=wowlet.squashfs bs=1 seek=$((0x8))

rm -f wowlet.AppImage

cat "${GITHUB_WORKSPACE:-/feather}/contrib/depends/${HOST}/runtime" >> wowlet.AppImage
cat wowlet.squashfs >> wowlet.AppImage
chmod a+x wowlet.AppImage
