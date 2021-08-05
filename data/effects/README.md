# Effects (and Transitions)

Kdenlive uses MLT for all video/audio effects/filters.
For filters that provide metadata the GUI can be generated automatically.
If the generated GUI is not sufficient a custom one can be build using a XML
file describing the effect and its parameters.

## Important notes
* Effects can be blacklisted in `kdenlive/data/blacklisted_effects.txt`. All effects with a custom XML GUI need to be blacklisted

* Effects can be added to "Main effects" list in `kdenlive/data/preferred_effects.txt`

* Effects can be assigned to an effect category in `kdenlive/data/kdenliveeffectscategory.rc`.

* Kdenlive parses the effect folder at each startup, so that if you have an XML file describing a new effect,
just copy it to your `~/.kde/share/apps/kdenlive/effects/` folder and restart Kdenlive to enable the new effect.

## The basic structure of a XML filter description:
```xml
01 <!DOCTYPE kpartgui>
02 <effect tag="mlt_filter" id="mlt_filter_custom1">
03      <name>Filter name</name>
04      <description>Filter the image</description>
05      <author>Anon</author>
06      <parameter type="constant" name="amount" default="10" min="0" max="1000" factor="1000">
07              <name>Amount of filtering</name>
08      </parameter>
09      <parameter type="bool" name="enable" default="0">
10              <name>Enable</name>
11      </parameter>
15 </effect>
```

