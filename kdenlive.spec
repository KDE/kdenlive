Name:           kdenlive
Version:        0.7
Release:        1%{?dist}
Summary:        Non-linear video editor

Group:          Applications/Multimedia
License:        GPL
URL:            http://www.kdenlive.org/
Source0:        kdenlive-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  mlt-dev, mlt++-dev, libavformat-dev, libsdl-dev, kdelibs4-dev, libqt4-dev
Requires:       kdelibs4

%description
Kdenlive is a non-linear video editor for GNU/Linux, which supports DV, HDV and AVCHD(not complete yet) editing.

%prep
%setup -q


%build
cmake -DCMAKE_INSTALL_PREFIX=/usr CMakeLists.txt
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%doc

%{_bindir}/kdenlive
 /usr/bin/kdenlive
 /usr/share/apps/kdenlive/kdenliveui.rc
 /usr/share/apps/kdenlive/kdenlive.notifyrc
 /usr/share/config.kcfg/kdenlivesettings.kcfg
 /usr/share/applications/kde/kdenlive.desktop
 /usr/share/icons/oxygen/scalable/mimetypes/application-x-kdenlive.svgz
 /usr/share/icons/oxygen/scalable/mimetypes/video-mlt-playlist.svgz
 /usr/share/icons/oxygen/64x64/apps/./kdenlive.png
 /usr/share/icons/oxygen/48x48/apps/./kdenlive.png
 /usr/share/icons/oxygen/32x32/apps/./kdenlive.png
 /usr/share/icons/hicolor/64x64/apps/./kdenlive.png
 /usr/share/icons/hicolor/48x48/apps/./kdenlive.png
 /usr/share/icons/hicolor/32x32/apps/./kdenlive.png
 /usr/share/mime/packages/kdenlive.xml
 /usr/share/mime/packages/westley.xml
 /usr/bin/kdenlive_render
 /usr/lib/kde4/westleypreview.so
 /usr/share/kde4/services/westleypreview.desktop
 /usr/share/apps/kdenlive/effects/automask.xml
 /usr/share/apps/kdenlive/effects/boxblur.xml
 /usr/share/apps/kdenlive/effects/brightness.xml
 /usr/share/apps/kdenlive/effects/charcoal.xml
 /usr/share/apps/kdenlive/effects/chroma_hold.xml
 /usr/share/apps/kdenlive/effects/chroma.xml
 /usr/share/apps/kdenlive/effects/freeze.xml
 /usr/share/apps/kdenlive/effects/gamma.xml
 /usr/share/apps/kdenlive/effects/greyscale.xml
 /usr/share/apps/kdenlive/effects/invert.xml
 /usr/share/apps/kdenlive/effects/ladspa_declipper.xml
 /usr/share/apps/kdenlive/effects/ladspa_equalizer.xml
 /usr/share/apps/kdenlive/effects/ladspa_limiter.xml
 /usr/share/apps/kdenlive/effects/ladspa_phaser.xml
 /usr/share/apps/kdenlive/effects/ladspa_pitch_scale.xml
 /usr/share/apps/kdenlive/effects/ladspa_pitch.xml
 /usr/share/apps/kdenlive/effects/ladspa_rate_scale.xml
 /usr/share/apps/kdenlive/effects/ladspa_reverb.xml
 /usr/share/apps/kdenlive/effects/ladspa_room_reverb.xml
 /usr/share/apps/kdenlive/effects/ladspa_vinyl.xml
 /usr/share/apps/kdenlive/effects/mirror.xml
 /usr/share/apps/kdenlive/effects/mute.xml
 /usr/share/apps/kdenlive/effects/normalise.xml
 /usr/share/apps/kdenlive/effects/obscure.xml
 /usr/share/apps/kdenlive/effects/rotation.xml
 /usr/share/apps/kdenlive/effects/sepia.xml
 /usr/share/apps/kdenlive/effects/sox_echo.xml
 /usr/share/apps/kdenlive/effects/sox_flanger.xml
 /usr/share/apps/kdenlive/effects/sox_pitch.xml
 /usr/share/apps/kdenlive/effects/sox_reverb.xml
 /usr/share/apps/kdenlive/effects/sox_vibro.xml
 /usr/share/apps/kdenlive/effects/threshold.xml
 /usr/share/apps/kdenlive/effects/volume.xml
 /usr/share/apps/kdenlive/effects/wave.xml
 /usr/share/apps/kdenlive/effects/fadein.xml
 /usr/share/apps/kdenlive/effects/fadeout.xml
 /usr/share/apps/kdenlive/effects/frei0r_squareblur.xml
 /usr/share/apps/kdenlive/effects/frei0r_distort0r.xml
 /usr/share/apps/kdenlive/export/profiles.xml
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-add-clip.png
 /usr/share/apps/kdenlive/icons/hicolor/22x22/actions/./kdenlive-add-clip.png
 /usr/share/apps/kdenlive/icons/hicolor/32x32/actions/./kdenlive-add-clip.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-add-color-clip.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-add-text-clip.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-add-slide-clip.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-show-video.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-show-audio.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-show-videothumb.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-show-audiothumb.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-show-markers.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-snap.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-insert-rect.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-align-vert.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-align-hor.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-hide-video.png
 /usr/share/apps/kdenlive/icons/hicolor/16x16/actions/./kdenlive-hide-audio.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-snap.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-show-videothumb.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-show-video.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-show-markers.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-show-audiothumb.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-show-audio.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-insert-rect.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-hide-video.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-hide-audio.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-align-vert.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-align-hor.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-add-text-clip.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-add-slide-clip.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-add-color-clip.png
 /usr/share/apps/kdenlive/icons/oxygen/16x16/actions/./kdenlive-add-clip.png
 /usr/share/apps/kdenlive/icons/hicolor/scalable/actions/./kdenlive-select-tool.svgz
 /usr/share/apps/kdenlive/icons/hicolor/scalable/actions/./kdenlive-add-clip.svgz
 /usr/share/apps/kdenlive/icons/hicolor/scalable/actions/./kdenlive-zone-end.svgz
 /usr/share/apps/kdenlive/icons/hicolor/scalable/actions/./kdenlive-zone-start.svgz
 /usr/share/apps/kdenlive/icons/oxygen/scalable/actions/./kdenlive-zone-start.svgz
 /usr/share/apps/kdenlive/icons/oxygen/scalable/actions/./kdenlive-zone-end.svgz
 /usr/share/apps/kdenlive/icons/oxygen/scalable/actions/./kdenlive-select-tool.svgz
 /usr/share/apps/kdenlive/banner.png
 /usr/share/apps/kdenlive/timeline_nothumbs.png
 /usr/share/apps/kdenlive/timeline_athumbs.png
 /usr/share/apps/kdenlive/timeline_vthumbs.png
 /usr/share/apps/kdenlive/timeline_avthumbs.png
 /usr/share/apps/kdenlive/transition.png
 /usr/share/apps/kdenlive/metadata.properties
 /usr/share/apps/kdenlive/blacklisted_effects.txt
 /usr/share/apps/kdenlive/blacklisted_transitions.txt
 /usr/share/locale/it/LC_MESSAGES/kdenlive.mo
 /usr/share/locale/ca/LC_MESSAGES/kdenlive.mo
 /usr/share/locale/cs/LC_MESSAGES/kdenlive.mo
 /usr/share/locale/da/LC_MESSAGES/kdenlive.mo
 /usr/share/locale/de/LC_MESSAGES/kdenlive.mo
 /usr/share/locale/es/LC_MESSAGES/kdenlive.mo
 /usr/share/locale/fr/LC_MESSAGES/kdenlive.mo
 /usr/share/locale/nl/LC_MESSAGES/kdenlive.mo
 /usr/share/locale/zh/LC_MESSAGES/kdenlive.mo

%changelog
