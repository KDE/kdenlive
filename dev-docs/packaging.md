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

[flatpak]: https://github.com/flathub/org.kde.kdenlive
[nightly-flatpak]: ../packaging/flatpak
[snap]: https://invent.kde.org/packaging/snapcraft-kde-applications/-/tree/Neon/release/kdenlive
[snap-kde-guide]: https://community.kde.org/Guidelines_and_HOWTOs/Snap
[craft]: https://invent.kde.org/packaging/craft-blueprints-kde/-/blob/master/kde/kdemultimedia/kdenlive
