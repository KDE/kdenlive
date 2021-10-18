# Package Kdenlive

## Overview

Package Type     | OS      | Script Location
---------------- | ------- | ----------------------------------------
Appimage         | Linux   | [../packaging/appimage][appimage]
Flatpak          | Linux   | [Flathub Repository][flatpak]
Nightly Flatpak  | Linux   | [../packaging/flatpak][nightly-flatpak]
Snap             | Linux   | [KDE Snapcraft Repository][snap]
exe              | Windows | [Craft Blueprints Repository][craft]
dmg              | macOS   | [Craft Blueprints Repository][craft]

## How to package

* For **Appimage** and **Flatpak** you can find instructions in the README.md file of the script folder (for Flatpak the [nightly script folder][nightly-flatpak])

* For **Craft** based packing the command is `craft --package kdenlive`. For further instructions see https://community.kde.org/Craft

[appimage]: ../packaging/appimage
[flatpak]: https://github.com/flathub/org.kde.kdenlive
[nightly-flatpak]: ../packaging/appimage
[snap]: https://invent.kde.org/packaging/snapcraft-kde-applications/-/tree/Neon/release/kdenlive
[craft]: https://invent.kde.org/packaging/craft-blueprints-kde/-/blob/master/kde/kdemultimedia/kdenlive
