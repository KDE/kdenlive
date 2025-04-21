# Building Kdenlive

## Supported platforms

Kdenlive is primarily developed on GNU/Linux, but it is also possible to [build Kdenlive on Microsoft Windows and macOS using Craft](#build-craft). For Windows also [other possibilities exist](https://community.kde.org/Kdenlive/Development/WindowsBuild).

Currently supported GNU/Linux distributions are:

* Ubuntu 24.10 Oracular Oriolea and derivatives, or later.
* Arch Linux

But it should work on any platforms fulfilling the requirements described below

The minimum required dependencies are:
- [Qt](https://doc.qt.io/) >= 6.6.0 (Kdenlive droped Qt5 support with version 24.12)
- [KDE frameworks 6 (KF6)](https://develop.kde.org/products/frameworks/) >= 6.0,
- [MLT](https://www.mltframework.org/) >= 7.28.0.

## Building on Linux

Kdenlive usually requires the latest version of MLT, in which go several API updates, bugfixes and optimizations.
So, except if your distribution ships a very recent version of MLT, you'll have to build MLT alongside kdenlive.

It is recommended to uninstall the official kdenlive packages to avoid potential conflicts.

```bash
sudo apt remove kdenlive kdenlive-data
```

### Get the build dependencies

First, make sure you have the required tooling installed:

```bash
sudo apt install build-essential git cmake extra-cmake-modules libsm-dev clang-format
# For faster builds install Ninja
sudo apt install ninja-build
```

You can use your distribution packages information (if not too old) to easily get a complete build environment:

```bash
# Debian/Ubuntu -- Enable deb-src entries /etc/apt/sources beforehand
sudo apt build-dep mlt kdenlive
# Fedora/CentOS -- Install builddep beforehand
dnf builddep mlt kdenlive
# OpenSUSE
zypper source-install --build-deps-only mlt kdenlive
```
Or install the dependencies explicitly:

```bash
# Qt6 modules
sudo apt install qt6-base-dev qt6-svg-dev qt6-multimedia-dev qt6-networkauth-dev \
qml6-module-qtqml-workerscript qml6-module-qtquick-window qml6-module-org-kde-desktop libqt6core5compat6-dev

# KDE Frameworks 6, based on Qt6
sudo apt install kf6-breeze-icon-theme libkf6archive-dev libkf6bookmarks-dev \
libkf6codecs-dev libkf6config-dev libkf6configwidgets-dev libkf6coreaddons-dev \
libkf6crash-dev libkf6dbusaddons-dev libkf6doctools-dev libkf6filemetadata-dev \
libkf6guiaddons-dev libkf6iconthemes-dev libkf6kio-dev libkf6newstuff-dev \
libkf6notifications-dev libkf6notifyconfig-dev libkf6purpose-dev \
libkf6solid-dev libkf6textwidgets-dev libkf6widgetsaddons-dev libkf6xmlgui-dev

# Multimedia stack
sudo apt install frei0r-plugins ffmpeg mediainfo

# Additional multimedia libraries
sudo apt install libavformat-dev libavcodec-dev libswresample-dev libavutil-dev

# Dependencies for localization
sudo apt install ruby subversion gnupg2 gettext
```
### Define your environment variables

- If you have specific needs and know what you're doing, you can define where you want to install your builds, like `=$HOME/.local`, as `INSTALL_PREFIX` variable:

```bash
INSTALL_PREFIX=$HOME/.local # or any other choice, the easiest would be to leave it empty ("")
```
Please note that even if you have specified a user-writable INSTALL_PREFIX, some Qt plugins like the MLT thumbnailer are
going to be installed in non-user-writable system paths to make them work. If you really do not want to give root privileges, you need to set KDE_INSTALL_USE_QT_SYS_PATHS to OFF in the line below.

- You can also set the `JOBS` variable to the number of threads your CPU can offer for builds.

```bash
JOBS=8
```

### Building MLT

If your distribution ships a recent enough version of MLT, you could install the following packages:

```bash
sudo apt install libmlt++-dev libmlt-dev melt
```

And start building Kdenlive itself.

Otherwise, you should make sure these packages are not installed:

```bash
sudo apt remove libmlt++-dev libmlt-dev melt
```

And build MLT:

```bash
# Install MLT dependencies
sudo apt install libxml++2.6-dev libavformat-dev libswscale-dev libavfilter-dev \
libavutil-dev libavdevice-dev libsdl1.2-dev librtaudio-dev libsox-dev \
libsamplerate0-dev librubberband-dev libebur128-dev libarchive-dev frei0r-plugins-dev

# Get MLT's source code
# You will need --recurse-submodules to get the Glaxnimate code
#
git clone --recurse-submodules https://github.com/mltframework/mlt.git
cd mlt
mkdir build && cd build
# To build with Qt6, you may also enable other optional modules in this command (-GNinja is optional, to build with the faster Ninja command)
cmake .. -GNinja -DMOD_QT=OFF -DMOD_QT6=ON -DMOD_GLAXNIMATE=OFF -DMOD_GLAXNIMATE_QT6=ON

#install (use make instead of ninja if ninja is not used)
ninja -j$JOBS
sudo ninja install
```
If you want to uninstall it later, you can try

```bash
sudo xargs -d '\n' rm < install_manifest.txt
```

### Build Kdenlive itself

In your development directory, run:

```bash
git clone https://invent.kde.org/multimedia/kdenlive.git
cd kdenlive
mkdir build && cd build
cmake .. -GNinja -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -DKDE_INSTALL_USE_QT_SYS_PATHS=ON -DRELEASE_BUILD=OFF -DQT_MAJOR_VERSION=6
ninja -j$JOBS
sudo ninja install
```

Please note that `ninja install` is required for Kdenlive, otherwise the effects will not be installed and cannot be used.

To uninstall Kdenlive:

```bash
sudo ninja uninstall
```

### Runnig Kdenlive

If you didn't build in a system path in which all libs and data are automatically found, you will need to set environment variables to point to them.
This is done by the auto-generated script in `kdenlive/build` that must be sourced (to keep variables set in current shell, unlike just executing the script):

```bash
. prefix.sh
kdenlive
```

## <a name="build-craft">Build with KDE Craft (Linux, Windows, macOS)</a>

[Craft](https://community.kde.org/Craft) is a tool to build the sources and its third-party requirements. It is an easy way to build software, but however not ideal if you want to build Kdenlive for development purposes.

1. Set up Craft as described [here](https://community.kde.org/Craft#Setting_up_Craft). (On Windows choose MinGW as compiler!)
2. Start building kdenlive. You can simply run `craft --option kdenlive.version=master kdenlive`
3. Within the craft environment running Kdenlive is as simple as `kdenlive`

### Tips for Craft

* If you want to compile kdenlive in debug mode, you can do so by running `craft --buildtype Debug kdenlive`
* If you want to compile the stable version instead of the master branch with that last changes, remove `--option kdenlive.version=master` from the craft command: `craft kdenlive`
* If you want to compile a specific branch from the kdenlive repository use `craft --option kdenlive.version=master --option kdenlive.branch=BRANCHNAME kdenlive` where `BRANCHNAME` is the name of the branch.
* With Craft you can also easily package Kdenlive as `.dmg`, `.exe` or `.appimage` (depending on your platform): `craft --target=master --package kdenlive` The output can be found in `CraftRoot/tmp`
* For more instructions and tipps on Craft see https://community.kde.org/Craft

## Various development tricks

### Debugging

Having debug symbols helps getting much more useful information from crash logs or analyzers outputs; this is enabled at configure stage.
- append `-DCMAKE_BUILD_TYPE=Debug` to `cmake` line of kdenlive and/or mlt


### Running tests

Kdenlive test coverage is focused mostly on timeline model code (extending tests to more parts is highly desired). To run those tests, append to `cmake` line for the build `-DBUILD_TESTING=ON` and execute `ctest` to run them.

### Fuzzer

Kdenlive embeds a fuzzing engine that can detect crashes and auto-generate tests. It requires to have clang installed (generally in `/usr/bin/clang++`). This can be activated in `cmake` line with:
`-DBUILD_FUZZING=ON -DCMAKE_CXX_COMPILER=/usr/bin/clang++`

To learn more fuzzing especially in the context of Kdenlive read this [blog post][fuzzer-blog].

### Help file for QtCreator, KDevelop, etc.

You can automatically build and install a `*.qch` file with the doxygen docs about the source code to use it with your IDE like Qt Assistant, Qt Creator or KDevelop. This can be activated in `cmake` line with:
`-DBUILD_QCH=ON`

You can find the `kdenlive.qch` at `build/src` and after `make install` at `${CMAKE_INSTALL_PREFIX}/lib/cmake/kdenlive`

To add the `kdenlive.qch` file to Qt Creator, select **Tools** > **Options** > **Help** > **Documentation** > **Add**.

### Speeding up compilations

Ninja build systems, compared to make, seems faster and better detecting which files are necessary to rebuild. You can enable it appending `-GNinja` to `cmake` line
CCache also helps: `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`

If you don't need tests, disabling them will also save build time (and disk space). Append to your `cmake` line `-DBUILD_TESTING=OFF`

### Analyzers

You can configure Kdenlive to embed tooling for runtime analysis, for example appending to cmake line:
`-DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DECM_ENABLE_SANITIZERS='address'`

This one will report in terminal all memory handling errors found during execution.

### Building OpenCV tracking module

MLT/Kdenlive tracking effect relies on a "contrib" a OpenCV module that is not shipped by distributions.
We build it in our AppImage but you may want it on your system (note OpenCV deserves its reputation of being difficult to build!).

```bash
wget https://github.com/opencv/opencv/archive/4.5.5.tar.gz -O opencv-4.5.5.tar.gz
wget https://github.com/opencv/opencv_contrib/archive/4.5.5.tar.gz -O opencv_contrib-4.5.5.tar.gz
tar xaf opencv-4.5.5.tar.gz
tar xaf opencv_contrib-4.5.5.tar.gz
cd opencv-4.5.5
mkdir build
cd build
# Important: if want to install to the default location
# (ie. you left INSTALL_PREFIX empty in the previous step where we configured it)
# remove -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX from the next command!
cmake .. -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
  -DOPENCV_EXTRA_MODULES_PATH=../../opencv_contrib-4.5.5/modules \
  -DOPENCV_GENERATE_PKGCONFIG=ON -DBUILD_LIST=tracking -DOPENCV_BUILD_3RDPARTY_LIBS=OFF
make install
```

Then you will have to rebuild MLT appending `-DMOD_OPENCV=ON` to `cmake` line!

### Building frei0r

You may be interested in building latest frei0r effects library. So get dependencies, clone repository, and build-install:

```bash
sudo apt build-dep frei0r
git clone https://github.com/dyne/frei0r.git
cd frei0r
mkdir build
cd build
cmake .. -GNinja -DWITHOUT_OPENCV=true -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX
```

Note: as of 20.04, frei0r doesn't support recent OpenCV (and effects using it seemed not very stable)


[fuzzer-blog]: https://kdenlive.org/en/2019/03/inside-kdenlive-how-to-fuzz-a-complex-gui-application/
