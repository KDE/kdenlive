# Release Kdenlive ##.##

## Introduction

Kdenlive is part of KDE Gear which means furtunately a lot of work is done by the KDE release team. The KDE release team cares of tagging and releasing the source code tarballs to https://downloads.kde.org. However, the Kdenlive team still needs to take care of writing a blog post for the release and of releasing the binaries like AppImage, Windows (`*.exe`) and macOS (`*.dmg`). This process is tracked below.

The schedule of KDE Gear can be found at https://community.kde.org/Schedules

## Tasks

### Preparation (~2 weeks before release)

- [ ] For major releases: if it was decided to change the splash screen, make sure it has been merged/committed
- [ ] Update MLT and other dependencies [in Craft](https://invent.kde.org/packaging/craft-blueprints-kde/)
- [ ] Run the [render test suite](https://invent.kde.org/multimedia/kdenlive-test-suite) on stable nightly
- [ ] Open a draft merge request on the Kdenlive website [for the announcement](https://invent.kde.org/websites/kdenlive-org/-/wikis/Posts/Release-Annoucements)

### Release Binaries

Start with the following steps after the source tarballs have been released by the KDE release team:

- [ ] Open a pull request to update Kdenlive and dependencies [on Flathub](https://github.com/flathub/org.kde.kdenlive). Check if there is already a feasible pull request by another person or the flathub-bot.
- [ ] Run again the [render test suite](https://invent.kde.org/multimedia/kdenlive-test-suite) on stable nightly
- [ ] Download binaries right after the test suite succeeded from https://cdn.kde.org/ci-builds/multimedia/kdenlive/
- [ ] Create Windows standalone version:
    - Extract the 7zip file
    - Put a file `qt.conf` with the following content into the `bin` folder (to help Qt find `qwindows.dll` in platforms/folder, see https://github.com/owncloud/client/issues/7034):
        ```
        [Paths]
        Plugins = .
        ```
    - Compress as self-extracting exe file with 7zip    
- [ ] [Upload](https://download.kde.org/README_UPLOAD) all binaries for https://download.kde.org/stable/kdenlive/. The structure and file names should be like:
    - 25.08/
        - linux/
            - kdenlive-25.08.0-x86_64.AppImage
        - macOS/
            - kdenlive-25.08.0-arm64.dmg
            - kdenlive-25.08.0-x86_64.dmg
        - windows/
            - kdenlive-25.08.0.exe
            - kdenlive-25.08.0_standalone.exe
- [ ] Wait for sysadmins to do the actual publishing

### Announcement and Documentation

- [ ] Write the announcement (see [here](https://invent.kde.org/websites/kdenlive-org/-/wikis/Posts/Release-Annoucements) for technical instructions)

    1. Take the input from the full log with all commits
    2. Start with a general paragraph or two about what has been done in the release cycle
    3. Then add detailed sections of each feature with a structure like this:  
        - Title of feature
        - Explanation
        - Descriptive images
    4. It is good practice to put a list of important fixes or changes after all the features.
    4. Finish it with the full log [(to be put to `changelog.md`)](https://invent.kde.org/websites/kdenlive-org/-/wikis/Posts/Release-Annoucements#change-log)
- [ ] After binaries are published: Publish the announcement by merging the merge request. Make sure date and time are set correctly in UTC in the `date` tag of the release announcement. It needs to be set to a time prior the merge time, otherwise it won't show up.
- [ ] [Merge documentation update](https://invent.kde.org/documentation/docs-kdenlive-org/-/merge_requests)
- [ ] Notify KDE Promo
- Publish on social media
    - [ ] Verify a post on KDE Discuss has automatically been created (triggered when the website post goes online)
    - [ ] [Mastodon](https://floss.social/@kdenlive)
    - [ ] Bluesky
    - [ ] Reddit ([Kdenlive](https://www.reddit.com/r/kdenlive/), [Linux](https://www.reddit.com/r/linux/), [KDE](https://www.reddit.com/r/kde/))
    - [ ] Discord
    - For important releases: contact websites like
        - [ ] [OMG! Ubuntu](https://www.omgubuntu.co.uk/)
        - [ ] [Libre Arts](https://librearts.org/)
    - [ ] [Kdenlive Telegram](https://t.me/kdenlive)
    - [ ] [Kdenlive Matrix](https://webchat.kde.org/#/room/#kdenlive:kde.org)
    - [ ] Other open-source Telegram channels
