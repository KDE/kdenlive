# Coding and Resources

* Qt5
  * [All Qt5 classes][qt5c]
  * [Signals and Slots][qt-sig]
* [MLT introduction][mlt-intro].
* [The KDE Frameworks][kf]
  * [XMLGUI Technology][xmlgui-tut] (e.g. for the `kdenliveui.rc` file)


## Locale handling

Locales are important e.g. for formatting numbers in a way that the user is
familiar with. Some countries would write `12500.42` as `12.000,42`, for
example.

Since 20.08, Kdenlive follows the following rules:

* When parsing data (by Kdenlive or by other libraries like MLT), **the `C`
  locale is used.** This is especially important for project files. The reason
  is that for passing data between programs, the format has to be well-defined
  and *not* depend on where the user happens to live.
* When presenting data to the user, the user’s locale is used.

MLT uses the C locale which is set by `setlocale()` and which must be set to
`C`. If it is set to e.g. `hu_HU.utf-8`, which uses `,` as decimal separator,
properties are converted to this format upon saving the project file, and the
project file ends up corrupted.

In Kdenlive, `QLocale` should only be used in one case: when data is shown to
the user or read from the user. Usually that is handled by Qt already. A
`QDoubleSpinBox`, for example, presents the double value in the user’s local
number format.


## Configuration

Named settings are stored in [`kdenlivesettings.kcfg`][sett]. To add a new
setting with default value, add an entry in the settings file, for example:

```xml
<entry name="logscale" type="Bool">
  <label>Use logarithmic scale</label>
  <default>true</default>
</entry>
```

The setting can then be read and written as follows:

```cpp
// Read
bool logScale = KdenliveSettings::logscale();

// Write
KdenliveSettings::setLogscale(true);
```

[sett]: ../src/kdenlivesettings.kcfg
[mlt-intro]: https://www.mltframework.org/docs/framework/
[qt5c]: https://doc.qt.io/qt-5/classes.html
[qt-sig]: https://doc.qt.io/qt-5/signalsandslots.html
[kf]: https://api.kde.org/frameworks-api/frameworks-apidocs/frameworks/index.html
[xmlgui-tut]: https://techbase.kde.org/Development/Architecture/KDE4/XMLGUI_Technology
