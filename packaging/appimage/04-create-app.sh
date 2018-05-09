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

QTVERSION=5.9.1
QVERSION_SHORT=5.9
#QTDIR=/usr/local/Qt-${QTVERSION}/
QTDIR=/usr/

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

export WLD=/app/usr/   # change this to another location if you prefer
export LD_LIBRARY_PATH=$WLD/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$WLD/lib/pkgconfig/:$WLD/share/pkgconfig/:/usr/lib/pkgconfig/:$PKG_CONFIG_PATH
export PATH=$WLD/bin:$PATH
export ACLOCAL_PATH=$WLD/share/aclocal
export ACLOCAL="aclocal -I $ACLOCAL_PATH"


echo "++++++++++++++++++\nBUILD FINSHED \n+++++++++++++++++";
###############################################################
# Build complete, AppImage bundling begins here
###############################################################

cd /app

# FIXME: How to find out which subset of plugins is really needed? I used strace when running the binary
mkdir -p ./usr/lib/qt5/plugins/

PLUGINS=$(dirname $QTDIR/plugins/bearer)

/bin/cp -r $PLUGINS/{bearer,generic,imageformats,platforms,iconengines,platforminputcontexts,xcbglintegrations} ./usr/lib/qt5/plugins/
/bin/cp -r $PLUGINS/kf5 ./usr/lib/qt5/plugins/
/bin/cp -r $PLUGINS/mltpreview.so ./usr/lib/qt5/plugins/
mkdir -p ./usr/lib/qt5/plugins/styles/
/bin/cp -r /usr/plugins/styles/breeze.so ./usr/lib/qt5/plugins/styles/
/bin/cp -r /usr/plugins/kstyle_breeze_config.so ./usr/lib/qt5/plugins/

