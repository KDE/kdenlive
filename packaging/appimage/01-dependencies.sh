#!/bin/bash

# Halt on errors
set -e

# Be verbose
set -x

# Now we are inside CentOS 6
grep -r "CentOS release 6" /etc/redhat-release || exit 1

CPU_CORES=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)

if [[ $CPU_CORES -gt 1 ]]; then
    CPU_CORES=$((CPU_CORES-1))
fi

echo "CPU Cores to use : $CPU_CORES"

# Determine which architecture should be built
if [[ "$(arch)" = "i686" || "$(arch)" = "x86_64" ]] ; then
  ARCH=$(arch)
else
  echo "Architecture could not be determined"
  exit 1
fi


#yum -y install wget epel-release git make autoconf automake libtool \
#	gettext perl-URI.noarch bzip2-devel libnuma-devel xz-devel \
#	libzip-devel libxml2-devel libxslt-devel libsqlite3x-devel \
#	libudev-devel libusbx-devel libcurl-devel libssh2-devel mesa-libGL-devel sqlite-devel #\
#	tar gzip which make autoconf automake gstreamer-devel mesa-libEGL coreutils grep \
#	media-player-info.noarch alsa-lib-devel polkit-devel sox-devel mesa-libGLU

#yum --enablerepo=epel -y install fuse-sshfs # install from EPEL

#if [[ "$(arch)" = "x86_64" ]] ; then
#    yum upgrade ca-certificates --disablerepo=epel
#fi

if [[ ! -f /etc/yum.repos.d/epel.repo ]] ; then

    yum -y install epel-release

    # we need to be up to date in order to install the xcb-keysyms dependency
    yum -y update
fi

# Packages for base dependencies and Qt5.
yum -y install wget \
               tar \
               bzip2 \
               xz \
               gettext \
               git \
               subversion \
               libtool \
               which \
               fuse \
               automake \
               mesa-libEGL \
               cmake3 \
               gcc-c++ \
               patch \
               libxcb \
               xcb-util \
               xkeyboard-config \
               gperf \
               ruby \
               bison \
               flex \
               zlib-devel \
               expat-devel \
               fuse-devel \
               libtool-ltdl-devel \
               glib2-devel \
               glibc-headers \
               mysql-devel \
               eigen3-devel \
               openssl-devel \
               cppunit-devel \
               libstdc++-devel \
               freetype-devel \
               fontconfig-devel \
               libxml2-devel \
               libstdc++-devel \
               libXrender-devel \
               lcms2-devel \
               xcb-util-keysyms-devel \
               libXi-devel \
               mesa-libGL-devel \
               mesa-libGLU-devel \
               libxcb-devel \
               xcb-util-devel \
               glibc-devel \
               libudev-devel \
               libicu-devel \
               sqlite-devel \
               libusb-devel \
               libexif-devel \
               libical-devel \
               libxslt-devel \
               xz-devel \
               lz4-devel \
               inotify-tools-devel \
               openssl-devel \
               cups-devel \
               openal-soft-devel \
               pixman-devel \
               alsa-lib-devel \
               sox-devel \
               polkit-devel


# Newer compiler than what comes with offcial CentOS 6 (only 64 bits)
yum -y install centos-release-scl-rh
yum -y install devtoolset-3-gcc devtoolset-3-gcc-c++

# required for Kdenlive related libs
yum -y install libXft-devel atk-devel libtiff-devel libjpeg-devel libXcomposite-devel

# Get helper functions
wget -q https://github.com/probonopd/AppImages/raw/master/functions.sh -O ./functions.sh
. ./functions.sh
rm -f functions.sh

echo -e "---------- Clean-up Old Packages\n"

# Remove system based devel package to prevent conflict with new one.
yum -y erase boost-devel libgphoto2 sane-backends libjpeg-devel jasper-devel libpng-devel libtiff-devel

# Prepare the install location
# rm -rf /app/ || true
mkdir -p /app/usr
mkdir -p /external/build
mkdir -p /external/download

export LLVM_ROOT=/opt/llvm/

# make sure lib and lib64 are the same thing
mkdir -p /app/usr/lib
cd  /app/usr
rm -Rf lib64
ln -s lib lib64

QTVERSION=5.9.2
QVERSION_SHORT=5.9
QTDIR=/usr/local/Qt-${QTVERSION}/

# Use the new compiler
. /opt/rh/devtoolset-3/enable

