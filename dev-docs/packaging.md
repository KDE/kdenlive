# Package Kdenlive

## Overview

Package Type     | OS      | Script Location
---------------- | ------- | ----------------------------------------
Appimage         | Linux   | [Craft Blueprints Repository][craft]
Flatpak          | Linux   | [Flathub Repository][flatpak]
Nightly Flatpak  | Linux   | [../packaging/flatpak][nightly-flatpak]
Snap             | Linux   | [KDE Snapcraft Repository][snap]
exe              | Windows | [Craft Blueprints Repository][craft]
dmg              | macOS   | [Craft Blueprints Repository][craft]

## How to package

* For **Flatpak** you can find instructions in the README.md file of the [nightly script folder][nightly-flatpak].

* For **Snap** there is a [KDE guide][snap-kde-guide] with instructions.

* For **Craft** based packing the command is `craft --package kdenlive`. For further instructions see https://community.kde.org/Craft.

### PPA

#### Permissions

You need to be in the kdenlive team on launchpad so you can push into git clones hosted there, to update the code

#### Repositories

The source code reposities of Kdenlive and MLT are mirrors of the original repositories and get updated automatically. You can not push to these repositories manually.

For translations and packaging data there are `*-packaging` repositories:


| Software | Command to clone repositories                                                   |
| -------- | ------------------------------------------------------------------------------- |
| MLT      | `git clone git+ssh://$USER@git.launchpad.net/~kdenlive/+git/mlt-packaging`      |
| Kdenlive | `git clone git+ssh://$USER@git.launchpad.net/~kdenlive/+git/kdenlive-packaging` |

Replcace `$USER` by your Launchpad username.

#### Translations

*(Only Kdenlive)*

The kdenlive-packaging repository contains a `po` branch containing the `po/` directory that is merged by KDE release service into [official tarballs][kde-release-tars]. You need to copy and past and push this manually.

At the moment this applies only to the release recipe, for master builds translations are in the source repository now. With 22.12 this will probably also be the case for release so this step will be obsolete.

#### Packaging data

The `*-packaging` repositores on Launchpad host a `debian` branch with the packaging data (from debian/ubuntu + few adjustments from Vincent)

* package metadata (like build or runtime dependencies) are in `debian/control` file
* build instructions are in `debian/rules` (highly automated)
* install instructions into sub-packages are in `debian/*.install`

##### Modify packaging data

It is recommended to test your packaging before pushing to builders with a command like `debuild -us -uc`

#### Trigger build

"master" builds are automatic every night; "stable" builds are triggered manually:

| Software | Url                                                          |
| -------- | ------------------------------------------------------------ |
| MLT      | https://code.launchpad.net/~kdenlive/+recipe/mlt-release     |
| Kdenlive | https://code.launchpad.net/~kdenlive/+recipe/kdenlive-stable |

1. Scroll down to "Recipe content"
2. Update the deb-version number
3. Update the branch or tag you want to load on the 2nd line
4. That's it for the base operation!

[flatpak]: https://github.com/flathub/org.kde.kdenlive
[nightly-flatpak]: ../packaging/flatpak
[snap]: https://invent.kde.org/packaging/snapcraft-kde-applications/-/tree/Neon/release/kdenlive
[snap-kde-guide]: https://community.kde.org/Guidelines_and_HOWTOs/Snap
[craft]: https://invent.kde.org/packaging/craft-blueprints-kde/-/blob/master/kde/kdemultimedia/kdenlive
[kde-release-tars]: https://download.kde.org/stable/release-service
