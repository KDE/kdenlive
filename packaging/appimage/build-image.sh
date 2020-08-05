#!/bin/bash

# Halt on errors and be verbose about what we are doing
set -e
set -x

# Read in our parameters
export BUILD_PREFIX=$1
export KDENLIVE_SOURCES=$2

# Save some frequently referenced locations in variables for ease of use / updating
export APPDIR=$BUILD_PREFIX/kdenlive.appdir
export PLUGINS=$APPDIR/usr/plugins/
export APPIMAGEPLUGINS=$APPDIR/usr/plugins/

mkdir -p $APPDIR
mkdir -p $APPDIR/usr/share/kdenlive
mkdir -p $APPDIR/usr/lib
mkdir -p $PLUGINS
mkdir -p $APPDIR/usr/plugins

# qjsonparser, used to add metadata to the plugins needs to work in a en_US.UTF-8 environment.
# That's not always the case, so make sure it is
export LC_ALL=en_US.UTF-8
export LANG=en_us.UTF-8

# We want to use $prefix/deps/usr/ for all our dependencies
export DEPS_INSTALL_PREFIX=$BUILD_PREFIX/deps/usr/
export DOWNLOADS_DIR=$BUILD_PREFIX/downloads/

# Setup variables needed to help everything find what we built
export LD_LIBRARY_PATH=$DEPS_INSTALL_PREFIX/lib/:$DEPS_INSTALL_PREFIX/lib/x86_64-linux-gnu/:$APPDIR/usr/lib/:$LD_LIBRARY_PATH
export PATH=$DEPS_INSTALL_PREFIX/bin/:$PATH
export PKG_CONFIG_PATH=$DEPS_INSTALL_PREFIX/share/pkgconfig/:$DEPS_INSTALL_PREFIX/lib/pkgconfig/:/usr/lib/pkgconfig/:$PKG_CONFIG_PATH
export CMAKE_PREFIX_PATH=$DEPS_INSTALL_PREFIX:$CMAKE_PREFIX_PATH

# Switch over to our build prefix
cd $BUILD_PREFIX

#
# Now we can get the process started!
#

# Step 1: Copy over all the resources provided by dependencies that we need
cp -r $DEPS_INSTALL_PREFIX/share/kf5 $APPDIR/usr/share
cp -r $DEPS_INSTALL_PREFIX/share/kstyle $APPDIR/usr/share
cp -r $DEPS_INSTALL_PREFIX/share/plasma $APPDIR/usr/share
cp -r $DEPS_INSTALL_PREFIX/share/alsa $APPDIR/usr/share
cp -r $DEPS_INSTALL_PREFIX/share/kservices5 $APPDIR/usr/share
cp -r $DEPS_INSTALL_PREFIX/share/qt5 $APPDIR/usr/share
cp -r $DEPS_INSTALL_PREFIX/share/mime $APPDIR/usr/share

if [ -d $DEPS_INSTALL_PREFIX/share/color-schemes ] ; then
    cp -r $DEPS_INSTALL_PREFIX/share/color-schemes $APPDIR/usr/share
fi

if [ -d $DEPS_INSTALL_PREFIX/share/icons/breeze ] ; then
    cp -r $DEPS_INSTALL_PREFIX/share/icons/breeze $APPDIR/usr/share/icons
    cp -r $DEPS_INSTALL_PREFIX/share/icons/breeze-dark $APPDIR/usr/share/icons
fi

cp -r $DEPS_INSTALL_PREFIX/translations $APPDIR/usr/

# TO remove once we switch to the newer Qt5.12 Appimage dependency
# cp -r $DEPS_INSTALL_PREFIX/openssl/lib/*  $APPDIR/usr/lib

cp -r $DEPS_INSTALL_PREFIX/share/mlt  $APPDIR/usr/share
cp -r $DEPS_INSTALL_PREFIX/lib/mlt  $APPDIR/usr/lib
cp -r $DEPS_INSTALL_PREFIX/lib/libmlt*  $APPDIR/usr/lib
cp -r $DEPS_INSTALL_PREFIX/lib/frei0r-1  $APPDIR/usr/lib
cp -r $DEPS_INSTALL_PREFIX/bin/melt  $APPDIR/usr/bin
cp -r $DEPS_INSTALL_PREFIX/bin/ffmpeg  $APPDIR/usr/bin
cp -r $DEPS_INSTALL_PREFIX/bin/ffplay  $APPDIR/usr/bin
cp -r $DEPS_INSTALL_PREFIX/bin/ffprobe  $APPDIR/usr/bin
cp -r $DEPS_INSTALL_PREFIX/plugins/kf5  $APPIMAGEPLUGINS
cp -r $DEPS_INSTALL_PREFIX/plugins/styles  $APPIMAGEPLUGINS
cp -r $DEPS_INSTALL_PREFIX/plugins/audio  $APPIMAGEPLUGINS
cp -r $DEPS_INSTALL_PREFIX/plugins/org.kde.kdecoration2 $APPIMAGEPLUGINS
cp -r $DEPS_INSTALL_PREFIX/plugins/kstyle_breeze_config.so $APPIMAGEPLUGINS

mkdir -p $APPDIR/usr/libexec

