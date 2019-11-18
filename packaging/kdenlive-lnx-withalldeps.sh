#!/bin/bash
# (C) 2018 V. Pinon <vpinon@kde.org>
# License: GPLv2+
# Tested in Debian sid x86_64

# Be verbose, halt on errors
set -xe

# Prepare the install location
# change this to another location if you prefer
export WLD=/app/usr
SRC_DIR=/external

mkdir -p $WLD/lib $SRC_DIR
# make sure lib and lib64 are the same thing
[[ -L $WLD/lib64 ]] || ln -s $WLD/lib $WLD/lib64
# qjsonparser, used to add metadata to the plugins needs to work in a en_US.UTF-8 environment.
export LC_ALL=en_US.UTF-8 LANG=en_us.UTF-8
export PATH=$WLD/bin:$PATH
export LD_LIBRARY_PATH=$WLD/lib:/usr/lib64/:/usr/lib
export PKG_CONFIG_PATH=$WLD/lib/pkgconfig/:$WLD/share/pkgconfig/:/usr/lib/pkgconfig
export ACLOCAL_PATH=$WLD/share/aclocal
export ACLOCAL="aclocal -I $ACLOCAL_PATH"

CPU_CORES=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu)
if [[ $CPU_CORES -gt 1 ]]; then
    CPU_CORES=$((CPU_CORES-1))
fi
ARCH=$(arch)
if [[ "$ARCH" != "i686" && "$ARCH" != "x86_64" ]] ; then
    echo "Architecture could not be determined" ; exit 1
fi

