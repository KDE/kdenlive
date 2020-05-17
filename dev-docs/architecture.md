# Architecture

*This document describes Kdenlive’s architecture top-down.*

* Top-down architecture
  * Dependencies MLT etc.
  * MVC
  * GUI elements and their counterpart


## Architectural Overview

Kdenlive uses a number of libraries. The most important library is MLT which is
responsible for the core video editing functionality: the process of applying
effects to clips which are organised in tracks and timeline(s). Kdenlive
provides the user interface for this functionality.

Kdenlive and MLT use a number of resources like frei0r for video effects.

```
              ┌────────┐   
              │Kdenlive├─────┐
              └────┬───┘     │
    Render projects│         │Configure
 Decode audio/video│         │effect settings
                   │         │        
              ┌────┴───┐     │        
              │  MLT   ├─────┤             
              └────────┘     │Audio/Video Effects
                             ├───────┬──────┬─────────┐
                          ┌──┴───┐┌──┴───┐┌─┴─┐┌──────┴──────┐
                          │frei0r││LADSPA││SoX││libsamplerate│
                          └──────┘└──────┘└───┘└─────────────┘
```
<!-- http://marklodato.github.io/js-boxdrawing/ for drawing -->


## Class Diagram

A class diagram can be generated with Doxygen and GraphViz with the commands
below. Instead of docker, you can also run `doxygen` directly.

```bash
# Generate a Doxyfile (configuration file)
docker run -it --rm -v $(pwd):/data hrektts/doxygen doxygen -g

# Now edit the file and set the following variables:
# EXTRACT_ALL   = YES
# HAVE_DOT      = YES
# UML_LOOK      = YES
# RECURSIVE     = YES
# INPUT         = src

# Now run Doxygen to generate the docs and UML files
docker run -it --rm -v $(pwd):/data hrektts/doxygen doxygen Doxyfile
```
