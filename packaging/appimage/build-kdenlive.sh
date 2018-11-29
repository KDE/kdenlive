#!/bin/bash

# Halt on errors and be verbose about what we are doing
set -e
set -x

# Read in our parameters
export BUILD_PREFIX=$1
export KDENLIVE_SOURCES=$2

# qjsonparser, used to add metadata to the plugins needs to work in a en_US.UTF-8 environment.
# That's not always the case, so make sure it is
export LC_ALL=en_US.UTF-8
export LANG=en_us.UTF-8

export APPDIR=$BUILD_PREFIX/kdenlive.appdir
export PLUGINS=$APPDIR/usr/lib/plugins/
export APPIMAGEPLUGINS=$APPDIR/usr/plugins/

mkdir -p $APPDIR
mkdir -p $APPDIR/usr/share/kdenlive
mkdir -p $APPDIR/usr/lib

# We want to use $prefix/deps/usr/ for all our dependencies
export DEPS_INSTALL_PREFIX=$BUILD_PREFIX/deps/usr/
export DOWNLOADS_DIR=$BUILD_PREFIX/downloads/

# Use newer gcc
export CC=/usr/bin/gcc-6
export CXX=/usr/bin/g++-6


# Setup variables needed to help everything find what we build
export LD_LIBRARY_PATH=$DEPS_INSTALL_PREFIX/lib:$DEPS_INSTALL_PREFIX/openssl/lib:$LD_LIBRARY_PATH
export PATH=$DEPS_INSTALL_PREFIX/bin:$DEPS_INSTALL_PREFIX/openssl/bin:$PATH
export PKG_CONFIG_PATH=$DEPS_INSTALL_PREFIX/share/pkgconfig:$DEPS_INSTALL_PREFIX/lib/pkgconfig:$DEPS_INSTALL_PREFIX/openssl/lib/pkgconfig:/usr/lib/pkgconfig:$PKG_CONFIG_PATH
export CMAKE_PREFIX_PATH=$DEPS_INSTALL_PREFIX:${DEPS_INSTALL_PREFIX}/openssl:$CMAKE_PREFIX_PATH

# Make sure our build directory exists
if [ ! -d $BUILD_PREFIX/kdenlive-build/ ] ; then
    mkdir -p $BUILD_PREFIX/kdenlive-build/
fi

# Make sure our downloads directory exists
if [ ! -d $DOWNLOADS_DIR ] ; then
    mkdir -p $DOWNLOADS_DIR
fi

# When using git master to build refactoring_timeline:

# Switch to our build directory as we're basically ready to start building...
mkdir -p $BUILD_PREFIX/deps-build/
cd $BUILD_PREFIX/deps-build/

mkdir -p $BUILD_PREFIX/kdenlive.appdir/usr

# Configure the dependencies for building
cmake $KDENLIVE_SOURCES/packaging/appimage/3rdparty -DCMAKE_INSTALL_PREFIX=$BUILD_PREFIX/kdenlive.appdir/usr/ -DEXT_INSTALL_DIR=$DEPS_INSTALL_PREFIX -DEXT_DOWNLOAD_DIR=$DOWNLOADS_DIR

cmake --build . --target ext_kdenlive

# Now switch to it
#cd $BUILD_PREFIX/kdenlive-build/

# Determine how many CPUs we have
#CPU_COUNT=`grep processor /proc/cpuinfo | wc -l`

# Configure Kdenlive
#cmake $KDENLIVE_SOURCES \
#    -DCMAKE_INSTALL_PREFIX:PATH=$BUILD_PREFIX/kdenlive.appdir/usr \
#    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
#    -DBUILD_TESTING=FALSE \
#    -DBUILD_TESTS=FALSE

# Build and Install Kdenlive (ready for the next phase)
#make -j$CPU_COUNT install
