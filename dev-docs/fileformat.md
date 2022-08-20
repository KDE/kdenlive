<!--
    SPDX-FileCopyrightText: 2021-2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: CC0-1.0
-->

# Kdenlive File Format

Kdenlive's project files (`.kdenlive` files) use an XML format, based on MLT's format (see [MLT’s XML documentation][mlt-xml-doc] and [MLT’s DTD/document type definition][mlt-xml-dtd]) to describe the source media used in a project, as well as the use of that media as in the timeline. For most media, such as video, audio, and images, Kdenlive stores only a reference in a project, but not the media itself. Only some media gets stored directly inside Kdenlive’s project files, most notably Kdenlive title and color clips.

Important aspects of this file format decision are:

* **MLT is able to directly render Kdenlive project files.** MLT simply ignores all the additional Kdenlive-specific project data and just sticks to its rendering information. The Kdenlive-specific data is the additional icing on top that makes working with projects much easier than editing at the (lower) rendering level.
* **Kdenlive can directly include and work with MLT rendering files,** just the same way it works with other media. In fact, Kdenlive’s library clips are simply MLT rendering files, and nothing more.

There are different file format generations depending on the version of Kdenlive you're using.

## History and Generations

### Generation 1

Generation 1 (gen-1) projects were created and edited the last time using Kdenlive up to 0.9.10. These are the Kdenlive versions developed for KDE 4.x. (While this isn’t strictly true, we won’t go into discussions about the early KDE 3.x versions of Kdenlive.)

The gen-1 projects contain a lot of project data that needs to be duplicated into the outer MLT XML data in order to MLT understanding what it needs to do. As an unfortunate consequence of this data duplicity, gen-1 projects had the habit of getting the outer MLT data out of sync with the inner Kdenlive project data. For instance, sometimes the effects as rendered got out of sync with the effect parameters as set in the Kdenlive user interface. As the proper remedy for this situation, the Kdenlive developers switched to Generation 2 as part of their porting Kdenlive to the KDE Frameworks 5 (KF5).

### Generation 2: KF5

Used in Kdenlive versions 15.04 to 17.08

With the KF5/Qt5 version of Kdenlive, we now store all Kdenlive data through MLT's xml module to remove the data duplicity from the Kdenlive project file structure. This fixed project data model results in much more stable Kdenlive project behavior. As another positive side effect, it’s now much easier to manually edit or create Kdenlive XML project files from outside Kdenlive.

#### Slightly Invalid XML?

When you need to process Kdenlive project files using XML tools, there’s a gotcha to be aware in case of gen-2 project files. As soon as a project makes use of track-wide effects, the project XML as written by Kdenlive becomes invalid. You won’t notice this when editing Kdenlive project files just inside Kdenlive, and render them using MLT. You will only notice when editing them using standard XML tools.

The reason is that gen-2 project files used their own attributes inside a separate “kdenlive” namespace. Unfortunately, Kdenlive forgets to declare this additional namespace it uses at the beginning of every XML project document. As a simple fix, you’ll need to add the missing namespace yourself before processing the Kdenlive project file with XML tools:

```xml
<?xml version='1.0' encoding='utf-8'?>

<mlt xmlns:kdenlive="http://www.kdenlive.org/project" ...>

  ...

</mlt>
```

### Generation 3: Timeline 2

Used in Kdenlive versions 19.04.0 to 20.04.3

Generation 3 projects are those projects created or edited using Kdenlive versions with the new Timeline 2 engine.

### Generation 4: Comma / Point

Used in Kdenlive versions since 20.08.0, Kdenlive document version: 1.00

With version 20.08.0 a major refactoring of the project file fixed a long standing [issue with the decimal separator][comma-point-issue] (comma/point) conflict causing many crashes. Projects created with 20.08 forward are not backwards compatible.

### Generation 5: Current format

Since Generation 4 there has not been a new major generation, however the Kdenlive file format evolves constantly to support new features.

Here is a list of noteworth changes:

* 20.12.0: Mixes (in-track-transitions) were introduced

## Project XML Structure

The overall structure of the XML data inside Kdenlive project files is roughly as illustrated next:

```xml
<mlt producer="main bin">

  <!-- definition of rendering profile -->

  <profile/>


  <!-- definition of master clips, as well as derived producers -->

  <producer id="#"/>

  <producer id="#_video"/>

  <producer id="#_playlist"/>

  <producer id="slowmotion:#:#:#"/>

  ...


  <!-- project settings, and more -->

  <playlist id="main bin">


  <property name="kdenlive:...">...</property>

    ...

  </playlist>


  <producer id="black"/>


  <!-- the individual timeline tracks -->

  <playlist id="playlist#"/>

  ...


  <!-- the main tractor is the last producer in the document,

       so MLT takes it as the default for playout -->

  <tractor id="maintractor">

    <track producer="..."/>

    ...


    <!-- all transitions -->

    <transition id="transition#"/> <!-- user transitions -->

    <transition id="transition#"> <!-- internally added trans -->

      <property name="internal_added">237</property>

    </transition>


  </tractor>

</mlt>
```

We avoid storing any inner Kdenlive project data that’s already present in the outer MLT data layer, this means that all information must be stored in MLT objects like Tractor, Playlist, Producer, etc.

