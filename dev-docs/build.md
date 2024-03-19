# Building Kdenlive

## Supported platforms

Kdenlive is primarily developed on GNU/Linux, but it is also possible to [build Kdenlive on Microsoft Windows and macOS using Craft](#build-craft). For Windows also [other possibilities exist](https://community.kde.org/Kdenlive/Development/WindowsBuild). 

Currently supported distributions are:

* Ubuntu 22.04 LTS Focal Fossa and derivatives
* Arch Linux
* All platforms fulfilling the requirements described below

Kdenlive is in a transition from Qt5/KF5 to Qt6/KF6. Qt6 is the prefered version, while Qt5 is yet still supported (but deprecated)

For Qt5 the minimum required dependencies are: Qt >= 5.15.2, KF5 >= 5.86, MLT >= 7.14.0.
For Qt6 the minimum required dependencies are: Qt >= 6.6.0, KF6 >= 6.0, MLT >= 7.14.0.

## Build on Linux

### Base procedure

Kdenlive usually requires the latest version of MLT, in which go several API updates, bugfixes and optimizations.
On Ubuntu, the easiest way is to add [Kdenlive's ppa](https://launchpad.net/~kdenlive/+archive/ubuntu/kdenlive-master)

```bash
sudo add-apt-repository ppa:kdenlive/kdenlive-master
sudo apt update
```

It is recommended to uninstall the official kdenlive packages to avoid potential conflicts. 

```bash
sudo apt remove kdenlive kdenlive-data
```


#### Get the build dependencies


First, make sure you have the required tooling installed:

```bash
sudo apt install build-essential git cmake extra-cmake-modules libsm-dev clang-format
# Optional for faster builds install Ninja
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
sudo apt install qt6-base-dev qt6-svg-dev qt6-multimedia-dev qt6-networkauth-dev

# KDE Frameworks 6, based on Qt6
sudo apt install kf6-karchive-dev kf6-kbookmarks-dev kf6-kcodecs-dev kf6-kcoreaddons-dev \
kf6-kconfig-dev kf6-kconfigwidgets-dev kf6-kio-dev kf6-kwidgetsaddons-dev kf6-knotifyconfig-dev \
kf6-knewstuff-dev kf6-kxmlgui-dev kf6-knotifications-dev kf6-kguiaddons-dev kf6-ktextwidgets-dev \
kf6-kiconthemes-dev kf6-solid-dev kf6-kfilemetadata-dev kf6-purpose-dev \
kf6-kdoctools-dev kf6-kcrash-dev kf6-kdbusaddons-dev kf6-breeze-icon-theme-dev

# DEPRECATED: Qt5 modules and KDE Frameworks 5
sudo apt install libkf5archive-dev libkf5bookmarks-dev libkf5coreaddons-dev libkf5config-dev \
libkf5configwidgets-dev libkf5dbusaddons-dev libkf5kio-dev libkf5widgetsaddons-dev \
libkf5notifyconfig-dev libkf5newstuff-dev libkf5xmlgui-dev libkf5declarative-dev \
libkf5notifications-dev libkf5guiaddons-dev libkf5textwidgets-dev libkf5purpose-dev \
libkf5iconthemes-dev libkf5crash-dev libkf5filemetadata-dev libkf5codecs-dev kio \
kinit qtdeclarative5-dev libqt5svg5-dev qml-module-qtquick-controls libqt5networkauth5-dev \
qtmultimedia5-dev qtquickcontrols2-5-dev qttools5-dev breeze-icon-theme breeze

# Multimedia stack
sudo apt install frei0r-plugins ffmpeg

# MLT, except if you want to build it manually 
sudo apt install libmlt++-dev libmlt-dev melt

# Dependencies for localization
sudo apt install ruby subversion gnupg2 gettext

```

#### Clone the repositories

In your development directory, run:

```bash
git clone https://invent.kde.org/multimedia/kdenlive.git
```

#### Building MLT (not usually necessary)
It is recommended to use your distro packages unless you have a reason to use MLT's git.
To build manually:

```bash
# Install MLT dependencies
sudo apt install libxml++2.6-dev libavformat-dev libswscale-dev libavfilter-dev libavutil-dev libavdevice-dev libsdl1.2-dev librtaudio-dev libsox-dev libsamplerate0-dev librubberband-dev libebur128-dev libarchive-dev

# Get MLT's source code
git clone https://github.com/mltframework/mlt.git
cd mlt
mkdir build
cd build
# To build with Qt6, you may also enable other optional modules in this command (-GNinja is optional, to build with the faster Ninja command)
cmake .. -GNinja -DMOD_QT=OFF -DMOD_QT6=ON -DMOD_GLAXNIMATE=OFF -DMOD_GLAXNIMATE_QT6=ON
# To build with Qt5 instead use
cmake .. -GNinja -DMOD_QT=ON -DMOD_GLAXNIMATE=ON -DMOD_QT6=OFF -DMOD_GLAXNIMATE_QT6=OFF

#install (use make instead of ninja if ninja is not used)
sudo ninja install

```

#### Build and install the projects

You should decide where you want to install your builds:

- by default it goes to `/usr/local` (good option if you are admin of the machine, normally programs there are automatically detected);
- you may use `$HOME/.local` user-writable directory (good option if you don't want to play with admin rights, programs there are also usually found)
- you may want to override the distribution files in `/usr` (then you have to remove MLT & Kdenlive binary & data packages first)
- you can pick any destination you like (eg in `/opt` or anywhere in `$HOME`, then you will have to set several environment variables for programs, libs and data to be found)

Let's define that destination as `INSTALL_PREFIX` variable; also you can set `JOBS` variable to the number of threads your CPU can offer for builds.

And build the dependencies (MLT) before the project (Kdenlive):

```bash
INSTALL_PREFIX=$HOME/.local # or any other choice, the easiest would be to leave it empty ("")
JOBS=4

# Only if you want to compile MLT manually
cd mlt
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX
make -j$JOBS
make install
# 'sudo make install' if INSTALL_PREFIX is not user-writable

# Kdenlive
cd ../../kdenlive
mkdir build && cd build

# Even if you specified a user-writable INSTALL_PREFIX, some Qt plugins like the MLT thumbnailer are
# going be installed in non-user-writable system paths to make them work. If you really do not want
# to give root privileges, you need to set KDE_INSTALL_USE_QT_SYS_PATHS to OFF in the line below.
# -GNinja is optional

# Qt6
cmake .. -GNinja -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -DKDE_INSTALL_USE_QT_SYS_PATHS=ON -DRELEASE_BUILD=OFF -DQT_MAJOR_VERSION=6

# Qt5 (deprecated alternative to Qt6)
cmake .. -GNinja -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -DKDE_INSTALL_USE_QT_SYS_PATHS=ON -DRELEASE_BUILD=OFF -DQT_MAJOR_VERSION=5 -DBUILD_WITH_QT6=OFF
```

```bash
make -j$JOBS
make install
# 'sudo make install' if INSTALL_PREFIX is not user-writable or if KDE_INSTALL_USE_QT_SYS_PATHS=ON
```

Note that `make install` is required for Kdenlive, otherwise the effects will not be installed and cannot be used.

#### Run Kdenlive

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

### Building in Docker

It is possible to run the above commands inside a container, with a fresh Ubuntu for example.
Note that Kdenlive cannot be easily run from inside the Docker container as it is a GUI application.

```bash
# Spin up a Docker container
# The --rm flag removes the container after it is stopped.
docker run -it --rm ubuntu:22.04

# Now install the dependencies etc.
# Note that you are root in the container, and sudo neither exists nor works.
apt install …

# When you are done, exit
exit
```

## Translating Kdenlive

TODO

[fuzzer-blog]: https://kdenlive.org/en/2019/03/inside-kdenlive-how-to-fuzz-a-complex-gui-application/