/bin/cp -r $QTDIR/qml ./usr/lib/qt5/
cp -ru /usr/share/mime/* /app/usr/share/mime
update-mime-database /app/usr/share/mime/

if ( test -d ./usr/lib/plugins )
then
    mv ./usr/lib/plugins/* ./usr/lib/qt5/plugins/
fi

/bin/cp $(ldconfig -p | grep libsasl2.so.2 | cut -d ">" -f 2 | xargs) ./usr/lib/
/bin/cp $(ldconfig -p | grep libGL.so.1 | cut -d ">" -f 2 | xargs) ./usr/lib/ # otherwise segfaults!?
/bin/cp $(ldconfig -p | grep libGLU.so.1 | cut -d ">" -f 2 | xargs) ./usr/lib/ # otherwise segfaults!?
# Fedora 23 seemed to be missing SOMETHING from the Centos 6.7. The only message was:
# This application failed to start because it could not find or load the Qt platform plugin "xcb".
# Setting export QT_DEBUG_PLUGINS=1 revealed the cause.
# QLibraryPrivate::loadPlugin failed on "/usr/lib64/qt5/plugins/platforms/libqxcb.so" : 
# "Cannot load library /usr/lib64/qt5/plugins/platforms/libqxcb.so: (/lib64/libEGL.so.1: undefined symbol: drmGetNodeTypeFromFd)"
# Which means that we have to copy libEGL.so.1 in too
# cp $(ldconfig -p | grep libEGL.so.1 | cut -d ">" -f 2 | xargs) ./usr/lib/ # Otherwise F23 cannot load the Qt platform plugin "xcb"
# let's not copy xcb itself, that breaks on dri3 systems https://bugs.kde.org/show_bug.cgi?id=360552
#cp $(ldconfig -p | grep libxcb.so.1 | cut -d ">" -f 2 | xargs) ./usr/lib/ 
/bin/cp $(ldconfig -p | grep libfreetype.so.6 | cut -d ">" -f 2 | xargs) ./usr/lib/ # For Fedora 20
# cp $(ldconfig -p | grep libfuse | cut -d ">" -f 2 | xargs) ./usr/lib/
#cp $(ldconfig -p | grep libselinux | cut -d ">" -f 2 | xargs) ./usr/lib/

ldd usr/bin/kdenlive | grep "=>" | awk '{print $3}' | xargs -I '{}' cp -v '{}' ./usr/lib || true
ldd usr/bin/plugins/platforms/libqxcb.so | grep "=>" | awk '{print $3}'  |  xargs -I '{}' cp -v '{}' ./usr/lib || true
 
# Copy in the indirect dependencies
FILES=$(find . -type f -executable)
 
for FILE in $FILES ; do
        ldd "${FILE}" | grep "=>" | awk '{print $3}' | xargs -I '{}' cp -v '{}' usr/lib || true
done

cd usr/ ; find . -type f -exec sed -i -e 's|/usr/lib|././/lib|g' {} \; ; cd ..

#DEPS=""
#for FILE in $FILES ; do
#  ldd "${FILE}" | grep "=>" | awk '{print $3}' | xargs -I '{}' echo '{}' > DEPSFILE
#done
#DEPS=$(cat DEPSFILE  |sort | uniq)
#for FILE in $DEPS ; do
#  if [ -f $FILE ] ; then
#    echo $FILE
#    cp --parents -rfL $FILE ./
#  fi
#done
#rm -f DEPSFILE

copy_deps
if [ -d lib64 ] ; then
    mv lib64/* usr/lib/
    rm -rf lib64/
fi

# YESOK1
 
# The following are assumed to be part of the base system
rm -f usr/lib/libcom_err.so.2 || true
rm -f usr/lib/libcrypt.so.1 || true
rm -f usr/lib/libdl.so.2 || true
rm -f usr/lib/libexpat.so.1 || true
#rm -f usr/lib/libfontconfig.so.1 || true
rm -f usr/lib/libgcc_s.so.1 || true
rm -f usr/lib/libglib-2.0.so.0 || true
rm -f usr/lib/libgpg-error.so.0 || true
rm -f usr/lib/libgssapi_krb5.so.2 || true
rm -f usr/lib/libgssapi.so.3 || true
rm -f usr/lib/libhcrypto.so.4 || true
rm -f usr/lib/libheimbase.so.1 || true
rm -f usr/lib/libheimntlm.so.0 || true
rm -f usr/lib/libhx509.so.5 || true
rm -f usr/lib/libICE.so.6 || true
rm -f usr/lib/libidn.so.11 || true
rm -f usr/lib/libk5crypto.so.3 || true
rm -f usr/lib/libkeyutils.so.1 || true
rm -f usr/lib/libkrb5.so.26 || true
rm -f usr/lib/libkrb5.so.3 || true
rm -f usr/lib/libkrb5support.so.0 || true
# rm -f usr/lib/liblber-2.4.so.2 || true # needed for debian wheezy
# rm -f usr/lib/libldap_r-2.4.so.2 || true # needed for debian wheezy
rm -f usr/lib/libm.so.6 || true
rm -f usr/lib/libp11-kit.so.0 || true
rm -f usr/lib/libpcre.so.3 || true
rm -f usr/lib/libpthread.so.0 || true
rm -f usr/lib/libresolv.so.2 || true
rm -f usr/lib/libroken.so.18 || true
rm -f usr/lib/librt.so.1 || true
rm -f usr/lib/libsasl2.so.2 || true
rm -f usr/lib/libSM.so.6 || true
rm -f usr/lib/libusb-1.0.so.0 || true
rm -f usr/lib/libuuid.so.1 || true
rm -f usr/lib/libwind.so.0 || true
rm -f usr/lib/libfontconfig.so.* || true
 
# Remove these libraries, we need to use the system versions; this means 11.04 is not supported (12.04 is our baseline)
# rm -f usr/lib/libEGL.so.* || true
rm -f usr/lib/libxcb* || true
rm -f usr/lib/libGL.so.* || true
rm -f usr/lib/libdrm.so.* || true
rm -f usr/lib/libX11.so.* || true

#YESOK2

#mv usr/local/Qt-*/lib/* usr/lib
#rm -rf usr/local/
# mv ./opt/python3.5/lib/* usr/lib
# mv ./opt/llvm/lib/* usr/lib
#rm  -rf ./opt/
rm -rf app/

#delete_blacklisted
BLACKLISTED_FILES=$(cat_file_from_url https://github.com/AppImage/AppImages/raw/master/excludelist | sed 's|#.*||g')
echo $BLACKLISTED_FILES
for FILE in $BLACKLISTED_FILES ; 
    do
    if [[ $FILE == libpango* ]] || [[ $FILE == libgdk_pixbuf* ]] ;
    then
      echo "keeping libpango"
    else
      FILES="$(find . -name "${FILE}" -not -path "./usr/optional/*")"
      for FOUND in $FILES ; do
        rm -vf "$FOUND" "$(readlink -f "$FOUND")"
      done
    fi
done

# Do not bundle developer stuff
rm -rf usr/include || true
rm -rf usr/lib/cmake || true
rm -rf usr/lib/pkgconfig || true
find . -name '*.la' | xargs -i rm {}
rm -rf usr/share/ECM/ || true
rm -rf usr/share/gettext || true
rm -rf usr/share/pkgconfig || true
rm -rf rm -rf ./usr/mkspecs/ || true

# Remove 

strip -g $(find usr) || true

if [ ! -z "$(ls -A usr/lib/libexec/kf5)" ]; then
    /bin/cp -R usr/lib/libexec/kf5/* /app/usr/bin/
fi;

cd /
if [ ! -d appimage-exec-wrapper ]; then
    git clone git://anongit.kde.org/scratch/brauch/appimage-exec-wrapper
fi;
cd /appimage-exec-wrapper/
make clean
make

cd /app
/bin/cp -v /appimage-exec-wrapper/exec.so exec_wrapper.so

cat > AppRun << EOF
#!/bin/bash

DIR="\`dirname \"\$0\"\`" 
DIR="\`( cd \"\$DIR\" && pwd )\`"
export APPDIR=\$DIR

export LD_PRELOAD=\$DIR/exec_wrapper.so

export APPIMAGE_ORIGINAL_QML2_IMPORT_PATH=\$QML2_IMPORT_PATH
export APPIMAGE_ORIGINAL_LD_LIBRARY_PATH=\$LD_LIBRARY_PATH
export APPIMAGE_ORIGINAL_QT_PLUGIN_PATH=\$QT_PLUGIN_PATH
export APPIMAGE_ORIGINAL_XDG_DATA_DIRS=\$XDG_DATA_DIRS
export APPIMAGE_ORIGINAL_XDG_CONFIG_HOME=\$XDG_CONFIG_HOME
export APPIMAGE_ORIGINAL_PATH=\$PATH

export APPIMAGE_ORIGINAL_MLT_REPOSITORY=\$MLT_REPOSITORY
export APPIMAGE_ORIGINAL_MLT_DATA=\$MLT_DATA
export APPIMAGE_ORIGINAL_MLT_ROOT_DIR=\$MLT_ROOT_DIR
export APPIMAGE_ORIGINAL_MLT_PROFILES_PATH=\$MLT_PROFILES_PATH
export APPIMAGE_ORIGINAL_MLT_PRESETS_PATH=\$MLT_PRESETS_PATH


export QML2_IMPORT_PATH=\$DIR/usr/lib/qt5/qml:\$QML2_IMPORT_PATH
export LD_LIBRARY_PATH=\$DIR/usr/lib/:\$LD_LIBRARY_PATH
export QT_PLUGIN_PATH=\$DIR/usr/lib/qt5/plugins/
export XDG_DATA_DIRS=\$DIR/usr/share/:\$XDG_DATA_DIRS
export XDG_CONFIG_HOME=\$HOME/.config/kdenlive-appimage/
export PATH=\$DIR/usr/bin:\$PATH
export KDE_FORK_SLAVES=1

export MLT_REPOSITORY=\$DIR/usr/lib/mlt/
export MLT_DATA=\$DIR/usr/share/mlt/
export MLT_ROOT_DIR=\$DIR/usr/
export LADSPA_PATH=\$DIR/usr/lib/ladspa
export FREI0R_PATH=\$DIR/usr/lib/frei0r-1
export MLT_PROFILES_PATH=\$DIR/usr/share/mlt/profiles/
export MLT_PRESETS_PATH=\$DIR/usr/share/mlt/presets/
export SDL_AUDIODRIVER=pulseaudio
export XDG_CURRENT_DESKTOP=

export APPIMAGE_STARTUP_QML2_IMPORT_PATH=\$QML2_IMPORT_PATH
export APPIMAGE_STARTUP_LD_LIBRARY_PATH=\$LD_LIBRARY_PATH
export APPIMAGE_STARTUP_QT_PLUGIN_PATH=\$QT_PLUGIN_PATH
export APPIMAGE_STARTUP_XDG_DATA_DIRS=\$XDG_DATA_DIRS
export APPIMAGE_STARTUP_XDG_CONFIG_HOME=\$XDG_CONFIG_HOME
export APPIMAGE_STARTUP_PATH=\$PATH

export APPIMAGE_STARTUP_MLT_REPOSITORY=\$MLT_REPOSITORY
export APPIMAGE_STARTUP_MLT_DATA=\$MLT_DATA
export APPIMAGE_STARTUP_MLT_ROOT_DIR=\$MLT_ROOT_DIR
export APPIMAGE_STARTUP_MLT_PROFILES_PATH=\$MLT_PROFILES_PATH
export APPIMAGE_STARTUP_MLT_PRESETS_PATH=\$MLT_PRESETS_PATH
export APPIMAGE_STARTUP_SDL_AUDIODRIVER=\$SDL_AUDIODRIVER
export APPIMAGE_STARTUP_XDG_CURRENT_DESKTOP=\$XDG_CURRENT_DESKTOP

kdenlive --config kdenlive-appimagerc \$@
EOF
chmod +x AppRun

/bin/cp /kdenlive/data/org.kde.kdenlive.desktop kdenlive.desktop
/bin/cp /kdenlive/data/icons/128-apps-kdenlive.png ./kdenlive.png

#TO-DO this will need some text manipulation in ruby to get this var.
APP=Kdenlive
LOWERAPP=kdenlive

get_desktopintegration kdenlive

cd  /

#cleanup previous image
rm -Rf /$APP/$APP.AppDir

mkdir -p /$APP/$APP.AppDir
cd /$APP/
cp -R ../app/* $APP.AppDir/

# Remove useless stuff
rm -Rf $APP.AppDir/usr/share/wallpapers/ || true
rm -Rf $APP.AppDir/usr/share/kconf_update/ || true
rm -Rf $APP.AppDir/usr/share/gtk-2.0/ || true
rm -Rf $APP.AppDir/usr/share/gtk-doc/ || true
rm -Rf $APP.AppDir/usr/share/kf5/kdoctools/ || true
rm -Rf $APP.AppDir/usr/share/kservices5/searchproviders/ || true
rm -Rf $APP.AppDir/usr/share/kservices5/useragentstrings/ || true
rm -Rf $APP.AppDir/usr/share/man/ || true
rm -Rf $APP.AppDir/usr/bin/ffserver || true
rm -Rf $APP.AppDir/usr/bin/gtk-demo || true
rm -Rf $APP.AppDir/usr/bin/yasm || true
rm -Rf $APP.AppDir/usr/bin/nasm || true
rm -Rf $APP.AppDir/usr/bin/ndisasm || true
rm -Rf $APP.AppDir/usr/bin/ytasm || true
rm -Rf $APP.AppDir/usr/bin/x264 || true
rm -Rf $APP.AppDir/usr/bin/x265 || true
rm -Rf $APP.AppDir/usr/bin/vsyasm || true
rm -Rf $APP.AppDir/usr/bin/vpxdec || true
rm -Rf $APP.AppDir/usr/bin/vpxenc || true
rm -Rf $APP.AppDir/usr/bin/meinproc5 || true
rm -Rf $APP.AppDir/usr/bin/desktoptojson || true
rm -Rf $APP.AppDir/usr/lib/kconf_update_bin/ || true



