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
