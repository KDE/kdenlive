#!/bin/bash

# Halt on errors
set -e

# Be verbose
set -x

# Now we are inside CentOS 6
grep -r "CentOS release 6" /etc/redhat-release || exit 1

. /opt/rh/devtoolset-3/enable

#necessary ?
#pulseaudio-libs 

QTVERSION=5.9.4
QVERSION_SHORT=5.9
QTDIR=/usr/local/Qt-${QTVERSION}/

# qjsonparser, used to add metadata to the plugins needs to work in a en_US.UTF-8 environment. That's
# not always set correctly in CentOS 6.7
export LC_ALL=en_US.UTF-8
export LANG=en_us.UTF-8

# Determine which architecture should be built
if [[ "$(arch)" = "i686" || "$(arch)" = "x86_64" ]] ; then
  ARCH=$(arch)
else
  echo "Architecture could not be determined"
  exit 1
fi

# Make sure we build from the /, parts of this script depends on that. We also need to run as root...
cd  /

# TO-DO ask about this.
export CMAKE_PREFIX_PATH=$QTDIR:/app/share/llvm/

# if the library path doesn't point to our usr/lib, linking will be broken and we won't find all deps either
export LD_LIBRARY_PATH=/usr/lib64/:/usr/lib:/app/usr/lib:$QTDIR/lib/:/opt/python3.5/lib/:$LD_LIBRARY_PATH
#export LD_LIBRARY_PATH=/usr/lib64/:/usr/lib:/app/usr/lib:$QTDIR/lib/:$LD_LIBRARY_PATH

# Prepare the install location
#rm -rf /app/ || true
mkdir -p /app/usr