To separate these properties from other MLT properties, we prefix them with "kdenlive:". This page lists the properties that we use in this new file format.
Properties applied to project clips (MLT Producer object)

| Name                 | Description |
| -------------------- | ----------- |
| kdenlive:clipname    | Stores the name that will be displayed for this clip in the Project Bin. |
| kdenlive:folderid    | Stores a string containing the id of the folder where this clip is (empty if clip is in the root folder). |
| kdenlive:zone_in     | Stores the "in" point for the play zone defined for this clip. |
| kdenlive:zone_out    | Stores the "out" point for the play zone defined for this clip. |
| kdenlive:originalurl | Stores the clip's original url. Useful to retrieve original url when a clip was proxied. |
| kdenlive:proxy       | Stores the url for the proxy clip, or "-" if no proxy should be used for this clip. |

### Project Bin

The (project) bin is represented by an MLT `<playlist>` element with the well-known id “main bin”. This element holds project-specific (meta) data, the bin folders as well as their hierarchy, clip groups, and some more stuff.

> **Note**
>
> While Kdenlive now mainly uses the term “bin” when talking about what formerly was the “project bin”, the early internal name “name bin” has stuck since the early days of Kdenlive.

Properties applied to the Bin Playlist (MLT Playlist object "main bin"):

| Name                    | Description |
| ----------------------- | ----------- |
| kdenlive:folder.xxx.yyy | This property stores the names of folders created in the Project Bin. xxx is the id of the parent folder (-1 for root) and yyy is the id for this folder. The value of this property is the name of the folder. |

### Timeline Tracks

The setup of the timeline tracks is represented by an MLT `<tractor>` element with the well-known id “maintractor“. The elements inside are the individual timeline tracks: these are referenced, with the actual tracks then being MLT producers (see next).

The indivdual Kdenlive timeline tracks can be found in form of MLT `<playlist>` elements. They can also easily be identfied according to their “playlist#” id naming scheme; here, “#” is a number internally maintained by Kdenlive. The tracks feature additional properties that describe their title, locking state, and some more.

There is one semi-internal track here, the built-in “black” track. As you can see here, while it is built into Kdenlive, it isn’t so into MLT. Instead, to MLT this is just another track that happen to be created by Kdenlive. Kdenlive never shows this background track in the timeline as an individual track. But you can reference it from transitions (the composite ones, that is).

### Subtitle Track

While for the user it feels like this subtitle track is a track like an audio or video track, in fact it is not. It is a "fake" track and internally a filter, the [`avfilter.subtitles`][subtitle-mlt-doc] filter to be more precise (see also [this ffmpeg doc][subtile-ffmpeg-doc]). If an internal avfilter.subtitles is detected, the subtitle track gets enabled.

The subtitles are stored in a `*.srt` file next to the project. If your project is named `mymovie.kdenlive`, the subtitle file will be `mymovie.kdenlive.srt`. This file will only be update if you save your project, but the internal subtitle model is not sufficient during work since the `avfilter.subtitles` needs a `*.srt` file to always show the up-to-date state of work in the monitor. Therefor there is another `*.srt` file in the temp directory (`/tmp` on Linux).

This is how the subtitle trakc is represented in the kdenlive xml:

```xml
  <filter id="filter9">
   <property name="mlt_service">avfilter.subtitles</property>
   <property name="internal_added">237</property>
   <property name="av.filename">/tmp/1654509160006.srt</property>
   <property name="kdenlive:locked">1</property>
  </filter>
```

### Transitions (Compositions)

Transitions come in two (three) flavors, but always represented by MLT `<transition>` elements:

* user transitions are the ones added by, you’ve guessed it … users. These are the transitions currently shown slightly moved down and overlaying timeline tracks and their clips.
* internally added transitions are basically a convenience to make the timeline do automatic audio mixing across multiple tracks, and (with more recent Kdenlive versions) transparent tracks.

### Same-Track-Transitions (Mixes)

TODO

# Filters (Effect)

TODO


### Not Shown

* bin clip producers
* proxy clips

## Further Reading

* The [Kdenlive Project Analyzer][project-analyizer-blog] is kind of a (crazy) simple Kdenlive doctor, showing some statistics and details when given a Kdenlive gen-2 project file. You can even try the Kdenlive Project Analyzer [online][project-analyizer-tool] using Chrome/Chromium or Firefox. [Sources are available on GitHub.][project-analyizer-code]

[mlt-xml-doc]: https://www.mltframework.org/docs/mltxml/
[mlt-xml-dtd]: https://github.com/mltframework/mlt/blob/master/src/modules/xml/mlt-xml.dtd
[subtitle-mlt-doc]: https://www.mltframework.org/plugins/FilterAvfilter-subtitles/
[subtile-ffmpeg-doc]: https://ffmpeg.org/ffmpeg-filters.html#subtitles-1
[comma-point-issue]: https://invent.kde.org/multimedia/kdenlive/-/issues/78
[project-analyizer-blog]: https://thediveo-e.blogspot.de/2016/07/inside-kdenlive-projects-analyzer.html
[project-analyizer-tool]: https://thediveo.github.io/kdenlive-project-analyzer/kdenlive-project-analyzer.html
[project-analyizer-code]: https://github.com/TheDiveO/kdenlive-project-analyzer


