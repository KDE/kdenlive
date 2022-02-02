# MLT Concepts

This article gives you some basic understanding about at least some MLT concepts. MLT is the media engine Kdenlive relies on for composing and rendering audio and video.

Please understand that we can’t and don’t strive to give an extensive and exhaustive introduction to MLT. Instead, we see this as a kind of layman’s intro to foster some rough first understanding of the workings of MLT in order to understand how Kdenlive makes use of MLT.

If you want to learn more about MLT please read the official [MLT Framework Design][mlt-docs] documentation.

## The Basic Four Services of MLT

At MLT’s core are its four basic services (or types of services):

* **producers** are sources of individual audio and video frames. As we’ll see later, there are basic (simple) producers, as well as more complex ones.
* **consumers** are the sinks for audio and video frames. Examples are a screen display such as Kdenlive’s monitors, or an audio/video encoder such as AAC/H.264 that writes video container files.
* **filters** (in MLT parlance) sit somewhere in between the producers and consomers. These filters modify frames as they are passing through them. In Kdenlive’s user interface, you usually see filters as effects.
* **transitions** (in MLT parlance) combine two input frames into a single new output frame. See below for more details.

### Producers

So producers produce frames. In the MLT framework, they produce frames only when asked to do so, because MLT adheres to pulling frames; that’s why there are tractors and consumers.

Alas, the producer services can be further divided into:

* **basic producers** produce frames directly from audio/video sources; these sources may be files, devices, title clips, color clips, and other sources. See MLT basic producers for more details.
* **playlists** are sequential containers for producers and optional intermediate blank spaces.
* **tractors** are …
* **multitracks** are …

#### Basic Producers

MLT comes with a lot of producers, which we cannot all tackle here. Instead, we’re just shining some light one what we think will be the most notable producers in the context of Kdenlive. These are (in order of appearance):

* **avformat** and **avformat-novalidate** for reading the myriad of different audio/video media format using FFmpeg
* **timewarp** for speeing up or slowing down audio/video media
* **pixbuf** for reading and understanding the many different image formats
* **kdenlivetitle** for rendering Kdenlive titles
* **color/colour** producer for producing uni-color frames

##### Avformat Producer

The `avformat` producer is probably the most-used staple producer that produces frames from audio/video media files using the famous FFmpeg multimedia handling library. Whatever you can throw successfully at FFmpeg to decode or encode it, can thus be used in Kdenlive. For more details, please refer to MLT’s avformat producer documentation.

##### Avformat-novalidate Producer

This variant of the `avformat` producer named `avformat-novalidate` is used to speed up loading playlists with known good media files. (See also this MLT commit message about avformat-novalidate.)

##### Timewarp Producer

The `timewarp` producer is relatively new that allows video and audio to be sped up (max. 20x) or slowed down (down to 0.01x). It can even reverse audio and video within the same range between 0.01x and 20x.

Timewarp actually is an encapsulated producer. See also the MLT timewarp producer documentation. Kdenlive uses this producer internally to emulate the speed effect (which isn’t, technically spoken, an MLT filter, but an MLT producer).

##### Pixbuf Producer

The `pixmap` producer produces frames from a wide range of bitmap and vector graphics formats. Under the hood, it uses the GDK pixbuf, hence its name (see also the
MLT pixbuf producer documentation). Some notable features of this producer are:

* it supports image sequences. For this, the resource property (that is, path and filename) needs to contain the wildcard “/.all.” (note the trailing dot). The producer then loads all image files matching the following file extensions from the path.
* some of the supported image file formats are:
 * PNG (.png)
 * JPEG (.jpg, jpeg)
 * SVG (.svg)
 * TIFF
 * XPM
 * …as well as many more.

##### Kdenlivetitle Producer

Feed the `kdenlivetitle` producer some XML and it will produce frames containing beautiful titles. The XML describing a title has to be directly included with this service in its `xmldata` property.
Color/Colour Producers

This producer has two names: `color` and `colour`. It simply produces frames of only a single color. The color and alpha information is encoded in the `resource` property of the producer. This resource property can take on these values:

* a **32bit hex value** in textual form of RRGGBBAA. Here, RR is the 8bit red channel value ranging from `00` to `ff`. Similar, GG and BB encode the 8bit green and blue channel values, respectively. AA is an 8 bit alpha value, with `ff` denoting full opacity, and `00` meaning full transparency.
* few **well-known colo(u)r names**, in particular, `black` and `white`.

**Note:** Kdenlive uses the `colour`(!) producer for its timeline-internal lowest black track. In contrast, Kdenlive uses `color` producers for all the (project) bin color clips. Now that’s politically correct!

### Tractors

MLT has a somewhat unique design in that it doesn’t make use of a rendering tree. Instead, MLT uses what it terms “networks” (in quotes, see also the [MLT Framework Design][mlt-docs] document). These networks connect producers to consumers, with filters (for the effects) and transitions (for mixing) in between.

An important element for pulling frames from producers on multiple (timeline) tracks and coordinating them correctly in time in order to arrive at a final frame for output is MLT’s tractor. Yes, it’s really called a “tractor”

**Note:** later on we will learn about playlists, which are a (sub)type of producers. The important difference between tractors and playlists is, that playlists can use multiple producers only one after another (that is, sequentially). In contrast, tractors can use producers in parallel at the same time.

### Consumers

TODO

### Filters

TODO

### Transitions

The MLT term transition will probably confuse a lot of readers; more so, the longer they have worked with other non-linear video editors. Simply spoken, MLT’s transitions are mixers that combine exactly two input frames into a new single output frame.

**Note:** this is slightly simplified on purpose; more technically spoken, MLT’s transitions still outputs two frames, where the so-called “B frame” comes out unmodified, and only the “A frame” is mixed from the input A and B frames.

[mlt-docs]: https://www.mltframework.org/docs/framework/

