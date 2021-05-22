![](data/pics/kdenlive-logo.png)

| Jenkins CI Name | Master / Nightly | Stable |
| --------------- | ---------------- | ------ |
| OpenSuse Qt 5.15 | [![Build Status](https://build.kde.org/job/Applications/job/kdenlive/job/kf5-qt5%20SUSEQt5.15/badge/icon)](https://build.kde.org/job/Applications/job/kdenlive/job/kf5-qt5%20SUSEQt5.15//) |[![Build Status](https://build.kde.org/job/Applications/job/kdenlive/job/stable-kf5-qt5%20SUSEQt5.15/badge/icon)](https://build.kde.org/job/Applications/job/kdenlive/job/stable-kf5-qt5%20SUSEQt5.15/)|
| FreeBSD Qt 5.15 | [![Build Status](https://build.kde.org/job/Applications/job/kdenlive/job/kf5-qt5%20FreeBSDQt5.15/badge/icon)](https://build.kde.org/job/Applications/job/kdenlive/job/kf5-qt5%20FreeBSDQt5.15/) |[![Build Status](https://build.kde.org/job/Applications/job/kdenlive/job/stable-kf5-qt5%20FreeBSDQt5.15/badge/icon)](https://build.kde.org/job/Applications/job/kdenlive/job/stable-kf5-qt5%20FreeBSDQt5.15/)|
| Flatpak | [![Build Status](https://binary-factory.kde.org/job/Kdenlive_x86_64_flatpak/badge/icon)](https://binary-factory.kde.org/job/Kdenlive_x86_64_flatpak/) | See [here](https://flathub.org/builds/#/apps/org.kde.kdenlive)|
| Nightly Appimage | [![Build Status](https://binary-factory.kde.org/job/Kdenlive_Nightly_Appimage_Build/badge/icon)](https://binary-factory.kde.org/job/Kdenlive_Nightly_Appimage_Build/) | |
| Nightly Appimage Dependency | [![Build Status](https://binary-factory.kde.org/job/Kdenlive_Nightly_Appimage_Dependency_Build/badge/icon)](https://binary-factory.kde.org/job/Kdenlive_Nightly_Appimage_Dependency_Build/)  | |
| MinGW64 | [![Build Status](https://binary-factory.kde.org/job/Kdenlive_Nightly_mingw64/badge/icon)](https://binary-factory.kde.org/job/Kdenlive_Nightly_mingw64/) | [![Build Status](https://binary-factory.kde.org/job/Kdenlive_Stable_mingw64/badge/icon)](https://binary-factory.kde.org/job/Kdenlive_Stable_mingw64/) |
| macOS | [![Build Status](https://binary-factory.kde.org/job/Kdenlive_Nightly_macos/badge/icon)](https://binary-factory.kde.org/job/Kdenlive_Nightly_macos/) | [![Build Status](https://binary-factory.kde.org/job/Kdenlive_Stable_macos/badge/icon)](https://binary-factory.kde.org/job/Kdenlive_Stable_macos/) |

# About Kdenlive

[Kdenlive](https://kdenlive.org) is a Free and Open Source video editing application, based on MLT Framework and KDE Frameworks 5. It is distributed under the [GNU General Public License Version 2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).

# Building from source

[Instructions to build Kdenlive](dev-docs/build.md) are available on the KDE wiki.

# Testing Kdenlive via Nightly Builds

- AppImage (Linux): https://binary-factory.kde.org/job/Kdenlive_Nightly_Appimage_Build/
- Flatpak (Linux): Add the kde flatpak repository (if not already done) by typing `flatpak remote-add --if-not-exists kdeapps --from https://distribute.kde.org/kdeapps.flatpakrepo` on a command line. Install kdenlive nightly with `flatpak install kdeapps org.kde.kdenlive`. Use `flatpak update` to update if the nightly is already installed. _Attention! If you use the stable kdenlive flatpak already, the `*.desktop` file (e.g. responsible for start menu entry) is maybe replaced by the nightly (and vice versa). You can still run the stable version with `flatpak run org.kde.kdenlive/x86_64/stable` and the nightly with `flatpak run org.kde.kdenlive/x86_64/master` (replace `x86_64` by `aarch64` or `arm` depending on your system)_
- Windows: https://binary-factory.kde.org/job/Kdenlive_Nightly_mingw64/

*Note * - nightly/daily builds are not meant to be used in production.*

# Contributing to Kdenlive

Please note that Kdenlive's Github repo is just a mirror: see [the detailed explanations on how to submit patches](https://community.kde.org/Infrastructure/Github_Mirror).

We welcome all feedback and offers for help!

* Talk about us!
* [Report bugs](https://kdenlive.org/en/bug-reports/) you encounter (if not already done)
* Help other users [on the forum](http://forum.kde.org/viewforum.php?f=262) and bug tracker
* [Help to fill the manual](https://community.kde.org/Kdenlive/Workgroup/Documentation)
* Complete and check [application and documentation translation](http://l10n.kde.org)
* Prepare video tutorials (intro, special tricks...) in your language
  and send us a link to add in homepage or doc
* Detail improvement suggestions
  we don't test every (any?) other video editor, so give precise explanations
* Code! Help fixing bugs, improving usability, optimizing, porting...
  register on KDE infrastructure, study its guidelines, and pick from roadmap. See [here](dev-docs/contributing.md) for more information
