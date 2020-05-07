# Building

*This page describes how to build Kdenlive with its dependencies.*


## Building Kdenlive

Building is done in 3 steps:

1. Install the development dependencies and remove existing installations
2. Clone the repositories
3. Build and install the projects


### Get the development dependencies

Kdenlive usually requires the latest versions of MLT. MLT depends on frei0r,
and on Ubuntu frei0r needs to be built as well as there is no
`frei0r-plugins-dev` package.

First, install the requirements. For Ubuntu 20.04 and 19.10, the required packages are:

```bash
# Basic packages
sudo apt install git build-essential cmake cmake-extras pkg-config

# frei0r
sudo apt install libopencv-dev libcairo-dev libgavl-dev

# MLT
sudo apt install libavformat-dev libsdl2-dev libswscale-dev libavfilter-dev \
                 libavdevice-dev libgdk-pixbuf2.0-dev libpango1.0-dev libexif-dev \
                 libfftw3-dev libebur128-dev librubberband-dev librtaudio-dev \
                 libvidstab-dev libxml2-dev qt5-default libqt5svg5-dev

# Kdenlive
sudo apt install libkf5archive-dev libkf5bookmarks-dev libkf5coreaddons-dev \
                 libkf5config-dev libkf5configwidgets-dev libkf5dbusaddons-dev \
                 libkf5kio-dev libkf5widgetsaddons-dev libkf5notifyconfig-dev \
                 libkf5newstuff-dev libkf5xmlgui-dev libkf5declarative-dev \
                 libkf5notifications-dev libkf5guiaddons-dev libkf5textwidgets-dev \
                 libkf5purpose-dev libkf5iconthemes-dev kdoctools-dev libkf5crash-dev \
                 libkf5filemetadata-dev  kio kinit qtdeclarative5-dev libqt5svg5-dev \
                 qml-module-qtquick-controls qtmultimedia5-dev qtquickcontrols2-5-dev \
                 appstream gettext libv4l-dev libqt5webkit5-dev librttr-dev breeze ffmpeg
```

Most development packages will already be installed with the following command:

```bash
# Ubuntu
# Enable deb-src entries /etc/apt/sources beforehand!
sudo apt-get build-dep mlt kdenlive

# OpenSuse
zypper build-dep

# Fedora
# Install builddep beforehand
dnf builddep
```

**Important:** If you are going to install the projects to your `/usr` after
building them, make sure to remove existing packages of Kdenlive, MLT, and
frei0r beforehand!

```bash
apt purge kdenlive frei0r-plugins libmlt++3
```

### Clone frei0r, MLT, and Kdenlive

```bash
git clone https://github.com/dyne/frei0r.git
git clone https://github.com/mltframework/mlt.git
git clone https://invent.kde.org/kde/kdenlive.git
```


### Build it!

Now you can build and install the projects. As Kdenlive depends on MLT which
depends on frei0r, build and install them in reverse order.

Note that `make install` is required for Kdenlive, otherwise the effects will
not be installed and cannot be used.

For frei0r, MLT, and Kdenlive (in that order), run the following steps
inside their directory to build, compile, and install it.

```bash
# Create a build directory – the build files remain in here
mkdir build
cd build

# Check dependencies and configure the build process
cmake .. -DCMAKE_INSTALL_PREFIX=/usr

# Compile
make

# Install the compiled files
sudo make install
```

#### Building on Ubuntu 20.04

Ubuntu 20.04 provides OpenCV 4.2 which is not supported by the frei0r plugins yet. Run CMake with

```bash
cmake .. -DWITHOUT_OPENCV=true -DCMAKE_INSTALL_PREFIX=/usr
```


### Install Kdenlive to a local path

As alternative to installing Kdenlive system wide in `/usr`, it can also be
installed in a custom directory, for example `~/.local`, in order to still have
a global sytem version for comparison. In that case, the `prefix.sh` script in
the build directory has to be executed prior to running Kdenlive from the
installation directory; it sets some environment variables.

```bash
# Configure with a different install directory
cmake .. -DCMAKE_INSTALL_PREFIX=~/.local

# Compile and install (sudo is not required for ~/.local)
make
make install

# Load environment variables
chmod u+x prefix.sh
./prefix.sh

# Run Kdenlive
~/.local/bin/kdenlive
```


### Building in Docker

For checking if the dependencies are still up-to-date, it is possible to run
all the above commands inside a Docker container with a fresh Ubuntu, for
example.

Note that Kdenlive cannot be run from inside the Docker container as it is a
GUI application and Docker is console-only.

```bash
# Spin up a Docker container
# The --rm flag removes the container after it is stopped.
docker run -it --rm ubuntu:19.10

# Now install the dependencies etc.
# Note that you are root in the container, and sudo neither exists nor works.
apt install …

# When you are done, exit
exit
```


## Translating Kdenlive

TODO
