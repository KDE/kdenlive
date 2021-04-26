# Building Kdenlive

## Supported platforms

Kdenlive is primarily developed on GNU/Linux, but there is also a working version of [Kdenlive on Microsoft Windows](https://community.kde.org/Kdenlive/Development/WindowsBuild). 

Currently supported distributions are:

* Ubuntu 20.04 LTS Focal Fossa and derivatives
* Arch Linux

But you should be able to build it on any platform that provides up-to-date versions of the following dependencies: Qt >= 5.7, KF5 >= 5.50,MLT >= 6.20.0.

## Base procedure

Kdenlive usually requires the latest version of MLT, in which go several API updates, bufixes and optimizations.
On Ubuntu, the easiest way is to add [https://launchpad.net/~kdenlive/+archive/ubuntu/kdenlive-master Kdenlive's ppa]

```bash
sudo add-apt-repository ppa:kdenlive/kdenlive-master
sudo apt update
```

It is recommended to uninstall the official kdenlive packages to avoid potential conflicts. 

```bash
sudo apt remove kdenlive kdenlive-data
```


### Get the build dependencies


First, make sure you have the required tooling installed:

```bash
sudo apt install build-essential git cmake extra-cmake-modules libsm-dev
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
Or install the dependencies explicitely:

```bash
# KDE Frameworks 5, based on Qt5
sudo apt install libkf5archive-dev libkf5bookmarks-dev libkf5coreaddons-dev libkf5config-dev \
libkf5configwidgets-dev libkf5dbusaddons-dev libkf5kio-dev libkf5widgetsaddons-dev \
libkf5notifyconfig-dev libkf5newstuff-dev libkf5xmlgui-dev libkf5declarative-dev \
libkf5notifications-dev libkf5guiaddons-dev libkf5textwidgets-dev libkf5purpose-dev \
libkf5iconthemes-dev kdoctools-dev libkf5crash-dev libkf5filemetadata-dev kio \
kinit qtdeclarative5-dev libqt5svg5-dev qml-module-qtquick-controls libqt5networkauth5-dev \
qtmultimedia5-dev qtquickcontrols2-5-dev breeze-icon-theme breeze

# Multimedia stack
sudo apt install frei0r-plugins ffmpeg

# MLT, except if you want to build it manually 
sudo apt install libmlt++-dev libmlt-dev melt

# Dependencies for localization
sudo apt install ruby subversion gnupg2 gettext


```
### Clone the repositories

In your development directory, run:

```bash
git clone https://invent.kde.org/multimedia/kdenlive.git
```

And if you want to build MLT manually:

```bash
git clone https://github.com/mltframework/mlt.git
```

### Build and install the projects

You should decide where you want to install your builds:

- by default it goes to `/usr/local` (good option if you are admin of the machine, normally programs there are automatically detected);
- you may use `$HOME/.local` user-writable directory (good option if you don't want to play with admin rights, programs there are also usually found)
- you may want to override the distribution files in `/usr` (then you have to remove MLT & Kdenlive binary & data packages first)
- you can pick any destination you like (eg in `/opt` or anywhere in `$HOME`, then you will have to set several environment variables for programs, libs and data to be found)

Let's define that destination as `INSTALL_PREFIX` variable; also you can set `JOBS` variable to the number of threads your CPU can offer for builds.

And build the dependencies (MLT) before the project (Kdenlive):

```bash
INSTALL_PREFIX=$HOME/.local # or any other choice
JOBS=4

# Only if you want to compile MLT manually
cd mlt
./configure --enable-gpl --enable-gpl3 --prefix=$INSTALL_PREFIX
make -j$JOBS
make install
# 'sudo make install' if INSTALL_PREFIX is not user-writable

# Kdenlive
cd ../kdenlive
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -DKDE_INSTALL_USE_QT_SYS_PATHS=ON -DRELEASE_BUILD=OFF
```

To compile the translations of the application, you need KDE Frameworks >= 5.76, make sure to delete the "po" subdirectory from your build folder if it exists, and use this cmake command to configure the project :
```bash
cmake .. -DKDE_INSTALL_USE_QT_SYS_PATHS=ON -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -DKDE_L10N_SYNC_TRANSLATIONS=ON -DRELEASE_BUILD=OFF
```

```bash
make -j$JOBS
make install
# 'sudo make install' if INSTALL_PREFIX is not user-writable
```

Note that `make install` is required for Kdenlive, otherwise the effects will not be installed and cannot be used.

### Run Kdenlive

If you didn't build in a system path in which all libs and data are automatically found, you will need to set environment variables to point to them.
This is done by the auto-generated script in `kdenlive/build` that must be sourced (to keep variables set in current shell, unlike just executing the script):

```bash
. prefix.sh
kdenlive
```

## Various development tricks

### Debugging

Having debug symbols helps getting much more useful information from crash logs or analyzers outputs; this is enabled at configure stage.
- in MLT, append `--enable-debug` to `./configure` line
- in Kdenlive, append `-DCMAKE_BUILD_TYPE=Debug` to `cmake` line


### Running tests

Kdenlive test coverage is focused mostly on timeline model code (extending tests to more parts is highly desired). To run those tests, append to `cmake` line:
`-DBUILD_TESTING=ON`

### Fuzzer

Kdenlive embeds a fuzzing engine that can detect crashes and auto-generate tests. It requires to have clang installed (generally in `/usr/bin/clang++`). This can be activated in `cmake` line with:
`-DBUILD_FUZZING=ON -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DECM_ENABLE_SANITIZERS='address'`

### Help file for QtCreator, KDevelop, etc.

You can automatically build and install a `*.qch` file with the doxygen docs about the source code to use it with your IDE like Qt Assistant, Qt Creator or KDevelop. This can be activated in `cmake` line with:
`-DBUILD_QCH=ON`

You can find the `kdenlive.qch` at `build/src` and after `make install` at `${CMAKE_INSTALL_PREFIX}/lib/cmake/kdenlive`

To add the `kdenlive.qch` file to Qt Creator, select **Tools** > **Options** > **Help** > **Documentation** > **Add**.

### Speeding up compilations

Ninja build systems, compared to make, seems faster and better detecting which files are necessary to rebuild. You can enable it appending `-GNinja` to `cmake` line
CCache also helps: `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`

### Analyzers

You can configure Kdenlive to embed tooling for runtime analysis, for example appending to cmake line:
`-DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DECM_ENABLE_SANITIZERS='address'`

This one will report in terminal all memory handling errors found during execution.

### Building OpenCV tracking module

MLT/Kdenlive tracking effect relies on a "contrib" a OpenCV module that is not shipped by distributions.
We build it in our AppImage but you may want it on your system (note OpenCV deserves its reputation of being difficult to build!).

```bash
wget https://github.com/opencv/opencv/archive/4.3.0.tar.gz -O opencv-4.3.0.tar.gz
wget https://github.com/opencv/opencv_contrib/archive/4.3.0.tar.gz -O opencv_contrib-4.3.0.tar.gz
tar xaf opencv-4.3.0.tar.gz
tar xaf opencv_contrib-4.3.0.tar.gz
cd opencv-4.3.0
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
  -DOPENCV_EXTRA_MODULES_PATH=../../opencv_contrib-4.3.0/modules \
  -DOPENCV_GENERATE_PKGCONFIG=ON -DBUILD_LIST=tracking -DOPENCV_BUILD_3RDPARTY_LIBS=OFF
```

Then you will have to rebuild MLT appending `--enable-opencv` to `configure` line!

### Building frei0r

You may be interested in building latest frei0r effects library. So get dependencies, clone repository, and build-install:

```bash
sudo apt build-dep frei0r
git clone https://github.com/dyne/frei0r.git
cd frei0r
mkdir build
cd build
cmake .. -DWITHOUT_OPENCV=true -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX
```

Note: as of 20.04, frei0r doesn't support recent OpenCV (and effects using it seemed not very stable)

### Building in Docker

It is possible to run the above commands inside a container, with a fresh Ubuntu for example.
Note that Kdenlive cannot be easily run from inside the Docker container as it is a GUI application.

```bash
# Spin up a Docker container
# The --rm flag removes the container after it is stopped.
docker run -it --rm ubuntu:20.04

# Now install the dependencies etc.
# Note that you are root in the container, and sudo neither exists nor works.
apt install …

# When you are done, exit
exit
```

## Translating Kdenlive

TODO