cp -r $DEPS_INSTALL_PREFIX/lib/x86_64-linux-gnu/libexec/kf5/*  $APPDIR/usr/libexec/

cp $(ldconfig -p | grep libGL.so.1 | cut -d ">" -f 2 | xargs) $APPDIR/usr/lib/
#cp $(ldconfig -p | grep libGLU.so.1 | cut -d ">" -f 2 | xargs) $APPDIR/usr/lib/

# Step 2: Relocate x64 binaries from the architecture specific directory as required for Appimages

if [ -d $APPDIR/usr/lib/x86_64-linux-gnu/plugins ] ; then
    mv $APPDIR/usr/lib/x86_64-linux-gnu/plugins/*  $APPDIR/usr/plugins
    rm -rf $APPDIR/usr/lib/x86_64-linux-gnu/
fi

# Step 3: Update the rpath in the various plugins we have to make sure they'll be loadable in an Appimage context
for lib in $APPIMAGEPLUGINS/*.so*; do
  patchelf --set-rpath '$ORIGIN/../lib' $lib;
done

for lib in $APPIMAGEPLUGINS/audio/*.so*; do
  patchelf --set-rpath '$ORIGIN/../../lib' $lib;
done

for lib in $APPIMAGEPLUGINS/styles/*.so*; do
  patchelf --set-rpath '$ORIGIN/../../lib' $lib;
done

for lib in $APPIMAGEPLUGINS/kf5/*.so*; do
  patchelf --set-rpath '$ORIGIN/../../lib' $lib;
done

for lib in $APPIMAGEPLUGINS/org.kde.kdecoration2/*.so*; do
  patchelf --set-rpath '$ORIGIN/../../lib' $lib;
done

for lib in $APPDIR/usr/lib/mlt/*.so*; do
  patchelf --set-rpath '$ORIGIN/..' $lib;
done

### GSTREAMER
# Requires gstreamer1.0-plugins-good
GST_PLUGIN_SRC_DIR=/usr/lib/x86_64-linux-gnu/
mkdir -p $APPDIR/usr/lib/x86_64-linux-gnu
GST_LIB_DEST_DIR=$APPDIR/usr/lib/x86_64-linux-gnu/gstreamer1.0
mkdir -p $GST_LIB_DEST_DIR
GST_PLUGIN_DEST_DIR=$APPDIR/usr/lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0
mkdir -p $GST_PLUGIN_DEST_DIR
cp $GST_PLUGIN_SRC_DIR/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner $GST_PLUGIN_DEST_DIR
cp $GST_PLUGIN_SRC_DIR/gstreamer-1.0/*.so $GST_LIB_DEST_DIR

rm $GST_LIB_DEST_DIR/libgstegl* || true

for p in $GST_LIB_DEST_DIR/libgst*.so*; do
  patchelf --set-rpath '$ORIGIN/../..' $p;
done

### end of GSTREAMER STUFF

# Step 4: Move plugins to loadable location in AppImage

# Make sure our plugin directory already exists
if [ ! -d $APPIMAGEPLUGINS ] ; then
    mkdir -p $APPIMAGEPLUGINS
fi

# mv $PLUGINS/* $APPIMAGEPLUGINS

# copy icon
cp $APPDIR/usr/share/icons/breeze/apps/48/kdenlive.svg $APPDIR

# Step 5: Build the image!!!
#linuxdeployqt $APPDIR/usr/bin/ffmpeg
#linuxdeployqt $APPDIR/usr/bin/ffplay
#linuxdeployqt $APPDIR/usr/bin/ffprobe
#linuxdeployqt $APPDIR/usr/bin/melt

linuxdeployqt $APPDIR/usr/share/applications/org.kde.kdenlive.desktop \
  -executable=$APPDIR/usr/bin/kdenlive \
  -qmldir=$DEPS_INSTALL_PREFIX/qml \
  -verbose=2 \
  -bundle-non-qt-libs \
  -extra-plugins=$APPDIR/usr/lib/mlt,$APPDIR/usr/plugins,$APPDIR/usr/qml \
  -exclude-libs=libnss3.so,libnssutil3.so,libGL.so.1

#  -appimage \

rm $APPDIR/usr/lib/libGL.so.1 || true
rm $APPDIR/usr/lib/libasound.so.2 || true


# libxcb and libxcb-dri{2,3} should be removed
rm $APPDIR/usr/lib/libxcb.so* || true
rm $APPDIR/usr/lib/libxcb-dri{2,3}.so* || true

rm $APPDIR/usr/lib/libgcrypt.so.20 || true

rm  $APPDIR/AppRun

cat > $APPDIR/AppRun << EOF
#!/bin/bash

DIR="\`dirname \"\$0\"\`" 
DIR="\`( cd \"\$DIR\" && pwd )\`"
export APPDIR=\$DIR


export LD_LIBRARY_PATH=\$DIR/usr/lib/:\$LD_LIBRARY_PATH
export XDG_DATA_DIRS=\$DIR/usr/share/:\$XDG_DATA_DIRS
export XDG_CONFIG_HOME=\$HOME/.config/
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
export GST_PLUGIN_SCANNER=\$DIR/usr/lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner
export GST_PLUGIN_PATH=\$DIR/usr/lib/x86_64-linux-gnu/gstreamer1.0/

kdenlive --config kdenlive-appimagerc \$@
EOF
chmod +x $APPDIR/AppRun

# Step 5: Find out what version of Kdenlive we built and give the Appimage a proper name
cd $BUILD_PREFIX/kdenlive-build/
KDENLIVE_VERSION=$(grep "KDENLIVE_VERSION" config-kdenlive.h | cut -d '"' -f 2)

# Also find out the revision of Git we built
# Then use that to generate a combined name we'll distribute
cd $KDENLIVE_SOURCES
if [[ -d .git ]]; then
	GIT_REVISION=$(git rev-parse --short HEAD)
	VERSION=$KDENLIVE_VERSION-$GIT_REVISION
else
	VERSION=$KDENLIVE_VERSION
fi

# Return to our build root
cd $BUILD_PREFIX

appimagetool $APPDIR

# Generate a new name for the Appimage file and rename it accordingly
APPIMAGE=kdenlive-"$VERSION"-x86_64.appimage
mv Kdenlive-x86_64.AppImage $APPIMAGE

