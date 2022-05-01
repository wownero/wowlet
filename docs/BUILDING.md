# Building WOWlet

Building for Linux and Windows via Docker is done in 3 steps:

1. Cloning this repository (+submodules)
2. Creating a base Docker image
3. Using the base image to compile a build

**important:** you only have to do step 2 (base docker image) once.

For Mac OS, scroll down.

# Linux

For more information, check the Dockerfile: `Dockerfile`.

### 1. Clone

```bash
git clone --branch master --recursive https://git.wownero.com/wowlet/wowlet.git
cd wowlet
```

Replace `master` with the desired version tag (e.g. `v3.1.0`) to build the release binary.

### 2. Base image

```bash
docker build --tag wowlet:linux --build-arg THREADS=6 .
```

Building the base image takes a while. **You only need to build the base image once.**

### 3. Build

```bash
docker run --rm -it -v $PWD:/wowlet -w /wowlet wowlet:linux sh -c 'make release-static -j6'
```

If you're re-running a build make sure to `rm -rf build/` first.

The resulting binary can be found in `build/bin/wowlet`.

# Windows

### 1. Clone

```bash
git clone --branch master --recursive https://git.wownero.com/wowlet/wowlet.git
cd wowlet
```

Replace `master` with the desired version tag (e.g. `v3.1.0`) to build the release binary.

### 2. Base image

```bash
docker build -f Dockerfile.windows --tag wowlet:win --build-arg THREADS=6 .
```

Building the base image takes a while. **You only need to build the base image once.**

### 3. Build

```bash
docker run --rm -it -v $PWD:/wowlet -w /wowlet wowlet:win sh -c 'make windows root=/depends target=x86_64-w64-mingw32 tag=win-x64 -j6'
```

If you're re-running a build make sure to `rm -rf build/` first.

The resulting binary can be found in `build/x86_64-w64-mingw32/release/bin/wowlet.exe`.

# Mac OS

## method 1 (easiest)

### 1. Get homebrew

Get [brew](https://brew.sh) to install the required dependencies. 

```bash
HOMEBREW_OPTFLAGS="-march=core2" HOMEBREW_OPTIMIZATION_LEVEL="O0" \
    brew install boost zmq openssl libpgm miniupnpc libsodium expat libunwind-headers protobuf libgcrypt qrencode ccache cmake pkgconfig git
```

### 2. Compile WOWlet

```bash
CMAKE_PREFIX_PATH=/usr/local/opt/qt5/ make -j4 mac
```

The resulting Mac OS application can be found `build/bin/wowlet.app` and will **not** have Tor embedded.

Since WOWlet needs Tor, install it, start it, and start at Mac OS boot:
```bash
brew install tor
brew services start tor
```

## method 2 (advanced, intel/m1 release binaries)

This assumes you have homebrew installed with the packages defined in the previous step ("Get homebrew").

### 1. Get Qt

Install Qt `5.15.2` from [the open-source Qt installer](https://www.qt.io/download).

### 2. Get static Boost

We'll install boost under `/Users/$USER/build/boost/`

```bash
mkdir -p "/Users/$USER/build/boost"

wget https://boostorg.jfrog.io/artifactory/main/release/1.73.0/source/boost_1_73_0.tar.gz && \
    echo "9995e192e68528793755692917f9eb6422f3052a53c5e13ba278a228af6c7acf boost_1_73_0.tar.gz" | sha256sum -c && \
    tar -xzf boost_1_73_0.tar.gz && \
    rm boost_1_73_0.tar.gz && \
    cd boost_1_73_0

./bootstrap.sh --without-icu
./b2 --disable-icu --with-atomic --with-system --with-filesystem --with-thread \
     --with-date_time --with-chrono --with-regex --with-serialization \
     --with-program_options --with-locale variant=release link=static \
     runtime-link=static cxxflags='-std=c++11' install -a --prefix="/Users/$USER/build/boost/"
```

### 3. Get static Tor

1. Download the official Tor Browser `.dmg`
2. Steal `tor.real` and `libevent-2.1.7.dylib` from the `.dmg`
3. Place them both in `src/assets/exec/`
  - `src/assets/exec/tor`
  - `src/assets/exec/libevent-2.1.7.dylib`

### 4. Get static Tor

```bash
CMAKE_PREFIX_PATH=/Users/dsc/Qt5.15.2/5.15.2/clang_64 TOR_BIN="foo" make -j10 mac-release
```

Resulting *static* Mac OS package: `build/bin/wowlet.app`

## method 3 (from source, WIP, does not work yet)

Download Qt `https://download.qt.io/archive/qt/5.15/5.15.3/single/`, unpack it somewhere.

Patch Qt 5.15.3 source.

- Context: [#1](https://github.com/microsoft/vcpkg/pull/21056) [#2](https://code.qt.io/cgit/qt/qtbase.git/diff/?id=dece6f5840463ae2ddf927d65eb1b3680e34a547)

```
diff --git a/src/plugins/platforms/cocoa/qiosurfacegraphicsbuffer.h b/src/plugins/platforms/cocoa/qiosurfacegraphicsbuffer.h
index 5d4b6d6a71..cc7193d8b7 100644
--- a/src/plugins/platforms/cocoa/qiosurfacegraphicsbuffer.h
+++ b/src/plugins/platforms/cocoa/qiosurfacegraphicsbuffer.h
@@ -43,6 +43,7 @@
 #include <qpa/qplatformgraphicsbuffer.h>
 #include <private/qcore_mac_p.h>
 
+#include <CoreGraphics/CGColorSpace.h>
 #include <IOSurface/IOSurface.h>
 
 QT_BEGIN_NAMESPACE
```

Build Qt:

```bash
./configure -prefix $PWD/qtbase -release -opensource -confirm-license -ccache \
-no-dbus -no-sql-sqlite -no-use-gold-linker -no-kms \
-qt-harfbuzz -qt-libjpeg -qt-libpng -qt-pcre -qt-zlib \
-skip qt3d -skip qtandroidextras -skip qtcanvas3d -skip qtcharts -skip qtconnectivity -skip qtdatavis3d \
-skip qtdoc -skip qtquickcontrols -skip qtquickcontrols2 -skip qtspeech  -skip qtgamepad \
-skip qtlocation -skip qtmacextras -skip qtnetworkauth -skip qtpurchasing -optimize-size \
-skip qtscript -skip qtscxml -skip qtsensors -skip qtserialbus -skip qtserialport -skip qtspeech -skip qttools \
-skip qtvirtualkeyboard -skip qtwayland -skip qtwebchannel -skip qtwebengine -skip qtwebview \
-skip qtwinextras -skip qtx11extras -skip gamepad -skip serialbus -skip location -skip webengine -skip qtdeclarative \
-no-feature-cups -no-feature-ftp -no-feature-pdf -no-feature-animation -nomake examples -nomake tests -nomake tools

./configure -prefix $PWD/qtbase -release -nomake examples -nomake tests -skip qtwebchannel -skip qtpurchasing -skip webengine -skip qtwebview
make -j 4
```

Problem: QtQuick does not seem to be compiled, `Qt5QuickConfig.cmake` missing.

Build:

```bash
CMAKE_PREFIX_PATH=/Users/$USER/Downloads/qt-everywhere-src-5.15.3/qtbase/ make -j4 mac
```
