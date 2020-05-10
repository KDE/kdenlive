# Coding and Resources

* Qt5
  * [All Qt5 classes][qt5c]
  * [Signals and Slots][qt-sig]
* [MLT introduction][mlt-intro].
* [The KDE Frameworks][kf]

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
