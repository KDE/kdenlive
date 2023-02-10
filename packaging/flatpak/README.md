# Package Kdenlive as Flatpak

## Stable Version
The manifest for the stable Kdenlive version on Flathub is hosted on https://github.com/flathub/org.kde.kdenlive

## Nightly Version
The nightly flatpak is on the kde flatpak repository (`kdeapps`). The build scripts for this repository are living at https://invent.kde.org/packaging/flatpak-kde-applications but in Kdenlives case it is just a link to the `org.kde.kdenlive-nightly.json` file in this folder. It is build and published each day by the [KDE Binary Factory](https://binary-factory.kde.org/job/Kdenlive_x86_64_flatpak/)

## How to build

First you need to setup `flatpak` and [`flatpak-builder`](https://docs.flatpak.org/en/latest/flatpak-builder.html). On ubuntu you do it by running
```
sudo apt install flatpak flatpak-builder
```

To build Kdenlive you also need some dependencies:
```
flatpak install org.kde.Sdk
```

To build and install the Flatpak you can now switch to `packaging/flatpak` insider the Kdenlive source code folder and run:

```
flatpak-builder ~/flatpak-buildir org.kde.kdenlive-nightly.json --install
```

Read the flatpak build instructions for further general instructions on the flatpak build system: https://docs.flatpak.org/en/latest/building.html

Tip: use `flatpak make-current master` or `flatpak make-current stable` to switch the current version for `flatpak run org.kde.kdenlive`

## Check for updates of dependencies

The flatpak manifest contains `x-checker-data` for [flatpak-external-data-checker](https://github.com/flathub/flatpak-external-data-checker).

That means that after you installed it with

```
flatpak install --from https://dl.flathub.org/repo/appstream/org.flathub.flatpak-external-data-checker.flatpakref
```

you can simply run

```
flatpak run org.flathub.flatpak-external-data-checker org.kde.kdenlive-nightly.json
```

to get a report about all dependencies with new versions available.
