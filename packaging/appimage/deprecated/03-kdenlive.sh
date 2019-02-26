#!/bin/bash

# Halt on errors
set -e

# Be verbose
set -x

# Now we are inside CentOS 6
grep -r "CentOS release 6" /etc/redhat-release || exit 1

# Get helper functions
wget -q https://github.com/probonopd/AppImages/raw/master/functions.sh -O ./functions.sh
. ./functions.sh
rm -f functions.sh

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

# Workaround for: On CentOS 6, .pc files in /usr/lib/pkgconfig are not recognized
# However, this is where .pc files get installed when bulding libraries... (FIXME)
# I found this by comparing the output of librevenge's "make install" command
# between Ubuntu and CentOS 6

#ln -sf /usr/share/pkgconfig /usr/lib/pkgconfig

#update ruby
yum -y remove ruby ruby-devel
if ( test -d /external/ruby-2.1.2 )
then
        echo "RUBY already cloned"
else
	cd /external
	wget http://cache.ruby-lang.org/pub/ruby/2.1/ruby-2.1.2.tar.gz
	tar xvfvz ruby-2.1.2.tar.gz
fi
cd /external/ruby-2.1.2
./configure
make
make install 
gem update --system

# Get project
if [ ! -d /kdenlive ] ; then
    git clone --depth 1 http://anongit.kde.org/kdenlive.git /kdenlive
    cd /kdenlive/
#    git checkout -b timeline2 origin/refactoring_timeline
else
    cd /kdenlive/
    if [ $# -eq 0 ]; then
        git pull
    fi
fi
cd /kdenlive/
#git_pull_rebase_helper

# Prepare the install location
#rm -rf /app/ || true
#mkdir -p /app/usr

# export LLVM_ROOT=/opt/llvm/

# make sure lib and lib64 are the same thing
mkdir -p /app/usr/lib
cd  /app/usr
#ln -s lib lib64

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


IN=frei0r,https://github.com/dyne/frei0r.git,true,""
IFS=',' read -a external_options <<< $IN
EXTERNAL="${external_options[0]}"
EXTERNAL_ADDRESS="${external_options[1]}"
EXTERNAL_CMAKE="${external_options[2]}"
EXTERNAL_CMAKE_OPTIONS="-DWITHOUT_OPENCV:BOOL=ON"
EXTERNAL_CONFIGURE="${external_options[3]}"
build_external $EXTERNAL $EXTERNAL_CMAKE_OPTIONS

#TODO: dependencies for MLT modules (xml, sdl, etc).

#movit - - Requires some adjustments to build with older automake
#cd /external
#if ( test -d /external/movit )
#then
#        echo "movit already cloned"
#if [ $# -eq 0 ]; then
#        cd movit
#        git pull
#        git reset --hard
#        git pull --rebase
#        cd ..
#fi
#else
#       git clone https://git.sesse.net/movit
#fi
#cd movit
#./autogen.sh
#./configure --prefix=/app/usr
#make -j5
#make install


#mlt
cd /external
if ( test -d /external/mlt )
then
        echo "mlt already cloned"
if [ $# -eq 0 ]; then
        cd mlt
        git reset --hard
        git pull --rebase
        #current git master incompatible (crashes Kdenlive on keyframes)
        git checkout 15105c4
        cd ..
fi
else
       git clone https://github.com/mltframework/mlt.git
fi
cd mlt


# patch MLT
	cat > mlt_fix.patch << 'EOF'
diff --git a/src/modules/vid.stab/configure b/src/modules/vid.stab/configure
index e501888..55f0307 100755
--- a/src/modules/vid.stab/configure
+++ b/src/modules/vid.stab/configure
@@ -19,7 +19,8 @@ then
		exit 0
	fi
 
-	echo > config.mak
+	echo "CFLAGS += $(pkg-config --cflags vidstab)" > config.mak
+	echo "LDFLAGS += $(pkg-config --libs vidstab)" >> config.mak
	case $targetos in
	Darwin)
		;;
EOF
cat mlt_fix.patch |patch -p1

export CXXFLAGS="-std=c++11"
./configure --enable-gpl --prefix=/app/usr --disable-rtaudio
make -j5
make install



# Build kdenlive
mkdir -p /kdenlive_build
mkdir -p /kdenlive_build/po

if [ ! -d /kdenlive/po ] ; then
ln -s /kdenlive_build/po /kdenlive/po
fi

cd /kdenlive_build
cmake3 ../kdenlive \
    -DCMAKE_INSTALL_PREFIX:PATH=/app/usr/ \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DPACKAGERS_BUILD=1 \
    -DKDE_INSTALL_USE_QT_SYS_PATHS:BOOL=ON \
    -DKDE_L10N_AUTO_TRANSLATIONS:BOOL=ON \
    -DBUILD_TESTING=FALSE
make -j8
make install

echo "++++++++++++++++++\nBUILD FINSHED \n+++++++++++++++++";
###############################################################
# Build complete, AppImage bundling begins here
###############################################################

