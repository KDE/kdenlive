Name:           kdenlive
Version:        0.7beta1
Release:        1%{?dist}
Summary:        Non-linear video editor

Group:          Applications/Multimedia
License:        GPL
URL:            http://www.kdenlive.org/
Source0:        kdenlive-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  mlt, mlt++, soprano-devel
Requires:       kdebase

%description
Kdenlive is a non-linear video editor for GNU/Linux, which supports 
DV, HDV and AVCHD(not complete yet) editing.

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
   /usr/bin/kdenlive_render
   /usr/lib/kde4/westleypreview.so
   /usr/share/applications/kde/kdenlive.desktop
   /usr/share/config.kcfg/kdenlivesettings.kcfg
   /usr/share/icons/oxygen/scalable/mimetypes/application-x-kdenlive.svgz
   /usr/share/icons/oxygen/scalable/mimetypes/video-mlt-playlist.svgz
   /usr/share/kde4/apps/kdenlive/banner.png
   /usr/share/kde4/apps/kdenlive/effects/automask.xml
   /usr/share/kde4/apps/kdenlive/effects/boxblur.xml
   /usr/share/kde4/apps/kdenlive/effects/brightness.xml
   /usr/share/kde4/apps/kdenlive/effects/charcoal.xml
   /usr/share/kde4/apps/kdenlive/effects/chroma.xml
   /usr/share/kde4/apps/kdenlive/effects/chroma_hold.xml
   /usr/share/kde4/apps/kdenlive/effects/fadein.xml
   /usr/share/kde4/apps/kdenlive/effects/fadeout.xml
   /usr/share/kde4/apps/kdenlive/effects/freeze.xml
   /usr/share/kde4/apps/kdenlive/effects/gamma.xml
   /usr/share/kde4/apps/kdenlive/effects/greyscale.xml
   /usr/share/kde4/apps/kdenlive/effects/invert.xml
   /usr/share/kde4/apps/kdenlive/effects/ladspa_declipper.xml
   /usr/share/kde4/apps/kdenlive/effects/ladspa_equalizer.xml
   /usr/share/kde4/apps/kdenlive/effects/ladspa_limiter.xml
   /usr/share/kde4/apps/kdenlive/effects/ladspa_phaser.xml
   /usr/share/kde4/apps/kdenlive/effects/ladspa_pitch.xml
   /usr/share/kde4/apps/kdenlive/effects/ladspa_pitch_scale.xml
   /usr/share/kde4/apps/kdenlive/effects/ladspa_rate_scale.xml
   /usr/share/kde4/apps/kdenlive/effects/ladspa_reverb.xml
   /usr/share/kde4/apps/kdenlive/effects/ladspa_room_reverb.xml
   /usr/share/kde4/apps/kdenlive/effects/ladspa_vinyl.xml
   /usr/share/kde4/apps/kdenlive/effects/mirror.xml
   /usr/share/kde4/apps/kdenlive/effects/mute.xml
   /usr/share/kde4/apps/kdenlive/effects/normalise.xml
   /usr/share/kde4/apps/kdenlive/effects/obscure.xml
   /usr/share/kde4/apps/kdenlive/effects/rotation.xml
   /usr/share/kde4/apps/kdenlive/effects/sepia.xml
   /usr/share/kde4/apps/kdenlive/effects/sox_echo.xml
   /usr/share/kde4/apps/kdenlive/effects/sox_flanger.xml
   /usr/share/kde4/apps/kdenlive/effects/sox_pitch.xml
   /usr/share/kde4/apps/kdenlive/effects/sox_reverb.xml
   /usr/share/kde4/apps/kdenlive/effects/sox_vibro.xml
   /usr/share/kde4/apps/kdenlive/effects/threshold.xml
   /usr/share/kde4/apps/kdenlive/effects/volume.xml
   /usr/share/kde4/apps/kdenlive/effects/wave.xml
   /usr/share/kde4/apps/kdenlive/export/profiles.xml
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-add-clip.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-add-color-clip.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-add-slide-clip.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-add-text-clip.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-align-hor.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-align-vert.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-insert-rect.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-show-audio.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-show-audiothumb.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-show-markers.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-show-video.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-show-videothumb.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/16x16/actions/kdenlive-snap.png
   /usr/share/kde4/apps/kdenlive/icons/hicolor/scalable/actions/kdenlive-select-tool.svgz
   /usr/share/kde4/apps/kdenlive/icons/hicolor/scalable/actions/kdenlive-zone-end.svgz
   /usr/share/kde4/apps/kdenlive/icons/hicolor/scalable/actions/kdenlive-zone-start.svgz
   /usr/share/kde4/apps/kdenlive/kdenlive.notifyrc
   /usr/share/kde4/apps/kdenlive/kdenliveui.rc
   /usr/share/kde4/apps/kdenlive/metadata.properties
   /usr/share/kde4/apps/kdenlive/timeline_athumbs.png
   /usr/share/kde4/apps/kdenlive/timeline_avthumbs.png
   /usr/share/kde4/apps/kdenlive/timeline_nothumbs.png
   /usr/share/kde4/apps/kdenlive/timeline_vthumbs.png
   /usr/share/kde4/apps/kdenlive/transition.png
   /usr/share/kde4/services/westleypreview.desktop
   /usr/share/locale/de/LC_MESSAGES/kdenlive.mo
   /usr/share/locale/zh/LC_MESSAGES/kdenlive.mo
   /usr/share/mime/packages/kdenlive.xml
   /usr/share/mime/packages/westley.xml




%changelog
