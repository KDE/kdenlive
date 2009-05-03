Name:           kdenlive
Version:        0.7
Release:        1%{?dist}
Summary:        Non-linear video editor

Group:          Applications/Multimedia
License:        GPL
URL:            http://kdenlive.org/
Source0:        kdenlive-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  mlt, mlt++, soprano-devel
Requires:       kdebase

%description
Kdenlive is a non-linear video editor for GNU/Linux and FreeBSD
which supports DV, HDV and AVCHD (not complete yet) editing.

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
   /usr/share/kde4/apps/kdenlive/*
   /usr/share/kde4/services/westleypreview.desktop
   /usr/share/locale/*/LC_MESSAGES/kdenlive.mo
   /usr/share/mime/packages/kdenlive.xml
   /usr/share/mime/packages/westley.xml

%changelog
