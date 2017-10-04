#!/bin/bash


# Halt on errors
set -e

# Be verbose
set -x

echo "++ building dependencies and frameworks..."
. ./01-dependencies.sh

echo "++ building dependencies and frameworks DONE"

echo "++ building AV libraries (ffmpeg, OpenCV) ..."
. ./02-avlibs.sh

echo "++ building AV libraries (ffmpeg, OpenCV) DONE"

echo "++ building MLT and Kdenlive ..."
. ./03-kdenlive.sh

echo "++ building MLT and Kdenlive DONE"

echo "++ Preparing AppImage folder ..."
. ./04-create-app.sh

echo "++ Preparing AppImage folder DONE"
