#!/bin/bash


# Halt on errors
set -e

# Be verbose
set -x

echo "++ building dependencies and frameworks..."
. /kdenlive/packaging/appimage/01-dependencies.sh

echo "++ building dependencies and frameworks DONE"

echo "++ building AV libraries (ffmpeg, OpenCV) ..."
. /kdenlive/packaging/appimage/02-avlibs.sh

echo "++ building AV libraries (ffmpeg, OpenCV) DONE"

echo "++ building MLT and Kdenlive ..."
. /kdenlive/packaging/appimage/03-kdenlive.sh

echo "++ building MLT and Kdenlive DONE"

echo "++ Preparing AppImage folder ..."
. /kdenlive/packaging/appimage/04-create-app.sh

echo "++ Preparing AppImage folder DONE"
