# How To Create Kdenlive Appimage

## Preparations (only need to be done once)
### Setup docker
1. Install docker from your package manager

2. Download docker config files from https://invent.kde.org/sysadmin/ci-tooling/-/tree/master/system-images/appimage-1604 into a new folder [or clone the repository and switch to `system-images/appimage-1604`]

3. Change to the new folder `cd appimage-1604`

4. Build `docker build -t kde-binary-factory .`

5. Create the container and start it `docker run -i -t kde-binary-factory:latest /bin/bash`

### Prepare Container
Inside the container:

1. Install git `apt-get install git`

2. Change to home directory `cd /home/appimage` (Attention: `cd ~` does not work!)

3. Create work directory `mkdir appimage-workspace`

4. Clone kdenlive repository `https://invent.kde.org/multimedia/kdenlive.git`

5. If you want to build a specific branch, now is the time, for example: `cd kdenlive && git checkout -b timeline2 origin/refactoring_timeline` (Otherwise skip this step)

## Start docker container

If you left the docker container get the containers id using `docker ps -a` and enter your docker container again by `docker start -a -i f7001a5ae294` (where `f7001a5ae294` is the id)

## Update Kdenlive

`cd /home/appimage/kdenlive && git fetch && git pull`

## Build Appimage

1. Set environment `export PATH=/home/appimage//tools/bin/:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin`

2. Build dependencies `/home/appimage/kdenlive/packaging/appimage/build-dependencies.sh /home/appimage//appimage-workspace/ /home/appimage/kdenlive/`

3. Build Kdenlive `/home/appimage/kdenlive/packaging/appimage/build-kdenlive.sh /home/appimage//appimage-workspace/ /home/appimage/kdenlive/`

4. Generate Appimage `/home/appimage/kdenlive/packaging/appimage/build-image.sh /home/appimage//appimage-workspace/ /home/appimage/kdenlive/`

Your appimage is finally located at `/home/appimage/appimage-workspace`. Do `ls /home/appimage/appimage-workspace` to view all files in the directory.

## Use the appimage on the host
1. If not already done leave the container by `exit`

2. On the host use `cd` to got to the location you want to copy your appimage to.

3. Copy appimage using `docker cp f7001a5ae294:/home/appimage/appimage-workspace/kdenlive-21.03.70-fd09a00-x86_64.appimage .` (You need to replace `f7001a5ae294` by your containers id and `kdenlive-21.03.70-fd09a00-x86_64.appimage` by the name of the appimage)

4. Make the appimage executable by `chmod +x kdenlive-21.03.70-fd09a00-x86_64.appimage`

5. And run it! `./kdenlive-21.03.70-fd09a00-x86_64.appimage`
