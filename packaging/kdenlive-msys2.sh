#!/bin/bash
# (C) 2018 V. Pinon <vpinon@kde.org>
# License: GPLv2+
# Uses msys2 package management, download from msys2.org

# Be verbose, halt on errors
set -xe

[ -z "$MSYSTEM" ] && echo "Please run under MSYS/MINGW(64)" && exit 1

# change this to another location if you prefer
export SRC=$PWD/src
mkdir -p $SRC

export PREFIX=$MINGW_PREFIX

#if [ -n "$PREFIX" ] ; then
#    mkdir -p $PREFIX/lib
#    export PATH=$PREFIX/bin:$PATH
#    export LD_LIBRARY_PATH=$PREFIX/lib:/usr/lib
#    export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig:/usr/lib/pkgconfig
#fi

THREADS=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)
[[ $THREADS -gt 1 ]] && THREADS=$((THREADS-1))

# $@: package base names
function pacman_install {
    pacman -Sy --needed ${@/#/mingw-w64-x86_64-}
}

# $1: repo URL, $2: branch
function git_pull {
    URL=$1 BRANCH=$2
    PKG=${URL##*/}; PKG=${PKG%%.git}
    cd $SRC
    if [ -d $PKG ]; then
        echo "$PKG already cloned"
        cd $PKG
        # git reset --hard
        git checkout $BRANCH
        git pull --rebase
        cd ..
    else
        git clone $URL
    fi
}

# $1: archive URL
function wget_extract {
    URL=$1
    ARCHIVE=${URL##*/}
    PKGVERS=${ARCHIVE%%.tar.*} 
    cd $SRC
    if [ -d $SRC/$PKGVERS ]; then
        echo "$PKGVERS already downloaded"
    else
        [ -f $ARCHIVE ] || wget $URL
        if [[ ${ARCHIVE##*.} == zip ]] ; then
            unzip $ARCHIVE
        else
            tar -xf ${URL##*/}
        fi
    fi
}

# $1: package name ; $@: cmake args
function cmake_ninja {
    PKG=$1; shift; CMAKE_ARGS="$@"
    [ -n "$PREFIX" ] && CMAKE_ARGS+=" -DCMAKE_INSTALL_PREFIX:PATH=$PREFIX"
    if [ ! -f $SRC/$PKG/build/CMakeCache.txt ] ; then
        mkdir -p $SRC/$PKG/build
        cd $SRC/$PKG/build
        cmake .. -G Ninja $CMAKE_ARGS
    fi
    cd $SRC/$PKG/build
    ninja install
}

# $1: package name ; $2: configure args
function configure_make {
    PKG=$1; shift; CONFIGURE_ARGS="$@"
    [ -n "$PREFIX" ] && CONFIGURE_ARGS+=" --prefix=$PREFIX"
    cd $SRC/$PKG
    ./configure $CONFIGURE_ARGS
    mingw32-make -j$THREADS
    mingw32-make install
}


#### DOWNLOAD DEPS
if pacman -Suy ; then
    # MSYS tools
    pacman -Sy --needed tar git automake1.16
    # MINGW packages
    TOOLS="make cmake ninja pkg-config libtool" # ruby
    DEPS="gcc drmingw gavl opencv dlfcn SDL2 exiv2 libexif vid.stab ffmpeg gtk2 qt5 fftw ladspa-sdk eigen3 extra-cmake-modules"
    KF5=(breeze-icons karchive kconfig kcoreaddons kdbusaddons kguiaddons \
         ki18n kitemviews kwidgetsaddons kcompletion kwindowsystem \
         kcrash kjobwidgets kauth kcodecs kconfigwidgets kiconthemes \
         solid sonnet attica kservice kglobalaccel ktextwidgets \
         kxmlgui kbookmarks knotifications kio knewstuff \
         kpackage kdeclarative kirigami2 \
	 kcmutils plasma-framework)
    pacman_install $DEPS $TOOLS ${KF5[@]/%/-qt5}
fi

#### BUILD EXTRA KF5

KF5_VERSION=$(pacman -Ss $MINGW_PACKAGE_PREFIX-extra-cmake-modules | sed -n 's/.* \(5\..*\)-.*/\1/p')
for FRAMEWORK in knotifyconfig purpose frameworkintegration ; do
    #git_pull git://anongit.kde.org/$FRAMEWORK v$KF5_VERSION
    wget_extract https://download.kde.org/stable/frameworks/${KF5_VERSION%.*}/$FRAMEWORK-$KF5_VERSION.tar.xz
    cmake_ninja $FRAMEWORK-$KF5_VERSION \
        -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON 
        #-DBUILD_TESTING:BOOL=OFF \
        #-DKDE_INSTALL_LIBDIR=lib \
        #-DKDE_INSTALL_QMLDIR=share/qt5/qml \
        #-DKDE_INSTALL_QTPLUGINDIR=share/qt5/plugins \
        #-DKDE_INSTALL_DBUSDIR=share/dbus-1 \
        #-DKDE_INSTALL_MANDIR=share/man \
        #-DKDE_INSTALL_APPDIR=share/applications \
        #-DKDE_INSTALL_MIMEDIR=share/mime/packages \
        #-DECM_MKSPECS_INSTALL_DIR=$MINGW_PREFIX/share/qt5/mkspecs/modules \
        #-DECM_DIR=$MINGW_PREFIX/share/ECM \
done
wget_extract https://download.kde.org/stable/plasma/5.16.5/kdecoration-5.16.5.tar.xz
cmake_ninja kdecoration-5.16.5 -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON
wget_extract https://download.kde.org/stable/plasma/5.16.5/breeze-5.16.5.tar.xz || true # symlink errors
cmake_ninja breeze-5.16.5 -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON

#### BUILD MULTIMEDIA DEPS

if false ; then # Still Failing
    wget_extract https://github.com/jackaudio/jack2/releases/download/v1.9.12/jack2-1.9.12.tar.gz
    git_pull https://github.com/jackaudio/jack2.git
    pushd $SRC/jack2 #-1.9.12
    ./waf configure build install -j $THREADS --prefix=$PREFIX
    popd
    
    git_pull https://git.sesse.net/movit
    pushd $SRC/movit
    ./autogen.sh
    popd
    configure_make movit
fi

git_pull https://github.com/dyne/frei0r.git
cmake_ninja frei0r -DWITHOUT_OPENCV=ON

git_pull https://github.com/mltframework/mlt.git
configure_make mlt --enable-gpl --enable-gpl3 --disable-windeploy

#### BUILD KDENLIVE

wget_extract https://github.com/rttrorg/rttr/archive/v0.9.6.tar.gz
cmake_ninja rttr-0.9.6 \
	-DBUILD_EXAMPLES=OFF -DBUILD_DOCUMENTATION=OFF \
	-DBUILD_UNIT_TESTS=OFF -DBUILD_PACKAGE=OFF -DBUILD_STATIC=ON

git_pull https://anongit.kde.org/kdenlive.git
#git_pull https://anongit.kde.org/releaseme.git
#pushd $SRC/kdenlive
#ruby ../releaseme/fetchpo.rb --origin stable --project kdenlive --output-dir po --output-poqm-dir poqm .
#popd
mkdir -p $SRC/kdenlive/build
cd $SRC/kdenlive/build
cmake_ninja kdenlive -DCMAKE_BUILD_TYPE=RelWithDebSymbols -DBUILD_TESTING:BOOL=OFF -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON


