#!/bin/bash

# Halt on errors
set -e

# Be verbose
set -x

# Read in our parameters
#export BUILD_PREFIX=$1
#export KDENLIVE_SOURCES=$2
export BUILD_PREFIX=/build
export KDENLIVE_SOURCES=/kdenlive
export DEPS_INSTALL_PREFIX=/external
export PKG_CONFIG_PATH=/build/deps/usr/lib/pkgconfig 

# qjsonparser, used to add metadata to the plugins needs to work in a en_US.UTF-8 environment.
# That's not always the case, so make sure it is
export LC_ALL=en_US.UTF-8
export LANG=en_us.UTF-8

apt-get -y install libpixman-1-dev

# We want to use $prefix/deps/usr/ for all our dependencies
export DEPS_INSTALL_PREFIX=$BUILD_PREFIX/deps/usr
export DOWNLOADS_DIR=$BUILD_PREFIX/downloads

# Setup variables needed to help everything find what we build
export LD_LIBRARY_PATH=$DEPS_INSTALL_PREFIX/lib:$DEPS_INSTALL_PREFIX/openssl/lib:$LD_LIBRARY_PATH
export PATH=$DEPS_INSTALL_PREFIX/bin:$DEPS_INSTALL_PREFIX/openssl/bin:$PATH
export PKG_CONFIG_PATH=$DEPS_INSTALL_PREFIX/share/pkgconfig:$DEPS_INSTALL_PREFIX/lib/pkgconfig:$DEPS_INSTALL_PREFIX/openssl/lib/pkgconfig:/usr/lib/pkgconfig:$PKG_CONFIG_PATH

# A kdenlive build layout looks like this:
# kdenlive/ -- the source directory
# downloads/ -- downloads of the dependencies from files.kde.org
# deps-build/ -- build directory for the dependencies
# deps/ -- the location for the built dependencies
# build/ -- build directory for kdenlive itself
# kdenlive.appdir/ -- install directory for kdenlive and the dependencies

# Make sure our downloads directory exists
if [ ! -d $DOWNLOADS_DIR ] ; then
    mkdir -p $DOWNLOADS_DIR
fi

# Make sure our build directory exists
if [ ! -d $BUILD_PREFIX/deps-build/ ] ; then
    mkdir -p $BUILD_PREFIX/deps-build/
fi

# The 3rdparty dependency handling in Kdenlive also requires the install directory to be pre-created
if [ ! -d $DEPS_INSTALL_PREFIX ] ; then
    mkdir -p $DEPS_INSTALL_PREFIX
fi

# Switch to our build directory as we're basically ready to start building...
cd $BUILD_PREFIX/deps-build/

# Configure the dependencies for building
cmake $KDENLIVE_SOURCES/packaging/appimage/3rdparty -DCMAKE_INSTALL_PREFIX=$DEPS_INSTALL_PREFIX -DEXT_INSTALL_DIR=$DEPS_INSTALL_PREFIX -DEXT_DOWNLOAD_DIR=$DOWNLOADS_DIR

# Now start building everything we need, in the appropriate order

#cmake --build . --target ext_fftw3

# ladspa expects fft3w.pc pkgconfig files
cp /build/deps/usr/lib/pkgconfig/fftw.pc /build/deps/usr/lib/pkgconfig/fftw3.pc
cp /build/deps/usr/lib/pkgconfig/fftwf.pc /build/deps/usr/lib/pkgconfig/fftw3f.pc

#cmake --build . --target ext_ladspa

#cmake --build . --target ext_x264
#cmake --build . --target ext_x265
#cmake --build . --target ext_libvpx
#cmake --build . --target ext_ffmpeg
cmake --build . --target ext_cairo
cmake --build . --target ext_harfbuzz
cmake --build . --target ext_pango
cmake --build . --target ext_gdkpixbuf
cmake --build . --target ext_gtk+
cmake --build . --target ext_mlt
cmake --build . --target ext_kdenlive