# $1: repo URL, $2: branch
function git_pull {
    URL=$1 BRANCH=$2
    PKG=${URL##*/}; PKG=${PKG%%.git}
    cd $SRC_DIR
    if [ -d $PKG ]; then
        echo "$PKG already cloned"
        cd $PKG
        git reset --hard
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
    cd $SRC_DIR
    if [ -d $SRC_DIR/$PKGVERS ]; then
        echo "$PKGVERS already downloaded"
    else
        [ -f $ARCHIVE ] || wget --no-check-certificate $URL
        if [[ ${ARCHIVE##*.} == zip ]] ; then
            unzip $ARCHIVE
        else
            tar -xf ${URL##*/}
        fi
    fi
}

# $1: package name ; $*: cmake args
function cmake_make {
    PKG=$1; shift; CMAKE_ARGS=$*
    mkdir -p $SRC_DIR/$PKG/build
    cd $SRC_DIR/$PKG/build
    cmake -DCMAKE_INSTALL_PREFIX:PATH=$WLD -DCMAKE_BUILD_TYPE=Release $CMAKE_ARGS ..
    make -j$CPU_CORES
    make install
}

# $1: package name ; $2: configure args
function configure_make {
    PKG=$1; shift; CONFIGURE_ARGS=$*
    cd $SRC_DIR/$PKG
    ./configure --prefix=$WLD $CONFIGURE_ARGS
    make -j$CPU_CORES
    make install
}

# Build tools
wget_extract https://www.tortall.net/projects/yasm/releases/yasm-1.3.0.tar.gz
cmake_make yasm-1.3.0
wget_extract http://www.nasm.us/pub/nasm/releasebuilds/2.13.02/nasm-2.13.02.tar.xz
configure_make nasm-2.13.02

# Media libs
wget_extract ftp://ftp.alsa-project.org/pub/lib/alsa-lib-1.1.5.tar.bz2
configure_make alsa-lib-1.1.5
wget_extract https://libsdl.org/release/SDL2-2.0.8.tar.gz
configure_make SDL2-2.0.8 --with-alsa-prefix=$WLD/lib --with-alsa-inc-prefix=$WLD/include

# Image libs
wget_extract https://sourceforge.net/projects/libjpeg-turbo/files/1.5.3/libjpeg-turbo-1.5.3.tar.gz
configure_make libjpeg-turbo-1.5.3
wget_extract http://www.ece.uvic.ca/~frodo/jasper/software/jasper-2.0.14.tar.gz
cmake_make jasper-2.0.14
wget_extract http://prdownloads.sourceforge.net/libpng/libpng-1.6.34.tar.xz
configure_make libpng-1.6.34
wget_extract https://codeload.github.com/Exiv2/exiv2/tar.gz/master
cmake_make exiv2-master \
    -DEXIV2_ENABLE_NLS=OFF -DEXIV2_ENABLE_PRINTUCS2=OFF -DEXIV2_ENABLE_LENSDATA=OFF -DEXIV2_ENABLE_BUILD_SAMPLES=OFF
# Codec libs
wget_extract http://www.mega-nerd.com/libsndfile/files/libsndfile-1.0.28.tar.gz
configure_make libsndfile-1.0.28
wget_extract http://www.mega-nerd.com/SRC/libsamplerate-0.1.9.tar.gz
configure_make libsamplerate-0.1.9
git_pull https://anonscm.debian.org/git/pkg-multimedia/libvpx.git
configure_make libvpx --enable-shared
git_pull https://anonscm.debian.org/git/pkg-multimedia/x264.git
configure_make x264 --enable-shared
wget_extract http://ftp.videolan.org/pub/videolan/x265/x265_2.7.tar.gz
pushd $SRC_DIR/x265_2.7/build/linux
cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="$WLD" -DENABLE_SHARED=ON ../../source
make -j$CPU_CORES
make install
popd
git_pull https://anonscm.debian.org/git/pkg-multimedia/ffmpeg.git debian/7%3.4.2-1
configure_make ffmpeg --extra-ldflags="-L$WLD/lib" --extra-cflags="-I$WLD/include" \
    --enable-shared --enable-gpl --disable-doc --enable-avfilter --enable-avresample \
    --enable-libvpx --enable-libx264 --enable-libx265

# Graphics libs
wget_extract https://www.cairographics.org/releases/cairo-1.14.12.tar.xz
configure_make cairo-1.14.12
wget_extract https://www.freedesktop.org/software/harfbuzz/release/harfbuzz-1.7.6.tar.bz2
configure_make harfbuzz-1.7.6
#requires libfribidi-dev
wget_extract https://download.gnome.org/sources/pango/1.42/pango-1.42.0.tar.xz
configure_make pango-1.42.0
wget_extract https://download.gnome.org/sources/gdk-pixbuf/2.32/gdk-pixbuf-2.32.3.tar.xz
configure_make gdk-pixbuf-2.32.3
wget_extract https://download.gnome.org/sources/gtk+/2.24/gtk+-2.24.32.tar.xz
configure_make gtk+-2.24.32
wget_extract https://download.qt.io/official_releases/qt/5.13/5.13.1/single/qt-everywhere-src-5.13.1.tar.xz
pushd $SRC_DIR/qt-everywhere-src-5.13.1
./configure -prefix $WLD -opensource -confirm-license -release -shared \
    -nomake examples -nomake tests -no-pch \
    -qt-zlib -qt-pcre -qt-harfbuzz -openssl \
    -qt-xcb -qt-xkbcommon-x11
make -j$CPU_CORES
make install
popd

# libxcb-keysyms1-dev
# KDE Frameworks
KF5_VERSION=5.62.0
for FRAMEWORK in \
        extra-cmake-modules breeze-icons karchive kconfig kcoreaddons kdbusaddons kguiaddons \
        ki18n kitemviews kwidgetsaddons kcompletion kwindowsystem \
        kcrash kjobwidgets kauth kcodecs kconfigwidgets kiconthemes \
        solid sonnet attica kservice kglobalaccel ktextwidgets \
        kxmlgui kbookmarks knotifications kio knewstuff knotifyconfig \
        kpackage kdeclarative ; do
    #git_pull git://anongit.kde.org/$FRAMEWORK v$KF5_VERSION
    wget_extract https://download.kde.org/stable/frameworks/${KF5_VERSION%.*}/$FRAMEWORK-$KF5_VERSION.tar.xz
    if [ "$FRAMEWORK" = "breeze-icons" ]; then
        cmake_make $FRAMEWORK-$KF5_VERSION -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON -DBUILD_TESTING:BOOL=OFF -DBINARY_ICONS_RESOURCE=1
    else
        cmake_make $FRAMEWORK-$KF5_VERSION -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON -DBUILD_TESTING:BOOL=OFF
    fi
done
wget_extract https://download.kde.org/stable/plasma/5.12.4/kdecoration-5.12.4.tar.xz
cmake_make kdecoration-5.12.4 -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON -DBUILD_TESTING:BOOL=OFF
wget_extract https://download.kde.org/stable/plasma/5.12.4/breeze-5.12.4.tar.xz
cmake_make breeze-5.12.4 -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON -DBUILD_TESTING:BOOL=OFF

wget_extract https://codeload.github.com/opencv/opencv/tar.gz/3.4.1
wget_extract https://github.com/opencv/opencv_contrib/archive/3.4.1.tar.gz
cmake_make opencv-3.4.1 -DOPENCV_EXTRA_MODULES_PATH=../../opencv_contrib-3.4.1/modules \
    -DJASPER_INCLUDE_DIR=$WLD/include \
    -DBUILD_opencv_plot=ON \
    -DBUILD_opencv_aruco=OFF -DBUILD_opencv_bgsegm=OFF -DBUILD_opencv_bioinspired=OFF \
    -DBUILD_opencv_ccalib=OFF -DBUILD_opencv_cnn_3dobj=OFF -DBUILD_opencv_cvv=OFF \
    -DBUILD_opencv_dnn=OFF -DBUILD_opencv_dnns_easily_fooled=OFF -DBUILD_opencv_dpm=OFF \
    -DBUILD_opencv_fuzzy=OFF -DBUILD_opencv_hdf=OFF -DBUILD_opencv_line_descriptor=OFF \
    -DBUILD_opencv_matlab=OFF -DBUILD_opencv_optflow=OFF \
    -DBUILD_opencv_reg=OFF -DBUILD_opencv_rgbd=OFF -DBUILD_opencv_saliency=OFF \
    -DBUILD_opencv_sfm=OFF -DBUILD_opencv_stereo=OFF -DBUILD_opencv_structured_light=OFF \
    -DBUILD_opencv_surface_matching=OFF -DBUILD_opencv_xfeatures2d=OFF \
    -DBUILD_opencv_xobjdetect=OFF -DBUILD_opencv_xphoto=OFF -DBUILD_opencv_calib3d=OFF \
    -DBUILD_opencv_cudaarithm=OFF -DBUILD_opencv_cudabgsegm=OFF \
    -DBUILD_opencv_cudacodec=OFF -DBUILD_opencv_cudafilters=OFF \
    -DBUILD_opencv_cudalegacy=OFF -DBUILD_opencv_cudaobjdetect=OFF \
    -DBUILD_opencv_cudaoptflow=OFF -DBUILD_opencv_cudastereo=OFF \
    -DBUILD_opencv_cudawarping=OFF -DBUILD_opencv_cudev=OFF \
    -DBUILD_opencv_java=OFF -DBUILD_opencv_shape=OFF -DBUILD_opencv_stitching=OFF \
    -DBUILD_opencv_superres=OFF -DBUILD_opencv_ts=OFF -DBUILD_opencv_videoio=OFF \
    -DBUILD_opencv_videostab=OFF -DBUILD_opencv_viz=OFF
#git_pull https://github.com/georgmartius/vid.stab
wget_extract https://codeload.github.com/georgmartius/vid.stab/tar.gz/v1.1.0
cmake_make vid.stab-1.1.0
wget_extract https://files.dyne.org/frei0r/releases/frei0r-plugins-1.6.1.tar.gz
cmake_make frei0r-plugins-1.6.1 -DWITHOUT_OPENCV:BOOL=ON
# git_pull https://git.sesse.net/movit
# pushd $SRC_DIR/movit
# ./autogen.sh
# popd
# configure_make movit

git_pull https://github.com/mltframework/mlt.git
configure_make mlt --enable-gpl --disable-rtaudio
#po->ruby
git_pull git://anongit.kde.org/kdenlive
cmake_make kdenlive -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON -DPACKAGERS_BUILD=1 -DBUILD_TESTING=FALSE
#-DKDE_L10N_AUTO_TRANSLATIONS:BOOL=ON \