BUILDING_DIR="/external/build"
DOWNLOAD_DIR="/external/download"

cd $BUILDING_DIR

rm -rf $BUILDING_DIR/* || true

cmake3 /kdenlive/packaging/appimage/3rdparty \
       -DCMAKE_INSTALL_PREFIX:PATH=/usr \
       -DINSTALL_ROOT=/usr \
       -DEXTERNALS_DOWNLOAD_DIR=$DOWNLOAD_DIR

cmake3 --build . --config RelWithDebInfo --target ext_jpeg       -- -j$CPU_CORES
cmake3 --build . --config RelWithDebInfo --target ext_jasper     -- -j$CPU_CORES
cmake3 --build . --config RelWithDebInfo --target ext_png        -- -j$CPU_CORES
cmake3 --build . --config RelWithDebInfo --target ext_tiff       -- -j$CPU_CORES
#cmake3 --build . --config RelWithDebInfo --target ext_opencv     -- -j$CPU_CORES
cmake3 --build . --config RelWithDebInfo --target ext_qt         -- -j$CPU_CORES
cmake3 --build . --config RelWithDebInfo --target ext_exiv2      -- -j$CPU_CORES


#necessary ?
#pulseaudio-libs

# qjsonparser, used to add metadata to the plugins needs to work in a en_US.UTF-8 environment. That's
# not always set correctly in CentOS 6.7
export LC_ALL=en_US.UTF-8
export LANG=en_us.UTF-8


# Make sure we build from the /, parts of this script depends on that. We also need to run as root...
cd  /

# TO-DO ask about this.
export CMAKE_PREFIX_PATH=$QTDIR:/app/share/llvm/

#update cmake https://xinyustudio.wordpress.com/2014/06/18/how-to-install-cmake-3-0-on-centos-6-centos-7/
#export PATH=/usr/local/cmake-3.0.0/bin:$PATH

# if the library path doesn't point to our usr/lib, linking will be broken and we won't find all deps either
export LD_LIBRARY_PATH=/usr/lib64/:/usr/lib:/app/usr/lib:$QTDIR/lib/:/opt/python3.5/lib/:$LD_LIBRARY_PATH

# start building the deps

function build_external
{ (
    # errors fatal
    echo "Compiler version:" $(g++ --version)
    set -e

    SRC=/external
    BUILD=/external/build
    PREFIX=/app/usr/

    # framework
    EXTERNAL=$1

    # clone if not there
    mkdir -p $SRC
    cd $SRC
    if ( test -d $EXTERNAL )
    then
        echo "$EXTERNAL already cloned"
        cd $EXTERNAL
        git reset --hard
        git pull --rebase
        cd ..
    else
        git clone $EXTERNAL_ADDRESS
    fi

    # create build dir
    mkdir -p $BUILD/$EXTERNAL

    # go there
    cd $BUILD/$EXTERNAL

    # cmake it
    if ( $EXTERNAL_CMAKE )
    then
        cmake3 -DCMAKE_INSTALL_PREFIX:PATH=$PREFIX $2 $SRC/$EXTERNAL
    else
        $EXTERNAL_CONFIGURE
    fi
    # make
    make -j8

    # install
    make install
) }


export WLD=/app/usr/   # change this to another location if you prefer
export LD_LIBRARY_PATH=$WLD/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$WLD/lib/pkgconfig/:$WLD/share/pkgconfig/:/usr/lib/pkgconfig/:$PKG_CONFIG_PATH
export PATH=$WLD/bin:$PATH
export ACLOCAL_PATH=$WLD/share/aclocal
export ACLOCAL="aclocal -I $ACLOCAL_PATH"

#yasm
IN=yasm,https://github.com/yasm/yasm.git,true,""
IFS=',' read -a external_options <<< $IN
EXTERNAL="${external_options[0]}"
EXTERNAL_ADDRESS="${external_options[1]}"
EXTERNAL_CMAKE="${external_options[2]}"
EXTERNAL_CONFIGURE="${external_options[3]}"
build_external $EXTERNAL

#sdl
cd /external
if ( test -d /external/SDL2-2.0.7 )
then
	echo "SDL already downloaded"
else
	wget http://libsdl.org/release/SDL2-2.0.7.tar.gz
	tar -xf SDL2-2.0.7.tar.gz
fi
cd /external/SDL2-2.0.7
./configure --prefix=$WLD
make
make install


function build_framework
{ (
    # errors fatal
    echo "Compiler version:" $(g++ --version)
    set -e

    SRC=/kf5
    BUILD=/kf5/build
    PREFIX=/app/usr/

    # framework
    FRAMEWORK=$1

    # clone if not there
    mkdir -p $SRC
    cd $SRC
    if ( test -d $FRAMEWORK )
    then
        echo "$FRAMEWORK already cloned"
        cd $FRAMEWORK
        if [ "$FRAMEWORK" = "polkit-qt-1" ] || [ "$FRAMEWORK" = "breeze" ] || [ "$FRAMEWORK" = "kdecoration" ]; then
            git checkout .
            git checkout master
            git reset --hard
            git pull --rebase
	elif [ "$FRAMEWORK" = "knotifications" ]; then
            git checkout .
            git checkout master
            git checkout v5.40.0
        else
            git fetch --tags
            git checkout v5.40.0
        fi
        #git checkout master
        #git reset --hard
        #git pull --rebase
        cd ..
    else
        git clone git://anongit.kde.org/$FRAMEWORK
        cd $FRAMEWORK
        if [ "$FRAMEWORK" = "polkit-qt-1" ] || [ "$FRAMEWORK" = "breeze" ] || [ "$FRAMEWORK" = "kdecoration" ]; then
            git checkout master
            git reset --hard
            git pull --rebase
	elif [ "$FRAMEWORK" = "knotifications" ]; then
	    git reset --hard
            git fetch --tags
	    git checkout v5.40.0
        else
            git fetch --tags
            git checkout v5.40.0
        fi
        cd ..
    fi

    if [ "$FRAMEWORK" = "knotifications" ]; then
	cd $FRAMEWORK
        echo "patching knotifications"
	cat > no_phonon.patch << EOF
diff --git a/CMakeLists.txt b/CMakeLists.txt
index 0104c73..de44e9a 100644
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -59,11 +59,11 @@ find_package(KF5Config ${KF5_DEP_VERSION} REQUIRED)
 find_package(KF5Codecs ${KF5_DEP_VERSION} REQUIRED)
 find_package(KF5CoreAddons ${KF5_DEP_VERSION} REQUIRED)
 
-find_package(Phonon4Qt5 4.6.60 REQUIRED NO_MODULE)
-set_package_properties(Phonon4Qt5 PROPERTIES
-   DESCRIPTION "Qt-based audio library"
-   TYPE REQUIRED
-   PURPOSE "Required to build audio notification support")
+#find_package(Phonon4Qt5 4.6.60 REQUIRED NO_MODULE)
+#set_package_properties(Phonon4Qt5 PROPERTIES
+#   DESCRIPTION "Qt-based audio library"
+#   TYPE REQUIRED
+#   PURPOSE "Required to build audio notification support")
 if (Phonon4Qt5_FOUND)
   add_definitions(-DHAVE_PHONON4QT5)
 endif()
EOF
	cat no_phonon.patch |patch -p1
	cd ..
    fi

    # create build dir
    mkdir -p $BUILD/$FRAMEWORK

    # go there
    cd $BUILD/$FRAMEWORK

    # cmake it
    cmake3 -DBUILD_TESTING:BOOL=OFF -DCMAKE_INSTALL_PREFIX:PATH=$PREFIX $2 $SRC/$FRAMEWORK  > /tmp/$FRAMEWORK.log

    # make
    make -j8

    # install
    make install
) }


#TO-DO script these extras
build_framework extra-cmake-modules "-DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON"
#Cmake is too old on centos6.... so does this mean no sound for KDE apps? blech.
#build_framework phonon -DPHONON_BUILD_PHONON4QT5=ON

for FRAMEWORK in karchive kconfig kwidgetsaddons kcompletion kcoreaddons polkit-qt-1 kauth kcodecs kdoctools kguiaddons ki18n kconfigwidgets kwindowsystem kcrash kdbusaddons kitemviews kiconthemes kjobwidgets kservice solid sonnet ktextwidgets attica kglobalaccel kxmlgui kbookmarks kio knotifications knotifyconfig knewstuff kpackage kdeclarative ; do
  build_framework $FRAMEWORK "-DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON"
done
build_framework breeze-icons "-DBINARY_ICONS_RESOURCE=1 -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON"
build_framework kdecoration "-DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON"
build_framework breeze "-DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON"

cd ..

echo "+++++++++++++\n BUILDING FRAMEWORKS DONE \n+++++++++++++++"