Line 1:
* required to make strings used in the effect translatable (see [here](https://api.kde.org/frameworks/ki18n/html/prg_guide.html))

Line 2:

| tag name     | description    |
| :------------| :------------- |
| `tag`        | MLT ("mlt_service") name of the effect (see [MLT Docs](https://www.mltframework.org/docs/)) |
| `id`         | internal kdenlive id, can be anything, but must be unique for each effect |
| `type`       |  _(default = `"video"`)_ whether effect modifies video or audio (use `"audio"` then) |
| `unique`     | _(default = `"0"`)_ this effect cannot be attached multiple times to one clip (speed, fades, ...) |
| `version`    | _(optional)_ minimum version of the effect required to be available (works only if the MLT filter provides the necessary metadata) |
| `dependency` | _(optional)_ ) MLT ("mlt_service") name of an effect or composition this asset depends on. If the dependency is not available this asset will not be available in Kdenlive too|

Line 3:
* name of the effect that will appear to the user

Line 4:
* Short description of the effect to be shown in the effects list
* Additionally a <full> part can be added inside. It's content will be available in the effect stack (see [frei0r_lightgraffiti.xml](frei0r_lightgraffiti.xml) for an example):
  * supports HTML formatting (requires the use of CDATA)

Line 5:
* name of the author(s) of the filter (not of the XML file ;))

The rest:

### list of tags for `<parameter>...</parameter>`

| tag name  | description    |
| :-------- | :------------- |
| `name`    | visible name of the parameter (depending on the GUI this parameter uses) |
| `comment` | _(optional)_ description of the parameter (support HTML formatting) (not yet supported by all widgets) |

### list of attributes for `<parameter ...>`
| attribute name | description    |
| :------------- | :------------- |
| `name`         | MLT filter parameter name |
| `paramprefix`  | a string to be prepended to the parameter value before passing it to MLT |
| `suffix`       | a string to be appended to the parameter (for UI display only) |
| `min`          | the minimal accepted value |
| `max`          | the maximal accepted value |
| `visualmin`    | the minimal value displayed in timeline keyframes (can be > than min) |
| `visualmax`    | the maximal value displayed in timeline keyframes (can be < than max) |
| `default`      | initial value, format depends on parameter type |
| `value`        |  |
| `optional`     | if it is set, it means that this parameter can have an empty value. So then loading a project, don't set its value to default |
| `type`         | widget (GUI) to use. See section below for possible values

For double values these placeholders are available:

| placeholder    | Header Two     |
| :------------- | :------------- |
| `%maxWidth `   | width of the current profile  |
| `%maxHeight`   | height of the current profile |
| `%width`       | synonym for `%maxWidth`       |
| `%height`      | synonym for `%maxHeight`      |
| `%out`         | the out position of the current item |
| `%fade"`       | the default fade duration (can be configured by the user) |



#### values for attribute `type`

##### `"fixed"`
* sets a (MLT filter) parameter, but does not expose it to the user (no GUI)

##### `"constant"`
* number
* represented by a slider
* ###### additional parameter attributes:
| attribute name | description    |
| :------------- | :------------- |
| `factor`       | _(optional)_ values coming from MLT will be multiplied with factor |
| `offset`       | _(optional)_ will be added to values coming from MLT after `factor` is applied |
| `min`          | smallest value possible (after multiplying with `factor`) |
| `max`          | largest value possible (after multiplying with `factor`)  |
| `suffix`       | _(optional)_ displayed unit of the value


##### `"double"`
* synonym for `"constant"`


##### `"bool"`
* true/false
* represented by a checkbox


##### `"switch"`
* 2 possible options defined by strings (max / min)
* represented by a checkbox


##### `"list"`
* multiple choice
* represented by a drop-down menu
* ###### additional parameter attributes:
| attribute name | description    |
| :------------- | :------------- |
| `paramlist`    | list of possible values separated by semicolon (no whitespaces!). Special keyword `%lumaPaths` available to show files in the applications luma directories |
* ###### additional tags:
| tag name           | description    |
| :----------------- | :------------- |
| `paramlistdisplay` | _(optional)_ list of names to use for the values separated by comma |


##### `"position"`
* time stored as frame number
* represented by a slider


##### `"color"`
* color value, similar to representation HTML (`"#rrggbb"`/`"#aarrggbb"` or `"0xrrggbbaa"`)
* represented by a button opening the KDE color dialog + a color picker button
* ###### additional attributes:
| attribute name | description    |
| :------------- | :------------- |
| `alpha`       | _(default = `"0"`)_ use to enable alpha support |


##### `"keyframe"`
* keyframable number
* keyframes are opt-in (only one keyframe by default -> should be preferred over "constant" whenever possible)
* works with MLT filters that utilize start/end values
* same attributes as "constant"
* ###### additional attributes:
| attribute name | description    |
| :------------- | :------------- |
| `factor`       | _(optional)_ values coming from MLT will be multiplied with factor |
| `intimeline`   | _(default = `"0"`)_ parameter to preselect for editing in the timeline (only one parameter can have `"1"`) |
| `widget`       | _(optional)_ GUI based on the standard keyframe GUI (possible values: `"corners"`) |


##### `"simplekeyframe"`
* works with MLT filters that use mlt_geometry for keyframe support (includes all frei0r filters)
* same attributes as "keyframe"


##### `"geometry"`
* a rectangle: position + dimension + additional value
* works with MLT filters using mlt_geometry
* the rect can be edited on the project monitor
* ###### additional attributes:
| attribute name | description    |
| :------------- | :------------- |
| `fixed`        | _(default = `"0"`)_ use to disable keyframe support |
| `showrotation` | _(default = `"0"`)_ use to enable support to 3 axis rotation |
| `opacity`      | _(default = `"true"`)_ use to disable support of the opacity setting |

You can set `default` to `"adjustcenter"`  to adjust the geometry to the frame size


##### `"url"`
* url/path
* represented by button to open "file open" dialog
* ###### additional attributes:
| attribute name | description    |
| :------------- | :------------- |
| `filter`       | Filter for file extensions. Example : `"*.cpp *.cc *.C\|C++ Source Files\n*.h *.H\|Header files"` or as using MIME type: `"image/png text html"` |
| `mode`         | _(optional)_ Default is empty = open. `"save"` means none-exsisting files can be selected and gui label is "save"

<!-- Attention if you see this comment (i.e. your editor does not support markdown), note that the string above is probably not show right. Please consider "*.cpp *.cc *.C|C++ Source Files\n*.h *.H|Header" to be right -->


##### `"urllist"`
* url/path
* represented by button to open "file open" dialog (like `url`) but in addition the file can be selected from a predefined list (like `"list"`) and it has support for KNewStuff (e.g. https://store.kde.org)
* ###### additional attributes:
| attribute name | description    |
| :------------- | :------------- |
| `filter`       | Filter for file extensions. Example : `"Source Files (*.cpp *.cc *.C);;Header files (*.h *.H)"` (warning: this format is different to `url`!) |
| `newstuff` | _(optional)_ KNewStuff config file (usually placed in `kdenlive/data` and added to to `kdenlive/src/uiresources.qrc` so the value looks like `":data/kdenlive_wipes.knsrc"`). If this is empty no download button is shown|
| `paramlist`    | list of possible values separated by semicolon (no whitespaces!). Special keywords `%lumaPaths` and `%lutPaths` are available to show files in the applications luma/lut directories |
* ###### additional tags:
| tag name         | description    |
| :--------------- | :------------- |
| `paramlistdisplay` | _(optional)_ list of names to use for the values separated by comma |

##### `"wipe"`
* special GUI for the wipe transition makes it possible to select a direction of a slide


##### `"addedgeometry"`
* parameter linked to a "geometry" parameter


##### `"curve"`
* cubic curve editor for the frei0r color curves filter (old version)


##### `"bezier_spline"`
* cubic Bézier spline editor for the frei0r color curves filter (new version, might be reused for other filters)


##### `"roto-spline"`
* GUI for the rotoscoping filter (spline on the monitor)


##### `"keywords"`
* Text entry with a selection of possible keywords to be inserted in the text.
* ###### additional tags:
| attribute name | description    |
| :------------- | :------------- |
| `keywords` | list of possible keyword values separated by semicolon |
| `keywordsdisplay` | list of names to use for the values separated by semicolon |


##### `"fontfamily"`
* Font typeface entry


##### `"readonly"`
* Data (usually an animated geometry) that can be pasted to clipboard or dragged/dropped on another geometry parameter. Cannot be modified directly by user.