# export LLVM_ROOT=/opt/llvm/

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
if [ $# -eq 0 ]; then
        cd $EXTERNAL
        git reset --hard
        git pull --rebase
        cd ..
fi
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

#libsndfile
cd /external
if ( test -d /external/libsndfile-1.0.28 )
then
        echo "libsndfile already downloaded"
else
	wget http://www.mega-nerd.com/libsndfile/files/libsndfile-1.0.28.tar.gz
	tar -xf libsndfile-1.0.28.tar.gz
fi
cd libsndfile-1.0.28
./configure --prefix=$WLD 
make -j5
make install

#libsamplerate
cd /external
if ( test -d /external/libsamplerate-0.1.9 )
then
        echo "libsamplerate already downloaded"
else
	wget http://www.mega-nerd.com/SRC/libsamplerate-0.1.9.tar.gz
	tar -xf libsamplerate-0.1.9.tar.gz
fi
cd libsamplerate-0.1.9
./configure --prefix=$WLD 
make -j5
make install

#ladspa-plugins (shw)
cd /external
if ( test -d /external/ladspa )
then
        echo "ladspa already downloaded, on failure, replace AC_CONFIG_MACRO_DIRS([m4]) with AC_CONFIG_MACRO_DIR([m4])"
        cd ladspa
        git reset --hard
        git pull --rebase
        cd ..
else
	git clone https://github.com/swh/ladspa.git
fi
cd ladspa
echo "Readying ladspa, on failure, replace AC_CONFIG_MACRO_DIRS([m4]) with AC_CONFIG_MACRO_DIR([m4]) in configure.ac"
autoreconf -i
./configure --prefix=$WLD
make -j5
make install


#alsa-lib
cd /external
if ( test -d /external/alsa-lib-1.1.5 )
then
        echo "alsa-lib already downloaded"
else
	wget ftp://ftp.alsa-project.org/pub/lib/alsa-lib-1.1.5.tar.bz2
	tar -xf alsa-lib-1.1.5.tar.bz2
fi
cd alsa-lib-1.1.5
./configure --prefix=$WLD 
make -j5
make install


#libx264
cd /external
if ( test -d /external/x264 )
then
        echo "libx264 already cloned"
if [ $# -eq 0 ]; then
        cd x264
        git reset --hard
        git pull --rebase
        cd ..
fi
        # make distclean
else
	#git clone git://git.videolan.org/x264.git
	git clone https://anonscm.debian.org/git/pkg-multimedia/x264.git
fi
cd x264
./configure --enable-static --enable-shared --prefix=$WLD 
make -j5
make install


#libx265
cd /external
if ( test -d /external/x265 )
then 
	echo "libx265 already downloaded"
        cd x265
if [ $# -eq 0 ]; then
        git reset --hard
        git pull --rebase
fi
        cd build/linux
        # make distclean
else
    git clone https://anonscm.debian.org/git/pkg-multimedia/x265.git
    cd x265/build/linux
fi
cmake3 -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="$WLD" -DENABLE_PIC=ON -DENABLE_SHARED=OFF ../../source
make -j5
make install

#libvpx
cd /external
if ( test -d /external/libvpx )
then
        echo "libvpx already cloned"
if [ $# -eq 0 ]; then
        cd libvpx
        git reset --hard
        git pull --rebase
        cd ..
fi
        #make clean
else
	git clone https://anonscm.debian.org/git/pkg-multimedia/libvpx.git
fi
cd libvpx
./configure --enable-static --as=yasm --enable-shared --prefix=$WLD 
make -j5
make install

#ffmpeg
cd /external
if ( test -d /external/ffmpeg )
then
        echo "ffmpeg already cloned"
if [ $# -eq 0 ]; then
        cd ffmpeg
        git reset --hard
#        git pull --rebase
	git checkout debian/7%3.3.4-2
        cd ..
fi
else
#       git clone https://git.ffmpeg.org/ffmpeg.git
#       Use debian source
        git clone https://anonscm.debian.org/git/pkg-multimedia/ffmpeg.git
fi
cd ffmpeg
./configure --prefix=$WLD --extra-ldflags="-L$WLD/lib -lstdc++ -lm -lrt -ldl" --extra-cflags="-I$WLD/include" --enable-shared --enable-avfilter --enable-gpl --enable-avresample --enable-libx265 --enable-libvpx --enable-libx264 --disable-doc
make -j5
make install

#opencv_contrib
# download manually (hash mismatch)
# https://github.com/opencv/opencv_3rdparty/tree/ippicv/master_20151201/ippicv


if ( test -d /external/cairo-1.14.10 )
then 
    echo "cairo already downloaded"
    cd /external/cairo-1.14.10
else
    cd /external
    wget https://www.cairographics.org/releases/cairo-1.14.10.tar.xz
    tar -xf cairo-1.14.10.tar.xz
    cd /external/cairo-1.14.10
    ./autogen.sh
fi
./configure --prefix=$WLD
make -j5
make install
/bin/cp /app/usr/lib/pkgconfig/cairo* /usr/lib/pkgconfig/

if ( test -d /external/harfbuzz-0.9.10 )
then
    echo "pango already downloaded"
    cd /external/harfbuzz-0.9.10
else
    cd /external
    wget https://www.freedesktop.org/software/harfbuzz/release/harfbuzz-0.9.10.tar.bz2
    tar -xf harfbuzz-0.9.10.tar.bz2
    cd /external/harfbuzz-0.9.10
fi
./configure --prefix=$WLD
make -j5
make install
/bin/cp /app/usr/lib/pkgconfig/harf* /usr/lib/pkgconfig/
/bin/cp /app/usr/lib/libharfbuzz.* /usr/lib

if ( test -d /external/pango-1.28.4 )
then
    echo "pango already downloaded"
    cd /external/pango-1.28.4
else
    cd /external
    wget http://ftp.gnome.org/pub/GNOME/sources/pango/1.28/pango-1.28.4.tar.gz
    tar -xf pango-1.28.4.tar.gz
    cd /external/pango-1.28.4
fi
./configure --prefix=$WLD
make -j5
make install
/bin/cp /app/usr/lib/pkgconfig/pango* /usr/lib/pkgconfig/

if ( test -d /external/gdk-pixbuf-2.23.5 )
then
    echo "gdk-pixbuf already downloaded"
    cd /external/gdk-pixbuf-2.23.5
else
    cd /external
    wget http://ftp.gnome.org/pub/GNOME/sources/gdk-pixbuf/2.23/gdk-pixbuf-2.23.5.tar.xz
    tar -xf gdk-pixbuf-2.23.5.tar.xz
    cd /external/gdk-pixbuf-2.23.5
fi
./configure --prefix=$WLD
make -j5
make install
/bin/cp /app/usr/lib/pkgconfig/gdk* /usr/lib/pkgconfig/


if ( test -d /external/gtk+-2.23.90 )
then
    echo "gtk2 already downloaded"
    cd /external/gtk+-2.23.90
else
    cd /external
    wget http://ftp.gnome.org/pub/GNOME/sources/gtk+/2.23/gtk+-2.23.90.tar.gz
    tar -xf gtk+-2.23.90.tar.gz
    cd /external/gtk+-2.23.90
fi
./configure --prefix=$WLD
make -j5
make install
/bin/cp /app/usr/lib/pkgconfig/gtk* /usr/lib/pkgconfig/
/bin/cp /app/usr/lib/pkgconfig/gdk* /usr/lib/pkgconfig/

cd /external
if ( test -d /external/opencv_contrib )
then
        echo "opencv_contrib already cloned"
if [ $# -eq 0 ]; then
        cd opencv_contrib
        git reset --hard
#        git pull --rebase
        git checkout 3.3.0
        cd ..
fi
else
       git clone https://github.com/opencv/opencv_contrib.git
fi

cd /external
if ( test -d /external/opencv )
then
        echo "opencv already cloned"
if [ $# -eq 0 ]; then
        cd opencv
        git reset --hard
#        git pull --rebase
        git checkout 3.3.0
        cd ..
fi
else
       git clone https://github.com/opencv/opencv.git
fi

#opencv
IN=opencv,https://github.com/opencv/opencv.git,true,"" 
IFS=',' read -a external_options <<< $IN
EXTERNAL="${external_options[0]}"
EXTERNAL_ADDRESS="${external_options[1]}"
EXTERNAL_CMAKE="${external_options[2]}"
EXTERNAL_CMAKE_OPTIONS="-DOPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules -DBUILD_opencv_aruco=OFF -DBUILD_opencv_bgsegm=OFF -DBUILD_opencv_bioinspired=OFF -DBUILD_opencv_ccalib=OFF -DBUILD_opencv_cnn_3dobj=OFF -DBUILD_opencv_cvv=OFF -DBUILD_opencv_dnn=OFF -DBUILD_opencv_dnns_easily_fooled=OFF -DBUILD_opencv_dpm=OFF -DBUILD_opencv_fuzzy=OFF -DBUILD_opencv_hdf=OFF -DBUILD_opencv_line_descriptor=OFF -DBUILD_opencv_matlab=OFF -DBUILD_opencv_optflow=OFF -DBUILD_opencv_plot=ON -DBUILD_opencv_reg=OFF -DBUILD_opencv_rgbd=OFF -DBUILD_opencv_saliency=OFF -DBUILD_opencv_sfm=OFF -DBUILD_opencv_stereo=OFF -DBUILD_opencv_structured_light=OFF -DBUILD_opencv_surface_matching=OFF -DBUILD_opencv_xfeatures2d=OFF -DBUILD_opencv_xobjdetect=OFF -DBUILD_opencv_xphoto=OFF -DBUILD_opencv_calib3d=OFF -DBUILD_opencv_cudaarithm=OFF -DBUILD_opencv_cudabgsegm=OFF -DBUILD_opencv_cudacodec=OFF -DBUILD_opencv_cudafilters=OFF -DBUILD_opencv_cudalegacy=OFF -DBUILD_opencv_cudaobjdetect=OFF -DBUILD_opencv_cudaoptflow=OFF -DBUILD_opencv_cudastereo=OFF -DBUILD_opencv_cudawarping=OFF -DBUILD_opencv_cudev=OFF -DBUILD_opencv_java=OFF -DBUILD_opencv_shape=OFF -DBUILD_opencv_stitching=OFF -DBUILD_opencv_superres=OFF -DBUILD_opencv_ts=OFF -DBUILD_opencv_videoio=OFF -DBUILD_opencv_videostab=OFF -DBUILD_opencv_viz=OFF"

# create build dir
SRC=/external
BUILD=/external/build
PREFIX=/app/usr/
mkdir -p $BUILD/opencv

# go there
cd $BUILD/opencv

cmake3 -DCMAKE_INSTALL_PREFIX:PATH=$PREFIX $EXTERNAL_CMAKE_OPTIONS $SRC/opencv

# make
make -j8

# install
make install


#EXTERNAL_CONFIGURE="${external_options[3]}"
#build_external $EXTERNAL $EXTERNAL_CMAKE_OPTIONS


#vidstab
IN=vid.stab,https://github.com/georgmartius/vid.stab.git,true,"" 
IFS=',' read -a external_options <<< $IN
EXTERNAL="${external_options[0]}"
EXTERNAL_ADDRESS="${external_options[1]}"
EXTERNAL_CMAKE="${external_options[2]}"
EXTERNAL_CONFIGURE="${external_options[3]}"
build_external $EXTERNAL


echo "/////////////////\nBUILDING AV LIBS DONE\n////////////////"
